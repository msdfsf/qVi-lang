#include "backend_driver_c.h"
#include "../emitter_drivers/emitter_driver_c.h"

static bool runClangPipline(Backend::BuildContext* ctx, Reg::Unit* unit) {
    return false;
}

bool clang_execute(Backend::BuildContext* ctx, Reg::Unit* unit) {
    return runClangPipline(ctx, unit);
}

Backend::Driver Backend::driverClang = {
    .name = "VM Backend",
    .emitter = &Emitter::driverClang,
    .execute = clang_execute
};
