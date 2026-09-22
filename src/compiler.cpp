#include "compiler.h"

#include "allocator.h"
#include "data_types.h"
#include "file_system.h"
#include "diagnostic.h"
#include "string.h"
#include "syntax.h"
#include "logger.h"
#include "task_system.h"
#include "foreign_code.h"
#include <cstdint>



static Logger::Type logErr = { .level = Logger::ERROR, .tag = "compiler" };
static Logger::Type logInf = { .level = Logger::INFO, .tag = "compiler" };

namespace Compiler {

    // All null-terminated
    String mainFile = String(NULL);
    String outFile  = String(NULL);
    String outDir   = String("./out");

    BuildCommand command               = BC_VALIDATE;
    Target*      targets[TK_COUNT + 1] = { 0 };
    bool         debugInfo             = false;
    uint8_t      optLevel              = 0;
    bool         cascade               = false;

    uint8_t threadCount = 0;

    inline const char* getTargetName(Target* target) {
        switch (target->kind) {
            case TK_DEBUG:  return "DEBUG";
            case TK_C_LANG: return "C_LANG";
            case TK_VM:     return "VM";
            default:        return "UNKNOWN";
        }
    }



    void init() {
        allocInit();
        nallocInit();

        Type::init();
        Ast::init();
        FileSystem::init();
        Diag::init();
        TaskSystem::init(threadCount);

        Extern::init();
    }

    void clear() {
        Extern::clear();

        // TODO: maybe add clear? TaskSystem::init(0);
        Diag::init();
        FileSystem::clear();
        Ast::clear();
        Type::clear();

        allocClear();
    }

    void release() {
        Extern::release();

        TaskSystem::release();
        Diag::release();
        FileSystem::release();
        Ast::release();
        Type::release();

        allocRelease();
    }



    int64_t runFrontend(FileSystem::Handle fileHandle) {
        TaskSystem::beginGroup();
        TaskSystem::dispatchParse(fileHandle);
        TaskSystem::wait();

        if (Diag::hasErrors()) return -1; // TODO: something meaningful
        Logger::log(logInf, "Parsing completed");



        TaskSystem::beginGroup();
        TaskSystem::dispatchPreValidation(fileHandle);
        TaskSystem::wait();

        if (Diag::hasErrors()) return -1; // TODO: something meaningful
        Logger::log(logInf, "Pre validation completed");



        TaskSystem::beginGroup();
        TaskSystem::dispatchValidation(fileHandle);
        TaskSystem::wait();

        if (Diag::hasErrors()) return -1; // TODO: something meaningful
        Logger::log(logInf, "Validating completed");

        return 0;
    }

    int64_t runBackends(FileSystem::Handle fileHandle) {
        TaskSystem::beginGroup();
        {
            int i = 0;
            while (targets[i] != NULL) {
                Target* const target = targets[i];

                Backend::BuildContext ctx = {
                    .command   = Compiler::command,
                    .outDir    = Compiler::outDir,
                    .outFile   = Compiler::outFile,
                    .debugInfo = Compiler::debugInfo,
                    .cascade   = Compiler::cascade,
                };

                TaskSystem::dispatchBackend(fileHandle, target->backend, &ctx);
                i++;
            }
        }
        TaskSystem::wait();

        if (Diag::hasErrors()) return -1; // TODO: something meaningful
        return 0;
    }

    int64_t compile() {
        if (!targets[0]) {
            Logger::log(logErr, "No build targets were specified.");
            return Err::UNEXPECTED_ERROR;
        }

        {
            int i = 0;
            while (targets[i]) {
                Target* const target = targets[i];

                if (command > target->buildCapability) {
                    Logger::log(
                        logErr,
                        "Target '%s' does not support the requested build command.",
                        NULL, getTargetName(target)
                    );

                    return Err::UNEXPECTED_ERROR;
                }

                i++;
            }

            if (command == BC_RUN && i > 1) {
                Logger::log(logErr,
                    "Cannot use 'run' command with multiple targets simultaneously. "
                    "Please specify a single target (e.g., --target vm).");
                return Err::UNEXPECTED_ERROR;
            }
        }



        init();
        Logger::log(logInf, "Initialization completed");

        // TODO: we should record final status in context, so we can check
        //       here for errors that werent registered as errors...

        FileSystem::Handle mainFileHandle
            = FileSystem::load(mainFile, FileSystem::Origins::COMPILER_SOURCE);
        if (mainFileHandle == FileSystem::null) {
            Logger::log(logErr,
                "Failed to load entry-point file: '%.*s'.\n"
                "Ensure the path is correct and the file is not locked by another process.",
                NULL, mainFile.len, mainFile.buff);

            return Err::FILE_LOAD_FAILED;
        }



        runFrontend(mainFileHandle);
        if (command == BC_VALIDATE) return 0;

        runBackends(mainFileHandle);

        Logger::log(logInf, "Compilation completed");



        release();
        return Err::OK;
    }

}
