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

    void initBuild(CompilerState* state) {
        constexpr int size = 1024 * 8;

        Arena::init(&state->locals, size);
        Arena::init(&state->bytecode, size);
        Arena::init(&state->rawData, size);

        OrderedDict::init(&state->localsInfoMap, size);
        state->localsInfoMap.flags |= OrderedDict::KEY_IS_INDEX;
        // state->localsInfoMap.flags |= OrderedDict::COPY_STRINGS;

        // DArray::init(&state->tmpStack, size, sizeof(void*));
        DArray::init(&state->lines, size, sizeof(LineInfo));

        state->populateLocals = false;
        state->defaultArgsSize = 0;
        state->fixedSize = 0;
        state->maxAlign = 0;
        state->currentOffsetStart = 0;
        state->currentLoopAddress = 0;
        state->lastOpcode = OC_NOP;
        state->maxArrayLiteralSize = 0;
        state->currentArrayLiteralOffset = 0;

        state->vecResult.isTmp = false;
    }



    void patchList(CompilerState* state, uint64_t listHead, uint64_t offset) {
        while (listHead != patchListHeadNull) {
            uint8_t* ptr = Arena::getPointerToLogicalOffset(&state->bytecode, listHead);

            const uint64_t nextListHead = *(uint64_t*) ptr;
            const uint64_t relativeOffset = offset - (listHead - 1);

            *(uint64_t*) ptr = relativeOffset;

            listHead = nextListHead;
        }
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
            case Type::DT_F32: return OFF_F32;
            case Type::DT_F64: return OFF_F64;
            case Type::DT_ARRAY:
            case Type::DT_SLICE:
            case Type::DT_POINTER: return OFF_PTR;
            default: return OFF_GENERIC;
        }
    }

    // TODO : think about -4 to make it stand enum changes
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
            case Type::DT_F32: return OFF_F32 - 4;
            case Type::DT_F64: return OFF_F64 - 4;
            case Type::DT_ARRAY:
            case Type::DT_SLICE:
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
            case Type::DT_F32: return OFF_F32 - 4;
            case Type::DT_F64: return OFF_F64 - 4;
            case Type::DT_ARRAY:
            case Type::DT_SLICE:
            case Type::DT_POINTER:
            case Type::DT_U64: return OFF_U64 - 4;
            default: return OFF_GENERIC - 4;
        }
    }

    // TODO : name
    int getDtypeOffsetSizeBased(Type::Kind dtype) {
        switch (dtype) {
            case Type::DT_I32: return 0;
            case Type::DT_U32: return 0;
            case Type::DT_I64: return 1;
            case Type::DT_U64: return 1;
            case Type::DT_F32: return 2;
            case Type::DT_F64: return 3;
            case Type::DT_ARRAY:
            case Type::DT_SLICE:
            case Type::DT_POINTER: return 1;
            default: return 1;
        }
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
        Type::TypeInfo* type = var->value.type;
        const uint64_t vmwordsCount = BYTES_TO_WORDS(type->size);

        *offset = state->locals.logicalPos + state->fixedSize;

        if (state->populateLocals) {
            const uint64_t allocSize = vmwordsCount * sizeof(vmword);
            uint8_t* body = (uint8_t*) Arena::push(&state->locals, allocSize, sizeof(vmword));
            memset(body, 0, allocSize);
        } else {
            state->locals.logicalPos += vmwordsCount * sizeof(vmword);
        }

        // store debug info
        LocalVarInfo* header = alloc<LocalVarInfo>();
        header->var = var;
        header->size = type->size;
        header->align = type->align;

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
        const uint64_t offset = addToConstPool(state, init->rawData.buff, init->rawData.len);

        pushOpcode(state, OC_LEA_CONST);
        pushOperand(state, offset);

        pushOpcode(state, OC_PUSH_I64);
        pushOperand(state, init->rawData.len);
    }

    void pushPushInstruction(CompilerState* state, Value* value) {
        Arena::Container* locals = &state->locals;
        Arena::Container* bytecode = &state->bytecode;

        switch (value->type->kind) {
            case Type::DT_U8: {
                pushOpcode(state, OC_PUSH_U32);

                int32_t val = (int32_t) value->i8;
                int32_t* ptr = (int32_t*) Arena::push(bytecode, 4, 1);
                *ptr = val;

                break;
            }

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

            case Type::DT_U16: {
                pushOpcode(state, OC_PUSH_U32);

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

            case Type::DT_U32: {
                pushOpcode(state, OC_PUSH_U32);

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

            case Type::DT_U64: {
                pushOpcode(state, OC_PUSH_U64);

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

            //case Type::DT_STRING: {
            //    pushString(state, value->sex);
            //    break;
            //}

            default: {
                // TODO
            }
        }
    }



    Opcode selectSetOpcode(Type::Kind dtype) {
        int dtypeOffset = getDtypeOffset(dtype);
        return (Opcode) (OC_SET_I8 + dtypeOffset);
    }

    Opcode selectSetGlobalOpcode(Type::Kind dtype) {
        int dtypeOffset = getDtypeOffset(dtype);
        return (Opcode) (OC_SET_GLOBAL_I8 + dtypeOffset);
    }

    Opcode selectGetOpcode(Type::Kind dtype) {
        int dtypeOffset = getDtypeOffset(dtype);
        return (Opcode) (OC_GET_I8 + dtypeOffset);
    }

    Opcode selectGetGlobalOpcode(Type::Kind dtype) {
        int dtypeOffset = getDtypeOffset(dtype);
        return (Opcode) (OC_GET_GLOBAL_I8 + dtypeOffset);
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
            case OP_UNARY_PLUS: {
                return OC_NOP;
            }

            case OP_UNARY_MINUS: {
                int offset = getDtypeOffsetNoCast(uex->operand->value.type->kind);
                return (Opcode) (OC_NEG_I32 + offset);
            }

            case OP_GET_ADDRESS: {
                return (Opcode) (OC_LEA);
            }

            case OP_GET_VALUE: {
                return selectLoadOpcode(uex->operand->value.type->kind);
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
        const Type::Kind typeKind = bex->left->value.type->kind;

        OperatorEnum op = bex->base.opType;
        int dtypeOffset = getDtypeOffsetNoCastArithmetic(typeKind);

        switch (op) {
            case OP_ADDITION: {
                return (Opcode) (OC_ADD_I32 + dtypeOffset);
            }

            case OP_SUBTRACTION: {
                return (Opcode) (OC_SUB_I32 + dtypeOffset);
            }

            case OP_MULTIPLICATION: {
                return (Opcode) (OC_MUL_I32 + dtypeOffset);
            }

            case OP_DIVISION: {
                return (Opcode) (OC_DIV_I32 + dtypeOffset);
            }

            case OP_BITWISE_AND: {
                return (Opcode) (OC_AND_I32 + dtypeOffset);
            }

            case OP_BITWISE_OR: {
                return (Opcode) (OC_OR_I32 + dtypeOffset);
            }

            case OP_BITWISE_XOR: {
                return (Opcode) (OC_XOR_I32 + dtypeOffset);
            }

            case OP_SHIFT_LEFT: {
                return (Opcode) (OC_SHL_I32 + dtypeOffset);
            }

            case OP_SHIFT_RIGHT: {
                return (Opcode) (OC_SHR_I32 + dtypeOffset);
            }

            case OP_MODULO: {
                return (Opcode) (OC_MOD_I32 + dtypeOffset);
            }

            case OP_SUBSCRIPT: {
                return OC_PTR_IDX;
            }

            case OP_CALL: {
                // TODO
            }

            case OP_LESS_THAN: {
                return (Opcode) (OC_LT_I32 + dtypeOffset);
                break;
            }

            case OP_GREATER_THAN: {
                return (Opcode) (OC_GT_I32 + dtypeOffset);
            }

            case OP_LESS_THAN_OR_EQUAL: {
                return (Opcode) (OC_LE_I32 + dtypeOffset);
            }

            case OP_GREATER_THAN_OR_EQUAL: {
                return (Opcode) (OC_GE_I32 + dtypeOffset);
            }

            case OP_EQUAL: {
                return (Opcode) (OC_EQ_I32 + getDtypeOffsetSizeBased(typeKind));
            }

            case OP_NOT_EQUAL: {
                return (Opcode) (OC_NE_I32 + getDtypeOffsetSizeBased(typeKind));
            }

            case OP_BOOL_AND: {
                return (Opcode) (OC_AND_I32 + dtypeOffset);
            }

            case OP_BOOL_OR: {
                return (Opcode) (OC_OR_I32 + dtypeOffset);
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

    Err::Err compileAsAny(CompilerState* state, Variable* var) {
        Err::Err err = Err::OK;

        Runtime::_TypeInfo* runtimeInfo = Runtime::toRuntimeType(var->value.type);
        if (!runtimeInfo) {
            return Err::NOT_YET_IMPLEMENTED;
        }

        pushOpcode(state, OC_PUSH_PTR);
        pushOperand(state, (uint64_t) runtimeInfo);

        if (isStructLike(var->value.type->kind)) {
            // TODO : move to a function?
            Type::TypeInfo* dtype = var->value.type;

            uint64_t offset = state->locals.logicalPos;
            push(&state->locals, dtype->size, dtype->align);

            err = compile(state,var);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_SET_BLOB);
            pushOperand(state, dtype->size);
            pushOperand(state, offset);

            pushOpcode(state, OC_LEA);
            pushOperand(state, offset);
        } else if (var->value.type->kind == Type::DT_ARRAY) {
            // TODO : adjust to any
            err = compile(state, var, NULL, FORCE_ARRAY_LENGTH | FORCE_VEC_OPCODES);
            if (err != Err::OK) return err;

            if (var->value.type->kind == Type::DT_ARRAY) {
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
        node->vmOwnerExe = state->exe;

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
        if (err != Err::OK) return err;

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

    // offset is relative offset to target specific part of variable/blob
    void pushSetOpcode(CompilerState* state, Type::TypeInfo* typeInfo, VariableDefinition* def, uint64_t offset) {
        Opcode op;

        if (def->vmOwnerExe == state->exe) {
            op = selectSetOpcode(typeInfo->kind);
            pushOpcode(state, op);
            offset = def->vmOffset + offset;
        } else {
            op = selectSetGlobalOpcode(typeInfo->kind);
            pushOpcode(state, op);
            pushOperand(state, (uint64_t) def);
        }

        if (op == OC_SET_BLOB || op == OC_SET_GLOBAL_BLOB) {
            pushOperand(state, typeInfo->size);
        }

        pushOperand(state, offset);
    }

    // offset is relative offset to target specific part of variable/blob
    // TODO : better flow
    void pushGetOpcode(CompilerState* state, Type::TypeInfo* typeInfo, VariableDefinition* def, uint64_t offset) {
        Opcode op;

        if (def->vmOwnerExe == state->exe) {
            op = selectGetOpcode(typeInfo->kind);
            pushOpcode(state, op);
            offset = def->vmOffset + offset;
        } else {
            op = selectGetGlobalOpcode(typeInfo->kind);
            pushOpcode(state, op);
            pushOperand(state, (uint64_t) def);
        }

        if (op == OC_GET_BLOB || op == OC_GET_GLOBAL_BLOB) {
            // the size of the chunk to withdraw
            pushOperand(state, typeInfo->size);
        }

        pushOperand(state, offset);
    }

    Err::Err compile(CompilerState* state, VariableAssignment* node) {
        updateSourceLocation(state, node->base.span);

        Err::Err err;

        // as we may be from VariableDefinition
        Variable* lvar = (node->lvar->def) ? node->lvar : unwrap(node->lvar);
        Variable* rvar = node->rvar;//unwrap(node->rvar);

        if (lvar->value.type->kind == Type::DT_ARRAY) {
            // we want to assign by value -> use of vec ops
            if (!lvar->def) {
                err = compile(state, lvar, NULL, IS_LVALUE | FORCE_ARRAY_LENGTH);
                if (err != Err::OK) return err;
            }

            err = compile(state, rvar, lvar, FORCE_ARRAY_LENGTH | FORCE_VEC_OPCODES | IS_ROOT);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_VEC_RESET);

            return Err::OK;
        }

        if (lvar->def) {
            err = compile(state, node->rvar);
            if (err != Err::OK) return err;

            pushSetOpcode(state, lvar->value.type, lvar->def, 0);
            return Err::OK;
        }

        // here we ecpect lvalue to be a 'random'
        // epression which should result into pointer
        // on stack if normaly compiled.
        err = compile(state, lvar, NULL, IS_LVALUE);
        if (err != Err::OK) return err;

        err = compile(state, node->rvar, lvar);
        if (err != Err::OK) return err;

        Opcode op = selectStoreOpcode(lvar->value.type->kind);
        pushOpcode(state, op);

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Variable* node, Variable* target, Flags flags) {
        // TODO : kinda wasteful, maybe we create either flag or
        //  force each line-like statement to be parsed as Statement
        updateSourceLocation(state, node->base.span);

        if (Type::isPrimitive(node->value.type->kind) && node->value.hasValue) {
            pushPushInstruction(state, &node->value);
            return Err::OK;
        }

        if (node->expression) {
            return compile(state, node->expression, target, flags);
        }

        // TODO: messy? additional checks if embeded primitive
        if (node->def) {
            Value* val = &node->def->var->value;

            uint64_t offset = node->def->vmOffset;
            uint64_t dtypeSize = 0;

            const Type::Kind dtypeEnum = val->type->kind;
            if (dtypeEnum == Type::DT_ARRAY) {
                Type::TypeInfoEx* info = (Type::TypeInfoEx*) val->type;

                // TODO: to a function
                if (state->exe == node->def->vmOwnerExe) {
                    pushOpcode(state, OC_LEA);
                    pushOperand(state, offset);
                } else {
                    pushOpcode(state, OC_LEA_GLOBAL);
                    pushOperand(state, (uint64_t) node->def);
                    pushOperand(state, 0);
                }

                if (flags & FORCE_ARRAY_LENGTH) {
                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, info->arr.elementCount);
                }

                return Err::OK;
            }

            pushGetOpcode(state, val->type, node->def, 0);

            return Err::OK;
        }

        pushPushInstruction(state, &node->value);

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, TypeDefinition* scope) {
        // TODO
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, TypeInitialization* scope) {
        // TODO
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Union* scope) {
        // TODO
        return Err::OK;
    }

    Function* getInternalFunction(int idx) {
        return Ast::Internal::functions + idx;
    }

    Err::Err compileShortCircuit(CompilerState* state, BinaryExpression* bex) {
        //    &&                       ||
        // 0: <left>                0: <left>
        // 1: dup                   1: dup
        // 2: jump_if_false 3       2: jump_if_true 3
        // 3: pop                   3: pop
        // 4: <right>               4: <right>
        // 5: ...                   5: ...

        compile(state, bex->left);
        pushBoolCast(state, bex->left->value.type->kind);

        pushOpcode(state, OC_DUP);

        const uint64_t jumpStartOffset = state->bytecode.logicalPos;
        if (bex->base.opType == OP_BOOL_AND) {
            pushOpcode(state, OC_JUMP_IF_FALSE);
        } else {
            pushOpcode(state, OC_JUMP_IF_TRUE);
        }

        uint8_t* jumpOperandPtr = pushOperand(state, 0); // 0 as placeholder

        pushOpcode(state, OC_POP);

        compile(state, bex->right);
        pushBoolCast(state, bex->left->value.type->kind);

        const uint64_t jumpTargetOffset = state->bytecode.logicalPos;
        const uint64_t jumpRelativeOffset = jumpTargetOffset - jumpStartOffset;
        memcpy(jumpOperandPtr, &jumpRelativeOffset, sizeof(uint64_t));

        return Err::OK;

    }

    Err::Err compileMemberSelection(CompilerState* state, BinaryExpression* bex, uint64_t flags) {
        Variable* parent = unwrap(bex->left);

        Variable* member = bex->right;
        Type::StructMemberInfo* mInfo = (Type::StructMemberInfo*) member->value.type;

        if (bex->base.opType == OP_DEREFERENCE_MEMBER_SELECTION) {
            // Calculate the absolute address on the stack
            const Err::Err err = compile(state, parent, NULL, flags);
            if (err != Err::OK) return err;

            // TODO : edit for lvalue
            pushOpcode(state, OC_PUSH_U64);
            pushOperand(state, mInfo->offset);
            pushOpcode(state, OC_ADD_U64);

            pushOpcode(state, selectLoadOpcode(mInfo->type->kind));
            if (mInfo->type->kind == Type::DT_STRUCT) {
                pushOperand(state, mInfo->type->size);
            }

            return Err::OK;
        }

        if (parent->def) {
            // TODO : edit for lvalue
            pushGetOpcode(state, mInfo->type, parent->def, mInfo->offset);
        } else {
            // Arbitrary expression on stack
            const Err::Err err = compile(state, parent, NULL, flags);
            if (err != Err::OK) return err;

            if (flags & IS_LVALUE) {
                pushOpcode(state, OC_PUSH_U64);
                pushOperand(state, mInfo->offset);
                pushOpcode(state, OC_ADD_U64);
            } else {
                pushOpcode(state, OC_CROP);
                pushOperand(state, BYTES_TO_WORDS(parent->value.type->size));
                pushOperand(state, mInfo->offset);
                pushOperand(state, mInfo->type->size);
            }
        }

        return Err::OK;
    }

    Err::Err compileArraySize(CompilerState* state, Variable* var) {
        var = unwrap(var);

        Type::ArrayInfo* aInfo = (Type::ArrayInfo*) var->value.type;

        if (!(var->value.type->kind == Type::DT_ARRAY)) {
            Logger::log(logErr, "Array length expected!'.", var->base.span);
            return Err::UNEXPECTED_ERROR;
        }

        pushOpcode(state, OC_PUSH_U64);
        pushOperand(state, aInfo->elementCount);

        Type::TypeInfo* elemType = aInfo->element;
        if (elemType->size > 1) {
            pushOpcode(state, OC_PUSH_U64);
            pushOperand(state, elemType->size);
            pushOpcode(state, OC_MUL_U64);
        }

        return Err::OK;
    }

    Err::Err compileInPlace(CompilerState* state, ArrayInitialization* init, Variable* target) {
        Type::TypeInfoEx* info = (Type::TypeInfoEx*) target->value.type;
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
        if (bex->left->value.type->kind == Type::DT_POINTER ||
            bex->left->value.type->kind == Type::DT_ARRAY) {
            var = bex->left;
        } else if (bex->right->value.type->kind == Type::DT_POINTER ||
            bex->right->value.type->kind == Type::DT_ARRAY) {
            var = bex->right;
        }

        if (!var) return false;

        const OperatorEnum op = bex->base.opType;
        if (op != OP_ADDITION && op != OP_SUBTRACTION) {
            return false;
        }

        Type::TypeInfo* info = var->value.type;

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

    void pushVecOperands(CompilerState* state, Variable* target, VecDescriptor desc, const bool isRoot) {
        uint64_t dest;
        if (isRoot) {
            if (target->def) {
                dest = target->def->vmOffset;
            } else {
                dest = 0;
                desc.flags |= DE_F_IS_DEST_STACK;
            }
            state->vecResult.isTmp = false;
        } else {
            dest = 0; // TODO
            desc.flags |= DE_F_DEST;
            state->vecResult.isTmp = true;
        }

        pushDescriptor(state, desc);
        pushOperand(state, dest);
    }

    inline bool tryVectorization(CompilerState* state, BinaryExpression* bex, VecResult lRes, VecResult rRes, Variable* target, const bool isRoot) {
        if (!Type::isArrayLike(bex->left->value.type->kind) &&
            !Type::isArrayLike(bex->right->value.type->kind)) {
            return false;
        }

        VecDescriptor desc = {
            .oper = bex->base.opType,
            .flags =
                (((uint32_t) lRes.isTmp) << DE_F_LEFT_SHIFT) |
                (((uint32_t) rRes.isTmp) << DE_F_RIGHT_SHIFT)
        };

        Type::TypeInfoEx* leftInfo  = (Type::TypeInfoEx*) bex->left->value.type;
        Type::TypeInfoEx* rightInfo = (Type::TypeInfoEx*) bex->right->value.type;

        if (Type::isArrayLike(bex->left->value.type->kind)) {
            desc.dtype = leftInfo->arr.element->kind;
            if (bex->base.opType == OP_CONCATENATION) {
                pushOpcode(state, OC_VEC_CAT);
            } else {
                pushOpcode(state, OC_VEC_VV);
            }
        } else if (bex->left->value.type->kind == Type::DT_ARRAY) {
            desc.dtype = leftInfo->arr.element->kind;
            pushOpcode(state, OC_VEC_VS);
        } else {
            desc.dtype = rightInfo->arr.element->kind;
            pushOpcode(state, OC_VEC_SV);
        }

        pushVecOperands(state, target, desc, isRoot);

        return true;
    }

    // Unary Version
    bool tryVectorization(CompilerState* state, UnaryExpression* uex, Variable* target, const bool isRoot) {
        if (!Type::isArrayLike(uex->operand->value.type->kind)) return false;

        pushOpcode(state, OC_VEC_UNARY);

        VecDescriptor desc = {
            .dtype = ((Type::ArrayInfo*) uex->operand->value.type)->element->kind,
            .oper = uex->base.opType,
            .flags = ((uint32_t) state->vecResult.isTmp) << DE_F_DEST_SHIFT
        };

        pushVecOperands(state, target, desc, isRoot);

        return true;
    }

    void pushSliceLength(CompilerState* state, Variable* var) {
        pushGetOpcode(state, var->value.type, var->def, 8);
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
                    pushBoolCast(state, uex->operand->value.type->kind);
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

                if (isMemberSelection(bex->base.opType)) {
                    return compileMemberSelection(state, bex, flags);
                }

                err = compile(state, bex->left, target, flags);
                if (err != Err::OK) return err;
                VecResult lResult = state->vecResult;

                // TODO :
                // If left side is an array, it may have length
                // we just remove it here...
                // Later we may want to catch this upfront and never
                // emit it in the first place... but not for now not sure
                // if it will work for all cases... here we shall be fine, as
                // we are already dealing with the result...
                if (flags & FORCE_ARRAY_LENGTH &&
                    bex->base.opType == OP_SUBSCRIPT &&
                    bex->left->value.type->kind == Type::DT_ARRAY) {
                    pushOpcode(state, OC_POP);
                }

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
                    Type::PointerInfo* pInfo = (Type::PointerInfo*) bex->left->value.type;
                    Type::TypeInfo* eInfo = pInfo->element;

                    pushOperand(state, eInfo->size);

                    if (!(flags & IS_LVALUE)) {
                        // TODO : to a function
                        oc = selectLoadOpcode(eInfo->kind);
                        pushOpcode(state, oc);
                        if (oc == OC_LOAD_BLOB) {
                            pushOperand(state, eInfo->size);
                        }
                    }
                }

                break;

            }

            case EXT_FUNCTION_CALL: {

                FunctionCall* call = (FunctionCall*) node;
                Function* fcn = call->fcn;

                if (!isValidFunctionIdx(call->fcn->internalIdx) && !fcn->exe) {
                    // TODO : do we realy want to wait here? Shall it rather be
                    //        propagated?
                    TaskSystem::dispatchLocalTask(fcn, false);
                    // TODO : handle somehow error.
                }

                // create empty slots for callee exe, fp and ip
                pushOpcode(state, OC_GROW);
                pushOperand(state, 3 * sizeof(vmword));

                // predetermine if the last arg is vardic, so we
                // dont have to lookup in main loop prototype definition
                int fixedCount = fcn->prototype.inArgCount;
                int varArgsCount = 0;
                bool isVariadic = false;

                if (fixedCount > 0) {
                    VariableDefinition* lastArgPrototype = fcn->prototype.inArgs[fixedCount - 1];
                    if (lastArgPrototype->var->value.type->kind == Type::DT_MULTIPLE_TYPES) {
                        isVariadic = true;
                        fixedCount--;
                    }
                }

                VariableDefinition** fcnInArgs = fcn->prototype.inArgs;

                for (int i = 0; i < fixedCount; i++) {
                    Variable* arg = call->inArgs[i];
                    if (fcnInArgs[i]->var->value.type->kind == Type::DT_ARRAY) {
                        err = compile(state, arg, NULL, FORCE_ARRAY_LENGTH);
                    } else {
                        err = compile(state, arg, NULL);
                    }
                    if (err != Err::OK) return err;
                }

                bool anyVarargIsArray = false;
                if (isVariadic) {
                    for (int i = fixedCount; i < call->inArgCount; i++) {
                        Variable* arg = call->inArgs[i];
                        compileAsAny(state, arg);
                        anyVarargIsArray = arg->value.type->kind == Type::DT_ARRAY;
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

                    VecDescriptor desc;
                    desc.dtype = cast->target->kind;
                    desc.flags = isRoot ? 0 : DE_F_DEST | DE_F_LEFT;

                    // Source can be element type
                    if (cast->kind == Cast::Kind::FROM_LOWER_LEVEL) {
                        desc.srcDtype = cast->operand->value.type->kind;
                        desc.flags = IS_CASTED_FROM_LOWER_LEVEL;
                    } else {
                        desc.srcDtype = ((Type::TypeInfoEx*) (cast->operand->value.type))->ptr.element->kind;
                        desc.flags = 0;
                    }

                    // even in root target can be general expression,
                    // ex. assignment x.y[i] = arr ...;
                    pushVecOperands(state, target, desc, isRoot);
                } else {
                    compile(state, cast->operand, target, flags);

                    Opcode op = selectCastOpcode(cast->operand->value.type->kind, cast->target->kind);
                    if (op == OC_NOP) break;

                    pushOpcode(state, op);
                }

                break;
            }

            case EXT_ALLOC: {
                Alloc* alc = (Alloc*) node;

                Type::TypeInfo* dtype = alc->def->var->value.type;

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

                if (ex->arr->value.type->kind == Type::DT_ARRAY) {
                    Type::ArrayInfo* aType = (Type::ArrayInfo*) ex->arr->value.type;
                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, aType->elementCount);
                } else {
                    pushSliceLength(state, ex->arr);
                }

                break;
            }

            case EXT_GET_SIZE: {
                GetSize* ex = (GetSize*) node;

                if (ex->arr->value.type->kind == Type::DT_ARRAY) {
                    Type::ArrayInfo* type = (Type::ArrayInfo*) ex->arr->value.type;
                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, type->elementCount * type->element->size);
                } else {
                    Type::ArrayInfo* type = (Type::ArrayInfo*) ex->arr->value.type;
                    pushSliceLength(state, ex->arr);
                    pushOperand(state, type->element->size);
                    pushOpcode(state, OC_MUL_I64);
                }

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

                Type::TypeInfo* elementInfo = NULL;
                if (elementCount > 0) {
                    elementInfo = init->attributes[0]->value.type;
                }

                if (init->flags & IS_CMP_TIME) {
                    Variable* first = init->attributes[0];

                    uint64_t offset = state->rawData.logicalPos;
                    uint64_t elementSize = first->value.type->size;

                    uint8_t* rawDataPtr = (uint8_t*) Arena::push(&state->rawData, elementCount * elementSize, 1);
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
                        .dtype = first->value.type->kind,
                    };

                    pushVecOperands(state, target, desc, isRoot);
                } else {
                    if (isRoot) {
                        if (target->def) {
                            pushOpcode(state, OC_LEA);
                            pushOperand(state, target->def->vmOffset);
                        } else {
                            // TODO : pointer should already be on stack, right? ... right?
                        }
                    } else {
                        pushOpcode(state, OC_PUSH_U64);
                        pushOperand(state, elementCount);

                        pushOpcode(state, OC_VEC_ALLOC);
                        pushOperand(state, elementInfo->size);

                        state->vecResult.isTmp = true;
                    }

                    for (int i = 0; i < elementCount; i++) {
                        Variable* arg = init->attributes[i];

                        pushOpcode(state, OC_DUP);
                        pushOpcode(state, OC_PUSH_U64);
                        pushOperand(state, i);
                        pushOpcode(state, OC_PTR_IDX);
                        pushOperand(state, elementInfo->size);

                        compile(state, arg);

                        Opcode op = selectStoreOpcode(elementInfo->kind);
                        pushOpcode(state, op);
                    }

                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, elementCount);
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
        // TODO
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Enumerator* scope) {
        // TODO
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Branch* node) {
        Err::Err err;

        uint64_t patchListHead = patchListHeadNull;

        uint32_t i = 0;
        for (; i < node->expressionCount; i++) {
            err = compile(state, node->expressions[i]);
            if (err != Err::OK) return err;

            const uint64_t jumpBlockStartOffset = state->bytecode.logicalPos;
            pushOpcode(state, OC_JUMP_IF_FALSE);
            uint8_t* jumpBlockEndPtr = pushOperand(state, 0);

            err = compile(state, node->scopes[i]);
            if (err != Err::OK) return err;

            if (i < node->scopeCount - 1) {
                pushOpcode(state, OC_JUMP);
                pushOperand(state, patchListHead);
                patchListHead = state->bytecode.logicalPos - 8;
            }

            const uint64_t jumpBlockOffset = state->bytecode.logicalPos - jumpBlockStartOffset;
            memcpy(jumpBlockEndPtr, &jumpBlockOffset, sizeof(uint64_t));
        }

        if (i < node->scopeCount) {
            err = compile(state, node->scopes[i]);
            if (err != Err::OK) return err;
        }

        const uint64_t jumpVeryEndOffset = state->bytecode.logicalPos;
        patchList(state, patchListHead, jumpVeryEndOffset);

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, SwitchCase* scope) {
        // TODO
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, WhileLoop* node) {
        // TODO : kinda wasteful, maybe we create either flag or
        //  force each line-like statement to be parsed as Statement
        Span tmpSpan = *node->base.span;
        tmpSpan.end = node->bodyScope->base.span->start;
        updateSourceLocation(state, &tmpSpan);

        Err::Err err;

        const uint64_t startOffset = state->bytecode.logicalPos;
        const uint64_t prevCurrentLoopAddress = state->currentLoopAddress;
        state->currentLoopAddress = startOffset;

        SyntaxNode* prevLoop = state->currentLoop;
        state->currentLoop = (SyntaxNode*) node;

        err = compile(state, node->expression);
        if (err != Err::OK) return err;

        const uint64_t jumpOperandStartOffset = state->bytecode.logicalPos;
        pushOpcode(state, OC_JUMP_IF_FALSE);
        uint8_t* jumpOperandPtr = pushOperand(state, 0);

        err = compile(state, node->bodyScope);
        if (err != Err::OK) return err;

        pushOpcode(state, OC_JUMP);
        pushOperand(state, startOffset - state->bytecode.logicalPos + 1);

        const uint64_t jumpOperandRelativeOffset = state->bytecode.logicalPos - jumpOperandStartOffset;
        memcpy(jumpOperandPtr, &jumpOperandRelativeOffset, sizeof(uint64_t));

        state->currentLoop = prevLoop;
        state->currentLoopAddress = prevCurrentLoopAddress;

        return Err::OK;
    }

    // TODO
    Err::Err compile(CompilerState* state, Loop* node) {
        // TODO : kinda wasteful, maybe we create either flag or
        //  force each line-like statement to be parsed as Statement
        Span tmpSpan = *node->base.span;
        tmpSpan.end = node->bodyScope->base.span->start;
        updateSourceLocation(state, &tmpSpan);

        Err::Err err;

        const uint64_t startOffset = state->bytecode.logicalPos;
        const uint64_t prevCurrentLoopAddress = state->currentLoopAddress;
        state->currentLoopAddress = startOffset;

        SyntaxNode* prevLoop = state->currentLoop;
        state->currentLoop = (SyntaxNode*) node;

        uint64_t oldListHeadBreak = state->listHeadBreak;
        uint64_t oldListHeadContinue = state->listHeadContinue;
        state->listHeadBreak = patchListHeadNull;
        state->listHeadContinue = patchListHeadNull;

        int64_t stride = 1;
        if (node->arg.range->step) {
            Variable* tmp = unwrap(node->arg.range->step);
            stride = *(int64_t*) &(tmp->value.u64);
        }

        int64_t indexOffset = -1;
        if (node->index.var) {
            SyntaxNode* index = (SyntaxNode*) node->index.var;
            compile(state, index);
            indexOffset = index->type == NT_VARIABLE ?
                node->index.var->def->vmOffset : node->index.def->vmOffset;
        }

        if (node->arg.kind == Loop::Arg::EXPRESSION) {
            compile(state, node->arg.exp);
            pushOpcode(state, OC_SWAP);
            pushOpcode(state, OC_POP);
        } else {
            compile(state, node->arg.range->eidx);
        }

        pushOpcode(state, stride >= 0 ? OC_LT_U64 : OC_LT_I64);

        const uint64_t jumpOperandStartOffset = state->bytecode.logicalPos;
        pushOpcode(state, OC_JUMP_IF_FALSE);
        uint8_t* jumpOperandPtr = pushOperand(state, 0);

        err = compile(state, node->bodyScope);
        if (err != Err::OK) return err;

        const uint64_t incOffset = state->bytecode.logicalPos;
        if (indexOffset >= 0) {
            pushOpcode(state, OC_GET_I64);
            pushOperand(state, indexOffset);

            pushOpcode(state, OC_PUSH_I64);
            pushOperand(state, stride);

            pushOpcode(state, OC_ADD_I64);

            pushOpcode(state, OC_SET_I64);
            pushOperand(state, indexOffset);
        }

        pushOpcode(state, OC_JUMP);
        pushOperand(state, startOffset - state->bytecode.logicalPos + 1);

        const uint64_t jumpOperandRelativeOffset = state->bytecode.logicalPos - jumpOperandStartOffset;
        memcpy(jumpOperandPtr, &jumpOperandRelativeOffset, sizeof(uint64_t));

        patchList(state, state->listHeadBreak, state->bytecode.logicalPos);
        patchList(state, state->listHeadContinue, incOffset);

        state->currentLoop = prevLoop;
        state->listHeadBreak = oldListHeadBreak;
        state->listHeadContinue = oldListHeadContinue;
        state->currentLoopAddress = prevCurrentLoopAddress;

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
        if (node->var) {
            Type::TypeInfo* dtype = node->var->value.type; // TODO
            pushOperand(state, dtype->size);
        } else {
            pushOperand(state, 0);
        }

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, ContinueStatement* node) {
        updateSourceLocation(state, node->base.span);

        pushOpcode(state, OC_JUMP);
        if (state->currentLoop->type == NT_WHILE_LOOP) {
            pushOperand(state, state->currentLoopAddress - state->bytecode.logicalPos + 1);
        } else {
            pushOperand(state, state->listHeadContinue);
            state->listHeadContinue = state->bytecode.logicalPos - 8;
        }

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, BreakStatement* node) {
        updateSourceLocation(state, node->base.span);

        pushOpcode(state, OC_JUMP);
        pushOperand(state, state->listHeadBreak);
        state->listHeadBreak = state->bytecode.logicalPos - 8;

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, GotoStatement* node) {
        updateSourceLocation(state, node->base.span);

        pushOpcode(state, OC_JUMP);
        pushOperand(state, node->label->vmAddress - state->bytecode.logicalPos + 1);

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Label* node) {
        node->vmAddress = state->bytecode.logicalPos;
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Namespace* node) {
        return compile(state, &node->scope);
    }

    Err::Err compile(CompilerState* state, Statement* node) {
        updateSourceLocation(state, node->base.span);
        compile(state, node->operand);
        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Using* node) {
        // TODO
        return Err::OK;
    }

    void commitCompileState(CompilerState* state, ExeBlock* exe, SyntaxNode* target) {
        exe->bytecodeSize = state->bytecode.logicalPos;
        exe->bytecode = (uint8_t*) alloc(Arena::getFlatSize(&state->bytecode), 1);
        Arena::flatCopy(&state->bytecode, exe->bytecode);

        exe->localsSize = state->locals.logicalPos;
        exe->locals = (uint8_t*) alloc(Arena::getFlatSize(&state->locals), state->maxAlign);
        memset(exe->locals, 0, exe->localsSize);
        Arena::flatCopy(&state->locals, (uint8_t*) exe->locals);

        exe->rawDataSize = state->rawData.logicalPos;
        exe->rawData = (uint8_t*) alloc(Arena::getFlatSize(&state->rawData), 1);
        Arena::flatCopy(&state->rawData, exe->rawData);

        exe->node = target;
        exe->localsInfoMap = OrderedDict::tightCopy(&state->localsInfoMap);

        exe->linesSize = state->lines.size;
        exe->lines = alloc<LineInfo>(exe->linesSize);
        memcpy(exe->lines, state->lines.buffer, exe->linesSize * sizeof(LineInfo));

        exe->liveFp = NULL;
        exe->fixedSize = state->fixedSize;
        exe->defaultArgsSize = state->defaultArgsSize;
    }

    Err::Err compile(CompilerState* state, Function* fcn) {
        Err::Err err;

        if (!fcn->exe) {
            fcn->exe = makeExeBlock();
        } else {
            return Err::OK;
        }

        state->exe = fcn->exe;

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

            if (def->var->value.type->kind == Type::DT_MULTIPLE_TYPES) {
                fcn->exe->isVariadic = true;
            }

            err = compile(state, def);
            if (err != Err::OK) return err;
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
            case NT_USING :
                return compile(state, (Using*) node);

            default:
                // TODO
                return Err::CANNOT_EVALUATE;
        }
    }

    Err::Err compileOnlyLocals(CompilerState* state, Scope* scope) {
        for (int i = 0; i < scope->childrenCount; i++) {
            SyntaxNode* node = scope->children[i];
            switch (node->type) {
                case NT_SCOPE :
                    return compileOnlyLocals(state, (Scope*) node);
                case NT_VARIABLE_DEFINITION :
                    return compile(state, (VariableDefinition*) node);
                case NT_NAMESPACE :
                    return compileOnlyLocals(state, (Scope*) node);
            }
        }

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Reg::Unit* unit) {
        if (!unit->exe) {
            unit->exe = makeExeBlock();
        }

        unit->exe->isVariadic = false;

        state->exe = unit->exe;
        state->defaultArgsSize = state->locals.logicalPos;

        // Assumption that if no scope, we are external function
        if (unit->ast->root) {
            // We want to process all function beforehand, so we
            // dont have to handle saving/restoring state at each
            // nested 'exe block' compilation...
            // TODO : We may want to create a task group here later...
            for (int i = 0; i < unit->reg->fcns.size; i++) {
                Function* inner = *(Function**) DArray::get(&unit->reg->fcns, i);
                TaskSystem::dispatchLocalTask(inner, true);
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
