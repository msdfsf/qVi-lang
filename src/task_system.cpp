
// This implementation utilizes a fixed-size worker pool of up to 64
// threads, where each worker's idle status is tracked via an atomic
// uint64 "check list" bitmask.
//
// When a task is enqueued, the system first attempts to secure an idle
// worker via the bitmask. If all workers are busy, a worker thread will
// push the new task to its own local queue, while the main thread will
// default to assigning the task to an existing worker's backlog.
//
// Each worker prioritizes processing its own queue first. After
// completing a task, if a worker has a backlog of multiple items, it
// attempts to distribute the extra tasks to any newly idle peers.
// If all other workers remain busy, the worker simply continues
// processing its own queue until the backlog is cleared.

#include "task_system.h"
#include "allocator.h"
#include "array_list.h"
#include "dynamic_arena.h"
#include "file_system.h"
#include "interpreter.h"
#include "parser.h"
#include "registry.h"
#include "config.h"
#include "syntax.h"
#include "translator.h"
#include "validator.h"
#include "task_status.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>



namespace TaskSystem {

    enum TaskKind {
        TK_PARSING,
        TK_PRE_VALIDATION,
        TK_VALIDATION,
        TK_COMPILE_TIME_BUILD,
    };

    struct TaskState {
        // TODO : better name?
        Parser::ParseContext p;
        Validator::ValidationContext v;
        Interpreter::CompilerState c;
    };

    union TaskArgument {
        // TK_PARSING
        FileSystem::Handle file;

        // TK_VALIDATION
        Reg::Unit*         unit;

        //
        Function*          fcn;

        // generic
        void*              ptr;
        uint64_t           val;
    };

    typedef void TaskFunction (TaskState*, TaskArgument);

    struct Task {
        TaskKind      kind;
        TaskFunction* fcn;
        TaskArgument  arg;
    };

    struct Worker {
        int id;

        std::thread thread;
        std::atomic<bool> hasWork;

        TaskState state;
        DArray::Container stack;
        // Sometimes we may need to do a bunch of sub-tasks
        // on the same thread without growing stack by recursion
        // too much. So this is a place to push them...
        DArray::Container localStack;
    };

    static Worker*  gWorkers = nullptr;
    static uint32_t gWorkerCount = 0;

    static thread_local Worker* gCurrentWorker = nullptr;
    static thread_local Arena::Container gCurrentArena;

    // The 'check list' mechanism, each thread
    // will use its own bit to write to mark if
    // its ready for the next task or not.
    // 1 - free, 0 - working
    std::atomic<uint64_t> checkList;

    // Internal counter to track the "Group" so Wait() knows when to stop
    static std::atomic<int32_t> gTaskCount = 0;



    inline uint64_t getWorkerId() {
        return gCurrentWorker->id;
    }

    inline int findFirstSetBit(uint64_t mask) {
        #if defined(_MSC_VER)
            unsigned long index;
            return _BitScanForward64(&index, mask) ? (int)index : -1;
        #else
            return (mask == 0) ? -1 : __builtin_ctzll(mask);
        #endif
    }

    void clearTaskState(TaskState* state, TaskKind kind) {

        if (kind == TK_PARSING) {
            Parser::ParseContext* ctx = &state->p;

            ctx->unit            = NULL;
            ctx->fileSpan        = NULL;
            ctx->currentScope    = NULL;
            ctx->currentFunction = NULL;
            ctx->currentLoop     = NULL;
            ctx->currentImport   = NULL;

            DArray::clear(&ctx->nodeStack);
            DArray::clear(&ctx->defStack);
            Arena::clear(&ctx->errBuff);

            ctx->varId &= THREAD_MASK;
            ctx->arrId &= THREAD_MASK;
            ctx->defId &= THREAD_MASK;
            ctx->errId &= THREAD_MASK;

            ctx->idxInScope = 0;
        } else if (kind == TK_VALIDATION || kind == TK_PRE_VALIDATION) {
            // TODO : We may separate cases if their clear requirements
            //  will differ drastically
            Validator::ValidationContext* ctx = &state->v;

            ctx->unit = NULL;
            DArray::clear(&ctx->fCandidates);
        } else if (kind == TK_COMPILE_TIME_BUILD) {
            Interpreter::CompilerState* ctx = &state->c;

            Arena::clear(&ctx->locals);
            Arena::clear(&ctx->bytecode);
            Arena::clear(&ctx->rawData);
            DArray::clear(&ctx->lines);
            OrderedDict::clear(&ctx->localsInfoMap);

            ctx->populateLocals      = false;
            ctx->currentLineSpan     = { 0, 0 };
            ctx->currentOffsetStart  = 0;
            ctx->maxAlign            = 0;
            ctx->fixedSize           = 0;
            ctx->defaultArgsSize     = 0;
            ctx->vecResult           = { 0 };
            ctx->maxArrayLiteralSize = 0;
            ctx->currentArrayLiteralOffset = 0;
            ctx->lastOpcode          = Interpreter::OC_NOP;
        } else {
            memset(state, 0, sizeof(TaskState));
        }

    }

    // returns free worker or NULL at failure
    Worker* secureWorker() {
        constexpr auto memorder = std::memory_order_relaxed;

        // for future me: memory_order_relaxed -
        //  only this operation's atomicity is guaranteed
        uint64_t checkListCopy = checkList.load(memorder);

        while (checkListCopy != 0) {
            int workerId = findFirstSetBit(checkListCopy);
            if (workerId < 0) break;

            // fetch_and returns the value of checkList
            // before the AND was applied
            const uint64_t mask = 1ULL << workerId;
            if (checkList.fetch_and(~mask) & mask) {
                return gWorkers + workerId;
            }

            checkListCopy = checkList.load(memorder);
        }

        return NULL;
    }

    void runWorker(Worker* worker) {
        gCurrentWorker = worker;

        if (!alc) {
            alc = &gCurrentArena;
            initAlloc(alc);
            initNAlloc(alc);
        }

        Parser::init(&worker->state.p);
        Validator::init(&worker->state.v);
        Interpreter::init(&worker->state.c);

        uint64_t prefixID = (uint64_t) worker->id << 56;

        worker->state.p.varId = prefixID;
        worker->state.p.arrId = prefixID;
        worker->state.p.defId = prefixID;
        worker->state.p.errId = prefixID;

        while (1) {

            worker->hasWork.wait(false);

            while (worker->stack.size > 0) {

                // TODO : As pop may invalidate its memory, we have to copy
                //        maybe its then more convinient to store a pointer
                Task task = *((Task*) DArray::getLast(&worker->stack));
                DArray::pop(&worker->stack);

                clearTaskState(&worker->state, task.kind);
                task.fcn(&worker->state, task.arg);

                // For our sanity, we ensure that all local tasks are completed
                // before moving on...
                while (gCurrentWorker->localStack.size > 0) {
                    Task* task = (Task*) DArray::getLast(&gCurrentWorker->localStack);
                    DArray::pop(&worker->stack);

                    clearTaskState(&gCurrentWorker->state, task->kind);
                    task->fcn(&gCurrentWorker->state, task->arg);
                }

                // If we have more than one job to do,
                // we try to pass them to other workers
                while (worker->stack.size > 1) {
                    Worker* subWorker = secureWorker();
                    if (!subWorker) break;

                    Task* task = (Task*) DArray::getLast(&worker->stack);
                    DArray::pop(&worker->stack);

                    DArray::push(&subWorker->stack, task);
                    subWorker->hasWork.store(true);
                    subWorker->hasWork.notify_all();
                }

            }

            worker->hasWork.store(false);
            // worker->hasWork.notify_one();

            checkList.fetch_or(1ULL << worker->id);
            checkList.notify_all();

        }
    }

    void init(uint64_t workerCount) {
        if (workerCount == 0) {
            workerCount = std::thread::hardware_concurrency();
            if (workerCount == 0) workerCount = 1;
        }
        if (workerCount > 64) workerCount = 64;

        checkList.store((1ULL << workerCount) - 1);

        gWorkerCount = workerCount;
        gWorkers = (Worker*) alloc(alc, workerCount * sizeof(Worker));

        for (int i = 0 ; i < workerCount; i++) {
            Worker* worker = gWorkers + i;

            worker->id = i;
            worker->hasWork.store(false);
            worker->thread = std::thread(runWorker, worker);

            DArray::init(&worker->stack, Config::threadWorkQueueSize, sizeof(Task));
            DArray::init(&worker->localStack, Config::threadWorkQueueSize, sizeof(Task));

            worker->thread.detach();
        }
    }

    void enqueue(Task task) {
        Worker* worker = secureWorker();

        if (worker) {
            DArray::push(&worker->stack, &task);
            worker->hasWork.store(true);
            worker->hasWork.notify_one();
        } else {
            // Everyone is busy
            if (gCurrentWorker) {
                DArray::push(&gCurrentWorker->stack, &task);
            } else {
                // We are on the main thread, we will assign it
                // to a 'random' worker
                Worker* worker = gWorkers + 0;

                DArray::push(&worker->stack, &task);
                worker->hasWork.store(true);
                worker->hasWork.notify_one();
            }
        }
    }



    static void runParse(TaskState* state, TaskArgument arg) {
        state->p.unit = Reg::get(arg.file);
        Parser::parse(&state->p, arg.file);

        FileSystem::FileInfo* finfo = FileSystem::getFileInfo(arg.file);
        finfo->status.store(FileSystem::FS_READY, std::memory_order_release);

        gTaskCount.fetch_sub(1, std::memory_order_relaxed);
        gTaskCount.notify_one();
    }

    void dispatchParse(FileSystem::Handle fhnd) {
        using namespace FileSystem;
        constexpr auto memorder = std::memory_order_relaxed;

        FileInfo* finfo = getFileInfo(fhnd);

        FileStatus expected = FS_DIRTY;
        if (!finfo->status.compare_exchange_strong(
            expected, FS_BUSY, memorder)) {
            return;
        }

        gTaskCount.fetch_add(1, memorder);

        Task task;
        task.arg.file = fhnd;
        task.kind = TK_PARSING;
        task.fcn = &runParse;

        enqueue(task);
    }



    static void runPreValidate(TaskState* state, TaskArgument arg) {
        state->v.unit = Reg::get(arg.file);
        Validator::preValidate(&state->v);

        FileSystem::FileInfo* finfo = FileSystem::getFileInfo(arg.file);
        finfo->status.store(FileSystem::FS_READY, std::memory_order_release);

        gTaskCount.fetch_sub(1, std::memory_order_relaxed);
        gTaskCount.notify_one();
    }

    void dispatchPreValidation(FileSystem::Handle fhnd) {
        using namespace FileSystem;

        FileInfo* finfo = getFileInfo(fhnd);

        gTaskCount.fetch_add(1, std::memory_order_relaxed);

        Task task;
        task.arg.file = fhnd;
        task.kind = TK_PRE_VALIDATION;
        task.fcn = &runPreValidate;

        enqueue(task);
    }



    static void runValidate(TaskState* state, TaskArgument arg) {
        state->v.unit = Reg::get(arg.file);
        Validator::validate(&state->v);

        FileSystem::FileInfo* finfo = FileSystem::getFileInfo(arg.file);
        finfo->status.store(FileSystem::FS_READY, std::memory_order_release);

        gTaskCount.fetch_sub(1, std::memory_order_relaxed);
        gTaskCount.notify_one();
    }

    void dispatchValidation(FileSystem::Handle fhnd) {
        // Are we sure about FileSystem::Handle being interface?
        using namespace FileSystem;
        constexpr auto memorder = std::memory_order_relaxed;

        FileInfo* finfo = getFileInfo(fhnd);

        gTaskCount.fetch_add(1, memorder);

        Task task;
        task.arg.file = fhnd;
        task.kind = TK_VALIDATION;
        task.fcn = &runValidate;

        enqueue(task);
    }



    // So, for now we wait for task to finish if it happens that
    // other thread is already doing the job. Later we may want
    // to immediately return value from a run function that worker
    // can continue working on next task in queue if available...
    // Although, in case of not local task, its risky to do that,
    // as we may start working on big task and block the progression...

    bool secureFunction(Function* fcn, bool waitForExecution) {
        TaskStatus expected = TS_PENDING;
        std::atomic_ref<TaskStatus> status(fcn->compilationStatus);

        if (!status.compare_exchange_strong(expected, TS_RUNNING)) {
            if (waitForExecution) {
                while (status == TS_RUNNING) {
                    status.wait(TS_RUNNING);
                    status = status.load(std::memory_order_acquire);
                }
            }
            return false;
        }

        return true;
    }

    void releaseFunction(Function* fcn) {
        std::atomic_ref<TaskStatus> status(fcn->compilationStatus);
        status.store(TaskStatus::TS_READY, std::memory_order_release);
        status.notify_all();
    }

    void runCompileTimeBuild(TaskState* state, TaskArgument arg) {
        if (secureFunction(arg.fcn, true)) {
            Interpreter::compile(&state->c, arg.fcn);
            releaseFunction(arg.fcn);
        }

        gTaskCount.fetch_sub(1, std::memory_order_relaxed);
    }

    void runCompileTimeBuildLocal(TaskState* state, TaskArgument arg) {
        if (secureFunction(arg.fcn, true)) {
            Interpreter::compile(&state->c, arg.fcn);
            releaseFunction(arg.fcn);
        }
    }

    void dispatchCompileTimeBuild(Function* fcn, bool waitForExecution) {
        if (fcn->compilationStatus == TS_READY) return;

        Task task;
        task.arg.fcn = fcn;
        task.kind = TK_COMPILE_TIME_BUILD;

        if (gCurrentWorker) {
            task.fcn = &runCompileTimeBuildLocal;
            DArray::push(&gCurrentWorker->localStack, &task);

            if (waitForExecution) {
                // We are called from context where we wait, so we act as master
                int startSize = gCurrentWorker->localStack.size - 1;
                while (gCurrentWorker->localStack.size > startSize) {
                    Task task = *(Task*) DArray::getLast(&gCurrentWorker->localStack);
                    DArray::pop(&gCurrentWorker->localStack);

                    clearTaskState(&gCurrentWorker->state, task.kind);
                    task.fcn(&gCurrentWorker->state, task.arg);
                }
            }
        } else {
            // We are for some reason the master, so, I guess,
            // we just queue the task...
            task.fcn = &runCompileTimeBuild;
            gTaskCount.fetch_add(1, std::memory_order_relaxed);
            enqueue(task);

            if (waitForExecution) {
                std::atomic_ref<TaskStatus> status(fcn->compilationStatus);
                while (status == TS_RUNNING) {
                    status.wait(TS_RUNNING);
                    status = status.load();
                }
            }
        }
    }



    void dispatchCodegen(Translator* translator, FileSystem::Handle file) {

    }

    void beginGroup() {
        gTaskCount.store(0, std::memory_order_seq_cst);
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void wait() {
        // Acquire ensures we see the finished ASTs produced by workers
        int32_t count = gTaskCount.load(std::memory_order_acquire);
        while (count > 0) {
            gTaskCount.wait(count);
            count = gTaskCount.load(std::memory_order_acquire);
        }
    }

    [[noreturn]] void panic(int code) {
        std::exit(code);
    }

}
