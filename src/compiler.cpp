#include "compiler.h"

#include "allocator.h"
#include "dynamic_arena.h"
#include "file_system.h"
#include "diagnostic.h"
#include "io.h"
#include "registry.h"
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


    inline const char* getTargetName(Target* target) {
        switch (target->kind) {
            case TK_DEBUG:  return "DEBUG";
            case TK_C_LANG: return "C_LANG";
            case TK_VM:     return "VM";
            default:        return "UNKNOWN";
        }
    }



    int compile() {

        // --- CONFIGURATION VALIDATION
        //

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


        // --- INITIALIZATION
        //

        Arena::Container arena;
        alc = &arena;
        initAlloc(&arena);
        initNAlloc(&arena);

        Ast::init();
        FileSystem::init();
        TaskSystem::init(0);

        Extern::init();

        Logger::log(logInf, "Initialization completed");



        // --- FRONT-END
        //

        FileSystem::Handle mainFileHandle
            = FileSystem::load(mainFile, FileSystem::Origins::COMPILER_SOURCE);
        if (mainFileHandle == FileSystem::null) {
            Logger::log(logErr,
                "Failed to load entry-point file: '%.*s'.\n"
                "Ensure the path is correct and the file is not locked by another process.",
                NULL, mainFile.len, mainFile.buff);

            return Err::FILE_LOAD_FAILED;
        }

        TaskSystem::beginGroup();
        TaskSystem::dispatchParse(mainFileHandle);
        TaskSystem::wait();

        Logger::log(logInf, "Parsing completed");



        TaskSystem::beginGroup();
        TaskSystem::dispatchPreValidation(mainFileHandle);
        TaskSystem::wait();

        Logger::log(logInf, "Pre validation completed");



        TaskSystem::beginGroup();
        TaskSystem::dispatchValidation(mainFileHandle);
        TaskSystem::wait();

        Logger::log(logInf, "Validating completed");

        if (command == BC_VALIDATE) return 0;



        // --- BACK-END
        //

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
                };

                TaskSystem::dispatchBackend(mainFileHandle, target->backend, &ctx);
                i++;
            }
        }
        TaskSystem::wait();

        Logger::log(logInf, "Compilation completed");

        return Err::OK;

    }

}
