
// This provides a compile-time abstraction over concurrency management.
// A custom implementation of the .cpp file can be provided if needed.
//
// The goal is to give the user full control over which tasks run on
// which thread, ensuring total ownership over the program's execution
// without requiring the re-implementation of basic routines that are
// handled concurrently. For example, a user shouldn't have to rewrite
// internal logic like automatic import fetching or dependency resolution
// just to use specific threading rules or an existing thread pool.
//
// This keeps the compiler coherent while granting the freedom to
// decide exactly how and where the work happens based on specific needs.

#pragma once

#include "backend.h"
#include "file_system.h"
#include "parser.h"
#include "syntax.h"
#include <cstdint>



namespace Compiler { enum BuildCommand : uint8_t; }

namespace TaskSystem {

    // Suppose to initialize workers and prepare
    // persistent thread-local contexts.
    void init(uint64_t workerCount);

    uint64_t getWorkerId();

    // Concrete dispatch functions, as we are a part
    // of concrete compiler.
    void dispatchParse(FileSystem::Handle file);
    void dispatchPreValidation(FileSystem::Handle file);
    void dispatchValidation(FileSystem::Handle file);
    void dispatchBackend(FileSystem::Handle file, Backend::Driver* driver, Backend::BuildContext* ctx);

    // TODO : to generic input args...
    void dispatchLocalTask(Function* fcn, bool sync);

    // Suppose to start a new synchronization group.
    // Ex. resets internal counters to synchronize parallel tasks
    // that has to be globally grouped before moving on.
    void beginGroup();

    // Blocks the caller until all dispatched tasks in the current
    // group are fully processed.
    void wait();

    // Shuts down the compiler immediately.
    // Allows either to do a raw exit or trigger a safe abort.
    [[noreturn]] void panic(int code);

}
