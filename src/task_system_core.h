#pragma once

#include "backend.h"
#include "interpreter.h"
#include "parser.h"
#include "task_status.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>



namespace TaskSystem::Core {

    enum TaskKind {
        TK_PARSING,
        TK_PRE_VALIDATION,
        TK_VALIDATION,
        TK_BACKEND,

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

        struct {
            Reg::Unit*             unit;
            Backend::Driver*       driver;
            Backend::BuildContext* ctx;
        } backend;

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



    void initGlobalState(Worker* workers, uint64_t workerCount);
    void clearTaskState(TaskState* state, TaskKind kind);

    TaskFunction* getTaskFunction(Core::TaskKind kind);
    Worker* getCurrentWorker();
    Worker* getWorkers();

    void enqueue(Task task);
    void runWorker(Worker* worker);

    void beginGroup();
    void wait();

    [[noreturn]] void panic(int code);

}

namespace TaskSystem::Default {

    void dispatchParse(FileSystem::Handle file);
    void dispatchPreValidation(FileSystem::Handle file);
    void dispatchValidation(FileSystem::Handle file);
    void dispatchBackend(FileSystem::Handle file, Backend::Driver* driver, Backend::BuildContext* ctx);
    void dispatchCompileTimeBuild(Function* fcn, bool sync);

}
