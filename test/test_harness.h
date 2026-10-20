#pragma once

#include "../src/compiler.h"
#include "../src/emitter_drivers/emitter_driver_debug.h"
#include "../src/syntax.h"
#include "../src/diagnostic.h"
#include "../src/registry.h"
#include "../src/config.h"
#include "test_core.h"
#include <cstdint>
#include <cstdio>



// Shared Configuration
// ---

inline Config::Options gDefaultOptions = Config::Options();

inline void gTestPreCase(bool visualize, bool enableBackends = true) {
    Config::opt = gDefaultOptions;
    Config::opt.loggingEnabled = visualize;
    Config::opt.maxErrorCount = 16;
    Config::opt.errorRecoveryEnabled = true;

    Compiler::command     = Compiler::BC_RUN;
    Compiler::mainFile    = "test_in.qvi";
    Compiler::outFile     = "test_out.qvi";
    Compiler::outDir      = "test_out.qvi";
    Compiler::debugInfo   = false;
    Compiler::optLevel    = 0;
    Compiler::threadCount = 1;
    Compiler::cascade     = false;

    if (enableBackends) {
        Compiler::targets[0] = (Compiler::Target*)&Compiler::bakedTargets[Compiler::TK_VM];
        Compiler::targets[1] = NULL;
    } else {
        Compiler::targets[0] = NULL; // Frontend only (Snapshots / Syntax checking)
    }

    Compiler::init();
}

// Lifecycle hooks for Suites
inline void gPreCaseE2E(bool viz)       { gTestPreCase(viz, true); }
inline void gPreCaseFrontend(bool viz)  { gTestPreCase(viz, false); }

inline void gTestPostCase(bool visualize) {
    if (visualize) {
        IO::Stream stream = { .kind = IO::Stream::SK_C_STREAM, .cstream = stdout };
        uint32_t count;
        AstContext** asts = Diag::getAllErrorFiles(&count);
        for (int i = 0; i < count; i++) {
            AstContext* ast = asts[i];
            for (int j = 0; j < ast->errorCount; j++) {
                IO::write(&stream, (ast->errors + j)->msg);
            }
        }
    }

    Diag::clear();
    Compiler::release();
}



// Test Execution Callbacks
// ---

// End-to-End execution of a valid code (asserts program stdout, handled via test_core)
inline void gTestRunPass(const char* input, Test::TestArgument arg, Test::Result* result) {
    FileSystem::Handle hnd = FileSystem::loadBuffer(Compiler::mainFile, input, FileSystem::Origins::USER_START);
    if (Compiler::runFrontend(hnd) >= 0) {
        Compiler::runBackends(hnd);
    }
}

// End-toEnd execution of a wrong code to capture compiler diagnostic
inline void gTestRunFail(const char* input, Test::TestArgument arg, Test::Result* result) {
    FileSystem::Handle hnd = FileSystem::loadBuffer(Compiler::mainFile, input, FileSystem::Origins::USER_START);

    int64_t finalError = Compiler::runFrontend(hnd);
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
        if (diag.hasLine) Test::assert(diag.line, astError->span->start.ln);
        Test::assert(diag.kind == Test::ExpectedDiagnostic::DK_ERROR
                         ? Severity::SEV_ERROR
                         : Severity::SEV_WARNING,
                     astError->severity);
    }
}

// AST Snapshot (AST is compared to a golden dump handled via test_core)
inline void gTestSnapshotAst(const char* input, Test::TestArgument arg, Test::Result* result) {
    FileSystem::Handle hnd = FileSystem::loadBuffer(Compiler::mainFile, input, FileSystem::Origins::USER_START);

    Compiler::runFrontend(hnd);

    Reg::Unit* unit = Reg::get(hnd);
    Test::assertOrDie(unit != NULL && unit->ast != NULL);

    if (arg.outStream) {
        IO::Stream stream = { .kind = IO::Stream::SK_C_STREAM, .cstream = arg.outStream };
        Emitter::Context ctx = {
            .userData = NULL,
            .style {
                .format = Emitter::Format::PRETTY,
                .color = false,
                .indentStep = 4,
            },
            .indentLevel = 0
        };
        Emitter::driverDebug.emitUnit(&ctx, unit, &stream);
    }
}
