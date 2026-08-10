#include "emitter.h"
#include <cstdio>

namespace DebugHelper {

    inline Emitter::Context emitter = {
        .userData = NULL,
        .style = {
            .format = Emitter::Format::PRETTY,
            .indentStep = 2
        },
        .indentLevel = 0
    };

    inline IO::Stream stream = {
        .kind = IO::Stream::SK_C_STREAM,
        .cstream = stdout
    };

}
