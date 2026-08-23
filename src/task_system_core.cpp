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

#include "task_system_core.h"
#include <atomic>



namespace TaskSystem::Core {

    inline Worker*  gWorkers = nullptr;
    inline uint32_t gWorkerCount = 0;

    inline thread_local Worker* gCurrentWorker = nullptr;
    inline thread_local Arena::Container gCurrentArena;

    // The 'check list' mechanism, each thread
    // will use its own bit to write to mark if
    // its ready for the next task or not.
    // 1 - free, 0 - working
    inline std::atomic<uint64_t> gCheckList;

    // Internal counter to track the "Group" so Wait() knows when to stop
    inline std::atomic<int32_t> gTaskCount = 0;



    static void runParse(TaskState* state, TaskArgument arg);
    static void runPreValidate(TaskState* state, TaskArgument arg);
    static void runValidate(TaskState* state, TaskArgument arg);
    static void runBackend(TaskState* state, TaskArgument arg);
    static void runCompileTimeBuild(TaskState* state, TaskArgument arg);

    TaskFunction* gTaskFunctions[] = {
        &runParse,
        &runPreValidate,
        &runValidate,
        &runBackend,
        &runBackend
    };



    TaskFunction* getTaskFunction(Core::TaskKind kind) {
        if (kind < 0 || kind > sizeof(gTaskFunctions) / sizeof(TaskFunction*)) {
            return NULL;
        }

        return gTaskFunctions[kind];
    }

    Worker* getCurrentWorker() {
        return gCurrentWorker;
    }

    Worker* getWorkers() {
        return gWorkers;
    }



    int findFirstSetBit(uint64_t mask) {
        #if defined(_MSC_VER)
            unsigned long index;
            return _BitScanForward64(&index, mask) ? (int)index : -1;
        #else
            return (mask == 0) ? -1 : __builtin_ctzll(mask);
        #endif
    }

    void initGlobalState(Worker* workers, uint64_t workerCount) {
        gCheckList.store((1ULL << workerCount) - 1);

        gWorkerCount = workerCount;
        gWorkers = workers;
    }

    void clearTaskState(TaskState* state, TaskKind kind) {
        switch (kind) {
            case TK_PARSING: {
                Parser::ParseContext* ctx = &state->p;

                ctx->unit            = NULL;
                ctx->fileSpan        = NULL;
                ctx->currentScope    = NULL;
                ctx->currentFunction = NULL;
                ctx->currentLoop     = NULL;
                ctx->currentImport   = NULL;

                DArray::clear(&ctx->nodeStack);
                DArray::clear(&ctx->defStack);

                ctx->varId &= THREAD_MASK;
                ctx->arrId &= THREAD_MASK;
                ctx->defId &= THREAD_MASK;
                ctx->errId &= THREAD_MASK;

                ctx->idxInScope = 0;
            }

            case TK_PRE_VALIDATION:
            case TK_VALIDATION: {
                // TODO : We may separate cases if their clear requirements
                //  will differ drastically
                Validator::ValidationContext* ctx = &state->v;

                ctx->unit = NULL;
                DArray::clear(&ctx->fCandidates);
            }

            case TK_BACKEND: // TODO
            case TK_COMPILE_TIME_BUILD: {
                Interpreter::CompilerState* ctx = &state->c;
                Interpreter::clear(ctx);
            }

            default: {
                memset(state, 0, sizeof(TaskState));
            }
        }
    }

    // returns free worker or NULL at failure
    Worker* secureWorker() {
        constexpr auto memorder = std::memory_order_relaxed;

        // for future me: memory_order_relaxed -
        //  only this operation's atomicity is guaranteed
        uint64_t checkListCopy = gCheckList.load(memorder);

        while (checkListCopy != 0) {
            int workerId = findFirstSetBit(checkListCopy);
            if (workerId < 0) break;

            // fetch_and returns the value of checkList
            // before the AND was applied
            const uint64_t mask = 1ULL << workerId;
            if (gCheckList.fetch_and(~mask) & mask) {
                return gWorkers + workerId;
            }

            checkListCopy = gCheckList.load(memorder);
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

                // TODO: maybe to a function to kinda have clear task
                //       contract - enqueue/release
                gTaskCount.fetch_sub(1, std::memory_order_relaxed);
                gTaskCount.notify_one();

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

            gCheckList.fetch_or(1ULL << worker->id);
            gCheckList.notify_all();

        }
    }

    void enqueue(Task task) {
        Worker* worker = secureWorker();

        Core::gTaskCount.fetch_add(1, std::memory_order_relaxed);

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

    // So, for now we wait for task to finish if it happens that
    // other thread is already doing the job. Later we may want
    // to immediately return value from a run function that worker
    // can continue working on next task in queue if available...
    // Although, in case of not local task, its risky to do that,
    // as we may start working on big task and block the progression...

    // TODO: why not acquireNode/Release
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

    static void runParse(TaskState* state, TaskArgument arg) {
        state->p.unit = Reg::get(arg.file);
        Parser::parse(&state->p, arg.file);

        FileSystem::FileInfo* finfo = FileSystem::getFileInfo(arg.file);
        finfo->status.store(FileSystem::FS_READY, std::memory_order_release);
    }

    static void runPreValidate(TaskState* state, TaskArgument arg) {
        state->v.unit = Reg::get(arg.file);
        Validator::preValidate(&state->v);

        FileSystem::FileInfo* finfo = FileSystem::getFileInfo(arg.file);
        finfo->status.store(FileSystem::FS_READY, std::memory_order_release);
    }

    static void runValidate(TaskState* state, TaskArgument arg) {
        state->v.unit = Reg::get(arg.file);
        Validator::validate(&state->v);

        FileSystem::FileInfo* finfo = FileSystem::getFileInfo(arg.file);
        finfo->status.store(FileSystem::FS_READY, std::memory_order_release);
    }

    static void runBackend(TaskState* state, TaskArgument arg) {
        arg.backend.driver->execute(arg.backend.ctx, arg.backend.unit);
    }

    static void runCompileTimeBuild(TaskState* state, TaskArgument arg) {
        if (secureFunction(arg.fcn, true)) {
            Interpreter::compile(&state->c, arg.fcn);
            releaseFunction(arg.fcn);
        }
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

namespace TaskSystem::Default {

    void dispatch(Core::TaskKind kind, Core::TaskArgument arg) {
        Core::Task task;
        task.arg  = arg;
        task.kind = kind;
        task.fcn  = getTaskFunction(kind);

        enqueue(task);
    }

    void dispatchParse(FileSystem::Handle fhnd) {
        using namespace FileSystem;

        FileInfo* finfo = getFileInfo(fhnd);

        FileStatus expected = FS_DIRTY;
        if (!finfo->status.compare_exchange_strong(
            expected, FS_BUSY, std::memory_order_relaxed)) {
            return;
        }

        Default::dispatch(Core::TK_PARSING, Core::TaskArgument { .file = fhnd });
    }

    void dispatchPreValidation(FileSystem::Handle fhnd) {
        dispatch(Core::TK_PRE_VALIDATION, Core::TaskArgument { .file = fhnd });
    }

    void dispatchValidation(FileSystem::Handle fhnd) {
        dispatch(Core::TK_VALIDATION, Core::TaskArgument { .file = fhnd });
    }

    void dispatchBackend(FileSystem::Handle file, Backend::Driver* driver, Backend::BuildContext* ctx) {
        Core::TaskArgument arg;
        arg.backend.unit = Reg::get(file);
        arg.backend.driver = driver;
        arg.backend.ctx = ctx;

        dispatch(Core::TK_BACKEND, arg);
    }

    // By setting the 'sync' flag, the task is appended to the thread's
    // local stack. The current thread is processing the task and any
    // subsequent sub-tasks it spawns iteratively.
    //
    // Note: If 'sync' is true, this function only guarantees that by
    //       the time it returns, the target task and its entire sub-task
    //       dependency tree are fully processed. It does not guarantees
    //       order of execution.
    //
    // TODO: Implement work stealing by other threads controlled either by other
    //       flag, or global TaskSystem/Compiler configuration.
    void dispatchCompileTimeBuild(Function* fcn, bool sync) {
        if (fcn->compilationStatus == TS_READY) return;

        Core::Task task;
        task.arg.fcn = fcn;
        task.fcn = &Core::runCompileTimeBuild;
        task.kind = Core::TK_COMPILE_TIME_BUILD;

        if (!Core::gCurrentWorker) {
            // We are for some reason the master, so, I guess,
            // we just queue the task as global...
            enqueue(task);

            if (sync) {
                std::atomic_ref<TaskStatus> status(fcn->compilationStatus);
                while (status == TS_RUNNING) {
                    status.wait(TS_RUNNING);
                    status = status.load();
                }
            }
        }

        DArray::push(&Core::gCurrentWorker->localStack, &task);

        if (sync) {
            const int startSize = Core::gCurrentWorker->localStack.size - 1;
            while (Core::gCurrentWorker->localStack.size > startSize) {
                Core::Task task = *(Core::Task*) DArray::getLast(&Core::gCurrentWorker->localStack);
                DArray::pop(&Core::gCurrentWorker->localStack);

                clearTaskState(&Core::gCurrentWorker->state, task.kind);
                task.fcn(&Core::gCurrentWorker->state, task.arg);
            }
        }
    }

}
