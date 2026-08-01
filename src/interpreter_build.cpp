// interpreter related code that focusing
// on building the bytecode

#include "array_list.h"
#include "data_types.h"
#include "dynamic_arena.h"
#include "globals.h"
#include "interpreter.h"
#include "operators.h"
#include "ordered_dict.h"
#include "supplement/runtime.h"
#include "syntax.h"
#include "logger.h"
#include "diagnostic.h"
#include "task_system.h"


#include <cstdint>
#include <cstdlib>
#include <float.h>



static Logger::Type logErr = { .level = Logger::ERROR, .tag = "VM" };

namespace Interpreter {

    void clear(CompilerState* state);

    void initBuild(CompilerState* state) {
        constexpr int size = 1024 * 8;

        Arena::init(&state->locals, size);
        Arena::init(&state->bytecode, size);
        Arena::init(&state->rawData, size);

        OrderedDict::init(&state->localsInfoMap, size);
        state->localsInfoMap.flags |= OrderedDict::KEY_IS_INDEX;
        // state->localsInfoMap.flags |= OrderedDict::COPY_STRINGS;

        DArray::init(&state->lines, size, sizeof(LineInfo));

        state->populateLocals = false;
        state->defaultArgsSize = 0;

        state->vecResult.isTmp = false;
    }

    void clear(CompilerState* state) {
        Arena::clear(&state->bytecode);
        Arena::clear(&state->locals);
        Arena::clear(&state->rawData);
        DArray::clear(&state->lines);
        OrderedDict::clear(&state->localsInfoMap);
    }



    bool isFixedArray(Array* arr) {
        // in hpes that length will always be pre-unwrapped
        return arr->length && arr->length->value.hasValue;
    }

    // LOOK_AT : seems unnecessary
    inline bool isLineInSpan(Span* span, uint64_t line) {
        return span->start.ln <= line && span->end.ln >= line;
    }

    inline void commitLineInfo(CompilerState* state) {

        LineInfo line;
        line.span = state->currentLineSpan;
        line.ocOffsetStart = state->currentOffsetStart;
        line.ocOffsetEnd = state->bytecode.logicalPos;

        DArray::push(&state->lines, &line);

    }

    // TODO : maybe not the best naming
    inline void updateSourceLocation(CompilerState* state, Span* span) {

        if (!span) return;

        if (state->currentLineSpan.start.ln == 0) {
            // the first time into this function
            state->currentLineSpan = *span;
            state->currentOffsetStart = 0;
            return;
        }

        if (!isLineInSpan(&state->currentLineSpan, span->start.ln)) {
            // new span (push old line)
            commitLineInfo(state);
            state->currentLineSpan = *span;
            state->currentOffsetStart = state->bytecode.logicalPos;
            return;
        }

        if (span->end.ln > state->currentLineSpan.end.ln) {
            // expand span
            state->currentLineSpan.end = span->end;
        }

    }

    inline int isOffsetValid(uint64_t offset) {
        return (offset + 1 != 0);
    }

    // unsigned and signed
    inline int isI32(Type::Kind dtype) {
        return dtype >= Type::DT_I8 && dtype < Type::DT_I64;
    }

    inline int isI64(Type::Kind dtype) {
        return dtype == Type::DT_I64 && dtype == Type::DT_U64;
    }

    inline int isF32(Type::Kind dtype) {
        return dtype == Type::DT_F32;
    }

    inline int isF64(Type::Kind dtype) {
        return dtype == Type::DT_F64;
    }

    int getDtypeOffset(Type::Kind dtype) {

        switch (dtype) {
            case Type::DT_I8:  return OFF_I8;
            case Type::DT_U8:  return OFF_U8;
            case Type::DT_I16: return OFF_I16;
            case Type::DT_U16: return OFF_U16;
            case Type::DT_I32: return OFF_I32;
            case Type::DT_U32: return OFF_U32;
            case Type::DT_I64: return OFF_I64;
            case Type::DT_U64: return OFF_U64;
            case Type::DT_ARRAY:
            case Type::DT_POINTER: return OFF_PTR;
            default: return OFF_GENERIC;
        }

    }

    int getDtypeOffsetNoCast(Type::Kind dtype) {

        switch (dtype) {
            case Type::DT_I8:  return OFF_I32 - 4;
            case Type::DT_U8:  return OFF_U32 - 4;
            case Type::DT_I16: return OFF_I32 - 4;
            case Type::DT_U16: return OFF_U32 - 4;
            case Type::DT_I32: return OFF_I32 - 4;
            case Type::DT_U32: return OFF_U32 - 4;
            case Type::DT_I64: return OFF_I64 - 4;
            case Type::DT_U64: return OFF_U64 - 4;
            case Type::DT_ARRAY:
            case Type::DT_POINTER: return OFF_PTR - 4;
            default: return OFF_GENERIC - 4;
        }

    }

    int getDtypeOffsetNoCastArithmetic(Type::Kind dtype) {

        switch (dtype) {
            case Type::DT_I8:  return OFF_I32 - 4;
            case Type::DT_U8:  return OFF_U32 - 4;
            case Type::DT_I16: return OFF_I32 - 4;
            case Type::DT_U16: return OFF_U32 - 4;
            case Type::DT_I32: return OFF_I32 - 4;
            case Type::DT_U32: return OFF_U32 - 4;
            case Type::DT_I64: return OFF_I64 - 4;
            case Type::DT_ARRAY:
            case Type::DT_POINTER:
            case Type::DT_U64: return OFF_U64 - 4;
            default: return OFF_GENERIC - 4;
        }

    }

    inline Type::TypeInfo* getDtype(void* data, Type::Kind dtype) {

        if (dtype <= Type::DT_F64) {
            return Type::basicTypes + dtype;
        }

        switch (dtype) {

            case Type::DT_CUSTOM: {
                TypeDefinition* def = (TypeDefinition*) data;
                return (Type::TypeInfo*) def->typeInfo;
            }

            case Type::DT_UNION: {
                break;
            }

            case Type::DT_ENUM: {
                return Type::basicTypes + ((Enumerator*) data)->dtype;
            }

            case Type::DT_ERROR: {
                break;
            }

            case Type::DT_FUNCTION: {
                break;
            }

            case Type::DT_POINTER: {
                return Type::basicTypes + dtype;
            }

            case Type::DT_ARRAY: {
                return (Type::TypeInfo*) ((Array*) data)->type;
            }

            case Type::DT_STRING: {

            }

            default: {
                // TODO
            }

        }

        return {};

    }

    // TODO : to global? or pointer to use Value?
    inline Value toValue(Pointer* ptr) {
        Value val;
        val.any = ptr->pointsTo;
        val.typeKind = ptr->pointsToKind;
        val.hasValue = 0;
        return val;
    }

    inline Type::TypeInfo* getDtype(Value* val) {
        return getDtype(val->any, val->typeKind);
    }

    inline uint64_t getDtypeSize(Value* val) {
        // TODO : for now this
        Type::TypeInfo* info = getDtype(val);
        return info->size;
    }

    inline uint64_t getDtypeAlign(Value* val) {
        return 0;
    }

    int getAlign(Type::TypeInfo* dtype) {
        return sizeof(vmword);
    }

    inline uint64_t addToConstPool(CompilerState* state, char* buff, uint64_t buffLen) {
        const uint64_t offset = state->rawData.logicalPos;

        char* ptr = (char*)Arena::push(&state->rawData, buffLen, 1);
        memcpy(ptr, buff, buffLen);

        return offset;
    }

    inline Err::Err pushLocal(CompilerState* state, Variable* var, uint64_t* offset) {
        Type::TypeInfo* info = getDtype(&var->value);
        const uint64_t vmwordsCount = BYTES_TO_WORDS(info->size);

        *offset = state->locals.logicalPos;

        if (state->populateLocals) {
            const uint64_t allocSize = vmwordsCount * sizeof(vmword);
            uint8_t* body = (uint8_t*) Arena::push(&state->locals, allocSize, sizeof(vmword));
            memset(body, 0, allocSize);
        } else {
            state->locals.logicalPos += vmwordsCount * sizeof(vmword);
        }

        // store debug info
        LocalVarInfo* header = (LocalVarInfo*) alloc(alc, sizeof(LocalVarInfo));
        header->var = var;
        header->size = info->size;
        header->align = info->align;

        // String key = String((char*) offset, sizeof(uint64_t));
        OrderedDict::set(&state->localsInfoMap, *offset, header);

        return Err::OK;
    }

    inline uint8_t* pushOpcode(CompilerState* state, Opcode opcode) {
        uint8_t* ptr = (uint8_t*) Arena::push(&state->bytecode, sizeof(Opcode), 1);
        memcpy(ptr, &opcode, sizeof(Opcode));
        state->lastOpcode = opcode;
        return ptr;
    }

    inline uint8_t* pushOperand(CompilerState* state, uint64_t val) {
        uint8_t* ptr = (uint8_t*) Arena::push(&state->bytecode, sizeof(uint64_t), 1);
        memcpy(ptr, &val, sizeof(uint64_t));
        return ptr;
    }

    void pushBoolCast(CompilerState* state, Type::Kind dtype) {

        if (isI32(dtype)) {
            pushOpcode(state, OC_BOOL_I32);
        } else if (isI64(dtype)) {
            pushOpcode(state, OC_BOOL_I64);
        } else if (isF32(dtype)) {
            pushOpcode(state, OC_BOOL_F32);
        } else if (isF64(dtype)) {
            pushOpcode(state, OC_BOOL_F64);
        }

    }

    void pushString(CompilerState* state, StringInitialization* init) {

        const uint64_t offset = addToConstPool(state, init->rawStr.buff, init->rawStr.len);

        pushOpcode(state, OC_PUSH_PTR);
        pushOperand(state, offset);

        pushOpcode(state, OC_PUSH_I64);
        pushOperand(state, init->rawStr.len);

    }

    void pushPushInstruction(CompilerState* state, Value* value) {

        Arena::Container* locals = &state->locals;
        Arena::Container* bytecode = &state->bytecode;

        switch (value->typeKind) {

            case Type::DT_I8: {
                pushOpcode(state, OC_PUSH_I32);

                int32_t val = (int32_t) value->i8;
                int32_t* ptr = (int32_t*) Arena::push(bytecode, 4, 1);
                *ptr = val;

                break;
            }

            case Type::DT_I16: {
                pushOpcode(state, OC_PUSH_I32);

                int32_t val = (int32_t) value->i16;
                int32_t* ptr = (int32_t*) Arena::push(bytecode, 4, 1);
                *ptr = val;

                break;
            }

            case Type::DT_I32: {
                pushOpcode(state, OC_PUSH_I32);

                int32_t* ptr = (int32_t*) Arena::push(bytecode, 4, 1);
                *ptr = value->i32;

                break;
            }

            case Type::DT_I64: {
                pushOpcode(state, OC_PUSH_I64);

                int64_t* ptr = (int64_t*) Arena::push(bytecode, 8, 1);
                memcpy(ptr, &value->i64, sizeof(int64_t));

                break;
            }

            case Type::DT_F32: {
                pushOpcode(state, OC_PUSH_F32);

                float* ptr = (float*) Arena::push(bytecode, 4, 1);
                *ptr = value->f32;

                break;
            }

            case Type::DT_F64: {
                pushOpcode(state, OC_PUSH_F64);

                double* ptr = (double*) Arena::push(bytecode, 8, 1);
                *ptr = value->f64;

                break;
            }

            case Type::DT_STRING: {
                // TODO: add StringInitialization to Value
                pushString(state, (StringInitialization*) value->any);
                break;
            }

            default: {
                // TODO
            }

        }

    }



    Opcode selectSetOpcode(Type::Kind dtype) {

        int dtypeOffset = getDtypeOffset(dtype);
        return (Opcode) (OC_SET_I8 + dtypeOffset);

    }

    Opcode selectGetOpcode(Type::Kind dtype) {

        int dtypeOffset = getDtypeOffset(dtype);
        return (Opcode) (OC_GET_I8 + dtypeOffset);

    }

    Opcode selectLoadOpcode(Type::Kind dtype) {

        int dtypeOffset = getDtypeOffset(dtype);
        return (Opcode) (OC_LOAD_I8 + dtypeOffset);

    }

    Opcode selectStoreOpcode(Type::Kind dtype) {

        int dtypeOffset = getDtypeOffset(dtype);
        return (Opcode) (OC_STORE_I8 + dtypeOffset);

    }

    Opcode selectCastOpcode(Type::Kind src, Type::Kind dest) {

        if (dest == Type::DT_I8 || dest == Type::DT_I16) dest = Type::DT_I32;
        if (dest == Type::DT_U8 || dest == Type::DT_U16) dest = Type::DT_U32;

        switch (src) {

            case Type::DT_I8:
            case Type::DT_I16:
            case Type::DT_I32: {

                if (dest == Type::DT_U32) return OC_CAST_I32_TO_U32; // OC_NOP
                if (dest == Type::DT_I64 || dest == Type::DT_U64) return OC_SEXT_32_TO_64;
                if (dest == Type::DT_F32) return OC_CAST_I32_TO_F32;
                if (dest == Type::DT_F64) return OC_CAST_I32_TO_F64;

                break;

            }

            case Type::DT_U8:
            case Type::DT_U16:
            case Type::DT_U32: {

                if (dest == Type::DT_I32) return OC_CAST_U32_TO_I32; // OC_NOP
                if (dest == Type::DT_I64 || dest == Type::DT_U64) return OC_ZEXT_32_TO_64;
                if (dest == Type::DT_F32) return OC_CAST_U32_TO_F32;
                if (dest == Type::DT_F64) return OC_CAST_U32_TO_F64;

                break;

            }

            case Type::DT_I64: {

                if (dest == Type::DT_I32 || dest == Type::DT_U32) return OC_TRUNC_64_TO_32;
                if (dest == Type::DT_U64) return OC_CAST_I64_TO_U64;
                if (dest == Type::DT_F32) return OC_CAST_I64_TO_F32;
                if (dest == Type::DT_F64) return OC_CAST_I64_TO_F64;

                break;

            }
            case Type::DT_U64: {

                if (dest == Type::DT_I32 || dest == Type::DT_U32) return OC_TRUNC_64_TO_32;
                if (dest == Type::DT_I64) return OC_CAST_U64_TO_I64;
                if (dest == Type::DT_F32) return OC_CAST_U64_TO_F32;
                if (dest == Type::DT_F64) return OC_CAST_U64_TO_F64;

                break;

            }

            case Type::DT_F32: {

                if (dest == Type::DT_I32) return OC_CAST_F32_TO_I32;
                if (dest == Type::DT_I64) return OC_CAST_F32_TO_I64;
                if (dest == Type::DT_U32) return OC_CAST_F32_TO_U32;
                if (dest == Type::DT_U64) return OC_CAST_F32_TO_U64;
                if (dest == Type::DT_F64) return OC_CAST_F32_TO_F64;

                break;

            }

            case Type::DT_F64: {

                if (dest == Type::DT_I32) return OC_CAST_F64_TO_I32;
                if (dest == Type::DT_I64) return OC_CAST_F64_TO_I64;
                if (dest == Type::DT_U32) return OC_CAST_F64_TO_U32;
                if (dest == Type::DT_U64) return OC_CAST_F64_TO_U64;
                if (dest == Type::DT_F32) return OC_CAST_F64_TO_F32;

                break;

            }

            default: {
            }

        }

        return OC_NOP;

    }

    Opcode selectOperatorOpcode(UnaryExpression* uex) {

        OperatorEnum op = uex->base.opType;
        switch (op) {

        case OP_ADDITION: {
            int offset = getDtypeOffsetNoCast(uex->operand->value.typeKind);
            return (Opcode)(OC_ADD_I32 + offset);
        }

        case OP_SUBTRACTION: {
            int offset = getDtypeOffsetNoCast(uex->operand->value.typeKind);
            return (Opcode)(OC_SUB_I32 + offset);
        }

        case OP_GET_ADDRESS: {
            return (Opcode)(OC_LEA);
        }

        case OP_GET_VALUE: {
            return selectLoadOpcode(uex->operand->value.typeKind);
        }

        case OP_NEGATION: {
            return OC_NOT_BOOL;
        }

        case OP_NONE: {
            return OC_NOP;
        }

        default: {
            // TODO
        }

        }

        return OC_NOP;

    }

    Opcode selectOperatorOpcode(BinaryExpression* bex) {

        OperatorEnum op = bex->base.opType;
        int dtypeOffset = getDtypeOffsetNoCastArithmetic(bex->left->value.typeKind);

        switch (op) {
        case OP_ADDITION: {
            return (Opcode)(OC_ADD_I32 + dtypeOffset);
        }

        case OP_SUBTRACTION: {
            return (Opcode)(OC_SUB_I32 + dtypeOffset);
        }

        case OP_MULTIPLICATION: {
            return (Opcode)(OC_MUL_I32 + dtypeOffset);
        }

        case OP_DIVISION: {
            return (Opcode)(OC_DIV_I32 + dtypeOffset);
        }

        case OP_BITWISE_AND: {
            return (Opcode)(OC_AND_I32 + dtypeOffset);
        }

        case OP_BITWISE_OR: {
            return (Opcode)(OC_OR_I32 + dtypeOffset);
        }

        case OP_BITWISE_XOR: {
            return (Opcode)(OC_XOR_I32 + dtypeOffset);
        }

        case OP_SHIFT_LEFT: {
            return (Opcode)(OC_SHL_I32 + dtypeOffset);
        }

        case OP_SHIFT_RIGHT: {
            return (Opcode)(OC_SHR_I32 + dtypeOffset);
        }

        case OP_SUBSCRIPT: {
            return OC_PTR_IDX;
        }

        default: {
            // TODO
        }
        }

        return OC_NOP;

    }



    // TODO : move in meaningful place
    enum {
        IS_LVALUE = 1,
        IS_ROOT = (1 << 1),
        FORCE_ARRAY_LENGTH = (1 << 2),
        FORCE_VEC_OPCODES = (1 << 3), // TODO : ? include FORCE_ARRAY_LENGTH ?
    };

    Err::Err compile(CompilerState* state, SyntaxNode* node);
    Err::Err compile(CompilerState* state, Expression* node, Variable* target = NULL, Flags flags = 0);
    Err::Err compile(CompilerState* state, Variable* node, Variable* target = NULL, Flags flags = 0);
    Err::Err compile(CompilerState* state, Function* node);
    Err::Err compile(CompilerState* state, VariableAssignment* node);

    // usually if function is defined with any it will
    // be used anyway, so I kinda have to create this
    // runtime type tree, and if i already have it,
    // I can just reuse it in vm, so i can actualy use
    // the same function
    // TODO : grammar
    Err::Err compileAsAny(CompilerState* state, Variable* var) {

        Err::Err err = Err::OK;

        Runtime::_TypeInfo* runtimeInfo = Runtime::getType(var);
        if (!runtimeInfo) {
            return Err::NOT_YET_IMPLEMENTED;
        }

        pushOpcode(state, OC_PUSH_PTR);
        pushOperand(state, (uint64_t) runtimeInfo);

        if (isStructLike(var->value.typeKind)) {

            // TODO : move to a function?
            Type::TypeInfo* dtype = getDtype(&var->value);

            uint64_t offset = state->locals.logicalPos;
            push(&state->locals, dtype->size, dtype->align);

            err = compile(state,var);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_SET_BLOB);
            pushOperand(state, dtype->size);
            pushOperand(state, offset);

            pushOpcode(state, OC_LEA);
            pushOperand(state, offset);

        } else if (var->value.typeKind == Type::DT_STRING) {

            StringInitialization* init = (StringInitialization*) var->expression;

            // we reserve space for slice in locals
            uint64_t offset = state->locals.logicalPos;
            uint64_t* sliceTemplate = (uint64_t*) Arena::push(&state->locals, 16, 8);

            sliceTemplate[0] = (uint64_t) init->rawStr.buff;
            sliceTemplate[1] = (uint64_t) init->rawStr.len;

            pushOpcode(state, OC_LEA);
            pushOperand(state, offset);

        } else if (var->value.typeKind == Type::DT_ARRAY) {

            err = compile(state, var, NULL, FORCE_ARRAY_LENGTH | FORCE_VEC_OPCODES);
            if (err != Err::OK) return err;

            if (var->value.typeKind == Type::DT_ARRAY) {
                pushOpcode(state, OC_VEC_TO_REF);
            }

        } else {

            err = compile(state, var);
            if (err != Err::OK) return err;

        }

        return err;
    }

    Err::Err compile(CompilerState* state, Scope* node) {

        for (int i = 0; i < node->childrenCount; i++) {
            compile(state, node->children[i]);
        }

        return Err::OK;

    }

    Err::Err compile(CompilerState* state, VariableDefinition* node) {

        updateSourceLocation(state, node->base.span);

        Err::Err err;
        uint64_t offset;

        err = pushLocal(state, node->var, &offset);
        if (err != Err::OK) return err;

        if (!isOffsetValid(offset)) {
            return Err::COMPILE_TIME_KNOWN_EXPRESSION_REQUIRED;
        }

        node->vmOffset = offset;

        // do we qualify for initialization?
        if (!node->var ||
            (!node->var->expression && !node->var->value.hasValue)
        ) {
            return Err::OK;
        }

        // ? will it alwyas work ?
        VariableAssignment ass;
        ass.base.span = node->base.span;
        ass.lvar = node->var;
        ass.rvar = node->var;

        err = compile(state, &ass);
        if (!err) return err;

        /*
        compile(state, node->var, node->var);
        if (node->var->cvalue.dtypeEnum == DT_ARRAY) {
            // arrays are assigned by value
            return Err::OK;
        }

        Opcode setOpcode = selectSetOpcode(node->var->cvalue.dtypeEnum);
        pushOpcode(state, setOpcode);
        if (setOpcode == OC_SET_BLOB) {
            DataType* dtype = getDtype(&node->var->cvalue); // TODO : to wasteful for size?
            pushOperand(state, dtype->size);
        }

        pushOperand(state, offset);
        */
        return Err::OK;

    }

    Err::Err compile(CompilerState* state, VariableAssignment* node) {

        updateSourceLocation(state, node->base.span);

        Err::Err err;

        // as we may be from VariableDefinition
        Variable* lvar = (node->lvar->def) ? node->lvar : unwrap(node->lvar);
        Variable* rvar = node->rvar;//unwrap(node->rvar);

        if (lvar->value.typeKind == Type::DT_ARRAY) {
            // we want to assign by value -> use of vec ops

            err = compile(state, rvar, lvar, FORCE_ARRAY_LENGTH | FORCE_VEC_OPCODES | IS_ROOT);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_VEC_RESET);

            return Err::OK;

        }

        if (lvar->def) {

            err = compile(state, node->rvar);
            if (err != Err::OK) return err;

            Opcode op = selectSetOpcode(lvar->value.typeKind);
            pushOpcode(state, op);
            pushOperand(state, lvar->def->vmOffset);

            return Err::OK;

        }

        // here we ecpect lvalue to be a 'random'
        // epression which should result into pointer
        // on stack if normaly compiled.
        err = compile(state, lvar, NULL, 1);
        if (err != Err::OK) return err;

        err = compile(state, node->rvar, lvar);
        if (err != Err::OK) return err;

        Opcode op = selectStoreOpcode(lvar->value.typeKind);
        pushOpcode(state, op);

        return Err::OK;

    }

    Err::Err compile(CompilerState* state, Variable* node, Variable* target, Flags flags) {

        // TODO : kinda wasteful, maybe we create either flag or
        //  force each line-like statement to be parsed as Statement
        updateSourceLocation(state, node->base.span);

        if (node->expression) {
            return compile(state, node->expression, target, flags);
        }

        if (node->def) {

            Value* val = &node->def->var->value;

            uint64_t offset = node->def->vmOffset;
            uint64_t dtypeSize = 0;

            const Type::Kind dtypeEnum = val->typeKind;
            if (dtypeEnum == Type::DT_ARRAY && isFixedArray(val->arr)) {
                Type::TypeInfoEx* info = val->arr->type;

                pushOpcode(state, OC_LEA);
                pushOperand(state, offset);

                if (flags & FORCE_ARRAY_LENGTH) {
                    compile(state, val->arr->length);
                }

                return Err::OK;
            }

            Opcode op = selectGetOpcode(dtypeEnum);
            pushOpcode(state, op);

            if (op == OC_GET_BLOB) {
                Type::TypeInfo* dtype = getDtype(val); // TODO
                pushOperand(state, dtype->size);
            }

            pushOperand(state, offset);

            return Err::OK;

        }

        pushPushInstruction(state, &node->value);

        return Err::OK;

    }

    Err::Err compile(CompilerState* state, TypeDefinition* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, TypeInitialization* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Union* scope) {
        return Err::OK;
    }

    Function* getInternalFunction(int idx) {
        return Ast::Internal::functions + idx;
    }

    Err::Err compileShortCircuit(CompilerState* state, BinaryExpression* bex) {
        //    &&                       ||
        // 0: <left>                0: <left>
        // 1: dup                   1: dup
        // 2: jump_if_false 5       2: jump_if_true 5
        // 3: pop                   3: pop
        // 4: <right>               4: <right>
        // 5: ...                   5: ...

        compile(state, bex->left);
        pushBoolCast(state, bex->left->value.typeKind);

        pushOpcode(state, OC_DUP);

        if (bex->base.opType == OP_BOOL_AND) {
            pushOpcode(state, OC_JUMP_IF_FALSE);
        } else {
            pushOpcode(state, OC_JUMP_IF_TRUE);
        }

        uint8_t* jumpOperandPtr = pushOperand(state, 0); // 0 as placeholder

        pushOpcode(state, OC_POP);

        compile(state, bex->right);
        pushBoolCast(state, bex->left->value.typeKind);

        uint64_t jumpTargetOffset = state->bytecode.logicalPos;
        memcpy(jumpOperandPtr, &jumpTargetOffset, sizeof(uint64_t));

        return Err::OK;

    }

    Err::Err compileMemberSelection(CompilerState* state, BinaryExpression* bex) {
        // One of the following three cases happens:
        // 1. Pointer Access: Calculates the absolute address on the stack.
        // 2. Local Access: Direct read from a pre-calculated frame offset.
        // 3. Arbitrary Expression: Extracts a member from a stack-allocated blob
        //    (e.g., from a function return) via a cropping 'crop' instruction

        Variable* parent = unwrap(bex->left);
        Variable* member = bex->right;

        const Type::Kind pType = parent->value.typeKind;
        Type::StructInfo* pInfo = (Type::StructInfo*) parent->value.def->typeInfo;

        Type::StructMemberInfo* mInfo = pInfo->members + member->name.id;

        if (pType == Type::DT_POINTER) {
            const Err::Err err = compile(state, parent);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_PUSH_U64);
            pushOperand(state, mInfo->offset);

            pushOpcode(state, OC_ADD_U64);

            pushOpcode(state, selectLoadOpcode(mInfo->type->kind));
            if (mInfo->type->kind == Type::DT_CUSTOM) {
                pushOperand(state, mInfo->type->size);
            }
        } else if (parent->def) {
            pushOpcode(state, selectGetOpcode(mInfo->type->kind));
            if (mInfo->type->kind == Type::DT_CUSTOM) {
                pushOperand(state, mInfo->type->size);
            }
            pushOperand(state, parent->def->vmOffset + mInfo->offset);
        } else {
            const Err::Err err = compile(state, parent);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_CROP);
            pushOperand(state, BYTES_TO_WORDS(getDtypeSize(&parent->value)));
            pushOperand(state, mInfo->offset);
            pushOperand(state, getDtypeSize(&member->value));
        }

        return Err::OK;
    }

    Err::Err compileArraySize(CompilerState* state, Variable* var) {

        var = unwrap(var);

        Array* arr = var->value.arr;
        arr->length = unwrap(arr->length);

        if (!arr->length) {
            Logger::log(logErr, "Array length expected!'.", var->base.span);
            return Err::UNEXPECTED_ERROR;
        }

        Err::Err err = compile(state, arr->length);
        if (err != Err::OK) return err;

        Type::TypeInfo* elemType = getDtype(&arr->length->value);
        if (elemType->size > 1) {
            pushOpcode(state, OC_PUSH_U64);
            pushOperand(state, elemType->size);
            pushOpcode(state, OC_MUL_U64);
        }

        return Err::OK;

    }

    Err::Err compileInPlace(CompilerState* state, ArrayInitialization* init, Variable* target) {
        Type::TypeInfoEx* info = target->value.arr->type;
        const uint64_t offset = target->def->vmOffset;

        for (int i = 0; i < init->attributeCount; i++) {

            pushOpcode(state, OC_LEA);
            pushOperand(state, offset);

            pushOpcode(state, OC_PUSH_U64);
            pushOperand(state, i);

            pushOpcode(state, OC_PTR_IDX);
            pushOperand(state, info->arr.element->size);

            Variable* var = init->attributes[i];
            Err::Err err = compile(state, var);
            if (err != Err::OK) return err;

            const Opcode storeOpcode = selectStoreOpcode(info->arr.element->kind);
            pushOpcode(state, storeOpcode);

        }

        return Err::OK;
    }

    // TODO : doesnt work for <val> +/- ptr, add OC_SWAP?
    inline bool tryAsPointerArithmetic(CompilerState* state, BinaryExpression* bex) {

        Variable* var = NULL;
        if (bex->left->value.typeKind == Type::DT_POINTER ||
            bex->left->value.typeKind == Type::DT_ARRAY) {
            var = bex->left;
        } else if (bex->right->value.typeKind == Type::DT_POINTER ||
            bex->right->value.typeKind == Type::DT_ARRAY) {
            var = bex->right;
        }

        if (!var) return false;

        const OperatorEnum op = bex->base.opType;
        if (op != OP_ADDITION && op != OP_SUBTRACTION) {
            return false;
        }

        Value val = toValue(var->value.ptr);
        Type::TypeInfo* info = getDtype(&val);

        pushOpcode(state, OC_PUSH_I64);
        pushOperand(state, info->size);

        pushOpcode(state, OC_MUL_U64);

        Opcode oc = selectOperatorOpcode(bex);
        pushOpcode(state, oc);

        return true;
    }

    void pushDescriptor(CompilerState* state, VecDescriptor desc) {
        pushOperand(state, encodeVecDescriptor(desc));
    }

    inline bool tryVectorization(CompilerState* state, BinaryExpression* bex, VecResult lRes, VecResult rRes, Variable* target, const bool isRoot) {

        if (bex->left->value.typeKind != Type::DT_ARRAY &&
            bex->right->value.typeKind != Type::DT_ARRAY
        ) {
            return false;
        }

        uint64_t dest = 0;
        if (isRoot) {
            dest = target->def->vmOffset;
            state->vecResult.isTmp = false;
        } else {
            state->vecResult.isTmp = true;
        }

        VecDescriptor desc = {
            .oper = bex->base.opType,
            .flags =
                (((uint32_t) state->vecResult.isTmp) << DE_F_DEST_SHIFT) |
                (((uint32_t) lRes.isTmp) << DE_F_LEFT_SHIFT) |
                (((uint32_t) rRes.isTmp) << DE_F_RIGHT_SHIFT)
        };

        if (bex->left->value.typeKind == Type::DT_ARRAY && bex->right->value.typeKind == Type::DT_ARRAY) {
            desc.dtype = bex->left->value.arr->base.pointsToKind;
            if (bex->base.opType == OP_CONCATENATION) {
                pushOpcode(state, OC_VEC_CAT);
            } else {
                pushOpcode(state, OC_VEC_VV);
            }
        } else if (bex->left->value.typeKind == Type::DT_ARRAY) {
            desc.dtype = bex->left->value.arr->base.pointsToKind;
            pushOpcode(state, OC_VEC_VS);
        } else {
            desc.dtype = bex->right->value.arr->base.pointsToKind;
            pushOpcode(state, OC_VEC_SV);
        }

        pushDescriptor(state, desc);
        pushOperand(state, dest);

        return true;

    }

    // Unary Version
    bool tryVectorization(CompilerState* state, UnaryExpression* uex, Variable* target, const bool isRoot) {

        if (uex->operand->value.typeKind != Type::DT_ARRAY) return false;

        uint64_t dest = 0;
        if (isRoot) {
            dest = target->def->vmOffset;
            state->vecResult.isTmp = false;
        } else {
            state->vecResult.isTmp = true;
        }

        pushOpcode(state, OC_VEC_UNARY);

        VecDescriptor desc = {
            .dtype = uex->operand->value.arr->base.pointsToKind,
            .oper = uex->base.opType,
            .flags = ((uint32_t) state->vecResult.isTmp) << DE_F_DEST_SHIFT
        };

        pushDescriptor(state, desc);
        pushOperand(state, dest);

        return true;

    }

    Err::Err compile(CompilerState* state, Expression* node, Variable* target, Flags flags) {

        Err::Err err;

        const bool isRoot = flags & IS_ROOT;
        flags &= ~IS_ROOT;

        switch (node->type) {

            case EXT_UNARY: {

                UnaryExpression* uex = (UnaryExpression*) node;
                const bool areWeNothingburger = uex->base.opType == OP_NONE;

                if (areWeNothingburger && isRoot) flags |= IS_ROOT;

                err = compile(state, uex->operand, target, flags);
                if (err != Err::OK) return err;

                if (areWeNothingburger) break;

                if (flags & FORCE_VEC_OPCODES &&
                    tryVectorization(state, uex, target, isRoot)
                ) {
                    break;
                }

                if (uex->base.opType == OP_GET_VALUE && (flags & IS_LVALUE)) {
                    break;
                }

                Opcode oc = selectOperatorOpcode(uex);
                if (oc == OC_NOP) break;

                if (oc == OC_NOT_BOOL) {
                    pushBoolCast(state, uex->operand->value.typeKind);
                }

                pushOpcode(state, oc);

                if (oc == OC_LEA) {
                    pushOperand(state, uex->operand->def->vmOffset);
                }

                break;

            }

            case EXT_BINARY: {

                BinaryExpression* bex = (BinaryExpression*) node;

                if (bex->base.opType == OP_BOOL_AND ||
                    bex->base.opType == OP_BOOL_OR) {
                    return compileShortCircuit(state, bex);
                }

                if (bex->base.opType == OP_MEMBER_SELECTION) {
                    return compileMemberSelection(state, bex);
                }

                err = compile(state, bex->left, target, flags);
                if (err != Err::OK) return err;
                VecResult lResult = state->vecResult;

                err = compile(state, bex->right, target, flags);
                if (err != Err::OK) return err;
                VecResult rResult = state->vecResult;

                // if either side is an array - switch to vec opcodes
                if (flags & FORCE_VEC_OPCODES &&
                    tryVectorization(state, bex, lResult, rResult, target, isRoot)
                ) {
                    break;
                }

                if (tryAsPointerArithmetic(state, bex)) {
                    break;
                }

                Opcode oc = selectOperatorOpcode(bex);
                if (oc == OC_NOP) break;

                pushOpcode(state, oc);

                if (oc == OC_PTR_IDX) {
                    Pointer* ptr = bex->left->value.ptr;
                    Type::TypeInfo* dtype = getDtype(ptr->pointsTo, ptr->pointsToKind);
                    pushOperand(state, dtype->size);

                    if (!(flags & IS_LVALUE)) {
                        oc = selectLoadOpcode(ptr->pointsToKind);
                        pushOpcode(state, oc);
                    }
                }

                break;

            }

            case EXT_FUNCTION_CALL: {

                FunctionCall* call = (FunctionCall*) node;
                Function* fcn = call->fcn;

                if (!isValidFunctionIdx(call->fcn->internalIdx) && !fcn->exe) {
                    TaskSystem::dispatchCompileTimeBuild(fcn, true);
                    // TODO : handle somehow error.
                }

                // create empty slots for callee fp and ip
                pushOpcode(state, OC_GROW);
                pushOperand(state, 2 * sizeof(vmword));

                // predetermine if the last arg is vardic, so we
                // dont have to lookup in main loop prototype definition
                int fixedCount = fcn->prototype.inArgCount;
                int varArgsCount = 0;
                bool isVariadic = false;

                if (fixedCount > 0) {
                    VariableDefinition* lastArgPrototype = fcn->prototype.inArgs[fixedCount - 1];
                    if (lastArgPrototype->var->value.typeKind == Type::DT_MULTIPLE_TYPES) {
                        isVariadic = true;
                        fixedCount--;
                    }
                }

                for (int i = 0; i < fixedCount; i++) {
                    Variable* arg = call->inArgs[i];
                    err = compile(state, arg, NULL, FORCE_ARRAY_LENGTH);
                    if (err != Err::OK) return err;
                }

                bool anyVarargIsArray = false;
                if (isVariadic) {
                    for (int i = fixedCount; i < call->inArgCount; i++) {
                        Variable* arg = call->inArgs[i];
                        compileAsAny(state, arg);
                        anyVarargIsArray = arg->value.typeKind == Type::DT_ARRAY;
                        varArgsCount++;
                    }

                    pushOpcode(state, OC_PUSH_I64);
                    pushOperand(state, varArgsCount);
                }

                pushOpcode(state, OC_CALL);

                // TODO: for now we push pointer, later it would be nice
                //       to provide transferable solution, or at least
                //       a way to generate such solution
                pushOperand(state, (uint64_t) fcn);
                // TODO: for now this, but maybe separate opcode
                // (for the dump log)
                pushOperand(state, (uint64_t) (2 * varArgsCount));

                if (anyVarargIsArray) {
                    pushOpcode(state, OC_VEC_MEM_RESET);
                }

                break;

            }

            case EXT_CAST: {

                Cast* cast = (Cast*) node;

                if (flags & FORCE_VEC_OPCODES) {

                    compile(state, cast->operand, target, flags);

                    pushOpcode(state, OC_VEC_CAST);

                    VecDescriptor desc = {
                        .dtype = cast->target,
                        .srcDtype = cast->operand->value.arr->base.pointsToKind,
                        .flags = 0
                    };

                    // even in root target can be general expression,
                    // ex. assignment x.y[i] = arr ...;
                    if (isRoot && target->def) {
                        pushDescriptor(state, desc);
                        pushOperand(state, target->def->vmOffset);
                        state->vecResult.isTmp = false;
                    } else {
                        desc.flags = DE_F_DEST | DE_F_LEFT;
                        pushDescriptor(state, desc);
                        pushOperand(state, 0); // TODO
                        state->vecResult.isTmp = true;
                    }

                } else {

                    compile(state, cast->operand, target);

                    Opcode op = selectCastOpcode(cast->operand->value.typeKind, cast->target);
                    if (op == OC_NOP) break;

                    pushOpcode(state, op);

                }

                break;

            }

            case EXT_ALLOC: {

                Alloc* alc = (Alloc*) node;

                Type::TypeInfo* dtype = getDtype(&alc->def->var->value);

                if (dtype->kind == Type::DT_ARRAY) {
                    err = compileArraySize(state, alc->def->var);
                    if (err != Err::OK) return err;
                } else {
                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, dtype->size);
                }

                Function* fcn = Ast::Internal::functions + Ast::Internal::IF_ALLOC;
                pushOpcode(state, OC_CALL);
                pushOperand(state, (uint64_t) fcn);

                if (!alc->def->var->expression) break;

                // init part
                pushOpcode(state, OC_DUP); // as we expect pointer on stack
                compile(state, alc->def->var->expression);

                Opcode oc = selectStoreOpcode(dtype->kind);
                pushOpcode(state, oc);
                if (oc == OC_STORE_BLOB) {
                    pushOperand(state, dtype->size);
                }

                break;

            }

            case EXT_FREE: {
                // TODO
                break;
            }

            case EXT_GET_LENGTH: {

                GetLength* ex = (GetLength*) node;
                if (ex->arr->value.arr->length) {
                    compile(state, ex->arr->value.arr->length);
                } else {
                    pushOpcode(state, OC_GET_U64);
                    pushOperand(state, ex->arr->def->vmOffset + 8);
                }

                break;

            }

            case EXT_GET_SIZE: {

                GetSize* ex = (GetSize*) node;
                compile(state, ex->arr->value.arr->length);
                /*
                DataType* dtype = getDtype(ex->arr->base.pointsTo, ex->arr->base.pointsToEnum);
                pushOperand(state, dtype->size);
                pushOpcode(state, OC_MUL_I64);
                */
                break;

            }

            case EXT_STRING_INITIALIZATION: {

                pushString(state, (StringInitialization*) node);
                break;

            }

            case EXT_ARRAY_INITIALIZATION: {

                ArrayInitialization* init = (ArrayInitialization*) node;

                const uint64_t elementCount = init->attributeCount;
                if (elementCount < 0) {
                    // TODO : can we even be there if not error in compiler
                    return Err::UNEXPECTED_ERROR;
                }

                if (init->flags & IS_CMP_TIME) {

                    Variable* first = init->attributes[0];

                    uint64_t offset = state->rawData.logicalPos;
                    uint64_t elementSize = getDtypeSize(&first->value);

                    uint8_t* rawDataPtr = (uint8_t*)Arena::push(&state->rawData, elementSize * elementSize, 1);
                    for (int i = 0; i < elementCount; i++) {
                        Variable* arg = init->attributes[i];
                        memcpy(rawDataPtr + (i * elementSize), &arg->value.u64, elementSize);
                    }

                    pushOpcode(state, OC_LEA_CONST);
                    pushOperand(state, offset);
                    state->vecResult.isTmp = false;

                    if (flags & FORCE_ARRAY_LENGTH) {
                        pushOpcode(state, OC_PUSH_U64);
                        pushOperand(state, elementCount);
                    }

                    pushOpcode(state, OC_VEC_COPY);
                    VecDescriptor desc = {
                        .dtype = first->value.typeKind,
                    };
                    if (isRoot) {
                        pushDescriptor(state, desc);
                        pushOperand(state, target->def->vmOffset);
                    } else {
                        desc.flags = DE_F_DEST;
                        pushDescriptor(state, desc);
                        pushOperand(state, 0); // TODO
                    }

                } else {

                    uint64_t elementSize = 0;

                    for (int i = 0; i < elementCount; i++) {
                        Variable* arg = init->attributes[i];
                        compile(state, arg);

                        if (i == 0) {
                            elementSize = getDtypeSize(&arg->value);
                        }

                        if (isRoot) {
                            pushOpcode(state, OC_STORE_INDEXED);
                            pushOperand(state, target->def->vmOffset);
                            pushOperand(state, (uint64_t) i);
                        } else {
                            pushOpcode(state, OC_STORE_INDEXED_TMP);
                            pushOperand(state, 0); // TODO
                            pushOperand(state, (uint64_t) i);
                        }
                    }

                    if (!isRoot) {
                        pushOpcode(state, OC_PUSH_U64);
                        pushOperand(state, elementCount);

                        pushOpcode(state, OC_VEC_ALLOC);
                        pushOperand(state, elementSize);

                        state->vecResult.isTmp = true;
                    }

                }

                break;

            }

            case EXT_TYPE_INITIALIZATION: {

                TypeInitialization* init = (TypeInitialization*) node;

                for (int i = 0; i < init->attributeCount; i++) {
                    compile(state, init->attributes[i]);
                }

                // TODO : fill var

                break;

            }

            default: {
                return Err::NOT_YET_IMPLEMENTED;
            }

        }

        return Err::OK;

    }

    Err::Err compile(CompilerState* state, ErrorSet* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Enumerator* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Branch* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, SwitchCase* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, WhileLoop* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Loop* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, ReturnStatement* node) {

        updateSourceLocation(state, node->base.span);

        if (node->var) {
            Err::Err err = compile(state, node->var);
            if (err != Err::OK) return err;
        }

        pushOpcode(state, OC_RET);

        // TODO : not the best
        Type::TypeInfo* dtype = getDtype(&node->var->value); // TODO
        pushOperand(state, dtype->size);

        return Err::OK;

    }

    Err::Err compile(CompilerState* state, ContinueStatement* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, BreakStatement* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, GotoStatement* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Label* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Namespace* scope) {
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Statement* node) {
        updateSourceLocation(state, node->base.span);
        compile(state, node->operand);
        return Err::OK;
    }

    ArgMappingType getArgMappingType(VariableDefinition* def) {
        switch (def->var->value.typeKind) {
            case Type::DT_ARRAY:  return AM_REFERENCE;
            case Type::DT_CUSTOM: return AM_REFERENCE;
            case Type::DT_STRING: return AM_REFERENCE;
            case Type::DT_SLICE:  return AM_REFERENCE;
            case Type::DT_UNION:  return AM_REFERENCE;
            default:              return AM_VALUE;
        }
    }

    void commitCompileState(CompilerState* state, ExeBlock* exe, SyntaxNode* target) {
        exe->bytecodeSize = state->bytecode.logicalPos;
        exe->bytecode = (uint8_t*) alloc(alc, Arena::getFlatSize(&state->bytecode), 1);
        Arena::flatCopy(&state->bytecode, exe->bytecode);

        exe->localsSize = state->locals.logicalPos;
        exe->locals = (vmword*) alloc(alc, Arena::getFlatSize(&state->locals), state->maxAlign);
        memset(exe->locals, 0, exe->localsSize * sizeof(vmword));
        Arena::flatCopy(&state->locals, (uint8_t*) exe->locals);

        exe->rawDataSize = state->rawData.logicalPos;
        exe->rawData = (uint8_t*) alloc(alc, Arena::getFlatSize(&state->rawData), 1);
        Arena::flatCopy(&state->rawData, exe->rawData);

        exe->node = target;
        exe->localsInfoMap = OrderedDict::tightCopy(&state->localsInfoMap);

        exe->linesSize = state->lines.size;
        exe->lines = (LineInfo*) alloc(alc, exe->linesSize * sizeof(LineInfo));
        memcpy(exe->lines, state->lines.buffer, exe->linesSize * sizeof(LineInfo));

        exe->fixedSize = state->fixedSize;
        exe->defaultArgsSize = state->defaultArgsSize;
    }

    Err::Err compile(CompilerState* state, Function* fcn) {
        Err::Err err;

        if (!fcn->exe) {
            fcn->exe = (ExeBlock*) alloc(alc, sizeof(ExeBlock));
        } else {
            return Err::OK;
        }

        fcn->exe->argMappingsCount = fcn->prototype.inArgCount;
        fcn->exe->argMappings = (ArgMapping*) alloc(alc, sizeof(ArgMapping) * fcn->exe->argMappingsCount);

        fcn->exe->isVariadic = false;
        for (int i = 0; i < fcn->prototype.inArgCount; i++) {
            VariableDefinition* def = fcn->prototype.inArgs[i];

            if (!state->populateLocals &&
                (def->var->value.hasValue || def->var->expression)
            ) {
                state->populateLocals = true;
                state->fixedSize = state->locals.logicalPos;
                state->locals.logicalPos = 0;
            }

            if (def->var->value.typeKind == Type::DT_MULTIPLE_TYPES) {
                fcn->exe->isVariadic = true;
            }

            err = compile(state, def);
            if (err != Err::OK) return err;

            fcn->exe->argMappings[i] = {
                .offset = (uint32_t) def->vmOffset,
                .type = getArgMappingType(def)
            };
        }

        if (!state->populateLocals) {
            state->fixedSize = state->locals.logicalPos;
            state->locals.logicalPos = 0;
            state->populateLocals = true;
        }

        state->defaultArgsSize = state->locals.logicalPos;

        // Assumption that if no scope, we are external function
        if (fcn->bodyScope) {
            err = compile(state, fcn->bodyScope);
            if (err != Err::OK) return err;

            state->fixedSize += state->locals.logicalPos;

            if (state->lastOpcode != OC_RET) {
                pushOpcode(state, OC_RET);
                pushOperand(state, 0);
            }
        }

        commitLineInfo(state);
        commitCompileState(state, fcn->exe, (SyntaxNode*) fcn);

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, SyntaxNode* node) {
        switch (node->type) {
            case NT_SCOPE :
                return compile(state, (Scope*) node);
            case NT_VARIABLE_DEFINITION :
                return compile(state, (VariableDefinition*) node);
            case NT_VARIABLE_ASSIGNMENT :
                return compile(state, (VariableAssignment*) node);
            case NT_TYPE_DEFINITION :
                return compile(state, (TypeDefinition*) node);
            case NT_TYPE_INITIALIZATION :
                return compile(state, (TypeInitialization*) node);
            case NT_UNION :
                return compile(state, (Union*) node);
            case NT_ERROR :
                return compile(state, (ErrorSet*) node);
            case NT_ENUMERATOR :
                return compile(state, (Enumerator*) node);
            case NT_VARIABLE :
                return compile(state, (Variable*) node);
            case NT_FUNCTION :
                return compile(state, (Function*) node);
            case NT_BRANCH :
                return compile(state, (Branch*) node);
            case NT_SWITCH_CASE :
                return compile(state, (SwitchCase*) node);
            case NT_WHILE_LOOP :
                return compile(state, (WhileLoop*) node);
            case NT_LOOP :
                return compile(state, (Loop*) node);
            case NT_RETURN_STATEMENT :
                return compile(state, (ReturnStatement*) node);
            case NT_CONTINUE_STATEMENT :
                return compile(state, (ContinueStatement*) node);
            case NT_BREAK_STATEMENT :
                return compile(state, (BreakStatement*) node);
            case NT_GOTO_STATEMENT :
                return compile(state, (GotoStatement*) node);
            case NT_LABEL :
                return compile(state, (Label*) node);
            case NT_NAMESPACE :
                return compile(state, (Namespace*) node);
            case NT_STATEMENT :
                return compile(state, (Statement*) node);

            default:
                // TODO
                return Err::CANNOT_EVALUATE;
        }
    }

    Err::Err compile(CompilerState* state, Reg::Unit* unit) {
        if (!unit->exe) {
            unit->exe = (ExeBlock*) alloc(alc, sizeof(ExeBlock));
        }

        unit->exe->argMappingsCount = 0;
        unit->exe->argMappings = NULL;
        unit->exe->isVariadic = false;

        state->defaultArgsSize = state->locals.logicalPos;

        // Assumption that if no scope, we are external function
        if (unit->ast->root) {
            // We want to process all function beforehand, so we
            // dont have to handle saving/restoring state at each
            // nested 'exe block' compilation...
            // TODO : We may want to create a task group here later...
            for (int i = 0; i < unit->reg->fcns.size; i++) {
                Function* inner = *(Function**) DArray::get(&unit->reg->fcns, i);
                TaskSystem::dispatchCompileTimeBuild(inner, true);
            }

            Err::Err err = compile(state, unit->ast->root);
            if (err != Err::OK) return err;

            state->fixedSize += state->locals.logicalPos;

            pushOpcode(state, OC_RET);
            pushOperand(state, 0);
        }

        commitLineInfo(state);
        commitCompileState(state, unit->exe, NULL);

        return Err::OK;
    }

}
