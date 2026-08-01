#pragma once

#include "backend.h"
#include "backend_drivers/backend_driver_c.h"
#include "backend_drivers/backend_driver_debug.h"
#include "backend_drivers/backend_driver_vm.h"
#include "emitter.h"
#include "emitter_drivers/emitter_driver_c.h"
#include "emitter_drivers/emitter_driver_debug.h"
#include "emitter_drivers/emitter_driver_vm.h"
#include <cstdint>



namespace Compiler {

    enum BuildCommand : uint8_t {
        BC_VALIDATE  = 1 << 0,
        BC_TRANSLATE = 1 << 1,
        BC_BUILD     = 1 << 2,
        BC_RUN       = 1 << 3,
    };

    enum TargetKind : uint8_t {
        TK_DEBUG,
        TK_C_LANG,
        TK_VM,
        TK_COUNT
    };

    struct Target {
        TargetKind kind;
        uint8_t    buildCapability; // BuildCommand bitmask

        Backend::Driver* backend;
    };

    constexpr Target bakedTargets[] = {
        {
            .kind = TK_DEBUG,
            .buildCapability = BC_TRANSLATE,
            .backend = &Backend::driverDebug,
        },

        {
            .kind = TK_C_LANG,
            .buildCapability = BC_TRANSLATE | BC_BUILD | BC_RUN,
            .backend = &Backend::driverClang,
        },

        {
            .kind = TK_VM,
            .buildCapability = BC_TRANSLATE | BC_BUILD | BC_RUN,
            .backend = &Backend::driverVM,
        }
    };

    extern String mainFile;
    extern String outFile;
    extern String outDir;

    extern BuildCommand command;
    extern Target*      targets[TK_COUNT + 1]; // Null-terminated
    extern bool         debugInfo;
    extern uint8_t      optLevel;

    int compile();

}
