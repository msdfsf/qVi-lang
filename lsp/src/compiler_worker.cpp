#include "compiler_worker.h"
#include "../../src/allocator.h"
#include "../../src/data_types.h"
#include "../../src/task_system.h"
#include "comm_provider.h"
#include "lsp.h"
#include <thread>



namespace CompilerWorker {

    constexpr int defaultFileCompilationCapacity = 16;
    static bool gIsAsync = false;

    struct Master {
        std::thread thread;
        std::atomic<bool> hasWork;

        TaskStatus state;
        DArray::Container stack;

        CommProvider::Info* comm;
    } master;

    // TODO: for now we rebuild everything
    // TODO: to a general type system
    void compileFileSync(FileSystem::Handle hnd) {
        if (Lsp::State::permission >= Lsp::Permission::P_PARSE) {
            TaskSystem::beginGroup();
            TaskSystem::dispatchParse(hnd);
            TaskSystem::wait();
        }

        if (Lsp::State::permission >= Lsp::Permission::P_VALIDATE) {
            TaskSystem::beginGroup();
            TaskSystem::dispatchPreValidation(hnd);
            TaskSystem::wait();

            TaskSystem::beginGroup();
            TaskSystem::dispatchValidation(hnd);
            TaskSystem::wait();
        }
    }

    void runMaster(Master* master) {
        while (true) {
            master->hasWork.wait(false);

            while (master->stack.size > 0) {
                FileSystem::Handle hnd = *((FileSystem::Handle*)DArray::getLast(&master->stack));
                DArray::pop(&master->stack);

                compileFileSync(hnd);
            }

            master->hasWork.store(false);
        }
    }

    void init(CommProvider::Info* comm, uint32_t threadCount) {
        Type::init();
        Ast::init();
        TaskSystem::init(threadCount);

        gIsAsync = threadCount == 0;

        master.comm = comm;
        master.state = TS_PENDING;
        DArray::init(&master.stack, defaultFileCompilationCapacity, sizeof(FileSystem::Handle));

        master.thread = std::thread(runMaster, &master);
        master.thread.detach();
    }

    void release() {
        Type::release();
        Ast::release();
    }

    // We can compile/recompile file-by file... parsing - obvious. validation - we touch only our symbols
    // but any file change can invalidate  other files. We have to revalidate all dependencies. We have to
    // introduce file state like pending, parsing, parsed, validating, validated and the version this state
    // is applicable to. Then we can lazily recompile/reuse all the files and make compilation work 'per-task'.
    // TODO: we need dependency graph in unit
    //       we need to have an array of global symbols:
    //           we can make diff with old version and compare check if
    //           we need to rebuild dependencies.
    // TODO: for now we use only one file to just test the lsp itself
    void enqueueCompilation(FileSystem::Handle hnd) {
        if (gIsAsync) {
            DArray::push(&master.stack, &hnd);
            master.hasWork.store(true);
            master.hasWork.notify_one();
        } else {
            compileFileSync(hnd);
        }
    }

    void cancelPendingCompilation(FileSystem::Handle fileHandle) {
        // TODO
    }

}
