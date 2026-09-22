#pragma once

#include "../../src/compiler.h"
#include "../../src/syntax.h"
#include "../../src/diagnostic.h"
#include "../../src/registry.h"
#include "../../src/config.h"
#include "../test_core.h"
#include <cstdint>
#include <cstdio>

inline Config::Options eteDefaultOptions = Config::Options();

inline void gEtEPreSuite(bool visualize) {
}

inline void gEtEPostSuite(bool visualize) {
}

inline void gEtEPreCase(bool visualize) {
    Config::opt.loggingEnabled = visualize;

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

inline void gEtEPassPreCase(bool visualize) {
    Config::opt = eteDefaultOptions;
    gEtEPreCase(visualize);
}

inline void gEtEFailPreCase(bool visualize) {
    Config::opt = eteDefaultOptions;
    Config::opt.maxErrorCount = 16;
    Config::opt.errorRecoveryEnabled = true;
    gEtEPreCase(visualize);
}

inline void gEtEPostCase(bool visualize) {
    if (visualize) {
        IO::Stream stream = { .kind = IO::Stream::SK_C_STREAM, .cstream = stdout };

        uint32_t count;
        AstContext** asts = Diag::getAllErrorFiles(&count);

        for (int i = 0; i < count; i++) {
            AstContext* ast = asts[i];
            for (int i = 0; i < ast->errorCount; i++) {
                IO::write(&stream, (ast->errors + i)->msg);
            }
        }
    }

    Diag::clear();
    Compiler::release();
}

// TODO: use tagged union and one function?
inline void gEtEFileToPass(const char* input, Test::TestArgument arg, Test::Result* result) {
    FileSystem::Handle hnd = FileSystem::loadBuffer(Compiler::mainFile, input, FileSystem::Origins::USER_START);
    if (Compiler::runFrontend(hnd) >= 0) {
        Compiler::runBackends(hnd);
    }
}

inline void gEtEFileToFail(const char* input, Test::TestArgument arg, Test::Result* result) {
    FileSystem::Handle hnd = FileSystem::loadBuffer(Compiler::mainFile, input, FileSystem::Origins::USER_START);

    int64_t finalError;

    finalError = Compiler::runFrontend(hnd);
    if (finalError == Err::OK) {
        finalError = Compiler::runBackends(hnd);
    }
    Test::assertOrDie(finalError != Err::OK);

    Reg::Unit* unit = Reg::get(hnd);
    Test::assertOrDie(arg.diag.count == unit->ast->totalErrorCount);

    for (int i = 0; i < arg.diag.count; i++) {
        Test::ExpectedDiagnostic diag = arg.diag.data[i];
        AstError* astError = unit->ast->errors + i;

        Test::assert(diag.code, astError->err);

        if (diag.hasLine) {
            Test::assert(diag.line, astError->span->start.ln);
        }

        if (diag.kind == Test::ExpectedDiagnostic::DK_ERROR) {
            Test::assert(astError->severity == Severity::SEV_ERROR);
        } else {
            Test::assert(astError->severity == Severity::SEV_WARNING);
        }
    }
}
