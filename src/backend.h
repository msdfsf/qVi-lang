#pragma once
#include "emitter.h"
#include <cstdint>



namespace Compiler { enum BuildCommand : uint8_t; }

namespace Backend {

    struct BuildContext {
        Compiler::BuildCommand command;

        const char* outDir;
        const char* outFile;

        bool debugInfo;
    };

    struct Driver {
        const char* name;

        Emitter::Driver* emitter;
        bool (*execute) (BuildContext* ctx, Reg::Unit* unit);
    };

}
