// interpreter related code that focusing
// on building the bytecode

#include "allocator.h"
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


#include <cassert>
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

    void releaseBuild(CompilerState* state) {
        Arena::release(&state->locals);
        Arena::release(&state->bytecode);
        Arena::release(&state->rawData);

        OrderedDict::release(&state->localsInfoMap);
        DArray::release(&state->lines);
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

    inline void recordLocalInfo(CompilerState* state, Variable* var, Type::TypeInfo* type, const uint64_t offset) {
        LocalVarInfo* header = alloc<LocalVarInfo>();
        header->var = var;
        header->type = type;

        // String key = String((char*) offset, sizeof(uint64_t));
        OrderedDict::set(&state->localsInfoMap, offset, header);
    }

    inline void pushLocal(CompilerState* state, Type::TypeInfo* type, uint64_t* offset) {
        const uint64_t vmwordsCount = BYTES_TO_WORDS(type->size);

        *offset = state->locals.logicalPos + state->fixedSize;

        if (state->populateLocals) {
            const uint64_t allocSize = vmwordsCount * sizeof(vmword);
            uint8_t* body = (uint8_t*) Arena::push(&state->locals, allocSize, sizeof(vmword));
            memset(body, 0, allocSize);
        } else {
            state->locals.logicalPos += vmwordsCount * sizeof(vmword);
        }
    }

    inline void pushAndRecordLocal(CompilerState* state, Variable* var, uint64_t* offset) {
        pushLocal(state, var->value.type, offset);
        recordLocalInfo(state, var, NULL, *offset);
    }

    inline void pushAndRecordLocal(CompilerState* state, Variable* var, Type::TypeInfo* type, uint64_t* offset) {
        pushLocal(state, type, offset);
        recordLocalInfo(state, var, type, *offset);
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

    void pushBoolCast(CompilerState* state, Type::Kind type) {
        if (isI32(type)) {
            pushOpcode(state, OC_BOOL_I32);
        } else if (isI64(type)) {
            pushOpcode(state, OC_BOOL_I64);
        } else if (isF32(type)) {
            pushOpcode(state, OC_BOOL_F32);
        } else if (isF64(type)) {
            pushOpcode(state, OC_BOOL_F64);
        } else if (type == Type::DT_POINTER) {
            pushOpcode(state, OC_BOOL_I64);
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

                uint32_t val = (uint32_t) value->u8;
                uint32_t* ptr = (uint32_t*) Arena::push(bytecode, 4, 1);
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

                uint32_t val = (uint32_t) value->i16;
                uint32_t* ptr = (uint32_t*) Arena::push(bytecode, 4, 1);
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

                uint32_t* ptr = (uint32_t*) Arena::push(bytecode, 4, 1);
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

                uint64_t* ptr = (uint64_t*) Arena::push(bytecode, 8, 1);
                memcpy(ptr, &value->i64, sizeof(uint64_t));

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

            case Type::DT_POINTER: {
                pushOpcode(state, OC_PUSH_PTR);

                int64_t* ptr = (int64_t*) Arena::push(bytecode, 8, 1);
                memcpy(ptr, &value->i64, sizeof(int64_t));
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

    void pushStoreOpcode(CompilerState* state, Type::TypeInfo* type) {
        Opcode opcode = selectStoreOpcode(type->kind);
        pushOpcode(state, opcode);
        if (opcode == OC_STORE_BLOB) {
            pushOperand(state, type->size);
        }
    }

    void pushLoadOpcode(CompilerState* state, Type::TypeInfo* type) {
        Opcode opcode = selectLoadOpcode(type->kind);
        pushOpcode(state, opcode);
        if (opcode == OC_LOAD_BLOB) {
            pushOperand(state, type->size);
        }
    }

    // offset is relative offset to target specific part of variable/blob
    void pushSetOpcode(CompilerState* state, Type::TypeInfo* typeInfo, VariableDefinition* def, uint64_t offset) {
        Opcode op;

        if (def->vmOwnerExe == state->exe) {
            op = selectSetOpcode(typeInfo->kind);
            pushOpcode(state, op);
            offset = def->vmOffset + offset;
        }
        else {
            op = selectSetGlobalOpcode(typeInfo->kind);
            pushOpcode(state, op);
            pushOperand(state, (uint64_t)def);
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
        }
        else {
            op = selectGetGlobalOpcode(typeInfo->kind);
            pushOpcode(state, op);
            pushOperand(state, (uint64_t)def);
        }

        if (op == OC_GET_BLOB || op == OC_GET_GLOBAL_BLOB) {
            // the size of the chunk to withdraw
            pushOperand(state, typeInfo->size);
        }

        pushOperand(state, offset);
    }

    inline void pushLeaOpcode(CompilerState* state, VariableDefinition* def, uint64_t offset = 0) {
        if (def->vmOwnerExe == state->exe) {
            pushOpcode(state, OC_LEA);
            pushOperand(state, def->vmOffset + offset);
        } else {
            pushOpcode(state, OC_LEA_GLOBAL);
            pushOperand(state, (uint64_t) def);
            pushOperand(state, offset);
        }
    }

    void pushOperator(CompilerState* state, UnaryExpression* uex) {
        OperatorEnum op = uex->base.opType;

        switch (op) {
            case OP_UNARY_PLUS: {
                break;
            }

            case OP_UNARY_MINUS: {
                int offset = getDtypeOffsetNoCast(uex->operand->value.type->kind);
                pushOpcode(state, (Opcode)(OC_NEG_I32 + offset));
                break;
            }

            case OP_GET_ADDRESS: {
                // pushLeaOpcode(state, uex->operand->def, 0);
                break;
            }

            case OP_GET_VALUE: {
                // TODO: consider passing owning var, so we can read directly a type that was resolved
                //       may eliminate some future bugs
                Type::PointerInfo* pType = (Type::PointerInfo*) uex->operand->value.type;
                pushLoadOpcode(state, pType->element);
                break;
            }

            case OP_NEGATION: {
                pushBoolCast(state, uex->operand->value.type->kind);
                pushOpcode(state, OC_NOT_BOOL);
                break;
            }

            case OP_NONE: {
                break;
            }

            default: {
                // TODO
            }
        }
    }

    // Pass unwrapped variable
    bool isConstantPoolCandidate(Variable* var) {
        if (!var || !var->expression) {
            return false;
        }

        if (var->expression->type == EXT_STRING_INITIALIZATION) {
            return true;
        }

        if (var->expression->type == EXT_ARRAY_INITIALIZATION) {
            ArrayInitialization* init = (ArrayInitialization*) var->expression;
            return init->flags & IS_CMP_TIME;
        }

        return false;
    }

    struct JumpPatch {
        uint64_t instructionOffset;
        uint8_t* operandPointer;
    };

    JumpPatch pushJumpPlaceholder(CompilerState* state, Opcode opcode) {
        pushOpcode(state, opcode);

        JumpPatch patch;
        patch.instructionOffset = state->bytecode.logicalPos - 1;
        patch.operandPointer = pushOperand(state, (uint64_t) 0);

        return patch;
    }

    void patchJumpToHere(CompilerState* state, JumpPatch patch) {
        uint64_t relativeOffset = state->bytecode.logicalPos - patch.instructionOffset;
        std::memcpy(patch.operandPointer, &relativeOffset, sizeof(uint64_t));
    }

    void pushJumpBack(CompilerState* state, uint64_t targetOffset) {
        pushOpcode(state, OC_JUMP);
        uint64_t relativeOffset = targetOffset - (state->bytecode.logicalPos - 1);
        pushOperand(state, relativeOffset);
    }

    uint64_t addJumpToList(CompilerState* state, uint64_t listHead) {
        pushOpcode(state, OC_JUMP);

        uint64_t operandOffset = state->bytecode.logicalPos;
        pushOperand(state, listHead);

        return operandOffset;
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

    void patchOperand(CompilerState* state, uint64_t offset, uint64_t value) {
        vmword* ptr = (vmword*) Arena::getPointerToLogicalOffset(
            &state->bytecode,
            offset
        );

        *ptr = (vmword) value;
    }

    void patchOperandIfZero(CompilerState* state, uint64_t offset, uint64_t value) {
        vmword* ptr = (vmword*) Arena::getPointerToLogicalOffset(
            &state->bytecode,
            offset
        );

        if (*ptr == 0) *ptr = (vmword) value;
    }

    // TODO : move in meaningful place
    enum {
        IS_LVALUE = 1,
        IS_BARE_STATEMENT = (1 << 1),
        IS_ROOT = (1 << 2),

        FORCE_ARRAY_LENGTH = (1 << 3),
        FORCE_VEC_OPCODES = (1 << 4), // TODO : ? include FORCE_ARRAY_LENGTH ?
        FORCE_STACK_VALUE = (1 << 5),
    };

    Err::Err compile(CompilerState* state, SyntaxNode* node);
    Err::Err compile(CompilerState* state, Variable* node, Type::TypeInfo* target = NULL, Flags flags = 0);
    Err::Err compile(CompilerState* state, Function* node);
    Err::Err compile(CompilerState* state, VariableAssignment* node);
    Err::Err compileExpression(CompilerState* state, Variable* node, Type::TypeInfo* target, Flags flags = 0);

    void pushDescriptor(CompilerState* state, VecDescriptor desc);

    // Pass only Type::isArrayLike types
    Err::Err compileLength(CompilerState* state, Variable* var) {
        Type::TypeInfoEx* typeEx = (Type::TypeInfoEx*)var->value.type;

        if (typeEx->base.kind == Type::DT_ARRAY) {
            pushOpcode(state, OC_PUSH_U64);
            pushOperand(state, typeEx->arr.elementCount);
        } else {
            Variable* tmp = unwrapWithCasts(var);
            if (tmp->def) {
                pushGetOpcode(state, &typeEx->base, tmp->def, sizeof(vmword));
            } else {
                Err::Err err = compile(state, var, NULL, FORCE_ARRAY_LENGTH);
                if (err != Err::OK) return err;

                pushOpcode(state, OC_SWAP);
                pushOpcode(state, OC_POP);
            }
        }

        return Err::OK;
    }

    // Pass only Type::isArrayLike types
    Err::Err compileSize(CompilerState* state, Variable* var) {
        Type::TypeInfoEx* typeEx = (Type::TypeInfoEx*)var->value.type;

        if (typeEx->base.kind == Type::DT_ARRAY) {
            pushOpcode(state, OC_PUSH_U64);
            pushOperand(state, typeEx->arr.elementCount * typeEx->arr.element->size);
        } else {
            Variable* tmp = unwrapWithCasts(var);
            if (tmp->def) {
                pushGetOpcode(state, &typeEx->base, tmp->def, sizeof(vmword));
            } else {
                Err::Err err = compile(state, var, NULL, FORCE_ARRAY_LENGTH);
                if (err != Err::OK) return err;

                pushOpcode(state, OC_SWAP);
                pushOpcode(state, OC_POP);
            }

            pushOpcode(state, OC_PUSH_I64);
            pushOperand(state, typeEx->ptr.element->size);
            pushOpcode(state, OC_MUL_I64);
        }

        return Err::OK;
    }

    Err::Err compileAsAny(CompilerState* state, Variable* var) {
        Err::Err err = Err::OK;

        Type::TypeInfo* type = var->value.type;

        Runtime::_TypeInfo* runtimeInfo = Runtime::toRuntimeType(var->value.type);
        if (!runtimeInfo) {
            return Err::NOT_YET_IMPLEMENTED;
        }

        pushOpcode(state, OC_PUSH_PTR);
        pushOperand(state, (uint64_t) runtimeInfo);

        if (Type::isPrimitive(type)) {
            err = compile(state, var);
            if (err != Err::OK) return err;

            return Err::OK;
        }

        {
            Variable* tmp = unwrap(var);
            if (tmp->def) {
                pushLeaOpcode(state, tmp->def);
                return Err::OK;
            }
        }

        if (Type::isStructLike(type)) {
            // TODO : move to a function?
            uint64_t offset = state->locals.logicalPos;
            push(&state->locals, type->size, type->align);

            err = compile(state,var);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_SET_BLOB);
            pushOperand(state, type->size);
            pushOperand(state, offset);

            pushOpcode(state, OC_LEA);
            pushOperand(state, offset);
        } else if (type->kind == Type::DT_ARRAY) {
            // TODO
            err = compile(state, var);
            if (err != Err::OK) return err;
        } else if (Type::isArrayLike(type)) {
            err = compile(state, var, NULL, FORCE_ARRAY_LENGTH | FORCE_VEC_OPCODES);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_VEC_TO_REF);
        } else {
            err = compile(state, var);
            if (err != Err::OK) return err;
        }

        return err;
    }

    Err::Err compile(CompilerState* state, Scope* node) {
        for (int i = 0; i < node->childrenCount; i++) {
            compile(state, node->children[i]);

            // TODO: generalize either concept of statements or
            //       expressions, so we can call it in ex. compile:statement
            if (state->vecTmpMemUsed) {
                state->vecTmpMemUsed = false;
                pushOpcode(state, OC_VEC_MEM_RESET);
            }
        }

        return Err::OK;
    }

    Err::Err compileInitialization(CompilerState* state, Variable* source, VariableDefinition* target) {
        if (!source || (!source->expression && !source->value.hasValue)) {
            return Err::OK;
        }

        Type::TypeInfo* type = target->var->value.type;
        if (type->kind == Type::DT_ARRAY) {
            Type::TypeInfo* eType = ((Type::PointerInfo*) type)->element;

            // We use vec opcodes, we have to prepare dest pointer on stack.
            // Arrays have its own data-place in locals, we have to 'lea'
            pushLeaOpcode(state, target);

            Err::Err err = compile(state, source, target->var->value.type,
                FORCE_ARRAY_LENGTH | FORCE_VEC_OPCODES | IS_ROOT);
            if (err != Err::OK) return err;

            // TODO: unite with ass ignment
            if (state->vecResult.isTmp) {
                VecDescriptor copyDesc = {
                    .type = eType->kind,
                    .oper = OP_NONE,
                    .flags = 0,
                    .dstElemSize = (uint16_t) eType->size,
                };

                pushOpcode(state, OC_VEC_COPY);
                pushOperand(state, encodeVecDescriptor(copyDesc));
                pushOpcode(state, OC_VEC_RESET);

                state->vecLhsWritten = false;
            } else if (state->vecResult.isScalar) {
                // TODO: we need to refactor this to return a type compile function left on stack
                //       so we dont have any ambiguity or guessing. And all our pre-guessing to optimize
                //       then can be validated and clened up if mispredicted.
                compileLength(state, target->var);
                pushOpcode(state, OC_VEC_FILL);
                VecDescriptor desc = { .type = eType->kind };
                pushDescriptor(state, desc);
            }

            if (state->vecTmpMemUsed) {
                state->vecTmpMemUsed = false;
                pushOpcode(state, OC_VEC_RESET);
            }
        } else if (type->kind == Type::DT_SLICE) {
            // Slices are stored as pointer + length, they have to receive
            // pointer + length from rvalue. So we have to either allocate
            // slot on stack for the rvalue or assign it directly via pointer
            // if value is array-like variable

            Variable* var = unwrapWithCasts(source);
            if (var->def) {
                // TODO:
                // pushGetOpcode(state, type, target, 0);
            } else if (isConstantPoolCandidate(var)) {
                // TODO:
            } else if (var->expression->type == EXT_BINARY &&
                ((BinaryExpression*) var->expression)->right->value.type->kind == Type::DT_RANGE) {
                // TODO: think about this...
            } else {
                // TODO: think if we need to strip all casts, or just Slice ones...
                uint64_t offset;
                pushAndRecordLocal(state, target->var, var->value.type, &offset);
                if (!isOffsetValid(offset)) {
                    return Err::COMPILE_TIME_KNOWN_EXPRESSION_REQUIRED;
                }

                pushOpcode(state, OC_LEA);
                pushOperand(state, offset);
            }

            Err::Err err = compile(state, source, target->var->value.type,
                FORCE_ARRAY_LENGTH | FORCE_VEC_OPCODES | IS_ROOT);
            if (err != Err::OK) return err;

            pushSetOpcode(state, type, target, 8);
            pushSetOpcode(state, type, target, 0);

            if (state->vecTmpMemUsed) {
                state->vecTmpMemUsed = false;
                pushOpcode(state, OC_VEC_MEM_RESET);
            }
        } else if (Type::isStructLike(type)) {
            // TODO
            Variable* var = unwrap(source);
            if (var->expression && var->expression->type == EXT_TYPE_INITIALIZATION) {
                pushLeaOpcode(state, target);

                Err::Err err = compile(state, source, NULL, IS_ROOT);
                if (err != Err::OK) return err;
            } else {
                Err::Err err = compile(state, source);
                if (err != Err::OK) return err;

                pushSetOpcode(state, type, target, 0);
                return Err::OK;
            }
        } else {
            Err::Err err = compile(state, source);
            if (err != Err::OK) return err;

            pushSetOpcode(state, type, target, 0);
            return Err::OK;
        }

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, VariableDefinition* node) {
        updateSourceLocation(state, node->base.span);

        Err::Err err;
        uint64_t offset;

        pushAndRecordLocal(state, node->var, &offset);
        if (!isOffsetValid(offset)) {
            return Err::COMPILE_TIME_KNOWN_EXPRESSION_REQUIRED;
        }

        node->vmOffset = offset;
        node->vmOwnerExe = state->exe;

        return compileInitialization(state, node->var, node);
    }

    Err::Err compile(CompilerState* state, VariableAssignment* node) {
        updateSourceLocation(state, node->base.span);

        Err::Err err;

        Variable* lvar = unwrap(node->lvar);
        Variable* rvar = node->rvar; // unwrap(node->rvar);

        Type::TypeInfo* lType = lvar->value.type;

        // Direct varibale assignment
        if (lvar->def) {
            // TODO TODO TODO
            if (Type::isStructLike(lType)) {
                rvar = unwrap(node->rvar);
                if (rvar->expression && rvar->expression->type == EXT_TYPE_INITIALIZATION) {
                    pushLeaOpcode(state, lvar->def);

                    Err::Err err = compile(state, rvar, NULL, IS_ROOT);
                    if (err != Err::OK) return err;
                } else {
                    Err::Err err = compile(state, rvar);
                    if (err != Err::OK) return err;

                    pushSetOpcode(state, lType, lvar->def, 0);
                }
                return Err::OK;
            }

            err = compile(state, node->rvar);
            if (err != Err::OK) return err;

            if (lType->kind == Type::DT_SLICE) {
                pushSetOpcode(state, lType, lvar->def, sizeof(vmword));
                pushSetOpcode(state, lType, lvar->def, 0);
            } else {
                pushSetOpcode(state, lType, lvar->def, 0);
            }

            return Err::OK;
        }

        // Shall produce pointer on stack
        err = compile(state, lvar, NULL, IS_LVALUE);
        if (err != Err::OK) return err;

        if (Type::isArrayLike(lType)) {
            Type::TypeInfo* eType = ((Type::PointerInfo*) lType)->element;

            err = compile(state, rvar, lType,
                FORCE_ARRAY_LENGTH | FORCE_VEC_OPCODES | IS_ROOT);
            if (err != Err::OK) return err;

            if (state->vecResult.isTmp) {
                VecDescriptor copyDesc = {
                    .type = eType->kind,
                    .oper = OP_NONE,
                    .flags = 0,
                    .dstElemSize = (uint16_t) eType->size,
                };

                pushOpcode(state, OC_VEC_COPY);
                pushOperand(state, encodeVecDescriptor(copyDesc));
                pushOpcode(state, OC_VEC_RESET);

                state->vecLhsWritten = false;
            } else if (state->vecResult.isScalar) {
                compileLength(state, lvar);
                pushOpcode(state, OC_VEC_FILL);
                VecDescriptor desc = { .type = eType->kind };
                pushDescriptor(state, desc);
            }

            if (state->vecTmpMemUsed) {
                // TODO: we shouldnt use this falg for vec_reset
                state->vecTmpMemUsed = false;
                pushOpcode(state, OC_VEC_RESET);
            }

            return Err::OK;
        } else if (Type::isStructLike(lType)) {
            // TODO
            pushLeaOpcode(state, lvar->def);

            Err::Err err = compile(state, rvar, NULL, IS_ROOT);
            if (err != Err::OK) return err;
        } else {
            err = compile(state, node->rvar, lType);
            if (err != Err::OK) return err;

            pushStoreOpcode(state, lType);
        }

        return Err::OK;
    }

    // Pushes variable definition on stack as a consumable.
    // Ex. Arrays are pushed as either pointer or slice, instead of stack dump.
    void pushDefinition(CompilerState* state, VariableDefinition* def, uint64_t flags) {

    }

    // By default suppose leave the value on stack
    Err::Err compile(CompilerState* state, Variable* node, Type::TypeInfo* target, Flags flags) {
        // TODO : kinda wasteful, maybe we create either flag or
        //  force each line-like statement to be parsed as Statement
        updateSourceLocation(state, node->base.span);

        if (node->value.hasValue && (Type::isPrimitive(node->value.type))) {
            pushPushInstruction(state, &node->value);
            return Err::OK;
        }

        if (node->expression) {
            return compileExpression(state, node, target, flags);
        }

        if (node->def) {
            VariableDefinition* def = node->def;
            Type::TypeInfo* type = def->var->value.type;

            // TODO: think more...
            if (target && target->kind == Type::DT_SLICE) {
                flags |= FORCE_ARRAY_LENGTH;
            }

            if (type->kind == Type::DT_ARRAY) {
                Type::ArrayInfo* aType = (Type::ArrayInfo*) type;

                pushLeaOpcode(state, def);

                if (flags & FORCE_STACK_VALUE) {
                    pushOpcode(state, OC_LOAD_BLOB);
                    pushOperand(state, aType->base.size);
                } else {
                    // TODO: use function for this check, so we can also check for IS_LVALUE
                    if (flags & FORCE_ARRAY_LENGTH) {
                        pushOpcode(state, OC_PUSH_U64);
                        pushOperand(state, aType->elementCount);
                    }
                }

                // TODO: Dunno, if we gonna keep it this way, change at least to enum
                //       with on_stack value...
                state->vecResult.isTmp = true;
            } else if (type->kind == Type::DT_SLICE) {
                Type::SliceInfo* sType = (Type::SliceInfo*) type;
                pushGetOpcode(state, type, def, 0);

                if (flags & FORCE_ARRAY_LENGTH) {
                    pushGetOpcode(state, type, def, sizeof(vmword));
                }

                state->vecResult.isTmp = true;
            } else {
                if (flags & IS_LVALUE) {
                    pushLeaOpcode(state, node->def);
                } else {
                    pushGetOpcode(state, type, node->def, 0);
                }
            }

            return Err::OK;
        }

        // TODO:
        //Diag::report(state->ast, node->base.span, Err::COMPILE_TIME_KNOWN_EXPRESSION_REQUIRED,
        //        Diag::Format { "Unresolved variable or missing value" });

        return Err::COMPILE_TIME_KNOWN_EXPRESSION_REQUIRED;
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
        Type::TypeInfoEx* pType = (Type::TypeInfoEx*) parent->value.type;

        if (pType->base.kind == Type::DT_ENUM) {
            Type::EnumMemberInfo* mType =
                (Type::EnumMemberInfo*) bex->right->value.type;

            pushOpcode(state, OC_PUSH_U64);
            pushOperand(state, mType->value);
        } else if (pType->base.kind == Type::DT_POINTER) {
            Type::StructMemberInfo* mType =
                (Type::StructMemberInfo*) bex->right->value.type;

            // Calculate the absolute address on the stack
            const Err::Err err = compile(state, parent, NULL, flags & (~IS_LVALUE));
            if (err != Err::OK) return err;

            pushOpcode(state, OC_PUSH_U64);
            pushOperand(state, mType->offset);
            pushOpcode(state, OC_ADD_U64);

            if (flags ^ IS_LVALUE) {
                pushLoadOpcode(state, mType->type);
            }
        } else if (Type::isStructLike(pType->base.kind)) {
            Type::StructMemberInfo* mType =
                (Type::StructMemberInfo*) bex->right->value.type;

            if (parent->def) {
                // TODO: not sure about array check here
                if (flags & IS_LVALUE || mType->type->kind == Type::DT_ARRAY) {
                    pushLeaOpcode(state, parent->def, 0);
                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, mType->offset);
                    pushOpcode(state, OC_PTR_IDX);
                    pushOperand(state, 1);
                } else {
                    pushGetOpcode(state, mType->type, parent->def, mType->offset);
                }
            } else {
                // Arbitrary expression on stack
                const Err::Err err = compile(state, parent, NULL, (flags & IS_LVALUE) ? IS_LVALUE : 0);
                if (err != Err::OK) return err;

                if (flags & IS_LVALUE) {
                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, mType->offset);
                    pushOpcode(state, OC_ADD_U64);
                } else {
                    pushOpcode(state, OC_CROP);
                    pushOperand(state, BYTES_TO_WORDS(parent->value.type->size));
                    pushOperand(state, mType->offset);
                    pushOperand(state, mType->type->size);
                }
            }

            // TODO: If we are gonna keep this, try to compute length with parent and swap the
            //       value here
            if (flags ^ IS_LVALUE && Type::isArrayLike(mType->type->kind) && flags & FORCE_ARRAY_LENGTH) {
                if (mType->type->kind == Type::DT_ARRAY) {
                    Type::ArrayInfo* aType = (Type::ArrayInfo*) mType->type;
                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, aType->elementCount);
                } else {
                    // TODO: we may be on stack already?
                }
            }
        } else {
            // TODO: error
        }

        return Err::OK;
    }

    Err::Err compileRangeSlicing(CompilerState* state, BinaryExpression* bex, Type::TypeInfo* target, uint64_t flags) {
        Err::Err err;

        RangeExpression* range = (RangeExpression*)bex->right->expression;

        Type::TypeInfoEx* tType = (Type::TypeInfoEx*) target;
        Type::TypeInfoEx* lType = (Type::TypeInfoEx*) bex->left->value.type;
        Type::TypeInfo*   eType = lType->ptr.element;

        if (!range->step || (range->step->value.hasValue && range->step->value.i64 == 1)) {
            // If we are slice, we want to prepare length on stack now,
            // So we can reuse computed index
            if (flags & FORCE_ARRAY_LENGTH && tType->base.kind != Type::DT_ARRAY) {
                if (range->bidx) {
                    err = compile(state, range->bidx);
                    if (err != Err::OK) return err;
                } else {
                    pushOpcode(state, OC_PUSH_I64);
                    pushOperand(state, 0);
                }

                pushOpcode(state, OC_DUP);

                if (range->eidx) {
                    err = compile(state, range->eidx);
                    if (err != Err::OK) return err;
                } else {
                    compileLength(state, bex->left);
                }

                // [begin begin len]
                pushOpcode(state, OC_SWAP);
                pushOpcode(state, OC_SUB_I64);
                if (range->eidx) {
                    pushOpcode(state, OC_PUSH_I64);
                    pushOperand(state, 1);
                    pushOpcode(state, OC_ADD_I64);
                }
                pushOpcode(state, OC_SWAP);

                // [len begin]
                err = compile(state, bex->left);
                if (err != Err::OK) return err;

                pushOpcode(state, OC_SWAP);
            } else {
                err = compile(state, bex->left, NULL, (flags & IS_LVALUE) ? IS_LVALUE : 0);
                if (err != Err::OK) return err;

                if (range->bidx) {
                    err = compile(state, range->bidx);
                    if (err != Err::OK) return err;
                } else {
                    pushOpcode(state, OC_PUSH_I64);
                    pushOperand(state, 0);
                }
            }

            pushOpcode(state, OC_PTR_IDX);
            pushOperand(state, eType->size);

            if (flags & FORCE_ARRAY_LENGTH) {
                if (tType->base.kind == Type::DT_ARRAY) {
                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, tType->arr.elementCount);
                } else {
                    pushOpcode(state, OC_SWAP);
                }
            }

            return Err::OK;
        }

        if (lType->base.kind == Type::DT_ARRAY) {
            if (flags ^ IS_ROOT) {
                pushOpcode(state, OC_PUSH_U64);
                pushOperand(state, lType->arr.elementCount);
                pushOpcode(state, OC_VEC_ALLOC);
                pushOperand(state, eType->size);
            }

            // dest[i] = src[start + i * step]
            for (uint64_t i = 0; i < lType->arr.elementCount; i++) {
                pushOpcode(state, OC_DUP);

                int64_t startIdx = range->bidx ? range->bidx->value.i64 : 0;
                int64_t stepVal  = range->step->value.i64;
                int64_t srcIdx   = startIdx + (i * stepVal);

                pushOpcode(state, OC_SWAP);
                pushOpcode(state, OC_DUP);

                pushOpcode(state, OC_PUSH_I64);
                pushOperand(state, srcIdx);
                pushOpcode(state, OC_PTR_IDX);
                pushOperand(state, eType->size);

                pushLoadOpcode(state, eType);

                pushOpcode(state, OC_SWAP);

                pushOpcode(state, OC_PUSH_I64);
                pushOperand(state, i);
                pushOpcode(state, OC_PTR_IDX);
                pushOperand(state, eType->size);

                pushOpcode(state, OC_SWAP);

                pushStoreOpcode(state, eType);
            }

            pushOpcode(state, OC_SWAP);
            pushOpcode(state, OC_POP);

            if (flags & FORCE_ARRAY_LENGTH) {
                pushOpcode(state, OC_PUSH_U64);
                pushOperand(state, lType->arr.elementCount);
            }

            state->vecTmpMemUsed = true;
            return Err::OK;
        }

        // We end up here if we are runtime thing
        // So we just emulate loop

        if (flags & IS_ROOT) {
            // (end - start) / step + 1
            err = compile(state, range->eidx);
            if (err != Err::OK) return err;

            err = compile(state, range->bidx);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_SUB_I64);

            err = compile(state, range->step);
            if (err != Err::OK) return err;

            pushOpcode(state, OC_DIV_I64);
            pushOpcode(state, OC_PUSH_I64);
            pushOperand(state, 1);
            pushOpcode(state, OC_ADD_I64);

            pushOpcode(state, OC_DUP);

            pushOpcode(state, OC_VEC_ALLOC);
            pushOperand(state, eType->size);
        }

        uint64_t destOffset;
        pushAndRecordLocal(state, bex->left, Type::getInfo(Type::DT_U64), &destOffset);

        pushOpcode(state, OC_SET_PTR);
        pushOperand(state, destOffset);

        pushOpcode(state, OC_DUP_N);
        pushOperand(state, 2);

        // Step direction check (step < 0)
        compile(state, range->step);
        pushOpcode(state, OC_PUSH_I64);
        pushOperand(state, 0);
        pushOpcode(state, OC_LT_I64);
        JumpPatch negStepBranch = pushJumpPlaceholder(state, OC_JUMP_IF_TRUE);

        // Positive step (end >= idx)
        pushOpcode(state, OC_GE_I64);
        JumpPatch exitPos = pushJumpPlaceholder(state, OC_JUMP_IF_FALSE);
        JumpPatch enterBody = pushJumpPlaceholder(state, OC_JUMP);

        // Negative step (end <= idx)
        patchJumpToHere(state, negStepBranch);
        pushOpcode(state, OC_LE_I64);
        JumpPatch exitNeg = pushJumpPlaceholder(state, OC_JUMP_IF_FALSE);

        // stack: [end, idx]
        // dest[dest_idx] = src[src_idx]
        patchJumpToHere(state, enterBody);

        compile(state, bex->left, NULL, IS_LVALUE);
        pushOpcode(state, OC_DUP);
        pushOpcode(state, OC_PTR_IDX);
        pushOperand(state, eType->size);
        pushLoadOpcode(state, eType);

        pushOpcode(state, OC_DUP);
        pushOpcode(state, OC_GET_PTR);
        pushOperand(state, destOffset);
        pushOpcode(state, OC_SWAP);
        pushOpcode(state, OC_PTR_IDX);
        pushOperand(state, eType->size);

        // TODO:::!!!
        //pushStoreOpcode(state, eType);

        // idx++
        pushOpcode(state, OC_DUP);
        //pushOperand(state, destIdxOffset);
        //pushPushInteger(state, 1);
        pushOpcode(state, OC_ADD_I64);
        pushOpcode(state, OC_SET_U64);
        //pushOperand(state, destIdxOffset);

        // 5. Increment src_idx = src_idx + step:
        compile(state, range->step);
        pushOpcode(state, OC_ADD_I64);

        // Loop back-edge
        //pushJumpBack(state, loopHeaderOffset);

        patchJumpToHere(state, exitPos);
        patchJumpToHere(state, exitNeg);

        // Pop [end, src_idx] invariant from stack
        pushOpcode(state, OC_POP);
        pushOpcode(state, OC_POP); // Stack: [count]

        // =========================================================================
        // 4. Push Resulting Slice: [dest_ptr, count]
        // =========================================================================
        pushOpcode(state, OC_GET_PTR);
        //pushOperand(state, destPtrOffset); // Stack: [count, dest_ptr]
        pushOpcode(state, OC_SWAP);        // Stack: [dest_ptr, count]

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

    void pushVecOperands(CompilerState* state, Type::TypeInfo* target, VecDescriptor desc, const bool isRoot) {
        uint64_t dest;
        if (isRoot) {
            if (!target) {
                desc.flags |= DE_F_IS_DEST_STACK;
            } else {
                state->vecLhsWritten = true;
            }
            state->vecResult.isTmp = false;
        } else {
            desc.flags |= DE_F_DEST;
            state->vecResult.isTmp = true;
            state->vecTmpMemUsed = true;
        }
        desc.dstElemSize = Type::getElement(target)->size; // TODO: type size check

        pushDescriptor(state, desc);
    }

    inline bool tryVectorization(CompilerState* state, BinaryExpression* bex, VecResult lRes, VecResult rRes, Type::TypeInfo* target, const bool isRoot) {
        if (!Type::isArrayLike(bex->left->value.type->kind) &&
            !Type::isArrayLike(bex->right->value.type->kind)) {
            return false;
        }

        VecDescriptor desc = {
            .oper = bex->base.opType,
            .flags = (uint8_t) ((lRes.isTmp ? DE_F_LEFT : 0) |
                                (rRes.isTmp ? DE_F_RIGHT : 0))
        };

        Type::TypeInfoEx* leftInfo  = (Type::TypeInfoEx*) bex->left->value.type;
        Type::TypeInfoEx* rightInfo = (Type::TypeInfoEx*) bex->right->value.type;

        Type::TypeInfoEx* mainType;
        if (Type::isArrayLike(bex->left->value.type->kind) &&
            Type::isArrayLike(bex->right->value.type->kind)) {
            mainType = leftInfo;
            if (bex->base.opType == OP_CONCATENATION) {
                pushOpcode(state, OC_VEC_CAT);
            } else {
                pushOpcode(state, OC_VEC_VV);
            }
        } else if (Type::isArrayLike(bex->left->value.type)) {
            mainType = leftInfo;
            pushOpcode(state, OC_VEC_VS);
        } else {
            mainType = rightInfo;
            pushOpcode(state, OC_VEC_SV);
        }

        // TODO: think of just passign mainType alongside target
        desc.type = mainType->ptr.element->kind;
        pushVecOperands(state, isRoot ? target : &mainType->base, desc, isRoot);

        return true;
    }

    // Unary Version
    bool tryVectorization(CompilerState* state, UnaryExpression* uex, Type::TypeInfo* target, const bool isRoot) {
        if (!Type::isArrayLike(uex->operand->value.type->kind)) return false;

        pushOpcode(state, OC_VEC_UNARY);

        VecDescriptor desc = {
            .type = ((Type::ArrayInfo*) uex->operand->value.type)->element->kind,
            .oper = uex->base.opType,
            .flags = (uint8_t) (state->vecResult.isTmp ? DE_F_DEST : 0)
        };

        pushVecOperands(state, target, desc, isRoot);

        return true;
    }

    void pushSliceLength(CompilerState* state, Variable* var) {
        pushGetOpcode(state, var->value.type, var->def, 8);
    }

    Err::Err compileExpression(CompilerState* state, Variable* node, Type::TypeInfo* target, Flags flags) {
        Err::Err err;

        const bool isRoot = flags & IS_ROOT;
        flags &= ~IS_ROOT;

        const bool isLval = flags & IS_LVALUE;
        flags &= ~IS_LVALUE;

        state->vecResult.isTmp    = false;
        state->vecResult.isScalar = false;

        Expression* exp = node->expression;
        switch (exp->type) {

            case EXT_UNARY: {
                UnaryExpression* uex = (UnaryExpression*) exp;
                const bool areWeNothingburger = uex->base.opType == OP_NONE;

                if (areWeNothingburger && isRoot) flags |= IS_ROOT;
                if (areWeNothingburger && isLval) flags |= IS_LVALUE;

                if (uex->base.opType == OP_GET_ADDRESS) flags |= IS_LVALUE;

                err = compile(state, uex->operand, target, flags);
                if (err != Err::OK) return err;

                if (areWeNothingburger || uex->base.opType == OP_GET_ADDRESS) {
                    // NOTE: for now we assume, '&' cannot occure on left side and
                    //       cannot be used tiwce in a multiple times on right side
                    break;
                }

                if (isLval && uex->base.opType == OP_GET_VALUE) {
                    break;
                }

                if (flags & FORCE_VEC_OPCODES &&
                    tryVectorization(state, uex, target, isRoot)
                ) {
                    break;
                }

                pushOperator(state, uex);
                break;
            }

            case EXT_BINARY: {

                BinaryExpression* bex = (BinaryExpression*) exp;

                if (bex->base.opType == OP_BOOL_AND ||
                    bex->base.opType == OP_BOOL_OR) {
                    return compileShortCircuit(state, bex);
                }

                if (isMemberSelection(bex->base.opType)) {
                    return compileMemberSelection(state, bex, flags | (isLval ? IS_LVALUE : 0));
                }

                if (isRangeSlicing(bex->base.opType)) {
                    uint64_t newFlags = flags | (isRoot ? IS_ROOT : 0) | (isLval ? IS_LVALUE : 0);
                    return compileRangeSlicing(state, bex, node->value.type, newFlags);
                }

                err = compile(state, bex->left, target, flags);
                if (err != Err::OK) return err;
                VecResult lResult = state->vecResult;

                // TODO: why do we have this as binary with cast exp as left?
                //       unite under one thing...
                if (isCast(bex->base.opType)) {
                    Opcode op = selectCastOpcode(bex->left->value.type->kind, node->value.type->kind);
                    if (op == OC_NOP) break;

                    pushOpcode(state, op);
                    break;
                }

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

                if (bex->base.opType == OP_SUBSCRIPT) {
                    Type::PointerInfo* pInfo = (Type::PointerInfo*) bex->left->value.type;
                    Type::TypeInfo* eInfo = pInfo->element;

                    pushOpcode(state, OC_PTR_IDX);
                    pushOperand(state, eInfo->size);

                    if (!isLval) {
                        pushLoadOpcode(state, eInfo);
                    }

                    break;
                }

                // if either side is an array - switch to vec opcodes
                if (flags & FORCE_VEC_OPCODES &&
                    tryVectorization(state, bex, lResult, rResult, target, isRoot)
                ) {
                    break;
                }

                if (tryAsPointerArithmetic(state, bex)) {
                    break;
                }

                // TODO: To a function pushOperand
                Opcode oc = selectOperatorOpcode(bex);
                if (oc == OC_NOP) break;
                pushOpcode(state, oc);

                break;

            }

            case EXT_FUNCTION_CALL: {

                FunctionCall* call = (FunctionCall*) exp;
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

                // TODO: for now we push format-string descriptor like this...
                if (fcn->internalIdx == Ast::Internal::IF_PRINTF) {
                    Expression* ex = unwrapWithCasts(call->inArgs[0])->expression;
                    pushOpcode(state, OC_PUSH_PTR);
                    pushOperand(state, (uint64_t) ((StringInitialization*) ex)->format);
                }

                for (int i = 0; i < fixedCount; i++) {
                    Type::TypeInfo* callType = call->inArgs[i]->value.type;
                    Type::TypeInfo* fcnType = fcnInArgs[i]->var->value.type;

                    // TODO
                    const uint64_t flags =
                        (fcnType->kind == Type::DT_ARRAY ? FORCE_STACK_VALUE : 0) |
                        (fcnType->kind == Type::DT_SLICE ? FORCE_ARRAY_LENGTH : 0) |
                        (fcnType->kind == Type::DT_SLICE ? FORCE_ARRAY_LENGTH : 0) |
                        (callType->kind == Type::DT_SLICE ? FORCE_ARRAY_LENGTH : 0);

                    err = compile(state, call->inArgs[i], fcnType, flags);
                    if (err != Err::OK) return err;
                }

                bool anyVarargIsArray = false; // TODO: deprecate -> status->vecMemUsed shall be used instead
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
                Cast* cast = (Cast*) exp;

                Type::TypeInfo* sType = cast->operand->value.type;
                Type::TypeInfo* tType = cast->target;

                // Discard cast in case of same underlying types
                if (Type::areElementsTheSame(sType, tType)) {
                    compile(state, cast->operand, target, flags | (isRoot ? IS_ROOT : 0));
                    break;
                }

                compile(state, cast->operand, target, flags);

                if (Type::isArrayLike(tType)) {
                    if (cast->kind == Type::CK_FROM_LOWER_LEVEL) {
                        Opcode op = selectCastOpcode(sType->kind, ((Type::PointerInfo*) tType)->element->kind);
                        if (op == OC_NOP) break;

                        pushOpcode(state, op);
                        break;
                    }

                    pushOpcode(state, OC_VEC_CAST);

                    VecDescriptor desc;
                    desc.type = ((Type::PointerInfo*) tType)->element->kind;
                    desc.flags = isRoot && flags ^ IS_BARE_STATEMENT ?
                        0 : DE_F_DEST | DE_F_LEFT;

                    if (cast->kind == Type::CK_FROM_LOWER_LEVEL) {
                        desc.srcType = Type::getUnderlyingKind(sType);
                        desc.flags = 0; // TODO
                    } else {
                        desc.srcType = ((Type::TypeInfoEx*) (sType))->ptr.element->kind;
                        desc.flags = 0;
                    }

                    // even in root target can be general expression,
                    // ex. assignment x.y[i] = arr ...;
                    pushVecOperands(state, cast->target, desc, isRoot && flags ^ IS_BARE_STATEMENT);
                } else {
                    Opcode op = selectCastOpcode(sType->kind, tType->kind);
                    if (op == OC_NOP) break;

                    pushOpcode(state, op);

                    if (isRoot) state->vecResult.isScalar = true;
                }

                break;
            }

            case EXT_ALLOC: {
                Alloc* alc = (Alloc*) exp;

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
                compileExpression(state, alc->def->var, alc->def->var->value.type);

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
                GetLength* ex = (GetLength*) exp;

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
                GetSize* ex = (GetSize*) exp;

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
                StringInitialization* init = (StringInitialization*) exp;

                pushString(state, init);
                if (isRoot) {
                    pushOpcode(state, OC_VEC_COPY);
                    VecDescriptor desc = {
                        .type = init->charType->kind,
                    };

                    pushVecOperands(state, target, desc, isRoot);
                }

                break;
            }

            case EXT_ARRAY_INITIALIZATION: {
                ArrayInitialization* init = (ArrayInitialization*) exp;

                Type::ArrayInfo* aType = (Type::ArrayInfo*) node->value.type;
                Type::TypeInfo*  eType = (Type::TypeInfo*) aType->element;

                if (init->flags & IS_CMP_TIME) {
                    const uint64_t startOffset = state->rawData.logicalPos;
                    uint8_t* rawDataPtr = (uint8_t*) Arena::push(&state->rawData, aType->base.size, 1);

                    int64_t offset = 0;
                    for (int i = 0; i < aType->elementCount; i++) {
                        Variable* arg = init->attributes[i];
                        // TODO: we need a way to either pass and ast or create alternate diagnostics
                        return Err::NOT_YET_IMPLEMENTED;
                        // Extern::Abi::marshal(state->ast, arg->value.type, arg, rawDataPtr + offset, Extern::Abi::MarshalMode::TYPE_DEFAULT);
                        offset += arg->value.type->size;
                    }

                    pushOpcode(state, OC_LEA_CONST);
                    pushOperand(state, startOffset);
                    state->vecResult.isTmp = false;

                    if (flags & FORCE_ARRAY_LENGTH) {
                        pushOpcode(state, OC_PUSH_U64);
                        pushOperand(state, aType->elementCount);
                    }

                    pushOpcode(state, OC_VEC_COPY);
                    VecDescriptor desc = {
                        .type = eType->kind,
                    };

                    pushVecOperands(state, target, desc, isRoot);
                } else {
                    // NOTE: in case of root, pointer should already be on stack
                    if (!isRoot) {
                        pushOpcode(state, OC_PUSH_U64);
                        pushOperand(state, aType->elementCount);

                        pushOpcode(state, OC_VEC_ALLOC);
                        pushOperand(state, eType->size);

                        state->vecResult.isTmp = true;
                    }

                    for (int i = 0; i < aType->elementCount; i++) {
                        Variable* arg = init->attributes[i];

                        pushOpcode(state, OC_DUP);
                        pushOpcode(state, OC_PUSH_U64);
                        pushOperand(state, i);
                        pushOpcode(state, OC_PTR_IDX);
                        pushOperand(state, eType->size);

                        compile(state, arg);

                        Opcode op = selectStoreOpcode(eType->kind);
                        pushOpcode(state, op);
                    }

                    if (isRoot && target->kind == Type::DT_ARRAY) {
                        // If we assign to root array we have to leave
                        // 'empty' stack
                        pushOpcode(state, OC_POP);
                    }else {
                        pushOpcode(state, OC_PUSH_U64);
                        pushOperand(state, aType->elementCount);
                    }
                }

                break;
            }

            case EXT_TYPE_INITIALIZATION: {
                TypeInitialization* init = (TypeInitialization*) exp;
                Type::StructInfo* type = (Type::StructInfo*) node->value.type;

                if (!isRoot) {
                    return Err::UNEXPECTED_ERROR;
                }

                for (int i = 0; i < init->attributeCount; i++) {
                    Variable* arg = init->attributes[i];
                    Type::StructMemberInfo* mType = type->members + i;

                    pushOpcode(state, OC_DUP);
                    pushOpcode(state, OC_PUSH_U64);
                    pushOperand(state, mType->offset);
                    pushOpcode(state, OC_PTR_IDX);
                    pushOperand(state, 1);

                    compile(state, arg, NULL, IS_ROOT);
                    // TODO
                    if (Type::isStructLike(mType->type)) {
                        // pushOpcode(state, OC_POP);
                        continue;
                    }

                    Opcode op = selectStoreOpcode(mType->type->kind);
                    pushOpcode(state, op);
                }

                if (isRoot) {
                    pushOpcode(state, OC_POP);
                }

                // TODO : fill var

                break;
            }

            case EXT_RANGE: {
                RangeExpression* range = (RangeExpression*) exp;
                Type::PointerInfo* type = (Type::PointerInfo*) node->value.type;

                if (!range->step) {
                    compile(state, range->bidx);
                    pushOpcode(state, OC_PTR_IDX);
                    pushOperand(state, type->element->size);
                } else {
                    return Err::NOT_YET_IMPLEMENTED;
                }
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

    // TODO: apply new patch functions
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

    Err::Err compile(CompilerState* state, SwitchCase* node) {
        Err::Err err;

        const int typeOffset = getDtypeOffsetSizeBased(node->switchExp->value.type->kind);

        if (node->strategy == CaseStrategy::CS_LINEAR) {
            uint64_t patchListHead = patchListHeadNull;

            err = compile(state, node->switchExp);
            if (err != Err::OK) return err;

            for (uint32_t i = 0; i < node->caseCount; i++) {
                pushOpcode(state, OC_DUP);

                err = compile(state, node->casesExp[i]);
                if (err != Err::OK) return err;

                pushOpcode(state, (Opcode) (OC_EQ_I32 + typeOffset));

                JumpPatch endJumpPatch = pushJumpPlaceholder(state, OC_JUMP_IF_FALSE);
                pushOpcode(state, OC_POP);

                err = compile(state, node->cases[i]);
                if (err != Err::OK) return err;

                patchListHead = addJumpToList(state, patchListHead);
                patchJumpToHere(state, endJumpPatch);
            }

            if (node->caseCount > 0) {
                pushOpcode(state, OC_POP);
            }

            if (node->elseCase) {
                err = compile(state, node->elseCase);
                if (err != Err::OK) return err;
            }

            const uint64_t jumpVeryEndOffset = state->bytecode.logicalPos;
            patchList(state, patchListHead, jumpVeryEndOffset);

            return Err::OK;
        }

        if (node->strategy == CaseStrategy::CS_BINARY) {
            // TODO
            return Err::NOT_YET_IMPLEMENTED;
        }

        if (node->strategy == CaseStrategy::CS_JUMP_TABLE) {
            const int64_t minValue = node->sorted[0].val;
            const int64_t maxValue = node->sorted[node->sortedCount - 1].val;

            err = compile(state, node->switchExp);
            if (err != Err::OK) return err;

            const uint64_t jumpTableBaseOffset = state->bytecode.logicalPos;

            pushOpcode(state, OC_JUMP_TABLE);
            pushOperand(state, (uint64_t) minValue);
            pushOperand(state, (uint64_t) maxValue);

            const uint64_t tableCount  = (uint64_t) (maxValue - minValue) + 1;
            const uint64_t tableOffset = state->bytecode.logicalPos;

            for (uint32_t i = 0; i < tableCount; i++) {
                pushOperand(state, 0);
            }
            pushOperand(state, 0); // Either else case or return addr

            uint64_t patchListHead = patchListHeadNull;

            for (uint32_t i = 0; i < node->sortedCount; i++) {
                SwitchCase::Sorted entry = node->sorted[i];

                const uint64_t tableIndex    = entry.val - minValue;
                const uint64_t operandOffset = tableOffset + sizeof(vmword) * tableIndex;

                const int64_t relativeDelta = (int64_t) state->bytecode.logicalPos - (int64_t) jumpTableBaseOffset;
                patchOperand(state, operandOffset, (uint64_t) relativeDelta);

                err = compile(state, node->cases[entry.idx]);
                if (err != Err::OK) return err;

                patchListHead = addJumpToList(state, patchListHead);
            }

            const uint64_t defaultOffset = state->bytecode.logicalPos;
            const int64_t defaultRelativeDelta = (int64_t) defaultOffset - (int64_t) jumpTableBaseOffset;

            for (uint32_t i = 0; i < tableCount; i++) {
                patchOperandIfZero(state, tableOffset + i * sizeof(vmword), (uint64_t) defaultRelativeDelta);
            }

            patchOperand(state, tableOffset + tableCount * sizeof(vmword), (uint64_t) defaultRelativeDelta);

            if (node->elseCase) {
                err = compile(state, node->elseCase);
                if (err != Err::OK) return err;
            }

            const uint64_t jumpVeryEndOffset = state->bytecode.logicalPos;
            patchList(state, patchListHead, jumpVeryEndOffset);

            return Err::OK;
        }

        return Err::UNEXPECTED_ERROR;
    }

    Variable* getLoopIndexVariable(Loop* node) {
        if (!node->index.var) return NULL;
        return node->index.var->base.type == NT_VARIABLE ?
            node->index.var : node->index.def->var;
    }

    void emitInitIndex(CompilerState* state, Loop* node, Variable* exp) {
        Variable* index = getLoopIndexVariable(node);
        if (index) {
            if (node->index.var->base.type == NT_VARIABLE_DEFINITION) {
                compile(state, node->index.def);
            }

            if (exp) {
                compile(state, exp);
            } else {
                pushOpcode(state, OC_PUSH_I64);
                pushOperand(state, 0);
            }

            pushSetOpcode(state, index->value.type, index->def, 0);
            pushGetOpcode(state, index->value.type, index->def, 0);
        } else {
            // Implicit stack index
            if (exp) {
                compile(state, exp);
            } else {
                pushOpcode(state, OC_PUSH_I64);
                pushOperand(state, 0);
            }
        }
    }

    void emitUpdateIndex(CompilerState* state, Loop* node, Variable* stepExp) {
        Variable* index = getLoopIndexVariable(node);
        if (index) {
            // Discard old loop comparison copy
            pushOpcode(state, OC_POP);

            pushGetOpcode(state, index->value.type, index->def, 0);
            if (stepExp) {
                compile(state, stepExp);
            } else {
                pushOpcode(state, OC_PUSH_I64);
                pushOperand(state, 1);
            }

            pushOpcode(state, OC_ADD_I64);
            pushSetOpcode(state, index->value.type, index->def, 0);
            pushGetOpcode(state, index->value.type, index->def, 0);
        } else {
            // Update implicit index on stack
            if (stepExp) {
                compile(state, stepExp);
            } else {
                pushOpcode(state, OC_PUSH_I64);
                pushOperand(state, 1);
            }
            pushOpcode(state, OC_ADD_I64);
        }
    }

    // Condition Loop: 'loop <condition> as condition_value at iteration_count { ... }'
    Err::Err compileConditionLoop(CompilerState* state, Loop* node, uint64_t* outContinue, uint64_t* outBreak) {
        Variable* index = getLoopIndexVariable(node);
        if (index) {
            emitInitIndex(state, node, nullptr);
        }

        const uint64_t loopHeaderOffset = state->bytecode.logicalPos;

        Err::Err err = compile(state, node->arg.exp);
        if (err != Err::OK) return err;

        if (node->item) {
            compile(state, node->item);

            pushOpcode(state, OC_DUP);
            pushOpcode(state, OC_SET_I64);
            pushOperand(state, node->item->vmOffset);

            if (node->item->type.qualifier & Type::Q_REF) {
                // TODO: I guess we forbid this
            }
        }

        JumpPatch exitJump = pushJumpPlaceholder(state, OC_JUMP_IF_FALSE);

        err = compile(state, node->bodyScope);
        if (err != Err::OK) return err;

        *outContinue = state->bytecode.logicalPos;

        if (index) {
            emitUpdateIndex(state, node, NULL);
        }

        pushJumpBack(state, loopHeaderOffset);

        patchJumpToHere(state, exitJump);
        *outBreak = state->bytecode.logicalPos;

        return Err::OK;
    }

    // TODO: compile-time ranges optimization
    // Range Loop: 'loop <start:step:end> as item at index { ... }'
    static Err::Err compileRangeLoop(CompilerState* state, Loop* node, uint64_t* outContinue, uint64_t* outBreak) {
        RangeExpression* range = node->arg.range;

        compile(state, range->eidx);
        emitInitIndex(state, node, range->bidx);
        // On stack: [end, start]

        const uint64_t loopHeaderOffset = state->bytecode.logicalPos;

        pushOpcode(state, OC_DUP_N);
        pushOperand(state, 2);

        if (node->item) {
            // Reference is forbidden by the validator
            compile(state, node->item);

            pushOpcode(state, OC_DUP);
            pushOpcode(state, OC_SET_I64);
            pushOperand(state, node->item->vmOffset);
        }

        // Step direction decider
        if (range->step) {
            compile(state, range->step);
        } else {
            pushOpcode(state, OC_PUSH_I64);
            pushOperand(state, 1);
        }

        pushOpcode(state, OC_PUSH_I64);
        pushOperand(state, 0);
        // On stack: [end, start, end, start, step, 0]

        pushOpcode(state, OC_LT_I64);
        JumpPatch negStepBranch = pushJumpPlaceholder(state, OC_JUMP_IF_TRUE);

        // Positive check (end >= index)
        pushOpcode(state, OC_GE_I64);
        JumpPatch exitPos = pushJumpPlaceholder(state, OC_JUMP_IF_FALSE);
        JumpPatch enterBody = pushJumpPlaceholder(state, OC_JUMP);

        // Negative check (end <= index)
        patchJumpToHere(state, negStepBranch);
        pushOpcode(state, OC_LE_I64);
        JumpPatch exitNeg = pushJumpPlaceholder(state, OC_JUMP_IF_FALSE);

        patchJumpToHere(state, enterBody);

        Err::Err err = compile(state, node->bodyScope);
        if (err != Err::OK) return err;

        *outContinue = state->bytecode.logicalPos;

        emitUpdateIndex(state, node, range->step);
        pushJumpBack(state, loopHeaderOffset);

        patchJumpToHere(state, exitPos);
        patchJumpToHere(state, exitNeg);
        *outBreak = state->bytecode.logicalPos;

        // Clean up [end, index]
        pushOpcode(state, OC_POP);
        pushOpcode(state, OC_POP);

        return Err::OK;
    }

    // Array/Slice: 'loop arr as item at index { ... }'
    Err::Err compileArrayLoop(CompilerState* state, Loop* node, uint64_t* outContinue, uint64_t* outBreak) {
        compile(state, node->arg.exp, NULL, FORCE_ARRAY_LENGTH);
        pushOpcode(state, OC_SWAP);

        uint64_t arrayOffset;
        if (node->item) {
            compile(state, node->item);

            pushAndRecordLocal(state, node->item->var, Type::getInfo(Type::DT_U64), &arrayOffset);
            if (!isOffsetValid(arrayOffset)) {
                return Err::COMPILE_TIME_KNOWN_EXPRESSION_REQUIRED;
            }

            pushOpcode(state, OC_SET_PTR);
            pushOperand(state, arrayOffset);
        } else {
            pushOpcode(state, OC_POP);
        }

        emitInitIndex(state, node, nullptr);
        // Stack [len, 0]

        const uint64_t loopHeaderOffset = state->bytecode.logicalPos;

        pushOpcode(state, OC_DUP_N);
        pushOperand(state, 2);

        // len > index
        pushOpcode(state, OC_GT_I64);
        JumpPatch exitJump = pushJumpPlaceholder(state, OC_JUMP_IF_FALSE);

        if (node->item) {
            Type::TypeInfo* type = node->item->var->value.type;

            Variable* index = getLoopIndexVariable(node);
            if (index) {
                pushOpcode(state, OC_GET_PTR);
                pushOperand(state, arrayOffset);
                pushGetOpcode(state, index->value.type, index->def, 0);
            } else {
                pushOpcode(state, OC_DUP);
                pushOpcode(state, OC_GET_PTR);
                pushOperand(state, arrayOffset);
                pushOpcode(state, OC_SWAP);
            }

            pushOpcode(state, OC_PTR_IDX);
            if (node->item->type.qualifier & Type::Q_REF) {
                pushOperand(state, ((Type::PointerInfo*) type)->element->size);
                pushSetOpcode(state, type, node->item, 0);
            } else {
                pushOperand(state, type->size);
                pushLoadOpcode(state, type);
                pushSetOpcode(state, type, node->item, 0);
            }
        }

        Err::Err err = compile(state, node->bodyScope);
        if (err != Err::OK) return err;

        *outContinue = state->bytecode.logicalPos;

        emitUpdateIndex(state, node, nullptr);
        pushJumpBack(state, loopHeaderOffset);

        patchJumpToHere(state, exitJump);
        *outBreak = state->bytecode.logicalPos;

        // Clean up [len, index]
        pushOpcode(state, OC_POP);
        pushOpcode(state, OC_POP);

        return Err::OK;
    }

    Err::Err compile(CompilerState* state, Loop* node) {
        Err::Err err;

        Span tmpSpan = *node->base.span;
        tmpSpan.end = node->bodyScope->base.span->start;
        updateSourceLocation(state, &tmpSpan);

        SyntaxNode* prevLoop             = state->currentLoop;
        uint64_t    prevLoopAddress      = state->currentLoopAddress;
        uint64_t    prevListHeadBreak    = state->listHeadBreak;
        uint64_t    prevListHeadContinue = state->listHeadContinue;

        state->currentLoop = (SyntaxNode*) node;
        state->currentLoopAddress = state->bytecode.logicalPos;
        state->listHeadBreak = patchListHeadNull;
        state->listHeadContinue = patchListHeadNull;

        uint64_t continueOffset = 0;
        uint64_t breakOffset    = 0;

        switch (node->arg.kind) {
            case Loop::Arg::CONDITION:
                err = compileConditionLoop(state, node, &continueOffset, &breakOffset);
                break;
            case Loop::Arg::RANGE:
                err = compileRangeLoop(state, node, &continueOffset, &breakOffset);
                break;
            default:
                err = compileArrayLoop(state, node, &continueOffset, &breakOffset);
                break;
        }

        if (err == Err::OK) {
            patchList(state, state->listHeadContinue, continueOffset);
            patchList(state, state->listHeadBreak, breakOffset);
        }

        state->currentLoop        = prevLoop;
        state->currentLoopAddress = prevLoopAddress;
        state->listHeadBreak      = prevListHeadBreak;
        state->listHeadContinue   = prevListHeadContinue;

        return err;
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

        // TODO: for empty loop
        // if (state->currentLoop->type == NT_LOOP) {
        //     pushOperand(state, state->currentLoopAddress - state->bytecode.logicalPos + 1);
        // } else {
        // }
        pushOperand(state, state->listHeadContinue);
        state->listHeadContinue = state->bytecode.logicalPos - 8;

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
        compile(state, node->operand, NULL, IS_BARE_STATEMENT);
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
