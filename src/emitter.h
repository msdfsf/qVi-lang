#pragma once

#include "io.h"
#include <cstdint>



struct SyntaxNode;

namespace Reg {
    struct Unit;
}

namespace Emitter {

    enum Format : uint8_t {
        COMPACT,    // Mambled together, no extra spaces/newlines
        PRETTY,     // Indented, readable
        VERBOSE     // May include additional metadata
    };

    struct Style {
        Format   format       = Format::PRETTY;
        uint8_t  indentStep   = 4;
    };

    struct Context {
        void* userData;

        const Style style;
        uint8_t indentLevel;
    };

    struct Driver {
        const char* name;
        void (*emitNode) (Context* ctx, SyntaxNode* node, IO::Stream* out);
        void (*emitUnit) (Context* ctx, Reg::Unit* unit, IO::Stream* out);
    };

}
