#include "compiler.h"

#include "allocator.h"
#include "dynamic_arena.h"
#include "file_system.h"
#include "diagnostic.h"
#include "syntax.h"
#include "logger.h"
#include "task_system.h"
#include "foreign_code.h"

#include "translator_debug.h"
#include "translator_c.h"

extern "C" {
    #include "../lib/libtcc.h"
}



#define NULL 0



char* Compiler::mainFile = NULL;
char* Compiler::outFile = NULL;
char* Compiler::outDir = (char*) "./out";

int Compiler::command = TRANSLATE;
int Compiler::outLangs = 0;
int Compiler::debugInfo = 0;

inline void runTranslator(Translator* t) {
    t->debugInfo = Compiler::debugInfo;
    t->init(Compiler::outDir);
    t->printNode(t->mainFile, 0, (SyntaxNode*) SyntaxNode::root, NULL);
    t->printForeignCode();
    t->exit();
}

int build();



int Compiler::compile() {

    Arena::Container arena;
    alc = &arena;
    initAlloc(&arena);
    initNAlloc(&arena);

    Ast::init();
    FileSystem::init();
    TaskSystem::init(0);

    Extern::init();

    Logger::log({ Logger::INFO }, "Initialization completed\n");



    FileSystem::Handle mainFileHandle
        = FileSystem::load(mainFile, FileSystem::Origins::COMPILER_SOURCE);

    TaskSystem::beginGroup();
    TaskSystem::dispatchParse(mainFileHandle);
    TaskSystem::wait();

    Logger::log({ Logger::INFO }, "Parsing completed\n");



    TaskSystem::beginGroup();
    TaskSystem::dispatchPreValidation(mainFileHandle);
    TaskSystem::wait();

    Logger::log({ Logger::INFO }, "Pre validation completed\n");



    TaskSystem::beginGroup();
    TaskSystem::dispatchValidation(mainFileHandle);
    TaskSystem::wait();

    Logger::log({ Logger::INFO }, "Validating completed\n");



    TaskSystem::beginGroup();

    if (outLangs & ITSELF_CONSOLE_LANG) {
        TaskSystem::dispatchCodegen(&translatorDebug, mainFileHandle);
    }

    if (outLangs & C_LANG) {
        TaskSystem::dispatchCodegen(&translatorC, mainFileHandle);
    }

    TaskSystem::wait();

    Logger::log({ Logger::INFO }, "Translation completed\n");
    if (Compiler::command == TRANSLATE) return 0;



    Err::Err err = (Err::Err) build();
    if (err < 0) return err;

    Logger::log({ Logger::INFO }, "Binary generation completed...\n");



    return Err::OK;

}

int build() {

    FileSystem::Path* exePath = FileSystem::getExePath();
    const char* libPath = FileSystem::catPaths(exePath, "../tcc/lib");

    const char* tccIncPath[] = {
        #ifdef _WIN32
            FileSystem::catPaths(exePath, "../tcc/inc"),
            FileSystem::catPaths(exePath, "../tcc/inc/winapi")
        #else
            "/usr/include"
        #endif
    };
    const int tccIncPathLen = sizeof(tccIncPath) / sizeof(const char*);

    const char* resIncPath = FileSystem::catPaths(exePath, "../resources");

    TCCState *state = tcc_new();
    if (!state) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to create TCC state.\n");
        return Err::TCC_ERROR;
    }

    #ifdef __unix__
        // tcc_set_options(state, "-nostdinc");
    #endif

    if (Compiler::debugInfo) {
        tcc_set_options(state, "-ggdb");
    }

    if (tcc_set_output_type(state, TCC_OUTPUT_EXE) < 0) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to set output type.\n");
        tcc_delete(state);
        return Err::TCC_ERROR;
    }

    if (tcc_add_library_path(state, libPath) < 0) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to add library path.\n");
        tcc_delete(state);
        return Err::TCC_ERROR;
    }

    for (int i = 0; i < tccIncPathLen; i++) {
        if (tcc_add_include_path(state, tccIncPath[i]) < 0) {
            Logger::log({ Logger::ERROR }, "TCC: Failed to add include path.\n");
            tcc_delete(state);
            return Err::TCC_ERROR;
        }
    }

    if (
        tcc_add_include_path(state, resIncPath) < 0 ||
        tcc_add_include_path(state, ".") < 0
    ) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to add include path.\n");
        tcc_delete(state);
        return Err::TCC_ERROR;
    }

    #ifdef _WIN32
    if (
        tcc_add_library(state, "gdi32") < 0 ||
        tcc_add_library(state, "kernel32") < 0 ||
        tcc_add_library(state, "user32") < 0 ||
        tcc_add_library(state, "msvcrt") < 0 ||
        tcc_add_library(state, "tcc1-64") < 0
    ) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to add required libraries.\n");
        tcc_delete(state);
        return Err::TCC_ERROR;
    }
    #else

    if (tcc_add_include_path(state, "/usr/lib/gcc/x86_64-linux-gnu/11/include") < 0) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to add library path.\n");
        tcc_delete(state);
        return Err::TCC_ERROR;
    }

    if (tcc_add_library_path(state, "/usr/lib") < 0) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to add library path.\n");
        tcc_delete(state);
        return Err::TCC_ERROR;
    }

    if (
        tcc_add_library(state, "m") < 0 ||        // Math library
        tcc_add_library(state, "c") < 0 ||        // Standard C library
        tcc_add_library(state, "dl") < 0 ||       // Dynamic linking library
        tcc_add_library(state, "pthread") < 0 || // POSIX threads library
        tcc_add_library(state, "tcc1-64") < 0
    ) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to add required libraries.\n");
        tcc_delete(state);
        return Err::TCC_ERROR;
    }
    #endif

    if (
        tcc_add_file(state, "main.c") < 0
    ) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to add .c files.\n");
        tcc_delete(state);
        return Err::TCC_ERROR;
    }

    if (tcc_output_file(state, Compiler::outFile) < 0) {
        Logger::log({ Logger::ERROR }, "TCC: Failed to generate output executable.\n");
        tcc_delete(state);
        return Err::TCC_ERROR;
    }

    tcc_delete(state);

    return Err::OK;

}
