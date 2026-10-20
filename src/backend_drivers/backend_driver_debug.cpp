#include "backend_driver_debug.h"
#include "../emitter_drivers/emitter_driver_debug.h"
#include <cstdio>

static bool runDebugPipline(Backend::BuildContext* ctx, Reg::Unit* unit) {
    Emitter::Context ectx = {
        .userData = NULL,
        .style {
            .format = Emitter::Format::PRETTY,
            .color = true,
            .indentStep = 2
        },
        .indentLevel = 0
    };

    IO::Stream stream = {
        .kind = IO::Stream::SK_C_STREAM,
        .cstream = stdout
    };

    Emitter::driverDebug.emitUnit(&ectx, unit, &stream);
    return true;
}

bool debug_execute(Backend::BuildContext* ctx, Reg::Unit* unit) {
    return runDebugPipline(ctx, unit);
}

Backend::Driver Backend::driverDebug = {
    .name = "Debug Backend",
    .emitter = &Emitter::driverDebug,
    .execute = debug_execute
};
