#include "../../src/compiler.h"
#include "../test_core.h"

inline void gEtEPreSuite() {
    Compiler::command = Compiler::BC_RUN;
    Compiler::mainFile = "test_in.qvi";
    Compiler::outFile  = "test_out.qvi";
    Compiler::debugInfo = false;
    Compiler::optLevel = 0;
    Compiler::outDir = "test_out.qvi";
    Compiler::threadCount = 1;
    Compiler::cascade = false;
    Compiler::targets[0] = (Compiler::Target*)
        &Compiler::bakedTargets[Compiler::TK_VM];
    Compiler::targets[1] = NULL;

    Compiler::init();
}

inline void gEtEPostSuite() {
    Compiler::release();
}

inline void gEtEPreCase() {
}

inline void gEtEPostCase() {
}

inline void gEtEFile(const char* input, const char* output, Test::Result* result) {
    FileSystem::Handle hnd = FileSystem::loadBuffer(Compiler::mainFile, input, FileSystem::Origins::USER_START);
    Compiler::runFrontend(hnd);
    Compiler::runBackends(hnd);
}
