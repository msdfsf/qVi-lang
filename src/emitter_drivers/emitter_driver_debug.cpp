#include "emitter_driver_debug.h"
#include "../syntax.h"
#include "../registry.h"
#include "../ansi_colors.h"



static void emitExpression(Emitter::Context* ctx, Expression* exp, IO::Stream* out);
static void emitNode(Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out);
static void emitUnit(Emitter::Context* ctx, Reg::Unit* unit, IO::Stream* out);


static void indent(Emitter::Context* ctx, IO::Stream* stream) {
    if (ctx->style.format == Emitter::Format::COMPACT) {
        return;
    }

    uint32_t spaces = ctx->indentLevel * ctx->style.indentStep;

    if (spaces > 0) {
        IO::write(stream, ' ', spaces);
    }
}

static void writeName(IO::Stream* out, INamed* name) {
    if (name && name->buff) {
        IO::writef(out, "%.*s", (int)name->len, name->buff);
    }
}

static void writeQualifiedName(IO::Stream* out, QualifiedName* name) {
    if (!name) return;
    for (uint16_t i = 0; i < name->pathSize; i++) {
        writeName(out, &name->path[i]);
        IO::write(out, "::", 2);
    }
    writeName(out, (INamed*)name);
}

static void writeAttr(Emitter::Context* ctx, IO::Stream* out, const char* label) {
    indent(ctx, out);
    IO::writef(out, AC_BOLD_CYAN "%s: " AC_RESET, label);
}

static void writeValue(IO::Stream* out, Value* val) {
    if (!val || !val->hasValue) {
        IO::writef(out, AC_DIM "<no_val>" AC_RESET);
        return;
    }

    switch (val->typeKind) {
        case Type::DT_I32: IO::writef(out, AC_NUMBER "%i" AC_RESET, val->i32); break;
        case Type::DT_I64: IO::writef(out, AC_NUMBER "%lli" AC_RESET, val->i64); break;
        case Type::DT_F32: IO::writef(out, AC_NUMBER "%g" AC_RESET, (double)val->f32); break;
        case Type::DT_F64: IO::writef(out, AC_NUMBER "%g" AC_RESET, val->f64); break;
        case Type::DT_STRING:
            if (val->str) IO::writef(out, AC_TYPE "\"%s\"" AC_RESET, (char*)val->str);
            else IO::writef(out, AC_DIM "null" AC_RESET);
            break;
        default: IO::writef(out, AC_DIM "(%s)" AC_RESET, Type::str(val->typeKind)); break;
    }
}



static void emitExpression(Emitter::Context* ctx, Expression* exp, IO::Stream* out) {
    if (!exp) return;

    indent(ctx, out);
    IO::writef(out, AC_BOLD_GREEN "[%s]" AC_RESET "\n", Ast::Node::str((ExpressionType) exp->type));

    ctx->indentLevel++;
    switch ((ExpressionType) exp->type) {
        case EXT_UNARY: {
            UnaryExpression* uex = (UnaryExpression*) exp;
            writeAttr(ctx, out, "Operator");
            IO::writef(out, "%s\n", OperatorToStr(uex->base.opType));
            emitNode(ctx, (SyntaxNode*) uex->operand, out);
            break;
        }

        case EXT_BINARY: {
            BinaryExpression* bex = (BinaryExpression*) exp;
            writeAttr(ctx, out, "Operator");
            IO::writef(out, "%s\n", OperatorToStr(bex->base.opType));
            emitNode(ctx, (SyntaxNode*) bex->left, out);
            emitNode(ctx, (SyntaxNode*) bex->right, out);
            break;
        }

        case EXT_FUNCTION_CALL: {
            FunctionCall* call = (FunctionCall*) exp;
            writeAttr(ctx, out, "Target");
            writeQualifiedName(out, &call->name);
            IO::write(out, '\n');
            for (uint32_t i = 0; i < call->inArgCount; i++) {
                emitNode(ctx, (SyntaxNode*) call->inArgs[i], out);
            }
            break;
        }
    }
    ctx->indentLevel--;
}

void emitNode(Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out) {
    if (!node) return;

    indent(ctx, out);

    if (ctx->style.format == Emitter::Format::VERBOSE) {
        IO::writef(out, AC_ADDRESS "[%p -> %p] " AC_RESET, node, node->ogNode);
    }

    IO::writef(out, "[" AC_BOLD_MAGENTA "%s" AC_RESET "] (Flags: %llu)\n",
                Ast::Node::str(node->type), node->flags);

    ctx->indentLevel++;
    switch (node->type) {
        case NT_SCOPE: {
            Scope* s = (Scope*)node;
            writeAttr(ctx, out, "Definitions");
            IO::writef(out, "%u\n", s->definitionCount);
            for (uint32_t i = 0; i < s->definitionCount; i++) {
                emitNode(ctx, s->definitions[i], out);
            }

            writeAttr(ctx, out, "Children");
            IO::writef(out, "%u\n", s->childrenCount);
            for (uint32_t i = 0; i < s->childrenCount; i++) {
                emitNode(ctx, s->children[i], out);
            }
            break;
        }

        case NT_VARIABLE: {
            Variable* var = (Variable*)node;

            writeAttr(ctx, out, "Name");
            writeQualifiedName(out, &var->name);
            IO::write(out, "\n", 1);

            if (var->value.hasValue) {
                writeAttr(ctx, out, "Const");
                writeValue(out, &var->value);
                IO::write(out, '\n');
            }

            if (var->expression) {
                emitExpression(ctx, var->expression, out);
            }

            break;
        }

        case NT_FUNCTION: {
            Function* fcn = (Function*)node;

            writeAttr(ctx, out, "Symbol");
            writeName(out, (INamed*) &fcn->name);
            IO::write(out, '\n');

            if (fcn->bodyScope) {
                emitNode(ctx, (SyntaxNode*) fcn->bodyScope, out);
            }

            break;
        }

        case NT_TYPE_DEFINITION: {
            TypeDefinition* td = (TypeDefinition*) node;
            writeAttr(ctx, out, "Struct");
            writeName(out, (INamed*) &td->name);
            IO::write(out, '\n');

            for (uint32_t i = 0; i < td->varCount; i++) {
                emitNode(ctx, (SyntaxNode*)td->vars[i], out);
            }

            break;
        }

        case NT_VARIABLE_DEFINITION: {
            VariableDefinition* def = (VariableDefinition*)node;

            if (def->var) {
                writeAttr(ctx, out, "Name");
                writeQualifiedName(out, &def->var->name);
                IO::write(out, "\n", 1);
            }

            if (def->dtype) {
                writeAttr(ctx, out, "Declared Type");
                writeQualifiedName(out, def->dtype);

                if (def->lastPtr) {
                    IO::writef(out, AC_BOLD_YELLOW "*" AC_RESET);
                }
                IO::write(out, "\n", 1);
            }

            if (def->var) {
                writeAttr(ctx, out, "Resolved Type");

                IO::writef(out, AC_TYPE);
                Type::writeTypeName(out, def->var->value.any, def->var->value.typeKind);
                IO::writef(out, AC_RESET " (" AC_NUMBER "Kind: %d" AC_RESET ")\n",
                (int) def->var->value.typeKind);
            }

            writeAttr(ctx, out, "VM Offset");
            IO::writef(out, AC_NUMBER "%llu bytes" AC_RESET "\n", def->vmOffset);

            if (def->var) {
                ctx->indentLevel++;
                emitNode(ctx, (SyntaxNode*)def->var, out);
                ctx->indentLevel--;
            }

            break;
        }

        case NT_VARIABLE_ASSIGNMENT: {
            VariableAssignment* ass = (VariableAssignment*) node;
            emitNode(ctx, (SyntaxNode*) ass->lvar, out);
            emitNode(ctx, (SyntaxNode*) ass->rvar, out);
            break;
        }

        case NT_RETURN_STATEMENT: {
            ReturnStatement* rs = (ReturnStatement*) node;
            if (rs->var) emitNode(ctx, (SyntaxNode*) rs->var, out);
            break;
        }

        default:
            break;
    }
    ctx->indentLevel--;
}

static void emitUnit(Emitter::Context* ctx, Reg::Unit* unit, IO::Stream* out) {

    IO::writef(out, AC_SECTION "=== AST DUMP: %s ===" AC_RESET "\n", unit->ast->tag);
    emitNode(ctx, (SyntaxNode*) unit->ast->root, out);
    IO::write(out, "\n", 1);
}



void debug_emitNode(Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out) {
    emitNode(ctx, node, out);
}

void debug_emitUnit(Emitter::Context* ctx, Reg::Unit* unit, IO::Stream* out) {
    emitUnit(ctx, unit, out);
}

Emitter::Driver Emitter::driverDebug = {
    .name = "Debug-AST-Visualizer",
    .emitNode = debug_emitNode,
    .emitUnit = debug_emitUnit
};
