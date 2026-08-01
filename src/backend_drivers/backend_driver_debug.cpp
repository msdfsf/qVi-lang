#include "backend_driver_debug.h"
#include "../emitter_drivers/emitter_driver_debug.h"

static bool runDebugPipline(Backend::BuildContext* ctx, Reg::Unit* unit) {
    return false;
}

bool debug_execute(Backend::BuildContext* ctx, Reg::Unit* unit) {
    return runDebugPipline(ctx, unit);
}

Backend::Driver Backend::driverDebug = {
    .name = "VM Backend",
    .emitter = &Emitter::driverDebug,
    .execute = debug_execute
};
