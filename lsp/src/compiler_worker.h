#pragma once
#include "comm_provider.h"
#include "../../src/file_system.h"
#include "../../src/dynamic_arena.h"
#include <cstdint>



namespace CompilerWorker {

    void init(CommProvider::Info* comm, uint32_t threadCount);
    void release();

    // async
    void enqueueCompilation      (FileSystem::Handle fileHandle);
    void cancelPendingCompilation(FileSystem::Handle fileHandle);

}
