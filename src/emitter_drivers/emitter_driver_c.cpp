#include "emitter_driver_c.h"
#include "../syntax.h"
#include "../registry.h"
#include "../io.h"

#include <cmath>


static void emitNode(Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out);
static void emitExpression(Emitter::Context* ctx, Expression* exp, IO::Stream* out, Variable* lvalue = nullptr);
static void emitVariable(Emitter::Context* ctx, IO::Stream* out, Variable* const node, Variable* lvalue = nullptr);


static void indent(Emitter::Context* ctx, IO::Stream* out) {
    if (!ctx || ctx->style.format != Emitter::Format::PRETTY) return;

    for (uint32_t i = 0; i < (uint32_t) (ctx->indentLevel * ctx->style.indentStep); i++) {
        IO::write(out, ' ');
    }
}

static void emitName(IO::Stream* out, const INamed* name) {
    if (!name || !name->buff) return;

    IO::writef(out, "%.*s", (int) name->len, name->buff);
}

static void emitName(IO::Stream* out, const INamedEx* name) {
    if (!name || !name->buff) return;

    if (name->id != 0) {
        IO::writef(out, "%.*s_%lli", (int) name->len, name->buff, (long long) name->id);
    } else {
        IO::writef(out, "%.*s", (int) name->len, name->buff);
    }
}

static void emitArrayLenName(IO::Stream* out, Variable* var) {
    if (!var || !var->name.buff) return;

    IO::writef(out, "%.*s_len_%lli", (int) var->name.len, var->name.buff, (long long) var->name.id);
}


static void emitDataType(IO::Stream* out, const Type::Kind dtypeEnum) {
    switch (dtypeEnum) {
        case Type::DT_I8:      IO::write(out, "int8_t"); break;
        case Type::DT_I16:     IO::write(out, "int16_t"); break;
        case Type::DT_I32:     IO::write(out, "int32_t"); break;
        case Type::DT_I64:     IO::write(out, "int64_t"); break;
        case Type::DT_U8:      IO::write(out, "uint8_t"); break;
        case Type::DT_U16:     IO::write(out, "uint16_t"); break;
        case Type::DT_U32:     IO::write(out, "uint32_t"); break;
        case Type::DT_U64:     IO::write(out, "uint64_t"); break;
        case Type::DT_F32:     IO::write(out, "float"); break;
        case Type::DT_F64:     IO::write(out, "double"); break;
        case Type::DT_POINTER: IO::write(out, "void*"); break;
        case Type::DT_STRING:  IO::write(out, "char*"); break;
        case Type::DT_ERROR:   IO::write(out, "int"); break;
        case Type::DT_VOID:    IO::write(out, "void"); break;
        default:               IO::write(out, "int"); break;
    }
}

static void emitDataType(Emitter::Context* ctx, IO::Stream* out, const Type::Kind dtypeEnum, void* dtype) {
    if (dtypeEnum == Type::DT_POINTER && dtype) {
        Pointer* const ptr = (Pointer*) dtype;
        emitDataType(ctx, out, ptr->pointsToKind, ptr->pointsTo);
        IO::write(out, '*');
    } else if (dtypeEnum == Type::DT_ARRAY && dtype) {
        Array* const arr = (Array*) dtype;
        emitDataType(ctx, out, arr->base.pointsToKind, arr->base.pointsTo);
    } else if (dtypeEnum == Type::DT_CUSTOM && dtype) {
        TypeDefinition* td = (TypeDefinition*) dtype;
        emitName(out, &td->name);
    } else if (dtypeEnum == Type::DT_UNION && dtype) {
        Union* u = (Union*) dtype;
        emitName(out, &u->base.name);
    } else if (dtypeEnum == Type::DT_FUNCTION && dtype) {
        FunctionPrototype* fptr = (FunctionPrototype*) dtype;
        if (fptr->outArg && fptr->outArg->var) {
            emitDataType(ctx, out, fptr->outArg->var->value.typeKind, fptr->outArg->var->value.any);
        } else {
            IO::write(out, "void");
        }
        IO::write(out, "(*)");
        IO::write(out, '(');
        const int len = fptr->inArgCount;
        for (int i = 0; i < len; i++) {
            if (fptr->inArgs[i] && fptr->inArgs[i]->var) {
                Value* val = &(fptr->inArgs[i]->var->value);
                emitDataType(ctx, out, val->typeKind, val->any);
            }
            if (i != len - 1) {
                IO::write(out, ", ");
            }
        }
        IO::write(out, ')');
    } else {
        emitDataType(out, dtypeEnum);
    }
}


static void emitOperandValue(IO::Stream* out, Variable* op) {
    if (!op) return;

    switch (op->value.typeKind) {
        case Type::DT_I32:
            IO::writef(out, "%i", op->value.i32);
            break;
        case Type::DT_POINTER:
        case Type::DT_I64:
            IO::writef(out, "%lli", (long long) op->value.i64);
            break;
        case Type::DT_U8:
        case Type::DT_U16:
        case Type::DT_U32:
        case Type::DT_U64:
            IO::writef(out, "%llu", (unsigned long long) op->value.i64);
            break;
        case Type::DT_F32:
            IO::writef(out, "%.9g%s", (double) op->value.f32, std::fmod(op->value.f32, 1.0f) == 0.0f ? ".f" : "f");
            break;
        case Type::DT_F64:
            IO::writef(out, "%.17g%s", op->value.f64, std::fmod(op->value.f64, 1.0) == 0.0 ? "." : "");
            break;
        case Type::DT_STRING:
            if (op->value.str) {
                IO::writef(out, "\"%s\"", (char*) op->value.str);
            }
            break;
        case Type::DT_ERROR:
            IO::writef(out, "%llu", (unsigned long long) op->value.i64);
            break;
        default:
            break;
    }
}

static void emitOperator(IO::Stream* out, OperatorEnum opType) {
    switch (opType) {
        case OP_UNARY_PLUS:                   IO::write(out, '+'); break;
        case OP_UNARY_MINUS:                  IO::write(out, '-'); break;
        case OP_ADDITION:                     IO::write(out, '+'); break;
        case OP_SUBTRACTION:                  IO::write(out, '-'); break;
        case OP_MULTIPLICATION:               IO::write(out, '*'); break;
        case OP_DIVISION:                     IO::write(out, '/'); break;
        case OP_MODULO:                       IO::write(out, '%'); break;
        case OP_GET_ADDRESS:                  IO::write(out, '&'); break;
        case OP_GET_VALUE:                    IO::write(out, '*'); break;
        case OP_BITWISE_AND:                  IO::write(out, '&'); break;
        case OP_BITWISE_OR:                   IO::write(out, '|'); break;
        case OP_BITWISE_XOR:                  IO::write(out, '^'); break;
        case OP_BITWISE_NEGATION:             IO::write(out, '~'); break;
        case OP_SHIFT_RIGHT:                  IO::write(out, ">>"); break;
        case OP_SHIFT_LEFT:                   IO::write(out, "<<"); break;
        case OP_NEGATION:                     IO::write(out, '!'); break;
        case OP_EQUAL:                        IO::write(out, "=="); break;
        case OP_NOT_EQUAL:                    IO::write(out, "!="); break;
        case OP_LESS_THAN:                    IO::write(out, '<'); break;
        case OP_GREATER_THAN:                 IO::write(out, '>'); break;
        case OP_LESS_THAN_OR_EQUAL:           IO::write(out, "<="); break;
        case OP_GREATER_THAN_OR_EQUAL:        IO::write(out, ">="); break;
        case OP_BOOL_AND:                     IO::write(out, "&&"); break;
        case OP_BOOL_OR:                      IO::write(out, "||"); break;
        case OP_INCREMENT:                    IO::write(out, "++"); break;
        case OP_DECREMENT:                    IO::write(out, "--"); break;
        case OP_SUBSCRIPT:                    IO::write(out, '['); break;
        case OP_MEMBER_SELECTION:             IO::write(out, '.'); break;
        case OP_DEREFERENCE_MEMBER_SELECTION: IO::write(out, "->"); break;
        default: break;
    }
}


static void emitVariable(Emitter::Context* ctx, IO::Stream* out, Variable* const node, Variable* lvalue) {
    if (!node) return;

    if (node->def && (node->def->base.flags & IS_ARRAY_LIST)) {
        if (node->base.flags & IS_SIZE) {
            Variable* tmp = node->def->var;
            IO::write(out, '(');
            if (tmp) emitName(out, &tmp->name);
            IO::write(out, "->size)");
        } else if (node->base.flags & IS_LENGTH) {
            Variable* tmp = node->def->var;
            IO::write(out, '(');
            if (tmp) emitName(out, &tmp->name);
            IO::write(out, "->len)");
        } else {
            IO::write(out, '(');
            emitName(out, &node->name);
            IO::write(out, "->data)");
        }
        return;
    }

    if (node->def && (node->def->base.flags & IS_CMP_TIME) && node->def->var->value.typeKind != Type::DT_ARRAY) {
        emitOperandValue(out, node->def->var);
    } else if (node->name.len > 0 && node->name.buff) {
        if (node->value.typeKind == Type::DT_FUNCTION) {
            IO::write(out, '&');
            emitName(out, &node->name);
        } else {
            emitName(out, &node->name);
        }
    } else {
        if ((node->base.flags & IS_LENGTH) && node->def && node->def->var != lvalue) {
            Variable* const tmp = node->def->var;
            if (tmp && tmp->value.arr && tmp->value.arr->length && tmp->value.arr->length->value.hasValue) {
                emitVariable(ctx, out, tmp->value.arr->length, lvalue);
            } else if (tmp) {
                emitArrayLenName(out, tmp);
            }
        } else if (node->expression && !node->value.hasValue) {
            emitExpression(ctx, node->expression, out, lvalue);
        } else {
            emitOperandValue(out, node);
        }
    }
}

static void emitFunctionCall(Emitter::Context* ctx, FunctionCall* const node, IO::Stream* out, Variable* lvalue = nullptr, Variable* err = nullptr) {
    if (!node) return;

    if (node->fptr) {
        emitName(out, &node->fptr->name);
        IO::write(out, '(');
    } else if (node->fcn && node->fcn->internalIdx <= 0) {
        emitName(out, &node->fcn->name);
        IO::write(out, '(');
    } else if (node->fcn && node->fcn->internalIdx == Ast::Internal::IF_ALLOC) {
        if (node->inArgCount > 0 && node->inArgs[0]) {
            Variable* var = node->inArgs[0];
            const Type::Kind dtype = var->value.typeKind;
            IO::write(out, "malloc(sizeof(");

            if (dtype == Type::DT_CUSTOM && var->value.def) {
                TypeDefinition* customDtype = var->value.def;
                emitName(out, &customDtype->name);
                IO::write(out, "))");

                if (lvalue && lvalue->value.ptr && var->expression) {
                    TypeDefinition* lvalueDtype = (TypeDefinition*) (lvalue->value.ptr->pointsTo);
                    TypeInitialization* typeInit = (TypeInitialization*) (var->expression);

                    for (uint32_t i = 0; i < typeInit->attributeCount; i++) {
                        IO::write(out, ';');
                        indent(ctx, out);
                        emitVariable(ctx, out, lvalue);
                        IO::write(out, "->");

                        if (lvalueDtype && typeInit->idxs && typeInit->idxs[i] >= 0) {
                            emitVariable(ctx, out, lvalueDtype->vars[typeInit->idxs[i]]);
                        }

                        IO::write(out, '=');
                        if (typeInit->attributes[i] && typeInit->attributes[i]->expression) {
                            emitExpression(ctx, typeInit->attributes[i]->expression, out);
                        }
                    }
                }
            } else if (dtype == Type::DT_ARRAY && var->value.arr) {
                Array* arr = var->value.arr;
                emitDataType(ctx, out, arr->base.pointsToKind, arr->base.pointsTo);
                IO::write(out, ")*");
                emitVariable(ctx, out, arr->length, lvalue);
                IO::write(out, ')');
            } else {
                emitDataType(out, dtype);
                IO::write(out, "))");

                if (var->value.hasValue && lvalue) {
                    IO::write(out, ';');
                    indent(ctx, out);
                    emitVariable(ctx, out, lvalue);
                    IO::write(out, '=');
                    emitVariable(ctx, out, var);
                } else if (var->expression && lvalue) {
                    IO::write(out, ";*");
                    emitVariable(ctx, out, lvalue);
                    IO::write(out, '=');
                    emitVariable(ctx, out, var);
                }
            }
        } else {
            IO::write(out, "malloc(0)");
        }
        return;
    } else {
        if (node->name.buff) {
            IO::writef(out, "%.*s(", (int) node->name.len, node->name.buff);
        } else {
            IO::write(out, "func(");
        }
    }

    Variable** const callInArgs = node->inArgs;
    const int inArgsCnt = node->inArgCount;
    bool multipleTypes = false;

    for (int i = 0; i < inArgsCnt; i++) {
        emitVariable(ctx, out, callInArgs[i]);

        if (!multipleTypes) {
            Variable* tmp = nullptr;
            if (node->fcn && i < (int) node->fcn->prototype.inArgCount && node->fcn->prototype.inArgs[i]) {
                tmp = node->fcn->prototype.inArgs[i]->var;
            } else {
                tmp = node->fptr;
            }

            if (tmp) {
                if (tmp->value.typeKind == Type::DT_MULTIPLE_TYPES) {
                    multipleTypes = true;
                } else if (tmp->value.typeKind == Type::DT_ARRAY && tmp->value.arr && !(tmp->value.arr->flags & IS_ARRAY_LIST)) {
                    IO::write(out, ", ");
                    if (callInArgs[i] && callInArgs[i]->value.arr) {
                        emitVariable(ctx, out, callInArgs[i]->value.arr->length);
                    }
                }
            }
        }

        if (i < inArgsCnt - 1) {
            IO::write(out, ", ");
        }
    }

    if (err) {
        if (inArgsCnt > 0) IO::write(out, ", ");
        IO::write(out, '&');
        emitVariable(ctx, out, err);
    }

    IO::write(out, ')');
}

static void emitTypeInitialization(Emitter::Context* ctx, TypeInitialization* const node, IO::Stream* out, Variable* lvalue = nullptr) {
    if (!node) return;

    if (lvalue) {
        TypeDefinition* const td = lvalue->def ? (lvalue->def->var ? lvalue->def->var->value.def : nullptr) : lvalue->value.def;
        if (td) {
            IO::write(out, "((");
            emitName(out, &td->name);
            IO::write(out, "){");
        } else {
            IO::write(out, '{');
        }

        if (lvalue->base.type == NT_UNION) {
            if (node->attributeCount > 0 && node->attributes[0]) {
                Variable* const var = node->attributes[0];
                IO::write(out, '.');
                emitName(out, &var->name);
                IO::write(out, " = ");
                if (var->expression) emitExpression(ctx, var->expression, out);
                else emitOperandValue(out, var);
            }
        } else if (node->fillVar && td) {
            uint32_t size = td->varCount;
            for (uint32_t i = 0; i < size; i++) {
                int idx = (node->idxs) ? node->idxs[i] : -1;
                if (idx >= 0 && (uint32_t) idx < node->attributeCount && node->attributes[idx]) {
                    Variable* const var = node->attributes[idx];
                    if (var->expression) emitExpression(ctx, var->expression, out);
                    else emitOperandValue(out, var);
                } else {
                    emitVariable(ctx, out, node->fillVar);
                }
                if (i < size - 1) IO::write(out, ", ");
            }
        } else {
            uint32_t size = node->attributeCount;
            for (uint32_t i = 0; i < size; i++) {
                Variable* const var = node->attributes[i];
                if (!var) continue;
                if (var->name.len > 0) {
                    IO::write(out, '.');
                    emitName(out, &var->name);
                    IO::write(out, " = ");
                }
                if (var->expression) emitExpression(ctx, var->expression, out);
                else emitOperandValue(out, var);

                if (i < size - 1) IO::write(out, ", ");
            }
        }
        IO::write(out, "})");
    } else {
        IO::write(out, '{');
        uint32_t size = node->attributeCount;
        for (uint32_t i = 0; i < size; i++) {
            Variable* const var = node->attributes[i];
            if (!var) continue;
            if (var->name.len > 0) {
                IO::write(out, '.');
                emitName(out, &var->name);
                IO::write(out, " = ");
            }
            if (var->expression) emitExpression(ctx, var->expression, out);
            else emitOperandValue(out, var);

            if (i < size - 1) IO::write(out, ", ");
        }
        IO::write(out, '}');
    }
}

static void emitStringInitialization(Emitter::Context* ctx, StringInitialization* const node, IO::Stream* out, Variable* lvalue = nullptr) {
    if (!node) return;

    if (!node->wideStr.buff) {
        if (node->rawStr.buff) {
            IO::writef(out, "\"%.*s\"", (int) node->rawStr.len, node->rawStr.buff);
        } else {
            IO::write(out, "\"\"");
        }
        return;
    }

    IO::write(out, '{');
    switch (node->wideType) {
        case Type::DT_U8: {
            uint8_t* arr = (uint8_t*) node->wideStr.buff;
            for (uint64_t i = 0; i < node->wideStr.len; i++) {
                IO::writef(out, "%u%s", arr[i], (i < node->wideStr.len - 1) ? "," : "");
            }
            break;
        }
        case Type::DT_U16: {
            uint16_t* arr = (uint16_t*) node->wideStr.buff;
            for (uint64_t i = 0; i < node->wideStr.len; i++) {
                IO::writef(out, "%u%s", arr[i], (i < node->wideStr.len - 1) ? "," : "");
            }
            break;
        }
        case Type::DT_U32: {
            uint32_t* arr = (uint32_t*) node->wideStr.buff;
            for (uint64_t i = 0; i < node->wideStr.len; i++) {
                IO::writef(out, "%u%s", arr[i], (i < node->wideStr.len - 1) ? "," : "");
            }
            break;
        }
        case Type::DT_U64: {
            uint64_t* arr = (uint64_t*) node->wideStr.buff;
            for (uint64_t i = 0; i < node->wideStr.len; i++) {
                IO::writef(out, "%llu%s", (unsigned long long) arr[i], (i < node->wideStr.len - 1) ? "," : "");
            }
            break;
        }
        default:
            break;
    }
    IO::write(out, '}');
}

static void emitArrayInitialization(Emitter::Context* ctx, ArrayInitialization* const node, IO::Stream* out, Variable* lvalue = nullptr) {
    if (!node) return;

    IO::write(out, '{');
    for (uint32_t i = 0; i < node->attributeCount; i++) {
        Variable* const var = node->attributes[i];
        if (!var) continue;

        if (var->expression) {
            emitExpression(ctx, var->expression, out);
        } else {
            emitOperandValue(out, var);
        }

        if (i < node->attributeCount - 1) {
            IO::write(out, ", ");
        }
    }
    IO::write(out, '}');
}

static void emitCatchExpression(Emitter::Context* ctx, Catch* node, IO::Stream* out, Variable* lvalue, bool isGlobal) {
    if (!node) return;

    Variable* const err = node->err;
    FunctionCall* const call = node->call;

    if (err) {
        IO::write(out, "int ");
        emitName(out, &err->name);
        IO::write(out, " = 0;\n");
        indent(ctx, out);
    }

    if (lvalue) {
        emitVariable(ctx, out, lvalue);
        IO::write(out, " = ");
    }

    if (call) {
        emitFunctionCall(ctx, call, out, lvalue, err);
    }
    IO::write(out, ';');

    if (node->scope) {
        IO::write(out, '\n');
        emitNode(ctx, (SyntaxNode*) node->scope, out);
    }
}


static void emitExpression(Emitter::Context* ctx, Expression* exp, IO::Stream* out, Variable* lvalue) {
    if (!exp) return;

    switch (exp->type) {
        case EXT_UNARY: {
            UnaryExpression* uex = (UnaryExpression*) exp;
            IO::write(out, '(');
            emitOperator(out, uex->base.opType);
            if (uex->operand) {
                if (uex->operand->expression) {
                    emitExpression(ctx, uex->operand->expression, out, lvalue);
                } else {
                    emitVariable(ctx, out, uex->operand, lvalue);
                }
            }
            IO::write(out, ')');
            break;
        }

        case EXT_BINARY: {
            BinaryExpression* bex = (BinaryExpression*) exp;
            if (bex->left && bex->left->value.typeKind == Type::DT_ARRAY && isMemberSelection(bex->base.opType)) {
                IO::write(out, '(');
                if (bex->left->value.arr && bex->left->value.arr->length) {
                    emitVariable(ctx, out, bex->left->value.arr->length);
                }
                IO::write(out, ')');
                return;
            }

            IO::write(out, '(');
            if (bex->left) {
                if (bex->left->expression) {
                    emitExpression(ctx, bex->left->expression, out, lvalue);
                } else {
                    emitVariable(ctx, out, bex->left, lvalue);
                }
            }

            emitOperator(out, bex->base.opType);

            if (bex->right) {
                if (bex->right->expression) {
                    emitExpression(ctx, bex->right->expression, out, lvalue);
                } else {
                    emitVariable(ctx, out, bex->right, lvalue);
                }
            }

            if (bex->base.opType == OP_SUBSCRIPT) {
                IO::write(out, ']');
            }
            IO::write(out, ')');
            break;
        }

        case EXT_TERNARY: {
            TernaryExpression* tex = (TernaryExpression*) exp;
            IO::write(out, '(');
            emitVariable(ctx, out, tex->condition, lvalue);
            IO::write(out, " ? ");
            emitVariable(ctx, out, tex->trueExp, lvalue);
            IO::write(out, " : ");
            emitVariable(ctx, out, tex->falseExp, lvalue);
            IO::write(out, ')');
            break;
        }

        case EXT_FUNCTION_CALL: {
            emitFunctionCall(ctx, (FunctionCall*) exp, out, lvalue);
            break;
        }

        case EXT_TYPE_INITIALIZATION: {
            emitTypeInitialization(ctx, (TypeInitialization*) exp, out, lvalue);
            break;
        }

        case EXT_STRING_INITIALIZATION: {
            emitStringInitialization(ctx, (StringInitialization*) exp, out, lvalue);
            break;
        }

        case EXT_ARRAY_INITIALIZATION: {
            emitArrayInitialization(ctx, (ArrayInitialization*) exp, out, lvalue);
            break;
        }

        case EXT_CATCH: {
            emitCatchExpression(ctx, (Catch*) exp, out, lvalue, false);
            break;
        }

        case EXT_CAST: {
            Cast* cast = (Cast*) exp;
            IO::write(out, '(');
            emitDataType(out, cast->target);
            IO::write(out, ") ");
            emitVariable(ctx, out, cast->operand, lvalue);
            break;
        }

        case EXT_GET_LENGTH: {
            GetLength* glen = (GetLength*) exp;
            if (glen->arr && glen->arr->value.arr && glen->arr->value.arr->length) {
                emitVariable(ctx, out, glen->arr->value.arr->length, lvalue);
            }
            break;
        }

        case EXT_GET_SIZE: {
            GetSize* gsz = (GetSize*) exp;
            if (gsz->arr && gsz->arr->value.arr && gsz->arr->value.arr->length) {
                emitVariable(ctx, out, gsz->arr->value.arr->length, lvalue);
            }
            break;
        }

        default:
            break;
    }
}


static void emitFunctionDefinition(Emitter::Context* ctx, IO::Stream* out, Function* const node, bool isHeader) {
    if (!node) return;

    if (node->prototype.outArg && node->prototype.outArg->var) {
        emitDataType(ctx, out, node->prototype.outArg->var->value.typeKind, node->prototype.outArg->var->value.any);
    } else {
        IO::write(out, "void");
    }

    IO::write(out, ' ');
    emitName(out, &node->name);
    IO::write(out, '(');

    const int inArgCnt = (int) node->prototype.inArgCount;
    for (int i = 0; i < inArgCnt; i++) {
        VariableDefinition* const varDef = node->prototype.inArgs[i];
        if (varDef && varDef->var) {
            emitDataType(ctx, out, varDef->var->value.typeKind, varDef->var->value.any);
            IO::write(out, ' ');
            emitName(out, &varDef->var->name);

            if (varDef->var->value.typeKind == Type::DT_ARRAY && varDef->var->value.arr) {
                if (!(varDef->var->value.arr->flags & IS_ARRAY_LIST)) {
                    IO::write(out, ", uint64_t ");
                    emitArrayLenName(out, varDef->var);
                }
            }
        }
        if (i < inArgCnt - 1) {
            IO::write(out, ", ");
        }
    }

    if (node->errorSet) {
        if (inArgCnt > 0) IO::write(out, ", ");
        IO::write(out, "int* err");
    }

    IO::write(out, ')');
}


static void emitNode(Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out) {
    if (!node) return;

    switch (node->type) {
        case NT_SCOPE: {
            Scope* s = (Scope*) node;
            indent(ctx, out);
            IO::write(out, "{\n");
            ctx->indentLevel++;

            for (uint32_t i = 0; i < s->childrenCount; i++) {
                emitNode(ctx, s->children[i], out);
            }

            ctx->indentLevel--;
            indent(ctx, out);
            IO::write(out, "}\n");
            break;
        }

        case NT_VARIABLE_DEFINITION: {
            VariableDefinition* varDef = (VariableDefinition*) node;
            if ((varDef->base.flags & IS_CMP_TIME) && varDef->var && varDef->var->value.typeKind != Type::DT_ARRAY) {
                break;
            }

            indent(ctx, out);

            if (varDef->var && varDef->var->expression && varDef->var->expression->type == EXT_CATCH) {
                emitCatchExpression(ctx, (Catch*) varDef->var->expression, out, varDef->var, (SyntaxNode::root == varDef->base.scope));
                IO::write(out, '\n');
                break;
            }

            if (varDef->var) {
                Variable* var = varDef->var;
                Type::Kind dtype = var->value.typeKind;

                if (dtype == Type::DT_ARRAY && var->value.arr) {
                    Array* arr = var->value.arr;
                    if (arr->flags & IS_ARRAY_LIST) {
                        emitDataType(ctx, out, arr->base.pointsToKind, arr->base.pointsTo);
                        IO::write(out, "* ");
                        emitName(out, &var->name);
                        IO::write(out, " = arrayListCreate(");
                        if (arr->length) {
                            emitVariable(ctx, out, arr->length);
                        }
                        IO::write(out, ");\n");
                        break;
                    } else if (arr->flags & IS_ALLOCATED) {
                        IO::write(out, "uint64_t ");
                        emitArrayLenName(out, var);
                        IO::write(out, " = ");
                        if (arr->length) {
                            emitVariable(ctx, out, arr->length);
                        } else {
                            IO::write(out, '0');
                        }
                        IO::write(out, ";\n");
                        indent(ctx, out);
                        emitDataType(ctx, out, arr->base.pointsToKind, arr->base.pointsTo);
                        IO::write(out, "* ");
                        emitName(out, &var->name);
                    } else {
                        emitDataType(ctx, out, arr->base.pointsToKind, arr->base.pointsTo);
                        IO::write(out, ' ');
                        emitName(out, &var->name);
                        IO::write(out, '[');
                        if (arr->length) {
                            emitVariable(ctx, out, arr->length);
                        }
                        IO::write(out, ']');
                    }
                } else if (dtype == Type::DT_FUNCTION) {
                    FunctionPrototype* fptr = (FunctionPrototype*) var->value.any;
                    if (fptr && fptr->outArg && fptr->outArg->var) {
                        emitDataType(ctx, out, fptr->outArg->var->value.typeKind, fptr->outArg->var->value.any);
                    } else {
                        IO::write(out, "void");
                    }
                    IO::write(out, " (*");
                    emitName(out, &var->name);
                    IO::write(out, ")(");
                    if (fptr) {
                        for (uint32_t i = 0; i < fptr->inArgCount; i++) {
                            if (fptr->inArgs[i] && fptr->inArgs[i]->var) {
                                emitDataType(ctx, out, fptr->inArgs[i]->var->value.typeKind, fptr->inArgs[i]->var->value.any);
                            }
                            if (i < fptr->inArgCount - 1) IO::write(out, ", ");
                        }
                    }
                    IO::write(out, ')');
                } else {
                    emitDataType(ctx, out, var->value.typeKind, var->value.any);
                    IO::write(out, ' ');
                    emitName(out, &var->name);
                }

                if (var->expression) {
                    IO::write(out, " = ");
                    emitExpression(ctx, var->expression, out, var);
                } else if (var->value.hasValue) {
                    IO::write(out, " = ");
                    emitOperandValue(out, var);
                }
            }

            IO::write(out, ";\n");
            break;
        }

        case NT_VARIABLE_ASSIGNMENT: {
            VariableAssignment* ass = (VariableAssignment*) node;
            indent(ctx, out);

            if (!ass->rvar) {
                if (ass->lvar) {
                    emitVariable(ctx, out, ass->lvar);
                }
                IO::write(out, ";\n");
                break;
            }

            if (ass->rvar->expression && ass->rvar->expression->type == EXT_CATCH) {
                emitCatchExpression(ctx, (Catch*) ass->rvar->expression, out, ass->lvar, false);
                IO::write(out, '\n');
                break;
            }

            if (ass->lvar) {
                emitVariable(ctx, out, ass->lvar);
                IO::write(out, " = ");
            }

            if (ass->rvar->expression) {
                emitExpression(ctx, ass->rvar->expression, out, ass->lvar);
            } else {
                emitVariable(ctx, out, ass->rvar, ass->lvar);
            }
            IO::write(out, ";\n");
            break;
        }

        case NT_TYPE_DEFINITION:
        case NT_UNION: {
            TypeDefinition* td = (TypeDefinition*) node;
            indent(ctx, out);

            if (node->type == NT_UNION) {
                IO::write(out, "typedef union ");
            } else {
                IO::write(out, "typedef struct ");
            }
            emitName(out, &td->name);
            IO::write(out, " {\n");

            ctx->indentLevel++;
            for (uint32_t i = 0; i < td->varCount; i++) {
                Variable* var = td->vars[i];
                if (!var) continue;
                indent(ctx, out);

                if (var->value.typeKind == Type::DT_ARRAY && var->value.arr) {
                    emitDataType(ctx, out, var->value.arr->base.pointsToKind, var->value.arr->base.pointsTo);
                    IO::write(out, ' ');
                    emitName(out, &var->name);
                    IO::write(out, '[');
                    if (var->value.arr->length) {
                        emitVariable(ctx, out, var->value.arr->length, var);
                    }
                    IO::write(out, ']');
                } else {
                    emitDataType(ctx, out, var->value.typeKind, var->value.any);
                    IO::write(out, ' ');
                    emitName(out, &var->name);
                }
                IO::write(out, ";\n");
            }
            ctx->indentLevel--;

            indent(ctx, out);
            IO::write(out, "} ");
            emitName(out, &td->name);
            IO::write(out, ";\n\n");
            break;
        }

        case NT_FUNCTION: {
            Function* fcn = (Function*) node;
            if ((fcn->base.flags & IS_RENDERED) || fcn->internalIdx == -1) break;

            indent(ctx, out);
            emitFunctionDefinition(ctx, out, fcn, false);
            IO::write(out, ' ');

            if (fcn->bodyScope) {
                emitNode(ctx, (SyntaxNode*) fcn->bodyScope, out);
            } else {
                IO::write(out, ";\n");
            }
            fcn->base.flags |= IS_RENDERED;
            break;
        }

        case NT_BRANCH: {
            Branch* branch = (Branch*) node;
            indent(ctx, out);

            for (uint32_t i = 0; i < branch->expressionCount; i++) {
                if (i == 0) {
                    IO::write(out, "if (");
                } else {
                    indent(ctx, out);
                    IO::write(out, "else if (");
                }

                if (branch->expressions[i]) {
                    emitVariable(ctx, out, branch->expressions[i]);
                }
                IO::write(out, ") ");

                if (i < branch->scopeCount && branch->scopes[i]) {
                    emitNode(ctx, (SyntaxNode*) branch->scopes[i], out);
                }
            }

            if (branch->scopeCount > branch->expressionCount && branch->scopes[branch->expressionCount]) {
                indent(ctx, out);
                IO::write(out, "else ");
                emitNode(ctx, (SyntaxNode*) branch->scopes[branch->expressionCount], out);
            }
            break;
        }

        case NT_SWITCH_CASE: {
            SwitchCase* sc = (SwitchCase*) node;
            indent(ctx, out);
            IO::write(out, "switch (");
            if (sc->switchExp) emitVariable(ctx, out, sc->switchExp);
            IO::write(out, ") {\n");

            ctx->indentLevel++;
            for (uint32_t i = 0; i < sc->caseCount; i++) {
                indent(ctx, out);
                IO::write(out, "case ");
                if (sc->casesExp[i]) emitVariable(ctx, out, sc->casesExp[i]);
                IO::write(out, ":\n");

                if (sc->cases[i]) emitNode(ctx, (SyntaxNode*) sc->cases[i], out);

                indent(ctx, out);
                IO::write(out, "break;\n");
            }

            if (sc->elseCase) {
                indent(ctx, out);
                IO::write(out, "default:\n");
                emitNode(ctx, (SyntaxNode*) sc->elseCase, out);
            }
            ctx->indentLevel--;

            indent(ctx, out);
            IO::write(out, "}\n");
            break;
        }

        case NT_WHILE_LOOP: {
            WhileLoop* wl = (WhileLoop*) node;
            indent(ctx, out);
            IO::write(out, "while (");
            if (wl->expression) emitVariable(ctx, out, wl->expression);
            IO::write(out, ") ");
            if (wl->bodyScope) emitNode(ctx, (SyntaxNode*) wl->bodyScope, out);
            break;
        }

        case NT_LOOP: {
            indent(ctx, out);
            IO::write(out, "for (;;) ");
            break;
        }

        case NT_RETURN_STATEMENT: {
            ReturnStatement* rs = (ReturnStatement*) node;
            indent(ctx, out);

            if (rs->err) {
                IO::write(out, "*err = ");
                emitVariable(ctx, out, rs->err);
                IO::write(out, ";\n");
                indent(ctx, out);
            }

            if (rs->var) {
                IO::write(out, "return ");
                emitVariable(ctx, out, rs->var);
                IO::write(out, ";\n");
            } else {
                IO::write(out, "return;\n");
            }
            break;
        }

        case NT_CONTINUE_STATEMENT: {
            indent(ctx, out);
            IO::write(out, "continue;\n");
            break;
        }

        case NT_BREAK_STATEMENT: {
            indent(ctx, out);
            IO::write(out, "break;\n");
            break;
        }

        case NT_GOTO_STATEMENT: {
            GotoStatement* gs = (GotoStatement*) node;
            indent(ctx, out);
            IO::write(out, "goto ");
            emitName(out, &gs->name);
            IO::write(out, ";\n");
            break;
        }

        case NT_LABEL: {
            Label* lbl = (Label*) node;
            emitName(out, &lbl->name);
            IO::write(out, ":\n");
            break;
        }

        case NT_NAMESPACE: {
            Namespace* ns = (Namespace*) node;
            for (uint32_t i = 0; i < ns->scope.childrenCount; i++) {
                emitNode(ctx, ns->scope.children[i], out);
            }
            break;
        }

        case NT_STATEMENT: {
            Statement* stmt = (Statement*) node;
            indent(ctx, out);
            if (stmt->operand) emitVariable(ctx, out, stmt->operand);
            IO::write(out, ";\n");
            break;
        }

        case NT_CODE_BLOCK: {
            CodeBlock* cb = (CodeBlock*) node;
            if (cb->code.codeStr.buff) {
                IO::write(out, cb->code.codeStr.buff, (uint32_t) cb->code.codeStr.len);
                IO::write(out, '\n');
            }
            break;
        }

        case NT_ERROR: {
            ErrorSet* errSet = (ErrorSet*) node;
            indent(ctx, out);
            IO::write(out, "const int ");
            emitName(out, &errSet->name);
            IO::writef(out, " = %llu;\n", (unsigned long long) errSet->value);

            for (uint32_t i = 0; i < errSet->varCount; i++) {
                Variable* var = errSet->vars[i];
                if (!var || !var->value.hasValue) continue;
                indent(ctx, out);
                IO::write(out, "const int ");
                emitName(out, &var->name);
                IO::writef(out, " = %llu;\n", (unsigned long long) var->value.u64);
            }
            break;
        }

        case NT_VARIABLE: {
            indent(ctx, out);
            emitVariable(ctx, out, (Variable*) node);
            IO::write(out, ";\n");
            break;
        }

        case NT_ENUMERATOR: {
            Enumerator* en = (Enumerator*) node;
            indent(ctx, out);
            IO::write(out, "typedef enum ");
            emitName(out, &en->name);
            IO::write(out, " {\n");

            ctx->indentLevel++;
            for (uint32_t i = 0; i < en->varCount; i++) {
                Variable* var = en->vars[i];
                if (!var) continue;
                indent(ctx, out);
                emitName(out, &var->name);

                if (var->expression) {
                    IO::write(out, " = ");
                    emitExpression(ctx, var->expression, out);
                }

                if (i < en->varCount - 1) IO::write(out, ',');
                IO::write(out, '\n');
            }
            ctx->indentLevel--;

            indent(ctx, out);
            IO::write(out, "} ");
            emitName(out, &en->name);
            IO::write(out, ";\n");
            break;
        }

        default:
            break;
    }
}

static void emitUnit(Emitter::Context* ctx, Reg::Unit* unit, IO::Stream* out) {
    if (!unit) return;

    if (unit->ast) {
        if (unit->ast->usedFunctionMask & (1 << (Ast::Internal::IF_PRINTF - 1))) {
            IO::write(out, "#include <stdio.h>\n");
        }
        if (unit->ast->usedFunctionMask & (1 << (Ast::Internal::IF_ALLOC - 1))) {
            IO::write(out, "#include <stdlib.h>\n");
        }
        IO::write(out, "#include <stdint.h>\n");
        IO::write(out, "#include <stdbool.h>\n\n");
    }

    if (unit->reg) {
        for (uint32_t i = 0; i < unit->reg->codeBlocks.size; i++) {
            CodeBlock* block = ((CodeBlock*) unit->reg->codeBlocks.buffer) + i;
            if (block && block->code.codeStr.buff) {
                if (block->code.tagStr.buff && (block->code.tagStr.buff[0] == 'C' || block->code.tagStr.buff[0] == 'c')) {
                    IO::write(out, block->code.codeStr.buff, (uint32_t) block->code.codeStr.len);
                    IO::write(out, '\n');
                }
            }
        }

        for (uint32_t i = 0; i < unit->reg->foreignFunctions.size; i++) {
            ForeignFunction* fcn = ((ForeignFunction*) unit->reg->foreignFunctions.buffer) + i;
            if (fcn) {
                emitFunctionDefinition(ctx, out, &fcn->fcn, true);
                IO::write(out, ";\n");
                emitFunctionDefinition(ctx, out, &fcn->fcn, false);
                IO::write(out, " {\n");
                if (fcn->code.codeStr.buff) {
                    IO::write(out, fcn->code.codeStr.buff, (uint32_t) fcn->code.codeStr.len);
                }
                IO::write(out, "}\n\n");
            }
        }
    }

    if (unit->ast && unit->ast->root) {
        emitNode(ctx, (SyntaxNode*) unit->ast->root, out);
    }
}



void c_emitNode(Emitter::Context* ctx, SyntaxNode* node, IO::Stream* out) {
    emitNode(ctx, node, out);
}

void c_emitUnit(Emitter::Context* ctx, Reg::Unit* unit, IO::Stream* out) {
    emitUnit(ctx, unit, out);
}

Emitter::Driver Emitter::driverClang = {
    .name = "C-Emitter",
    .emitNode = c_emitNode,
    .emitUnit = c_emitUnit
};
