#include "task_system.h"
#include "task_system_core.h"
#include "config.h"


namespace TaskSystem {

    using namespace TaskSystem::Core;

    void init(uint64_t workerCount) {
        if (workerCount == 0) {
            workerCount = std::thread::hardware_concurrency();
            if (workerCount == 0) workerCount = 1;
        }
        if (workerCount > 64) workerCount = 64;

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

    void dispatchParse(FileSystem::Handle fhnd) {
        Default::dispatchParse(fhnd);
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
