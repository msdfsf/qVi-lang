#include "backend_driver_vm.h"
#include "../compiler.h"
#include "../interpreter.h"
#include "../ansi_colors.h"


thread_local bool compilerStateInitilized = false;
thread_local Interpreter::CompilerState compilerState;

static bool runVMPipline(Backend::BuildContext* ctx, Reg::Unit* unit) {
    Err::Err err = Err::OK;

    IO::Stream stream = {
        .kind = IO::Stream::SK_C_STREAM,
        .cstream = stdout
    };

    if (compilerStateInitilized) {
        // Interpreter::clear(&compilerState);
    } else {
        Interpreter::init(&compilerState);
    }

    err = Interpreter::compile(&compilerState, unit);
    if (err != Err::OK) return false;


    if (ctx->command >= Compiler::BC_TRANSLATE) {
        Emitter::Context ectx = {
            .userData = NULL,
            .style = {
                .format = Emitter::Format::PRETTY,
                .indentStep = 0
            },
            .indentLevel = 0
        };

        Emitter::driverVM.emitUnit(&ectx, unit, &stream);
    }

    if (ctx->command == Compiler::BC_BUILD) {
        // FileDriver::openFile(ctx->outFile);
        // Interpreter::serialize(unit, );
    }

    if (ctx->command >= Compiler::BC_RUN) {
        IO::write(&stream, "\n" AC_BOLD AC_BRIGHT_CYAN "Output:\n" AC_RESET);
        err = Interpreter::exec(unit);
        IO::write(&stream, '\n');
        if (err != Err::OK) return false;
    }

    return true;
}



bool vm_execute(Backend::BuildContext* ctx, Reg::Unit* unit) {
    return runVMPipline(ctx, unit);
}

Backend::Driver Backend::driverVM = {
    .name = "VM Backend",
    .emitter = &Emitter::driverVM,
    .execute = vm_execute
};
