#include "task_system.h"
#include "array_list.h"
#include "interpreter.h"
#include "parser.h"
#include "task_system_core.h"
#include "config.h"
#include "validator.h"
#include <atomic>
#include <cstdint>


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

            DArray::init(&worker->stack, Config::opt.threadWorkQueueSize, sizeof(Core::Task));
            DArray::init(&worker->localStack, Config::opt.threadWorkQueueSize, sizeof(Core::Task));

            Parser::init(&worker->state.p);
            Validator::init(&worker->state.v);
            Interpreter::init(&worker->state.c);

            worker->thread = std::thread(Core::runWorker, worker);
        }
    }

    void release() {
        Core::Worker*  workers     = TaskSystem::Core::getWorkers();
        const uint64_t workerCount = TaskSystem::Core::getWorkerCount();

        for (int i = 0 ; i < workerCount; i++) {
            Core::Worker* worker = workers + i;

            DArray::release(&worker->stack);
            DArray::release(&worker->localStack);

            Interpreter::release(&worker->state.c);
            Validator::release(&worker->state.v);
            Parser::release(&worker->state.p);

            worker->amIAlive.store(false, std::memory_order_release);
            worker->hasWork.store(true, std::memory_order_release);
            worker->hasWork.notify_all();
        }

        for (int i = 0; i < workerCount; i++) {
            if (workers[i].thread.joinable()) {
                workers[i].thread.join();
            }
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
