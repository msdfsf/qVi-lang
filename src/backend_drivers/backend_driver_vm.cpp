#include "backend_driver_vm.h"
#include "../compiler.h"
#include "../interpreter.h"
#include "../ansi_colors.h"
#include "../file_driver.h"



thread_local bool compilerStateInitilized = false;
thread_local Interpreter::CompilerState compilerState;

static bool runVMPipline(Backend::BuildContext* ctx, Reg::Unit* unit) {
    Err::Err err = Err::OK;

    Emitter::Context ectx = {
        .userData = NULL,
        .style {
            .format = Emitter::Format::PRETTY,
            .indentStep = 0
        },
        .indentLevel = 0
    };

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
        Emitter::driverVM.emitUnit(&ectx, unit, &stream);
    }

    if (true || ctx->command == Compiler::BC_BUILD) {
        // TODO
        FILE* file = std::fopen("PUK.ansi", "wb");//FileDriver::openFile(ctx->outFile, "w");
        if (!file) {
            // TODO: error
            return Err::UNEXPECTED_ERROR;
        }

        stream.cstream = file;
        Emitter::driverVM.emitUnit(&ectx, unit, &stream);

        std::fclose(file);
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
