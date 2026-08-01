#include "emitter_driver_vm.h"
#include "../interpreter.h"

void emitNode(Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out) {
    if (node->type == NT_FUNCTION) {
        Interpreter::print(out, (Function*) node);
    } else {
        IO::writef(out, "<%s>", Ast::Node::str(node->type));
    }
}

void emitUnit(Emitter::Context* ctx, Reg::Unit* unit, IO::Stream* out) {
    Interpreter::print(out, unit);
}



void vm_emitNode(Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out) {
    emitNode(ctx, node, out);
}

void vm_emitUnit(Emitter::Context* ctx, Reg::Unit* unit, IO::Stream* out) {
    emitUnit(ctx, unit, out);
}

Emitter::Driver Emitter::driverVM = {
    .name = "VM-Bytecode-Visualizer",
    .emitNode = vm_emitNode,
    .emitUnit = vm_emitUnit,
};
