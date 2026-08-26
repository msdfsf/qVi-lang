#include "../../src/task_system_core.h"
#include "../../src/config.h"
#include "lsp.h"


namespace TaskSystem {

    using namespace TaskSystem::Core;

    void init(uint64_t workerCount) {
        if (workerCount == 0) {
            workerCount = std::thread::hardware_concurrency();
            if (workerCount == 0) workerCount = 1;
        }
        if (workerCount > 64) workerCount = 64;

        // We use global allocator here, as it has to persist
        Core::Worker* workers = alloc<Core::Worker>(workerCount);
        Core::initGlobalState(workers, workerCount);

        for (int i = 0 ; i < workerCount; i++) {
            Core::Worker* worker = workers + i;

            worker->id = i;
            worker->hasWork.store(false);
            worker->thread = std::thread(Core::runWorker, worker);

            DArray::init(&worker->stack, Config::threadWorkQueueSize, sizeof(Core::Task));
            DArray::init(&worker->localStack, Config::threadWorkQueueSize, sizeof(Core::Task));

            Parser::init(&worker->state.p);
            Validator::init(&worker->state.v);
            Interpreter::init(&worker->state.c);

            worker->thread.detach();
        }
    }

    uint64_t getWorkerId() {
        return Core::getCurrentWorker()->id;
    }

     static void runParse(TaskState* state, TaskArgument arg) {
        Lsp::FileData* data = (Lsp::FileData*) FileSystem::getUserData(arg.file);

        int committedIdx = data->committedIdx.load(std::memory_order_relaxed);
        int workingIdx = 1 - committedIdx;

        // We wait for all UI queries to finish reading buffer we want to modify.
        int readerCount = data->readerCount[workingIdx].load(std::memory_order_acquire);
        while (readerCount > 0) {
            data->readerCount[workingIdx].wait(readerCount);
            readerCount = data->readerCount[workingIdx].load(std::memory_order_acquire);
        }

        Lsp::setAndClearCompilerAllocator(data->arenas + workingIdx);

        state->p.unit = Reg::get(arg.file);
        Parser::parse(&state->p, arg.file);

        Lsp::updateSemanticTokens(data);

        data->committedIdx.store(workingIdx);

        FileSystem::FileInfo* finfo = FileSystem::getFileInfo(arg.file);
        finfo->status.store(FileSystem::FS_READY, std::memory_order_release);
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

        Task task;
        task.arg.file = fhnd;
        task.kind = TK_PARSING;
        task.fcn = &runParse;

        Core::enqueue(task);
    }

    void dispatchPreValidation(FileSystem::Handle fhnd) {
        Default::dispatchPreValidation(fhnd);
    }

    void dispatchValidation(FileSystem::Handle fhnd) {
        Default::dispatchValidation(fhnd);
    }

    void dispatchBackend(FileSystem::Handle file, Backend::Driver* driver, Backend::BuildContext* ctx) {
        Default::dispatchBackend(file, driver, ctx);
    }

    void dispatchLocalTask(Function* fcn, bool sync) {
        Default::dispatchCompileTimeBuild(fcn, sync);
    }

    void beginGroup() {
        Core::beginGroup();
    }

    void wait() {
        Core::wait();
    }

    [[noreturn]] void panic(int code) {
        Core::panic(code);
    }

}
