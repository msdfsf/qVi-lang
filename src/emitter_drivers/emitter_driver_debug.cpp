// Renders an AST back as the language itself.
//
// It does not try to be 1:1 language representation, but close
// enough to be easy readable by an eye.
//
// Whatever the AST has unexpected, missing node, or meta node,
// they get rendered, unknown/hidden pieces stay inspectable.
//

#include "emitter_driver_debug.h"
#include "../syntax.h"
#include "../registry.h"
#include "../ansi_colors.h"
#include <cstdarg>

#define AC_KEYWORD AC_BOLD_MAGENTA



static void emitExpression(Emitter::Context* ctx, Expression* exp, IO::Stream* out);
static void emitNode      (Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out);
static void emitTerm      (Emitter::Context* ctx, IO::Stream* out, Variable* var);
static void emitScopeBlock(Emitter::Context* ctx, IO::Stream* out, Scope* scope);
static void emitVariableDefinition(Emitter::Context* ctx, IO::Stream* out, VariableDefinition* def);
static void emitDeclarationInline(Emitter::Context* ctx, IO::Stream* out, VariableDefinition* def);



// === Plain Formatting ======
// -----

static void indent(Emitter::Context* ctx, IO::Stream* out) {
    if (ctx->style.format == Emitter::Format::COMPACT) {
        return;
    }

    uint32_t spaces = ctx->indentLevel * ctx->style.indentStep;

    if (spaces > 0) {
        IO::write(out, ' ', spaces);
    }
}

static void writeNewline(Emitter::Context* ctx, IO::Stream* out) {
    if (ctx->style.format == Emitter::Format::COMPACT) {
        return;
    }

    IO::write(out, '\n');
}

static void writeColored(Emitter::Context* ctx, IO::Stream* out, const char* code, const char* fmt, ...) {
    if (ctx->style.color) IO::write(out, code);

    va_list args;
    va_start(args, fmt);
    IO::writefv(out, fmt, args);
    va_end(args);

    if (ctx->style.color) IO::write(out, AC_RESET);
}

static void writeKeyword(Emitter::Context* ctx, IO::Stream* out, const char* text) {
    writeColored(ctx, out, AC_KEYWORD, "%s", text);
}



// === Names ======
// -----

static void writeName(IO::Stream* out, INamed* name) {
    if (!name || !name->buff) {
        IO::write(out, "<none>");
        return;
    }

    IO::writef(out, "%.*s", (int) name->len, name->buff);
}

static void writeName(Emitter::Context* ctx, IO::Stream* out, INamed* name) {
    if (!name || !name->buff) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }
    if (ctx->style.color) IO::write(out, AC_VAR);
    writeName(out, name);
    if (ctx->style.color) IO::write(out, AC_RESET);
}

static void writeQualifiedName(Emitter::Context* ctx, IO::Stream* out, QualifiedName* name) {
    if (!name) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    if (!name->buff && name->pathSize == 0) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    if (ctx->style.color) IO::write(out, AC_VAR);

    for (uint16_t i = 0; i < name->pathSize; i++) {
        writeName(out, &name->path[i]);
        IO::write(out, "::", 2);
    }
    writeName(out, (INamed*) name);

    if (ctx->style.color) IO::write(out, AC_RESET);
}



// === Values ======
// -----

static void writeValue(Emitter::Context* ctx, IO::Stream* out, Value* val) {
    if (!val || !val->hasValue) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    switch (val->type ? val->type->kind : Type::DT_UNDEFINED) {
        case Type::DT_I8:  writeColored(ctx, out, AC_NUMBER, "%i",   (int) val->i8);        break;
        case Type::DT_I16: writeColored(ctx, out, AC_NUMBER, "%i",   (int) val->i16);       break;
        case Type::DT_I32: writeColored(ctx, out, AC_NUMBER, "%i",   val->i32);             break;
        case Type::DT_I64: writeColored(ctx, out, AC_NUMBER, "%lli", (long long) val->i64); break;
        case Type::DT_U8:  writeColored(ctx, out, AC_NUMBER, "%u",   (unsigned) val->u8);   break;
        case Type::DT_U16: writeColored(ctx, out, AC_NUMBER, "%u",   (unsigned) val->u16);  break;
        case Type::DT_U32: writeColored(ctx, out, AC_NUMBER, "%u",   val->u32);             break;
        case Type::DT_U64: writeColored(ctx, out, AC_NUMBER, "%llu", (unsigned long long) val->u64); break;
        case Type::DT_F32: writeColored(ctx, out, AC_NUMBER, "%g",   (double) val->f32);    break;
        case Type::DT_F64: writeColored(ctx, out, AC_NUMBER, "%g",   val->f64);             break;
        default:           writeColored(ctx, out, AC_DIM, "(%s)", Type::str(val->type));    break;
    }
}

static void writeStringLiteral(Emitter::Context* ctx, IO::Stream* out, String str) {
    if (ctx->style.color) IO::write(out, AC_GREEN);

    IO::write(out, '"');
    IO::write(out, str);
    IO::write(out, '"');

    if (ctx->style.color) IO::write(out, AC_RESET);
}



// === Types ======
// -----
// Resolved types are written left-oriented, as the language does:
//     ^i32   [4]f32   [^2]i32   ^(i32, i32) -> i32

static void writeTypeRefRaw(IO::Stream* out, Type::TypeInfo* type) {
    switch (type->kind) {
        case Type::DT_POINTER: {
            Type::PointerInfo* pType = (Type::PointerInfo*) type;

            IO::write(out, '^');
            writeTypeRefRaw(out, pType->element);
            break;
        }

        case Type::DT_ARRAY: {
            Type::ArrayInfo* aType = (Type::ArrayInfo*) type;

            IO::write(out, '[');
            if (aType->elementCount != Type::ARRAY_LEN_UNKNOWN) {
                IO::writef(out, "%llu", (unsigned long long) aType->elementCount);
            }
            IO::write(out, ']');

            writeTypeRefRaw(out, aType->element);
            break;
        }

        case Type::DT_SLICE: {
            Type::SliceInfo* sType = (Type::SliceInfo*) type;

            if (sType->flags & IS_CONST) {
                IO::write(out, "[const]");
            } else {
                IO::write(out, "[]");
            }

            writeTypeRefRaw(out, sType->element);
            break;
        }

        case Type::DT_STRUCT:
        case Type::DT_UNION: {
            Type::StructInfo* sType = (Type::StructInfo*) type;

            if (sType->name.buff && sType->name.len > 0) {
                IO::writef(out, "%.*s", (int) sType->name.len, sType->name.buff);
            } else {
                IO::write(out, "<anon struct>");
            }
            break;
        }

        case Type::DT_ENUM: {
            Type::EnumInfo* eType = (Type::EnumInfo*) type;

            if (eType->name.buff && eType->name.len > 0) {
                IO::writef(out, "%.*s", (int) eType->name.len, eType->name.buff);
            } else {
                IO::write(out, "<anon enum>");
            }
            break;
        }

        case Type::DT_FUNCTION: {
            Type::FunctionInfo* fType = (Type::FunctionInfo*) type;

            IO::write(out, '(');
            for (uint64_t i = 0; i < fType->argCount; i++) {
                writeTypeRefRaw(out, fType->argTypes[i]);
                if (i + 1 < fType->argCount) {
                    IO::write(out, ", ");
                }
            }
            IO::write(out, ") -> ");

            if (fType->retType) {
                writeTypeRefRaw(out, fType->retType);
            } else {
                IO::write(out, "<none>");
            }
            break;
        }

        default: {
            IO::write(out, Type::str(type->kind));
            break;
        }
    }
}

static void writeTypeRef(Emitter::Context* ctx, IO::Stream* out, Type::TypeInfo* type) {
    if (!type) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    if (ctx->style.color) IO::write(out, AC_TYPE);
    writeTypeRefRaw(out, type);
    if (ctx->style.color) IO::write(out, AC_RESET);
}

// True when the specifier carries anything worth writing
static bool hasDeclaredType(TypeSpecifier* spec) {
    return spec->baseType != Type::DT_UNDEFINED
        || spec->decoratorCount > 0
        || (spec->baseName && spec->baseName->buff);
}

static void writeDeclaredTypeRef(Emitter::Context* ctx, IO::Stream* out, VariableDefinition* def) {
    TypeSpecifier* spec = &def->type;

    // Nothing declared, pin the resolved type instead, if there is one
    if (!hasDeclaredType(spec)) {
        writeTypeRef(ctx, out, def->var && def->var->value.type ? def->var->value.type : NULL);
        return;
    }

    if (ctx->style.color) IO::write(out, AC_TYPE);

    if (spec->qualifier & Type::Q_CONST) IO::write(out, "const ");
    if (spec->qualifier & Type::Q_EMBED) IO::write(out, "embed ");

    for (uint32_t i = 0; i < spec->decoratorCount; i++) {
        TypeDecorator* dec = spec->decorators[i];
        if (!dec) continue;

        if (dec->kind == TypeDecorator::DEC_POINTER) {
            IO::write(out, '^');
        } else {
            IO::write(out, '[');

            if (dec->len) {
                emitTerm(ctx, out, dec->len);
            } else if (dec->flags & IS_CONST) {
                IO::write(out, "const");
            } else if (dec->flags & IS_DYNAMIC) {
                IO::write(out, "alloc");
            }

            IO::write(out, ']');
        }
    }

    // Core type
    if (spec->baseType == Type::DT_FUNCTION && spec->baseFcn) {
        FunctionPrototype* proto = spec->baseFcn;

        IO::write(out, '(');
        for (uint32_t i = 0; i < proto->inArgCount; i++) {
            if (i) IO::write(out, ", ");
            emitDeclarationInline(ctx, out, proto->inArgs[i]);
        }
        IO::write(out, ") -> ");

        if (proto->outArg) {
            writeDeclaredTypeRef(ctx, out, proto->outArg);
        }
    } else if (spec->baseType == Type::DT_UNDEFINED && spec->baseName) {
        writeName(out, (INamed*) spec->baseName);
    } else if (spec->baseType == Type::DT_MULTIPLE_TYPES) {
        IO::write(out, "...");
    } else {
        IO::write(out, Type::str(spec->baseType));
    }

    if (ctx->style.color) IO::write(out, AC_RESET);
}



// === Expressions ======
// -----
// Variables are the leaves of expression trees: they either carry
// an AST expression, a compile-time value, or resolve to a name.

static void emitTerm(Emitter::Context* ctx, IO::Stream* out, Variable* var) {
    if (!var) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    if (var->expression) {
        emitExpression(ctx, var->expression, out);
        return;
    }

    if (var->value.hasValue) {
        writeValue(ctx, out, &var->value);
        return;
    }

    writeQualifiedName(ctx, out, &var->name);
}

static void writeOperator(Emitter::Context* ctx, IO::Stream* out, OperatorEnum op) {
    switch (op) {
        case OP_ADDITION:                     writeColored(ctx, out, AC_DIM, "+");     break;
        case OP_MULTIPLICATION:               writeColored(ctx, out, AC_DIM, "*");     break;
        case OP_DIVISION:                     writeColored(ctx, out, AC_DIM, "/");     break;
        case OP_MODULO:                       writeColored(ctx, out, AC_DIM, "%%");    break;
        case OP_SUBTRACTION:                  writeColored(ctx, out, AC_DIM, "-");     break;
        case OP_CONCATENATION:                writeColored(ctx, out, AC_DIM, "..");    break;
        case OP_SHIFT_RIGHT:                  writeColored(ctx, out, AC_DIM, ">>");    break;
        case OP_SHIFT_LEFT:                   writeColored(ctx, out, AC_DIM, "<<");    break;
        case OP_LESS_THAN:                    writeColored(ctx, out, AC_DIM, "<");     break;
        case OP_GREATER_THAN:                 writeColored(ctx, out, AC_DIM, ">");     break;
        case OP_LESS_THAN_OR_EQUAL:           writeColored(ctx, out, AC_DIM, "<=");    break;
        case OP_GREATER_THAN_OR_EQUAL:        writeColored(ctx, out, AC_DIM, ">=");    break;
        case OP_EQUAL:                        writeColored(ctx, out, AC_DIM, "==");    break;
        case OP_NOT_EQUAL:                    writeColored(ctx, out, AC_DIM, "!=");    break;
        case OP_BITWISE_AND:                  writeColored(ctx, out, AC_DIM, "&");     break;
        case OP_BITWISE_XOR:                  writeColored(ctx, out, AC_DIM, "^");     break;
        case OP_BITWISE_OR:                   writeColored(ctx, out, AC_DIM, "|");     break;
        case OP_BOOL_AND:                     writeColored(ctx, out, AC_DIM, "&&");    break;
        case OP_BOOL_OR:                      writeColored(ctx, out, AC_DIM, "||");    break;
        case OP_CAST_STATIC:                  writeColored(ctx, out, AC_DIM, "->");    break;
        case OP_CAST_BIT:                     writeColored(ctx, out, AC_DIM, "=>");    break;
        default:                              writeColored(ctx, out, AC_DIM, "<?%u>", (unsigned) op); break;
    }
}

static bool needsParens(Variable* var) {
    return var && var->expression && var->expression->type == EXT_BINARY;
}

static void emitParenTerm(Emitter::Context* ctx, IO::Stream* out, Variable* var) {
    if (!needsParens(var)) {
        emitTerm(ctx, out, var);
        return;
    }

    IO::write(out, '(');
    emitTerm(ctx, out, var);
    IO::write(out, ')');
}

static void emitRange(Emitter::Context* ctx, IO::Stream* out, RangeExpression* range) {
    if (!range) {
        writeColored(ctx, out, AC_DIM, ":");
        return;
    }

    if (range->bidx) emitTerm(ctx, out, range->bidx);
    IO::write(out, ':');
    if (range->eidx) emitTerm(ctx, out, range->eidx);
    if (range->step) {
        IO::write(out, ':');
        emitTerm(ctx, out, range->step);
    }
}

static void emitAttributeVariable(Emitter::Context* ctx, IO::Stream* out, Variable* var) {
    // When the field name survived the AST, keep the named form:  .{ id : 4 }
    if (var && var->name.buff && var->name.len) {
        writeQualifiedName(ctx, out, &var->name);
        IO::write(out, " : ");
    }

    if (var && var->expression) {
        emitExpression(ctx, var->expression, out);
        return;
    }

    emitTerm(ctx, out, var);
}

static void emitTypeInitialization(Emitter::Context* ctx, IO::Stream* out, TypeInitialization* init) {
    if (!init) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    IO::write(out, ".{");

    for (uint32_t i = 0; i < init->attributeCount; i++) {
        if (i) IO::write(out, ", ");
        emitAttributeVariable(ctx, out, init->attributes[i]);
    }

    // Rest defaults: .{ .. : 0 }
    if (init->fillVar) {
        if (init->attributeCount) IO::write(out, ", ");
        writeColored(ctx, out, AC_DIM, "..");
        IO::write(out, " : ");
        emitAttributeVariable(ctx, out, init->fillVar);
    }

    IO::write(out, '}');
}

static void emitArrayInitialization(Emitter::Context* ctx, IO::Stream* out, ArrayInitialization* init) {
    if (!init) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    IO::write(out, ".[");

    for (uint32_t i = 0; i < init->attributeCount; i++) {
        if (i) IO::write(out, ", ");
        emitAttributeVariable(ctx, out, init->attributes[i]);
    }

    IO::write(out, ']');
}

static void emitStringInitialization(Emitter::Context* ctx, IO::Stream* out, StringInitialization* siex) {
    if (!siex) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    // Typed dot-prefix when the string decodes to code points wider than u8.
    if (siex->charType && siex->charType->kind != Type::DT_U8) {
        writeTypeRef(ctx, out, siex->charType);
        IO::write(out, '.');
    }

    writeStringLiteral(ctx, out, siex->rawData);
}

static void emitFunctionCall(Emitter::Context* ctx, IO::Stream* out, FunctionCall* call) {
    if (!call) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    if (call->name.buff || call->name.pathSize) {
        writeQualifiedName(ctx, out, &call->name);
    } else if (call->fptr) {
        emitTerm(ctx, out, call->fptr);
    } else if (call->fcn) {
        writeName(ctx, out, (INamed*) &call->fcn->name);
    } else {
        writeColored(ctx, out, AC_DIM, "<none>");
    }

    IO::write(out, '(');
    for (uint32_t i = 0; i < call->inArgCount; i++) {
        if (i) IO::write(out, ", ");
        emitTerm(ctx, out, call->inArgs[i]);
    }
    IO::write(out, ')');
}

static void emitCatch(Emitter::Context* ctx, IO::Stream* out, Catch* cex) {
    if (!cex) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    if (cex->call) {
        emitFunctionCall(ctx, out, cex->call);
    } else {
        writeColored(ctx, out, AC_DIM, "<none>");
    }

    IO::write(out, ' ');
    writeKeyword(ctx, out, "catch");

    // catch return : no error captured, propagate as is.
    if (!cex->err && !cex->scope) {
        IO::write(out, ' ');
        writeKeyword(ctx, out, "return");
        return;
    }

    if (cex->err) {
        IO::write(out, ' ');
        emitTerm(ctx, out, cex->err);
    }

    if (cex->scope) {
        IO::write(out, ' ');
        emitScopeBlock(ctx, out, cex->scope);
    }
}

static void emitExpression(Emitter::Context* ctx, Expression* exp, IO::Stream* out) {
    if (!exp) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    switch (exp->type) {
        case EXT_FUNCTION_CALL: {
            emitFunctionCall(ctx, out, (FunctionCall*) exp);
            break;
        }

        case EXT_OPERATION: {
            OperationExpression* op = (OperationExpression*) exp;

            if (op->opType != OP_NONE) {
                writeOperator(ctx, out, op->opType);
            }
            break;
        }

        case EXT_UNARY: {
            UnaryExpression* uex = (UnaryExpression*) exp;

            if (uex->base.opType == OP_NONE) {
                emitTerm(ctx, out, uex->operand);
                break;
            }

            if (uex->base.opType == OP_GET_VALUE) {
                emitTerm(ctx, out, uex->operand);
                IO::write(out, '^');
                break;
            }

            switch (uex->base.opType) {
                case OP_UNARY_PLUS:         IO::write(out, '+');                break;
                case OP_UNARY_MINUS:        IO::write(out, '-');                break;
                case OP_GET_ADDRESS:        IO::write(out, '&');                break;
                case OP_NEGATION:           IO::write(out, '!');                break;
                case OP_BITWISE_NEGATION:   IO::write(out, '~');                break;
                case OP_INCREMENT:          IO::write(out, "++");               break;
                case OP_DECREMENT:          IO::write(out, "--");               break;
                default:                    writeOperator(ctx, out, uex->base.opType); break;
            }

            // A nested binary keeps its grouping: -(a + b)
            emitParenTerm(ctx, out, uex->operand);
            break;
        }

        case EXT_BINARY: {
            BinaryExpression* bex = (BinaryExpression*) exp;

            switch (bex->base.opType) {
                case OP_MEMBER_SELECTION:
                case OP_DEREFERENCE_MEMBER_SELECTION: {
                    emitParenTerm(ctx, out, bex->left);
                    IO::write(out, '.');
                    emitTerm(ctx, out, bex->right);
                    break;
                }

                case OP_SUBSCRIPT: {
                    emitParenTerm(ctx, out, bex->left);
                    IO::write(out, '[');
                    emitTerm(ctx, out, bex->right);
                    IO::write(out, ']');
                    break;
                }

                case OP_SLICE: {
                    emitParenTerm(ctx, out, bex->left);
                    IO::write(out, '[');

                    if (bex->right && bex->right->expression &&
                        bex->right->expression->type == EXT_RANGE) {
                        emitRange(ctx, out, (RangeExpression*) bex->right->expression);
                    } else {
                        emitTerm(ctx, out, bex->right);
                    }

                    IO::write(out, ']');
                    break;
                }

                default: {
                    emitParenTerm(ctx, out, bex->left);

                    IO::write(out, ' ');
                    writeOperator(ctx, out, bex->base.opType);

                    // A cast pins the target type; only value casts land in
                    // the AST as binary operators, so this is how they seem.
                    if (isCast(bex->base.opType) && bex->right && bex->right->value.type) {
                        IO::write(out, ' ');
                        writeTypeRef(ctx, out, bex->right->value.type);
                    } else {
                        IO::write(out, ' ');
                        emitParenTerm(ctx, out, bex->right);
                    }
                    break;
                }
            }
            break;
        }

        case EXT_TERNARY: {
            TernaryExpression* tex = (TernaryExpression*) exp;

            emitTerm(ctx, out, tex->condition);
            IO::write(out, " ? ");
            emitTerm(ctx, out, tex->trueExp);
            IO::write(out, " : ");
            emitTerm(ctx, out, tex->falseExp);
            break;
        }

        case EXT_RANGE: {
            emitRange(ctx, out, (RangeExpression*) exp);
            break;
        }

        case EXT_TYPE_INITIALIZATION: {
            emitTypeInitialization(ctx, out, (TypeInitialization*) exp);
            break;
        }

        case EXT_ARRAY_INITIALIZATION: {
            emitArrayInitialization(ctx, out, (ArrayInitialization*) exp);
            break;
        }

        case EXT_STRING_INITIALIZATION: {
            emitStringInitialization(ctx, out, (StringInitialization*) exp);
            break;
        }

        case EXT_CATCH: {
            emitCatch(ctx, out, (Catch*) exp);
            break;
        }

        case EXT_CAST: {
            Cast* cst = (Cast*) exp;

            emitTerm(ctx, out, cst->operand);
            IO::write(out, " -> ");

            if (cst->target) {
                writeTypeRef(ctx, out, cst->target);
            } else {
                writeColored(ctx, out, AC_DIM, "<none>");
            }
            break;
        }

        case EXT_ALLOC: {
            Alloc* alc = (Alloc*) exp;

            writeKeyword(ctx, out, "alloc");
            IO::write(out, ' ');
            emitTerm(ctx, out, alc->def && alc->def->var ? alc->def->var : NULL);
            break;
        }

        case EXT_FREE: {
            Free* fre = (Free*) exp;

            writeKeyword(ctx, out, "free");
            IO::write(out, ' ');
            emitTerm(ctx, out, fre->var);
            break;
        }

        case EXT_GET_LENGTH: {
            GetLength* gln = (GetLength*) exp;

            emitTerm(ctx, out, gln->arr);
            writeColored(ctx, out, AC_DIM, ".length");
            break;
        }

        case EXT_GET_SIZE: {
            GetSize* gsz = (GetSize*) exp;

            emitTerm(ctx, out, gsz->arr);
            writeColored(ctx, out, AC_DIM, ".size");
            break;
        }

        default: {
            writeColored(ctx, out, AC_DIM, "<no_emitter>");
            break;
        }
    }
}



// === Block ======
// -----

static void emitBlockHeader(Emitter::Context* ctx, IO::Stream* out) {
    IO::write(out, '{');
    writeNewline(ctx, out);
    ctx->indentLevel++;
}

static void emitBlockFooter(Emitter::Context* ctx, IO::Stream* out) {
    ctx->indentLevel--;
    indent(ctx, out);
    IO::write(out, '}');
}

static void emitScopeBody(Emitter::Context* ctx, IO::Stream* out, Scope* scope) {
    if (!scope) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    for (uint32_t i = 0; i < scope->childrenCount; i++) {
        emitNode(ctx, scope->children[i], out);
    }
}

static void emitScopeBlock(Emitter::Context* ctx, IO::Stream* out, Scope* scope) {
    emitBlockHeader(ctx, out);
    emitScopeBody(ctx, out, scope);
    emitBlockFooter(ctx, out);
}

// VERBOSE adds a node-kind comment on top of every statement.
static void emitMarker(Emitter::Context* ctx, IO::Stream* out, SyntaxNode* node) {
    if (ctx->style.format != Emitter::Format::VERBOSE) {
        return;
    }

    indent(ctx, out);
    writeColored(ctx, out, AC_DIM, "// %s (flags: %llu)\n",
                 Ast::Node::str(node->type), (unsigned long long) node->flags);
}



// === Statement ======
// -----

static void emitDeclarationInline(Emitter::Context* ctx, IO::Stream* out, VariableDefinition* def) {
    if (!def) {
        writeColored(ctx, out, AC_DIM, "<none>");
        return;
    }

    if (def->var && (def->var->name.buff || def->var->name.pathSize)) {
        writeQualifiedName(ctx, out, &def->var->name);
    } else {
        writeColored(ctx, out, AC_DIM, "<none>");
    }

    IO::write(out, " : ");
    writeDeclaredTypeRef(ctx, out, def);

    if (def->var && def->var->expression) {
        if (def->var->expression->type == EXT_CATCH) {
            IO::write(out, " = ");
            emitCatch(ctx, out, (Catch*) def->var->expression);
        } else {
            IO::write(out, " = ");
            emitExpression(ctx, def->var->expression, out);
        }
    } else if (def->var && def->var->value.hasValue) {
        IO::write(out, " = ");
        writeValue(ctx, out, &def->var->value);
    }
}

static void emitVariableDefinition(Emitter::Context* ctx, IO::Stream* out, VariableDefinition* def) {
    indent(ctx, out);

    emitDeclarationInline(ctx, out, def);

    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitVariableAssignment(Emitter::Context* ctx, IO::Stream* out, VariableAssignment* ass) {
    if (!ass) return;

    indent(ctx, out);

    emitTerm(ctx, out, ass->lvar);
    IO::write(out, " = ");

    if (ass->rvar && ass->rvar->expression && ass->rvar->expression->type == EXT_CATCH) {
        emitCatch(ctx, out, (Catch*) ass->rvar->expression);
    } else {
        emitTerm(ctx, out, ass->rvar);
    }

    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitBranch(Emitter::Context* ctx, IO::Stream* out, Branch* br) {
    if (!br) return;

    uint32_t count = br->expressionCount > br->scopeCount
        ? br->expressionCount : br->scopeCount;

    bool first = true;
    for (uint32_t i = 0; i < count; i++) {
        bool hasCondition = (i < br->expressionCount && br->expressions[i] != NULL);
        bool hasBody      = (i < br->scopeCount      && br->scopes[i]      != NULL);
        if (!hasCondition && !hasBody) {
            continue;
        }

        indent(ctx, out);
        if (first) {
            writeKeyword(ctx, out, "if");
            first = false;
        } else if (hasCondition) {
            writeKeyword(ctx, out, "else");
            IO::write(out, ' ');
            writeKeyword(ctx, out, "if");
        } else {
            writeKeyword(ctx, out, "else");
        }
        IO::write(out, ' ');

        if (hasCondition) {
            emitTerm(ctx, out, br->expressions[i]);
            IO::write(out, ' ');
        }

        emitScopeBlock(ctx, out, hasBody ? br->scopes[i] : NULL);
        writeNewline(ctx, out);
    }
}

static void emitSwitchCase(Emitter::Context* ctx, IO::Stream* out, SwitchCase* sc) {
    if (!sc) return;

    indent(ctx, out);
    writeKeyword(ctx, out, "case");
    IO::write(out, ' ');
    emitTerm(ctx, out, sc->switchExp);
    IO::write(out, ' ');
    emitBlockHeader(ctx, out);

    uint32_t count = sc->caseCount > sc->caseExpCount
        ? sc->caseCount : sc->caseExpCount;

    for (uint32_t i = 0; i < count; i++) {
        bool hasWhen = (i < sc->caseExpCount && sc->casesExp[i] != NULL);
        bool hasBody = (i < sc->caseCount    && sc->cases[i]      != NULL);

        if (hasWhen) {
            indent(ctx, out);
            writeKeyword(ctx, out, "when");
            IO::write(out, ' ');
            emitTerm(ctx, out, sc->casesExp[i]);
            IO::write(out, ' ');
            emitScopeBlock(ctx, out, hasBody ? sc->cases[i] : NULL);
            writeNewline(ctx, out);
        } else if (hasBody) {
            emitNode(ctx, (SyntaxNode*) sc->cases[i], out);
        }
    }

    if (sc->elseCase) {
        indent(ctx, out);
        writeKeyword(ctx, out, "else");
        IO::write(out, ' ');
        emitScopeBlock(ctx, out, sc->elseCase);
        writeNewline(ctx, out);
    }

    emitBlockFooter(ctx, out);
    writeNewline(ctx, out);
}

static void emitLoop(Emitter::Context* ctx, IO::Stream* out, Loop* lp) {
    if (!lp) return;

    indent(ctx, out);
    writeKeyword(ctx, out, "loop");

    if (lp->arg.kind == Loop::Arg::RANGE) {
        IO::write(out, ' ');
        emitRange(ctx, out, lp->arg.range);
    } else if (lp->arg.exp) {
        IO::write(out, ' ');
        emitTerm(ctx, out, lp->arg.exp);
    }

    if (lp->item && lp->item->var) {
        IO::write(out, ' ');
        writeKeyword(ctx, out, "as");
        IO::write(out, ' ');
        writeName(ctx, out, (INamed*) &lp->item->var->name);
    }

    if (lp->index.var) {
        IO::write(out, ' ');
        writeKeyword(ctx, out, "at");
        IO::write(out, ' ');

        INamed* indexName = NULL;
        if (((SyntaxNode*) lp->index.var)->type == NT_VARIABLE_DEFINITION) {
            VariableDefinition* indexDef = (VariableDefinition*) lp->index.var;
            if (indexDef->var) indexName = (INamed*) &indexDef->var->name;
        } else {
            indexName = (INamed*) &((Variable*) lp->index.var)->name;
        }

        if (indexName) {
            writeName(ctx, out, indexName);
        } else {
            writeColored(ctx, out, AC_DIM, "<none>");
        }
    }

    IO::write(out, ' ');
    emitScopeBlock(ctx, out, lp->bodyScope);
    writeNewline(ctx, out);
}

static void emitReturnStatement(Emitter::Context* ctx, IO::Stream* out, ReturnStatement* rs) {
    if (!rs) return;

    indent(ctx, out);
    writeKeyword(ctx, out, "return");

    if (rs->var || rs->err) {
        IO::write(out, ' ');

        // The discard token stands in for a skipped value.
        if (rs->var) emitTerm(ctx, out, rs->var);
        else if (rs->err) writeColored(ctx, out, AC_DIM, "<none>");

        if (rs->err) {
            writeColored(ctx, out, AC_DIM, ", ");
            emitTerm(ctx, out, rs->err);
        }
    }

    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitFlowStatement(Emitter::Context* ctx, IO::Stream* out, const char* keywordText) {
    indent(ctx, out);
    writeKeyword(ctx, out, keywordText);
    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitGotoStatement(Emitter::Context* ctx, IO::Stream* out, GotoStatement* gs) {
    if (!gs) return;

    indent(ctx, out);
    writeKeyword(ctx, out, "goto");
    IO::write(out, ' ');
    writeName(ctx, out, (INamed*) &gs->name);
    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitLabel(Emitter::Context* ctx, IO::Stream* out, Label* lb) {
    if (!lb) return;

    indent(ctx, out);
    writeName(ctx, out, (INamed*) &lb->name);
    writeColored(ctx, out, AC_DIM, ":");
    writeNewline(ctx, out);
}

static void emitEnumerator(Emitter::Context* ctx, IO::Stream* out, Enumerator* en) {
    if (!en) return;

    indent(ctx, out);
    writeName(ctx, out, (INamed*) &en->name);
    writeColored(ctx, out, AC_DIM, " := ");
    writeKeyword(ctx, out, "enum");

    if (en->type && en->type->kind == Type::DT_ENUM) {
        Type::EnumInfo* eType = (Type::EnumInfo*) en->type;
        if (eType->memberKind != Type::DT_I64) {
            IO::write(out, ' ');
            writeTypeRef(ctx, out, Type::getInfo(eType->memberKind));
        }
    }

    IO::write(out, ' ');
    emitBlockHeader(ctx, out);

    for (uint32_t i = 0; i < en->varCount; i++) {
        Variable* var = en->vars[i];
        if (!var) continue;

        indent(ctx, out);
        writeName(ctx, out, (INamed*) &var->name);

        if (var->expression) {
            writeColored(ctx, out, AC_DIM, " = ");
            emitExpression(ctx, var->expression, out);
        } else if (var->value.hasValue) {
            writeColored(ctx, out, AC_DIM, " = ");
            writeValue(ctx, out, &var->value);
        }

        if (i + 1 < en->varCount) {
            IO::write(out, ',');
            writeNewline(ctx, out);
        }
    }

    emitBlockFooter(ctx, out);
    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitTypeDefinition(Emitter::Context* ctx, IO::Stream* out, SyntaxNode* node) {
    TypeDefinition* td = (TypeDefinition*) node;
    if (!td) return;

    indent(ctx, out);
    writeName(ctx, out, (INamed*) &td->name);
    writeColored(ctx, out, AC_DIM, " := ");
    writeKeyword(ctx, out, node->type == NT_UNION ? "union" : "struct");

    if (td->varCount == 0) {
        // A transparent alias: Name := i32;
        IO::write(out, ' ');

        if (td->type) {
            writeTypeRef(ctx, out, (Type::TypeInfo*) td->type);
        } else {
            writeColored(ctx, out, AC_DIM, "<none>");
        }

        IO::write(out, ';');
        writeNewline(ctx, out);
        return;
    }

    IO::write(out, ' ');
    emitBlockHeader(ctx, out);

    for (uint32_t i = 0; i < td->varCount; i++) {
        emitVariableDefinition(ctx, out, td->vars[i]);
    }

    emitBlockFooter(ctx, out);
    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitErrorSet(Emitter::Context* ctx, IO::Stream* out, ErrorSet* es) {
    if (!es) return;

    indent(ctx, out);
    writeName(ctx, out, (INamed*) &es->name);
    writeColored(ctx, out, AC_DIM, " := ");
    writeKeyword(ctx, out, "error");
    IO::write(out, ' ');
    emitBlockHeader(ctx, out);

    for (uint32_t i = 0; i < es->varCount; i++) {
        Variable* var = es->vars[i];
        if (!var) continue;

        indent(ctx, out);
        writeName(ctx, out, (INamed*) &var->name);

        if (var->value.hasValue) {
            writeColored(ctx, out, AC_DIM, " = ");
            writeValue(ctx, out, &var->value);
        }

        IO::write(out, ';');
        writeNewline(ctx, out);
    }

    emitBlockFooter(ctx, out);
    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitFunction(Emitter::Context* ctx, IO::Stream* out, Function* fcn) {
    if (!fcn) return;

    if (fcn->internalIdx != 0) {
        // TODO
        return;
    }

    indent(ctx, out);
    writeName(ctx, out, (INamed*) &fcn->name);
    writeColored(ctx, out, AC_DIM, " : ");
    IO::write(out, '(');

    for (uint32_t i = 0; i < fcn->prototype.inArgCount; i++) {
        if (i) IO::write(out, ", ");
        emitDeclarationInline(ctx, out, fcn->prototype.inArgs[i]);
    }

    IO::write(out, ')');

    if (fcn->prototype.outArg) {
        writeColored(ctx, out, AC_DIM, " -> ");

        if (hasDeclaredType(&fcn->prototype.outArg->type)) {
            writeDeclaredTypeRef(ctx, out, fcn->prototype.outArg);
        } else if (fcn->prototype.outArg->var && fcn->prototype.outArg->var->value.type) {
            writeTypeRef(ctx, out, fcn->prototype.outArg->var->value.type);
        } else {
            writeColored(ctx, out, AC_DIM, "<none>");
        }
    }

    if (fcn->errorSetName) {
        IO::write(out, ' ');
        writeKeyword(ctx, out, "using");
        IO::write(out, ' ');
        writeQualifiedName(ctx, out, fcn->errorSetName);
    }

    if (fcn->bodyScope) {
        IO::write(out, ' ');
        emitScopeBlock(ctx, out, fcn->bodyScope);
        writeNewline(ctx, out);
    } else {
        IO::write(out, ';');
        writeNewline(ctx, out);
    }
}

static void emitNamespace(Emitter::Context* ctx, IO::Stream* out, Namespace* ns) {
    if (!ns) return;

    indent(ctx, out);
    writeName(ctx, out, (INamed*) &ns->name);
    writeColored(ctx, out, AC_DIM, " : ");
    emitScopeBlock(ctx, out, &ns->scope);
    writeNewline(ctx, out);
}

static void emitImport(Emitter::Context* ctx, IO::Stream* out, ImportStatement* im) {
    if (!im) return;

    indent(ctx, out);
    writeKeyword(ctx, out, "import");
    IO::write(out, ' ');

    if (im->tag.buff && im->tag.len) {
        writeColored(ctx, out, AC_DIM, "[");
        IO::write(out, im->tag);
        writeColored(ctx, out, AC_DIM, "] ");
    }

    if (im->fname.buff && im->fname.len) {
        IO::write(out, im->fname);
    } else {
        writeColored(ctx, out, AC_DIM, "<none>");
    }

    IO::write(out, ' ');
    writeKeyword(ctx, out, "as");
    IO::write(out, ' ');

    if (im->param.buff && im->param.len) {
        IO::write(out, im->param);
    } else {
        writeColored(ctx, out, AC_DIM, "<none>");
    }

    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitCodeBlock(Emitter::Context* ctx, IO::Stream* out, CodeBlock* cb) {
    if (!cb) return;

    indent(ctx, out);
    writeColored(ctx, out, AC_DIM, "[#");
    if (cb->code.tagStr.buff) {
        IO::write(out, cb->code.tagStr);
    }
    writeColored(ctx, out, AC_DIM, "] {");
    writeNewline(ctx, out);

    IO::write(out, cb->code.codeStr);
    writeNewline(ctx, out);

    emitBlockFooter(ctx, out);
    writeNewline(ctx, out);
}

static void emitVariable(Emitter::Context* ctx, IO::Stream* out, Variable* var) {
    if (!var) return;

    indent(ctx, out);

    if (var->name.buff || var->name.pathSize) {
        writeQualifiedName(ctx, out, &var->name);
    } else {
        writeColored(ctx, out, AC_DIM, "<none>");
    }

    if (var->value.hasValue || var->expression) {
        writeColored(ctx, out, AC_DIM, " = ");
    }

    if (var->expression) {
        emitExpression(ctx, var->expression, out);
    } else if (var->value.hasValue) {
        writeValue(ctx, out, &var->value);
    }

    IO::write(out, ';');
    writeNewline(ctx, out);
}

static void emitStatement(Emitter::Context* ctx, IO::Stream* out, Statement* st) {
    if (!st) return;

    indent(ctx, out);

    if (st->operand) {
        emitTerm(ctx, out, st->operand);
    }

    IO::write(out, ';');
    writeNewline(ctx, out);
}



// === Nodes ======
// -----

static void emitNode(Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out) {
    if (!node) return;

    switch (node->type) {
        case NT_SCOPE: {
            Scope* scope = (Scope*) node;

            // The unit root is the source itself, it does not get braces
            if (ctx->indentLevel == 0) {
                emitScopeBody(ctx, out, scope);
            } else {
                emitScopeBlock(ctx, out, scope);
                writeNewline(ctx, out);
            }
            break;
        }

        case NT_VARIABLE_DEFINITION: {
            emitMarker(ctx, out, node);
            emitVariableDefinition(ctx, out, (VariableDefinition*) node);
            break;
        }

        case NT_VARIABLE_ASSIGNMENT: {
            emitMarker(ctx, out, node);
            emitVariableAssignment(ctx, out, (VariableAssignment*) node);
            break;
        }

        case NT_TYPE_DEFINITION:
        case NT_UNION: {
            emitMarker(ctx, out, node);
            emitTypeDefinition(ctx, out, node);
            break;
        }

        case NT_TYPE_INITIALIZATION: {
            emitMarker(ctx, out, node);
            indent(ctx, out);
            emitTypeInitialization(ctx, out, (TypeInitialization*) node);
            IO::write(out, ';');
            writeNewline(ctx, out);
            break;
        }

        case NT_ENUMERATOR: {
            emitMarker(ctx, out, node);
            emitEnumerator(ctx, out, (Enumerator*) node);
            break;
        }

        case NT_VARIABLE: {
            emitMarker(ctx, out, node);
            emitVariable(ctx, out, (Variable*) node);
            break;
        }

        case NT_FUNCTION: {
            emitMarker(ctx, out, node);
            emitFunction(ctx, out, (Function*) node);
            break;
        }

        case NT_BRANCH: {
            emitMarker(ctx, out, node);
            emitBranch(ctx, out, (Branch*) node);
            break;
        }

        case NT_SWITCH_CASE: {
            emitMarker(ctx, out, node);
            emitSwitchCase(ctx, out, (SwitchCase*) node);
            break;
        }

        case NT_LOOP: {
            emitMarker(ctx, out, node);
            emitLoop(ctx, out, (Loop*) node);
            break;
        }

        case NT_RETURN_STATEMENT: {
            emitMarker(ctx, out, node);
            emitReturnStatement(ctx, out, (ReturnStatement*) node);
            break;
        }

        case NT_CONTINUE_STATEMENT: {
            emitMarker(ctx, out, node);
            emitFlowStatement(ctx, out, "continue");
            break;
        }

        case NT_BREAK_STATEMENT: {
            emitMarker(ctx, out, node);
            emitFlowStatement(ctx, out, "break");
            break;
        }

        case NT_GOTO_STATEMENT: {
            emitMarker(ctx, out, node);
            emitGotoStatement(ctx, out, (GotoStatement*) node);
            break;
        }

        case NT_LABEL: {
            emitMarker(ctx, out, node);
            emitLabel(ctx, out, (Label*) node);
            break;
        }

        case NT_NAMESPACE: {
            emitMarker(ctx, out, node);
            emitNamespace(ctx, out, (Namespace*) node);
            break;
        }

        case NT_STATEMENT: {
            emitMarker(ctx, out, node);
            emitStatement(ctx, out, (Statement*) node);
            break;
        }

        case NT_CODE_BLOCK: {
            emitMarker(ctx, out, node);
            emitCodeBlock(ctx, out, (CodeBlock*) node);
            break;
        }

        case NT_ERROR: {
            emitMarker(ctx, out, node);
            emitErrorSet(ctx, out, (ErrorSet*) node);
            break;
        }

        case NT_IMPORT: {
            emitMarker(ctx, out, node);
            emitImport(ctx, out, (ImportStatement*) node);
            break;
        }

        default: {
            indent(ctx, out);
            writeColored(ctx, out, AC_DIM, "<no_emitter>");
            writeNewline(ctx, out);
            break;
        }
    }
}

static void emitUnit(Emitter::Context* ctx, Reg::Unit* unit, IO::Stream* out) {
    if (!unit || !unit->ast || !unit->ast->root) {
        return;
    }

    emitNode(ctx, (SyntaxNode*) unit->ast->root, out);
    writeNewline(ctx, out);
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
