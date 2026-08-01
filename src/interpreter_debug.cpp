// interpreter related code that provide
// some helper function to get around
// while debuging or any other stuff when
// its good to get some info and hang out
// with metadata closer

#include "array_list.h"
#include "data_types.h"
#include "globals.h"
#include "interpreter.h"
#include "io.h"
#include "ordered_dict.h"
#include "syntax.h"
#include "set.h"
#include "utils.h"
#include "logger.h"
#include "ansi_colors.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <emmintrin.h>
#include <stdio.h>



namespace Interpreter {

    thread_local IO::Stream* stream = NULL;

    Set::Container functionsSet;
    DArray::Container functionsArray;

    void initDebug(CompilerState* state) {
        Set::init(&functionsSet, 256);
        DArray::init(&functionsArray, 128, sizeof(Function*));
    }

    // lets assume designated initialization was used ;)
    constexpr uint64_t getOpcodeSize(Opcode op) {
        switch (op) {
            case OC_PUSH_I8:    return 2;
            case OC_PUSH_U8:    return 2;
            case OC_PUSH_I16:   return 3;
            case OC_PUSH_U16:   return 3;
            case OC_PUSH_I32:   return 5;
            case OC_PUSH_U32:   return 5;
            case OC_PUSH_I64:   return 9;
            case OC_PUSH_U64:   return 9;
            case OC_PUSH_F32:   return 5;
            case OC_PUSH_F64:   return 9;
            case OC_PUSH_PTR:   return 9;
            case OC_PUSH_BLOB:  return 9;

            case OC_SET_I8:     return 9;
            case OC_SET_U8:     return 9;
            case OC_SET_I16:    return 9;
            case OC_SET_U16:    return 9;
            case OC_SET_I32:    return 9;
            case OC_SET_U32:    return 9;
            case OC_SET_I64:    return 9;
            case OC_SET_U64:    return 9;
            case OC_SET_F32:    return 9;
            case OC_SET_F64:    return 9;
            case OC_SET_PTR:    return 9;
            case OC_SET_BLOB:   return 9;

            case OC_GET_I8:
            case OC_GET_U8:
            case OC_GET_I16:
            case OC_GET_U16:
            case OC_GET_I32:
            case OC_GET_U32:
            case OC_GET_I64:
            case OC_GET_U64:
            case OC_GET_F32:
            case OC_GET_F64:
            case OC_GET_PTR:
            case OC_GET_BLOB:   return 9;

            case OC_LOAD_I8:
            case OC_LOAD_U8:
            case OC_LOAD_I16:
            case OC_LOAD_U16:
            case OC_LOAD_I32:
            case OC_LOAD_U32:
            case OC_LOAD_I64:
            case OC_LOAD_U64:
            case OC_LOAD_F32:
            case OC_LOAD_F64:
            case OC_LOAD_PTR:
            case OC_LOAD_BLOB:  return 1;

            case OC_STORE_I8:
            case OC_STORE_U8:
            case OC_STORE_I16:
            case OC_STORE_U16:
            case OC_STORE_I32:
            case OC_STORE_U32:
            case OC_STORE_I64:
            case OC_STORE_U64:
            case OC_STORE_F32:
            case OC_STORE_F64:
            case OC_STORE_PTR:
            case OC_STORE_BLOB: return 1;

            case OC_LEA:
            case OC_LEA_CONST: return (1 + 8);

            case OC_STORE_INDEXED:
            case OC_STORE_INDEXED_TMP: return (1 + 2 * 8);

            case OC_PTR_IDX: return 9;
            case OC_DEREF:   return 1;

            case OC_POP:    return 1;
            case OC_POP_N:  return 9;
            case OC_DUP:    return 1;
            case OC_CROP:   return 3 * 8 + 1;

            case OC_ADD_I32: return 1;
            case OC_ADD_U32: return 1;
            case OC_ADD_I64: return 1;
            case OC_ADD_U64: return 1;
            case OC_ADD_F32: return 1;
            case OC_ADD_F64: return 1;

            case OC_SUB_I32: return 1;
            case OC_SUB_U32: return 1;
            case OC_SUB_I64: return 1;
            case OC_SUB_U64: return 1;
            case OC_SUB_F32: return 1;
            case OC_SUB_F64: return 1;

            case OC_MUL_I32: return 1;
            case OC_MUL_U32: return 1;
            case OC_MUL_I64: return 1;
            case OC_MUL_U64: return 1;
            case OC_MUL_F32: return 1;
            case OC_MUL_F64: return 1;

            case OC_DIV_I32: return 1;
            case OC_DIV_U32: return 1;
            case OC_DIV_I64: return 1;
            case OC_DIV_U64: return 1;
            case OC_DIV_F32: return 1;
            case OC_DIV_F64: return 1;

            case OC_MOD_I32: return 1;
            case OC_MOD_U32: return 1;
            case OC_MOD_I64: return 1;
            case OC_MOD_U64: return 1;

            case OC_AND_I32: return 1;
            case OC_AND_U32: return 1;
            case OC_AND_I64: return 1;
            case OC_AND_U64: return 1;

            case OC_OR_I32: return 1;
            case OC_OR_U32: return 1;
            case OC_OR_I64: return 1;
            case OC_OR_U64: return 1;

            case OC_XOR_I32: return 1;
            case OC_XOR_U32: return 1;
            case OC_XOR_I64: return 1;
            case OC_XOR_U64: return 1;

            case OC_SHL_I32: return 1;
            case OC_SHL_U32: return 1;
            case OC_SHL_I64: return 1;
            case OC_SHL_U64: return 1;

            case OC_SHR_I32: return 1;
            case OC_SHR_U32: return 1;
            case OC_SHR_I64: return 1;
            case OC_SHR_U64: return 1;

            case OC_EQ_I32: return 1;
            case OC_EQ_I64: return 1;
            case OC_EQ_F32: return 1;
            case OC_EQ_F64: return 1;

            case OC_NE_I32: return 1;
            case OC_NE_I64: return 1;
            case OC_NE_F32: return 1;
            case OC_NE_F64: return 1;

            case OC_LT_I32: return 1;
            case OC_LT_U32: return 1;
            case OC_LT_I64: return 1;
            case OC_LT_U64: return 1;
            case OC_LT_F32: return 1;
            case OC_LT_F64: return 1;

            case OC_LE_I32: return 1;
            case OC_LE_U32: return 1;
            case OC_LE_I64: return 1;
            case OC_LE_U64: return 1;
            case OC_LE_F32: return 1;
            case OC_LE_F64: return 1;

            case OC_GT_I32: return 1;
            case OC_GT_U32: return 1;
            case OC_GT_I64: return 1;
            case OC_GT_U64: return 1;
            case OC_GT_F32: return 1;
            case OC_GT_F64: return 1;

            case OC_GE_I32: return 1;
            case OC_GE_U32: return 1;
            case OC_GE_I64: return 1;
            case OC_GE_U64: return 1;
            case OC_GE_F32: return 1;
            case OC_GE_F64: return 1;

            case OC_NOT_BOOL: return 1;
            case OC_BOOL_I32: return 1;
            case OC_BOOL_I64: return 1;
            case OC_BOOL_F32: return 1;
            case OC_BOOL_F64: return 1;

            case OC_SEXT_32_TO_64: return 1;
            case OC_ZEXT_32_TO_64: return 1;
            case OC_TRUNC_64_TO_32: return 1;

            case OC_CAST_I32_TO_U32: return 1;
            case OC_CAST_I32_TO_F32: return 1;
            case OC_CAST_I32_TO_F64: return 1;
            case OC_CAST_U32_TO_I32: return 1;
            case OC_CAST_U32_TO_F32: return 1;
            case OC_CAST_U32_TO_F64: return 1;
            case OC_CAST_I64_TO_U64: return 1;
            case OC_CAST_I64_TO_F32: return 1;
            case OC_CAST_I64_TO_F64: return 1;
            case OC_CAST_U64_TO_I64: return 1;
            case OC_CAST_U64_TO_F32: return 1;
            case OC_CAST_U64_TO_F64: return 1;
            case OC_CAST_F32_TO_I32: return 1;
            case OC_CAST_F32_TO_U32: return 1;
            case OC_CAST_F32_TO_I64: return 1;
            case OC_CAST_F32_TO_U64: return 1;
            case OC_CAST_F32_TO_F64: return 1;
            case OC_CAST_F64_TO_I32: return 1;
            case OC_CAST_F64_TO_U32: return 1;
            case OC_CAST_F64_TO_I64: return 1;
            case OC_CAST_F64_TO_U64: return 1;
            case OC_CAST_F64_TO_F32: return 1;

            case OC_JUMP:           return 9;
            case OC_JUMP_IF_TRUE:   return 9;
            case OC_JUMP_IF_FALSE:  return 9;

            case OC_GROW:   return (1 + 8);

            case OC_CALL:   return (1 + 8 + 8);
            case OC_RET:    return 1;

            case OC_NOP:    return 1;
            case OC_HALT:   return 1;

            case OC_VEC_VV:
            case OC_VEC_VS:
            case OC_VEC_SV:
            case OC_VEC_UNARY:
            case OC_VEC_CAT:
            case OC_VEC_COPY:
            case OC_VEC_FILL:
            case OC_VEC_STORE_INDIRECT:
            case OC_VEC_LOAD_INDIRECT: return (1 + 8 * 2);
            case OC_VEC_CAST: return (1 + 8 * 2);
            case OC_VEC_ALLOC: return (1 + 8);
            case OC_VEC_TO_REF: return 1;
            case OC_VEC_MEM_RESET: return 1;
            case OC_VEC_RESET: return 1;

            default: return 0;
        }
    }

    const char* toStr(Opcode opcode) {

        switch (opcode) {

            case OC_PUSH_I8:    return "push_i8";
            case OC_PUSH_U8:    return "push_u8";
            case OC_PUSH_I16:   return "push_i16";
            case OC_PUSH_U16:   return "push_u16";
            case OC_PUSH_I32:   return "push_i32";
            case OC_PUSH_U32:   return "push_u32";
            case OC_PUSH_I64:   return "push_i64";
            case OC_PUSH_U64:   return "push_u64";
            case OC_PUSH_F32:   return "push_f32";
            case OC_PUSH_F64:   return "push_f64";
            case OC_PUSH_PTR:   return "push_ptr";
            case OC_PUSH_BLOB: return "push_const";

            case OC_SET_I8:    return "set_i8";
            case OC_SET_U8:    return "set_u8";
            case OC_SET_I16:   return "set_i16";
            case OC_SET_U16:   return "set_u16";
            case OC_SET_I32:   return "set_i32";
            case OC_SET_U32:   return "set_u32";
            case OC_SET_I64:   return "set_i64";
            case OC_SET_U64:   return "set_u64";
            case OC_SET_F32:   return "set_f32";
            case OC_SET_F64:   return "set_f64";
            case OC_SET_PTR:   return "set_ptr";
            case OC_SET_BLOB: return "set_local";

            case OC_GET_I8:    return "get_i8";
            case OC_GET_U8:    return "get_u8";
            case OC_GET_I16:   return "get_i16";
            case OC_GET_U16:   return "get_u16";
            case OC_GET_I32:   return "get_i32";
            case OC_GET_U32:   return "get_u32";
            case OC_GET_I64:   return "get_i64";
            case OC_GET_U64:   return "get_u64";
            case OC_GET_F32:   return "get_f32";
            case OC_GET_F64:   return "get_f64";
            case OC_GET_PTR:   return "get_ptr";
            case OC_GET_BLOB: return "get_local";

            case OC_LEA:       return "lea";
            case OC_LEA_CONST: return "lea_const";

            case OC_STORE_INDEXED:     return "store_indexed";
            case OC_STORE_INDEXED_TMP: return "store_indexed";

            case OC_PTR_IDX: return "ptr_idx";

            case OC_LOAD_I8:   return "load_i8";
            case OC_LOAD_U8:   return "load_u8";
            case OC_LOAD_I16:  return "load_i16";
            case OC_LOAD_U16:  return "load_u16";
            case OC_LOAD_I32:  return "load_i32";
            case OC_LOAD_U32:  return "load_u32";
            case OC_LOAD_I64:  return "load_i64";
            case OC_LOAD_U64:  return "load_u64";
            case OC_LOAD_F32:  return "load_f32";
            case OC_LOAD_F64:  return "load_f64";
            case OC_LOAD_PTR:  return "load_ptr";
            case OC_LOAD_BLOB: return "load_blob";

            case OC_STORE_I8:   return "store_i8";
            case OC_STORE_U8:   return "store_u8";
            case OC_STORE_I16:  return "store_i16";
            case OC_STORE_U16:  return "store_u16";
            case OC_STORE_I32:  return "store_i32";
            case OC_STORE_U32:  return "store_u32";
            case OC_STORE_I64:  return "store_i64";
            case OC_STORE_U64:  return "store_u64";
            case OC_STORE_F32:  return "store_f32";
            case OC_STORE_F64:  return "store_f64";
            case OC_STORE_PTR:  return "store_ptr";
            case OC_STORE_BLOB: return "store_blob";

            case OC_ADD_I32: return "add_i32";
            case OC_ADD_U32: return "add_u32";
            case OC_ADD_I64: return "add_i64";
            case OC_ADD_U64: return "add_u64";
            case OC_ADD_F32: return "add_f32";
            case OC_ADD_F64: return "add_f64";

            case OC_SUB_I32: return "sub_i32";
            case OC_SUB_U32: return "sub_u32";
            case OC_SUB_I64: return "sub_i64";
            case OC_SUB_U64: return "sub_u64";
            case OC_SUB_F32: return "sub_f32";
            case OC_SUB_F64: return "sub_f64";

            case OC_MUL_I32: return "mul_i32";
            case OC_MUL_U32: return "mul_u32";
            case OC_MUL_I64: return "mul_i64";
            case OC_MUL_U64: return "mul_u64";
            case OC_MUL_F32: return "mul_f32";
            case OC_MUL_F64: return "mul_f64";

            case OC_DIV_I32: return "div_i32";
            case OC_DIV_U32: return "div_u32";
            case OC_DIV_I64: return "div_i64";
            case OC_DIV_U64: return "div_u64";
            case OC_DIV_F32: return "div_f32";
            case OC_DIV_F64: return "div_f64";

            case OC_AND_I32: return "and_i32";
            case OC_AND_U32: return "and_u32";
            case OC_AND_I64: return "and_i64";
            case OC_AND_U64: return "and_u64";

            case OC_OR_I32: return "or_i32";
            case OC_OR_U32: return "or_u32";
            case OC_OR_I64: return "or_i64";
            case OC_OR_U64: return "or_u64";

            case OC_XOR_I32: return "xor_i32";
            case OC_XOR_U32: return "xor_u32";
            case OC_XOR_I64: return "xor_i64";
            case OC_XOR_U64: return "xor_u64";

            case OC_SHL_I32: return "shl_i32";
            case OC_SHL_U32: return "shl_u32";
            case OC_SHL_I64: return "shl_i64";
            case OC_SHL_U64: return "shl_u64";

            case OC_SHR_I32: return "shr_i32";
            case OC_SHR_U32: return "shr_u32";
            case OC_SHR_I64: return "shr_i64";
            case OC_SHR_U64: return "shr_u64";

            case OC_LT_I32: return "lt_i32";
            case OC_LT_U32: return "lt_u32";
            case OC_LT_I64: return "lt_i64";
            case OC_LT_U64: return "lt_u64";
            case OC_LT_F32: return "lt_f32";
            case OC_LT_F64: return "lt_f64";

            case OC_GT_I32: return "gt_i32";
            case OC_GT_U32: return "gt_u32";
            case OC_GT_I64: return "gt_i64";
            case OC_GT_U64: return "gt_u64";
            case OC_GT_F32: return "gt_f32";
            case OC_GT_F64: return "gt_f64";

            case OC_NOT_BOOL: return "not_bool";

            case OC_BOOL_I32: return "bool_cast_i32";
            case OC_BOOL_I64: return "bool_cast_i64";
            case OC_BOOL_F32: return "bool_cast_f32";
            case OC_BOOL_F64: return "bool_cast_f64";

            case OC_SEXT_32_TO_64: return "sext_32_to_64";
            case OC_ZEXT_32_TO_64: return "zext_32_to_64";
            case OC_TRUNC_64_TO_32: return "trunc_64_to_32";

            case OC_CAST_I32_TO_U32: return "cast_i32_to_u32";
            case OC_CAST_I32_TO_F32: return "cast_i32_to_f32";
            case OC_CAST_I32_TO_F64: return "cast_i32_to_f64";

            case OC_CAST_U32_TO_I32: return "cast_u32_to_i32";
            case OC_CAST_U32_TO_F32: return "cast_u32_to_f32";
            case OC_CAST_U32_TO_F64: return "cast_u32_to_f64";

            case OC_CAST_I64_TO_U64: return "cast_i64_to_u64";
            case OC_CAST_I64_TO_F32: return "cast_i64_to_f32";
            case OC_CAST_I64_TO_F64: return "cast_i64_to_f64";

            case OC_CAST_U64_TO_I64: return "cast_u64_to_i64";
            case OC_CAST_U64_TO_F32: return "cast_u64_to_f32";
            case OC_CAST_U64_TO_F64: return "cast_u64_to_f64";

            case OC_CAST_F32_TO_I32: return "cast_f32_to_i32";
            case OC_CAST_F32_TO_I64: return "cast_f32_to_i64";
            case OC_CAST_F32_TO_U32: return "cast_f32_to_u32";
            case OC_CAST_F32_TO_U64: return "cast_f32_to_u64";
            case OC_CAST_F32_TO_F64: return "cast_f32_to_f64";

            case OC_CAST_F64_TO_I32: return "cast_f64_to_i32";
            case OC_CAST_F64_TO_I64: return "cast_f64_to_i64";
            case OC_CAST_F64_TO_U32: return "cast_f64_to_u32";
            case OC_CAST_F64_TO_U64: return "cast_f64_to_u64";
            case OC_CAST_F64_TO_F32: return "cast_f64_to_f32";

            case OC_JUMP:          return "jump";
            case OC_JUMP_IF_TRUE:  return "jump_if_true";
            case OC_JUMP_IF_FALSE: return "jump_if_false";

            case OC_POP:  return "pop";
            case OC_DUP:  return "dup";
            case OC_CROP: return "crop";

            case OC_GROW: return "grow";

            case OC_CALL: return "call";
            case OC_RET:  return "return";

            case OC_VEC_VV:    return "vec_vv";
            case OC_VEC_VS:    return "vec_vs";
            case OC_VEC_SV:    return "vec_sv";
            case OC_VEC_UNARY: return "vec_unary";
            case OC_VEC_CAT:   return "vec_cat";
            case OC_VEC_COPY:  return "vec_copy";
            case OC_VEC_FILL:  return "vec_fill";
            case OC_VEC_LOAD_INDIRECT: return "vec_load_indirect";
            case OC_VEC_STORE_INDIRECT: return "vec_store_indirect";
            case OC_VEC_CAST:      return "vec_cast";
            case OC_VEC_ALLOC:     return "vec_alloc";
            case OC_VEC_TO_REF:    return "vec_to_ref";
            case OC_VEC_MEM_RESET: return "vec_mem_reset";
            case OC_VEC_RESET:     return "vec_reset";

            case OC_NOP: return "no_operation";
            default: return "unknown";

        }

    }

    const char* toStr(Type::Kind dtype) {
        switch (dtype) {
            case Type::DT_VOID: return "void";
            case Type::DT_I8:   return "i8";
            case Type::DT_I16:  return "i16";
            case Type::DT_I32:  return "i32";
            case Type::DT_I64:  return "i64";
            case Type::DT_U8:   return "u8";
            case Type::DT_U16:  return "u16";
            case Type::DT_U32:  return "u32";
            case Type::DT_U64:  return "u64";
            case Type::DT_F32:  return "f32";
            case Type::DT_F64:  return "f64";

            case Type::DT_STRING:  return "string";
            case Type::DT_POINTER: return "ptr";
            case Type::DT_ARRAY:   return "array";
            case Type::DT_SLICE:   return "slice";
            case Type::DT_CUSTOM:  return "DT_CUSTOM";
            case Type::DT_UNION:   return "DT_UNION";
            case Type::DT_ERROR:   return "DT_ERROR";
            case Type::DT_ENUM:    return "DT_ENUM";

            case Type::DT_FUNCTION: return "DT_FUNCTION";

            case Type::DT_UNDEFINED:      return "DT_UNDEFINED";
            case Type::DT_MULTIPLE_TYPES: return "DT_MULTIPLE_TYPES";
        }
        return "Unknown";
    }

    const char* toStr(OperatorEnum op) {
        switch (op) {
            case OP_NONE:                          return "none";
            case OP_UNARY_PLUS:                    return "plus";
            case OP_UNARY_MINUS:                   return "minus";
            case OP_ADDITION:                      return "add";
            case OP_SUBTRACTION:                   return "sub";
            case OP_MULTIPLICATION:                return "mul";
            case OP_DIVISION:                      return "div";
            case OP_MODULO:                        return "mod";
            case OP_GET_ADDRESS:                   return "get_addr";
            case OP_GET_VALUE:                     return "get_val";
            case OP_BITWISE_AND:                   return "bit_and";
            case OP_BITWISE_OR:                    return "bit_or";
            case OP_BITWISE_XOR:                   return "bit_xor";
            case OP_BITWISE_NEGATION:              return "bit_neg";
            case OP_SHIFT_RIGHT:                   return "shift_right";
            case OP_SHIFT_LEFT:                    return "shift_left";
            case OP_EQUAL:                         return "eq";
            case OP_NOT_EQUAL:                     return "neq";
            case OP_LESS_THAN:                     return "lt";
            case OP_GREATER_THAN:                  return "gt";
            case OP_LESS_THAN_OR_EQUAL:            return "lte";
            case OP_GREATER_THAN_OR_EQUAL:         return "gte";
            case OP_BOOL_AND:                      return "bool_and";
            case OP_BOOL_OR:                       return "bool_or";
            case OP_INCREMENT:                     return "inc";
            case OP_DECREMENT:                     return "dec";
            case OP_SUBSCRIPT:                     return "subscript";
            case OP_MEMBER_SELECTION:              return "member_selection";
            case OP_DEREFERENCE_MEMBER_SELECTION:  return "dereference_member_selection";
            case OP_NEGATION:                      return "neg";
            case OP_CONCATENATION:                 return "cat";
            case OP_COUNT:                         return "count";
            case OP_INVALID:                       return "invalid";
            default:                               return "nop";
        }
    }

    struct DataTypeStack {
        uint64_t actualSize = 0;

        static constexpr uint64_t size = 8;
        uint8_t buffer[size];
    };

    void push(DataTypeStack* stack, Type::Kind dtype) {
        if (stack->actualSize < DataTypeStack::size) {
            stack->buffer[stack->actualSize] = dtype;
        }
        stack->actualSize++;
    }

    void push(DataTypeStack* stack, uint64_t size) {
        const uint64_t wordsCnt = BYTES_TO_WORDS(size);
        if (stack->actualSize + wordsCnt <= DataTypeStack::size) {
            memset(stack->buffer + stack->actualSize, Type::DT_VOID, wordsCnt);
            stack->actualSize += wordsCnt;
        } else if (DataTypeStack::size > stack->actualSize) {
            memset(stack->buffer + stack->actualSize, Type::DT_VOID, DataTypeStack::size - stack->actualSize);
            stack->actualSize = DataTypeStack::size;
        } else {
            stack->actualSize = DataTypeStack::size;
        }
    }

    void pop(DataTypeStack* stack) {
        if (stack->actualSize == 0) return;
        stack->actualSize--;
    }

    void pop(DataTypeStack* stack, uint64_t size) {
        const uint64_t wordsCnt = BYTES_TO_WORDS(size);
        if (stack->actualSize < wordsCnt) {
            stack->actualSize = 0;
        } else {
            stack->actualSize -= wordsCnt;
        }
    }

    int printLocalName(ExeBlock* block, uint64_t offset, char* const str, const int strLen, bool isLocal = true) {
        String key;
        key.buff = (char*) &offset;
        key.len = sizeof(uint64_t);

        LocalVarInfo* info = isLocal ? (LocalVarInfo*) OrderedDict::get(block->localsInfoMap, key) : NULL;
        if (info && info->var) {
            return snprintf(str, strLen, "[%llu] <%.*s>", offset, (int) info->var->name.len, info->var->name.buff);
        } else {
            return snprintf(str, strLen, "[%llu]", offset);
        }
    }

    // only definitions
    void printDtype(const Type::Kind dtypeEnum, void* payload) {

        switch (dtypeEnum) {
            case Type::DT_ARRAY: {
                Array* arr = (Array*) payload;
                printDtype(arr->base.pointsToKind, arr->base.pointsTo);
                IO::write(stream, '[');
                if (arr->length) {
                    IO::writef(stream, "%llu", arr->length->value.u64);
                } else {
                    const uint64_t flags = arr->flags;
                    if (flags & IS_CONST) {
                        IO::write(stream, "const");
                    } else if (flags & IS_EMBEDED) {
                        IO::write(stream, "embed");
                    } else {
                        IO::write(stream, "unknown");
                    }
                }
                IO::write(stream, ']');
                break;
            }

            default: {
                IO::writef(stream, "%s", toStr(dtypeEnum));
            }
        }

    }

    void printSignature(Function* fcn) {

        FunctionPrototype* fp = &fcn->prototype;

        const int size = fp->inArgCount;
        for (int i = 0; i < size; i++) {
            VariableDefinition* def = fp->inArgs[i];
            Value* val = &def->var->value;
            printDtype(val->typeKind, val->any);
            if (i != size - 1) printf(", ");
            else printf(" ");
        }

        IO::write(stream, "-> ");

        Value* outVal = &fp->outArg->var->value;
        printDtype(outVal->typeKind, outVal->any);

    }

    // TODO : to a generic file
    uint32_t getTypeStackSlots(Type::Kind dtype) {
        switch (dtype) {
            case Type::DT_VOID: return 0;
            case Type::DT_I8:
            case Type::DT_U8:
            case Type::DT_I16:
            case Type::DT_U16:
            case Type::DT_I32:
            case Type::DT_U32:
            case Type::DT_I64:
            case Type::DT_U64:
            case Type::DT_POINTER:
                return 1;

            case Type::DT_ARRAY:
            case Type::DT_SLICE:
            case Type::DT_STRING:
                return 2;

            case Type::DT_MULTIPLE_TYPES:
                return 1; // we account for vararg count
            default: return 1;
        }
    }

    // TODO : for now only for ones that doesnt have operands
    uint32_t getOpcodePopSize(Opcode opcode) {
        if (opcode >= OC_STORE_I8 && opcode < OC_STORE_BLOB) {
            return 2;
        }

        if (opcode >= OC_ADD_I32 && opcode < OC_GE_F64) {
            return 1;
        }

        return 0;
    }

    int computePopSize(Function* fcn) {

        FunctionPrototype* fp = &fcn->prototype;

        int popSize = 0;
        const int size = fp->inArgCount;
        for (int i = 0; i < size; i++) {
            VariableDefinition* def = fp->inArgs[i];
            popSize += getTypeStackSlots(def->var->value.typeKind);
        }

        return popSize;

    }

    int printVecDescriptor(uint64_t slot, char* const str, const int strLen) {
        VecDescriptor info = decodeVecDescriptor(slot);
        return snprintf(str, strLen, "%s | %s | %s ", toStr(info.dtype), toStr(info.oper), info.flags & DE_F_DEST ? "tmp" : "loc");
    }

    // TOOD : descriptor to a type
    bool isDestinationLocal(uint64_t descriptor) {
        return !(1 & (descriptor >> 16));
    }

    void printBytecode(ExeBlock* block, uint64_t maxDepth, uint64_t depth = 0) {

        // width of the operator column in actual
        // printable chars, color code etc. not included
        constexpr int OPERATOR_WIDTH = 33;

        // string here may be stored with the
        // color codes etc. does not represent
        // the length in chars
        constexpr int operandStrSize = 128;
        char operandStr[operandStrSize];

        // influences max displayed args
        DataTypeStack typeStack;

        uint8_t* buffer = block->bytecode;
        uint64_t size = block->bytecodeSize;

        uint8_t* startPtr = buffer;
        uint8_t* endPtr = buffer + size;

        uint64_t lineIdx = 0;
        uint64_t lineEnd = 0;

        // TODO : get the value from the last line and
        //   then force each span to be accordingly aligned
        int maxLineDigits = 0;
        if (block->linesSize > 0) {
            maxLineDigits = Utils::countDigits(block->lines[block->linesSize - 1].span.end.ln);
        }

        while (buffer < endPtr) {

            const uint64_t offset = (buffer - startPtr);

            while (offset >= lineEnd) {
                // TODO: maybe safety size check?
                LineInfo* line = block->lines + lineIdx;
                Logger::printSpanStrict(stream, &line->span);
                lineEnd = line->ocOffsetEnd;
                lineIdx++;
            }

            IO::writef(stream, AC_DIM "  [%04ld] " AC_RESET, (long) offset);

            Opcode opcode;
            memcpy(&opcode, buffer, sizeof(Opcode));
            buffer += sizeof(Opcode);

            IO::writef(stream, "%-15s ", toStr(opcode));

            // potential len of the formated output
            // has to be adjusted if colors were used
            int idealLen = 0;
            int idealDif = 0; // idealLen - <actual string size>
            operandStr[0] = '\0';
            switch (opcode) {

                case OC_PUSH_I8: {
                    push(&typeStack, Type::DT_I8);

                    int8_t val;
                    memcpy(&val, buffer, 1);
                    buffer += 1;

                    idealLen = snprintf(operandStr, operandStrSize, "%d", val);
                    break;
                }

                case OC_PUSH_U8: {
                    push(&typeStack, Type::DT_U8);

                    uint8_t val;
                    memcpy(&val, buffer, 1);
                    buffer += 1;

                    idealLen = snprintf(operandStr, operandStrSize, "%u", val);
                    break;
                }

                case OC_PUSH_I16: {
                    push(&typeStack, Type::DT_I16);

                    int16_t val;
                    memcpy(&val, buffer, 2);
                    buffer += 2;

                    idealLen = snprintf(operandStr, operandStrSize, "%d", val);
                    break;
                }

                case OC_PUSH_U16: {
                    push(&typeStack, Type::DT_U16);

                    uint16_t val;
                    memcpy(&val, buffer, 2);
                    buffer += 2;

                    idealLen = snprintf(operandStr, operandStrSize, "%u", val);
                    break;
                }

                case OC_PUSH_I32: {
                    push(&typeStack, Type::DT_I32);

                    int32_t val;
                    memcpy(&val, buffer, 4);
                    buffer += 4;

                    idealLen = snprintf(operandStr, operandStrSize, "%d", val);
                    break;
                }

                case OC_PUSH_U32: {
                    push(&typeStack, Type::DT_U32);

                    uint32_t val;
                    memcpy(&val, buffer, 4);
                    buffer += 4;

                    idealLen = snprintf(operandStr, operandStrSize, "%u", val);
                    break;
                }

                case OC_PUSH_I64: {
                    push(&typeStack, Type::DT_I64);

                    int64_t val;
                    memcpy(&val, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "%lld", val);
                    break;
                }

                case OC_PUSH_U64: {
                    push(&typeStack, Type::DT_U64);

                    uint64_t val;
                    memcpy(&val, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "%llu", val);
                    break;
                }

                case OC_PUSH_F32: {
                    push(&typeStack, Type::DT_F32);

                    float val;
                    memcpy(&val, buffer, 4);
                    buffer += 4;

                    idealLen = snprintf(operandStr, operandStrSize, "%f", val);
                    break;
                }

                case OC_PUSH_F64: {
                    push(&typeStack, Type::DT_F64);

                    double val;
                    memcpy(&val, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "%lf", val);
                    break;
                }

                case OC_PUSH_PTR: {
                    push(&typeStack, Type::DT_POINTER);

                    uint64_t val;
                    memcpy(&val, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "0x%016llx", val);
                    break;
                }

                case OC_SET_I8:  case OC_GET_I8:
                case OC_SET_U8:  case OC_GET_U8:
                case OC_SET_I16: case OC_GET_I16:
                case OC_SET_U16: case OC_GET_U16:
                case OC_SET_I32: case OC_GET_I32:
                case OC_SET_U32: case OC_GET_U32:
                case OC_SET_I64: case OC_GET_I64:
                case OC_SET_U64: case OC_GET_U64:
                case OC_SET_F32: case OC_GET_F32:
                case OC_SET_F64: case OC_GET_F64:
                case OC_SET_PTR: case OC_GET_PTR: {
                    // TODO : to a function isGetOpcode?
                    if (OC_GET_I8 <= opcode && opcode <= OC_GET_PTR) {
                        Type::Kind dtype = (Type::Kind) (Type::DT_I8 + (opcode - OC_GET_I8));
                        push(&typeStack, dtype);
                    } else {
                        pop(&typeStack);
                    }

                    uint64_t offset;
                    memcpy(&offset, buffer, 8);
                    buffer += 8;

                    idealLen = printLocalName(block, offset, operandStr, operandStrSize);
                    break;
                }

                case OC_PUSH_BLOB:
                case OC_GET_BLOB:
                case OC_SET_BLOB: {
                    uint64_t size;
                    memcpy(&size, buffer, 8);
                    buffer += 8;

                    uint64_t offset;
                    memcpy(&offset, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "size=%llu offset=%llu", size, offset);
                    break;
                }

                case OC_JUMP: {
                    uint64_t target;
                    memcpy(&target, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "%llu", target);
                    break;
                }

                case OC_JUMP_IF_TRUE:
                case OC_JUMP_IF_FALSE: {
                    pop(&typeStack);

                    uint64_t target;
                    memcpy(&target, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "%llu", target);
                    break;
                }

                case OC_CALL: {
                    uint64_t target;
                    memcpy(&target, buffer, 8);
                    buffer += 8;

                    uint64_t varargSize;
                    memcpy(&varargSize, buffer, 8);
                    buffer += 8;

                    Function* fcn = (Function*) target;
                    pop(&typeStack, (computePopSize(fcn) + varargSize + 2) * sizeof(vmword));
                    if (fcn->prototype.outArg) {
                        push(&typeStack, fcn->prototype.outArg->var->value.typeKind);
                    }

                    idealLen = snprintf(operandStr, operandStrSize, AC_MAGENTA "%.*s" AC_RESET, fcn->name.len, fcn->name.buff);
                    idealDif = sizeof(AC_MAGENTA) + sizeof(AC_RESET) - 2;
                    idealLen -= idealDif;
                    break;
                }

                case OC_LEA:
                case OC_LEA_CONST: {
                    push(&typeStack, Type::DT_POINTER);

                    uint64_t offset;
                    memcpy(&offset, buffer, 8);
                    buffer += 8;

                    if (opcode == OC_LEA) {
                        idealLen = printLocalName(block, offset, operandStr, operandStrSize);
                    } else {
                        idealLen = snprintf(operandStr, operandStrSize, "[%llu]", offset);
                    }

                    break;
                }

                case OC_STORE_INDEXED:
                case OC_STORE_INDEXED_TMP: {
                    uint64_t id;
                    memcpy(&id, buffer, 8);
                    buffer += 8;

                    uint64_t idx;
                    memcpy(&idx, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "id: %llu idx: %llu", id, idx);
                    break;
                }

                case OC_PTR_IDX: {
                    pop(&typeStack);
                    pop(&typeStack);
                    push(&typeStack, Type::DT_POINTER);

                    uint64_t offset;
                    memcpy(&offset, buffer, 8);
                    buffer += 8;

                    idealLen = printLocalName(block, offset, operandStr, operandStrSize);
                    break;
                }

                case OC_RET: {
                    uint64_t size;
                    memcpy(&size, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "size: %llu", size);
                    break;
                }

                case OC_CROP: {
                    uint64_t blobSize;
                    memcpy(&blobSize, buffer, 8);
                    buffer += 8;

                    uint64_t memberOffset;
                    memcpy(&memberOffset, buffer, 8);
                    buffer += 8;

                    uint64_t memberSize;
                    memcpy(&memberSize, buffer, 8);
                    buffer += 8;

                    idealLen = snprintf(operandStr, operandStrSize, "blob_size: %llu, member_offset: %llu, member_size: %llu", blobSize, memberOffset, memberSize);
                    break;
                }

                case OC_GROW: {
                    uint64_t size;
                    memcpy(&size, buffer, 8);
                    buffer += 8;

                    push(&typeStack, size);

                    idealLen = snprintf(operandStr, operandStrSize, "size: %llu", size);
                    break;
                }

                case OC_VEC_CAT:
                case OC_VEC_VV:
                case OC_VEC_VS:
                case OC_VEC_SV: {
                    uint64_t descriptor;
                    memcpy(&descriptor, buffer, 8);
                    buffer += 8;

                    uint64_t dest;
                    memcpy(&dest, buffer, 8);
                    buffer += 8;

                    pop(&typeStack, 8 * 4);
                    push(&typeStack, Type::DT_POINTER);
                    push(&typeStack, Type::DT_U64);

                    idealLen = printVecDescriptor(descriptor, operandStr, operandStrSize);
                    idealLen += printLocalName(block, dest, operandStr + idealLen, operandStrSize - idealLen, isDestinationLocal(descriptor));

                    break;
                }

                case OC_VEC_FILL:
                case OC_VEC_COPY:
                case OC_VEC_UNARY: {
                    uint64_t descriptor;
                    memcpy(&descriptor, buffer, 8);
                    buffer += 8;

                    uint64_t dest;
                    memcpy(&dest, buffer, 8);
                    buffer += 8;

                    pop(&typeStack, 8 * 2);
                    push(&typeStack, Type::DT_POINTER);
                    push(&typeStack, Type::DT_U64);

                    idealLen = printVecDescriptor(descriptor, operandStr, operandStrSize);
                    idealLen += printLocalName(block, dest, operandStr + idealLen, operandStrSize - idealLen, isDestinationLocal(descriptor));

                    break;
                }

                case OC_VEC_CAST: {
                    uint64_t descriptor;
                    memcpy(&descriptor, buffer, 8);
                    buffer += 8;

                    uint64_t dest;
                    memcpy(&dest, buffer, 8);
                    buffer += 8;

                    pop(&typeStack, 8 * 2);
                    push(&typeStack, Type::DT_POINTER);
                    push(&typeStack, Type::DT_U64);

                    VecDescriptor desc = decodeVecDescriptor(descriptor);

                    // idealLen = printVecDescriptor(descriptor, operandStr, operandStrSize);
                    idealLen += printLocalName(block, dest, operandStr + idealLen, operandStrSize - idealLen, isDestinationLocal(descriptor));
                    idealLen += snprintf(operandStr + idealLen, operandStrSize - idealLen, " %s -> %s", toStr((Type::Kind) desc.srcDtype), toStr((Type::Kind) desc.dtype));

                    break;
                }

                case OC_VEC_ALLOC: {
                    uint64_t size;
                    memcpy(&size, buffer, 8);
                    buffer += 8;

                    pop(&typeStack, 8);

                    idealLen += snprintf(operandStr, operandStrSize, "size: %llu", size);
                    break;
                }

                case OC_VEC_TO_REF: {
                    pop(&typeStack, 8);
                    break;
                }

                case OC_VEC_RESET: {
                    pop(&typeStack, 8 * 2);
                    break;
                }

                default: {
                    if (opcode < OC_COUNT) {
                        pop(&typeStack, getOpcodePopSize(opcode));
                        break;
                    }

                    idealLen = snprintf(operandStr, operandStrSize, "(Unknown Opcode)");
                    return;
                }
            }

            // Operands:
            if (idealLen > OPERATOR_WIDTH) {
                IO::writef(stream, "%-*.*s..", OPERATOR_WIDTH - 2, OPERATOR_WIDTH - 2, operandStr);
            } else {
                IO::writef(stream, "%-*.*s", OPERATOR_WIDTH + idealDif, operandStrSize, operandStr);
            }

            // Stack changes:
            IO::write(stream, AC_DIM "  ; [ ");

            if (typeStack.actualSize > DataTypeStack::size) {
                const uint64_t diff = typeStack.actualSize - DataTypeStack::size;
                IO::writef(stream, "...+%llu more, ", diff);
            }

            uint64_t visibleCount = std::min(typeStack.actualSize, DataTypeStack::size);
            for (uint64_t i = 0; i < visibleCount; i++) {
                IO::writef(stream, "%s%s", toStr((Type::Kind) typeStack.buffer[i]), (i == visibleCount - 1) ? "" : ", ");
            }

            IO::write(stream, " ]" AC_RESET "\n");

        }
    }

    // TODO : format name column based on max var name length
    //        or dots .. . make as option with constexpr toggler
    // TODO : print actual type names instead of DT_CUSTOM
    void printLocals(OrderedDict::Container* dict) {
        uint64_t maxNameSize = 18;
        constexpr bool printFullNames = false;

        if (dict->pairs.size <= 0) {
            printf("  (no locals)\n");
            return;
        }

        printf("  %-8s | %-6s | %-6s | %-18s | %-12s\n",
            "Offset", "Size", "Align", "Name", "Type");

        OrderedDict::Pair* first = OrderedDict::getNext(dict);
        OrderedDict::Pair* ptr = first;

        if constexpr (printFullNames) {
            do {
                LocalVarInfo* info = (LocalVarInfo*)ptr->data;
                maxNameSize = max(info->var->name.len, maxNameSize);
                ptr = OrderedDict::getNext(dict);
            } while (ptr && ptr != first);

            ptr = first;
        }

        do {

            if (!ptr) break;

            uint64_t offset = ptr->key.idx;

            // offset
            printf("  " AC_DIM "%-8llu" AC_RESET, offset);

            if (!ptr->data) {
                printf(" | Internal Variable\n");
                ptr = OrderedDict::getNext(dict);
                continue;
            }

            LocalVarInfo* info = (LocalVarInfo*) ptr->data;

            // size
            printf(" | %-6llu | ", info->size);

            // align
            printf("%-6llu | ", info->align);

            // name
            if (printFullNames) {
                printf(AC_BOLD_GREEN "%*.*s " AC_RESET "| ",
                    (int) (maxNameSize - info->var->name.len),
                    (int) info->var->name.len,
                    info->var->name.buff);
            } else {
                if (info->var->name.len > maxNameSize) {
                    printf(AC_BOLD_GREEN "%.*s.. " AC_RESET "| ",
                        (int) maxNameSize - 2,
                        info->var->name.buff);
                } else {
                    printf(AC_BOLD_GREEN "%*.*s " AC_RESET "| ",
                        (int) maxNameSize,
                        (int) info->var->name.len,
                        info->var->name.buff);
                }
            }

            // type
            printDtype(info->var->value.typeKind, info->var->value.any);
            IO::write(stream, '\n');

            ptr = OrderedDict::getNext(dict);

        } while (ptr && ptr != first);

    }

    void printConstants(uint8_t* buffer, uint64_t size) {
        printf("%-8s | %-48s | %s\n", "  Offset", "Hex Data", "Preview");

        uint64_t i = 0;
        while (i < size) {
            printf(AC_DIM "  0x%04llx" AC_RESET " | ", i);

            // 1. Print Hex Block (e.g., 16 bytes at a time)
            uint64_t lineStart = i;
            for (int j = 0; j < 16; j++) {
                if (i + j < size) printf("%02x ", buffer[i + j]);
                else printf("   ");
            }
            printf(" | ");

            // 2. Print ASCII Preview
            for (int j = 0; j < 16; j++) {
                if (i < size) {
                    uint8_t c = buffer[i++];
                    if (c >= 32 && c <= 126) printf("%c", c);
                    else printf("."); // Non-printable
                }
            }
            printf("\n");
        }
    }

    void collectFunctions(ExeBlock* block, int maxDepth, int depth) {

        if (!block || depth > maxDepth) return;

        uint8_t* ip = block->bytecode;
        uint8_t* endPtr = ip + block->bytecodeSize;

        while (ip < endPtr) {

            Opcode opcode;
            memcpy(&opcode, ip, sizeof(Opcode));
            ip += getOpcodeSize(opcode);

            if (opcode == OC_CALL) {

                uint64_t target;
                memcpy(&target, ip - 16, 8);

                Function* fcn = (Function*) target;
                if (!fcn->exe) continue;

                if (depth <= maxDepth &&
                    Set::insert(&functionsSet, (uint8_t*) fcn)
                    ) {
                    DArray::push(&functionsArray, &fcn);
                }

                collectFunctions(fcn->exe, maxDepth, depth + 1);

            }

        }

    }

    void printFullFunctionName(Function* fcn) {
        Namespace* path[16];
        constexpr int maxPathLen = sizeof(path) / sizeof(Namespace*);
        int pathLen = 0;

        Scope* scope = fcn->base.scope;
        while (scope && scope->base.type == NT_NAMESPACE) {
            if (pathLen < maxPathLen) {
                path[pathLen] = (Namespace*) scope;
            }
            pathLen++;

            scope = scope->base.scope;
        }

        if (pathLen >= maxPathLen) {
            IO::write(stream, "...::");
        }

        for (int i = 0; i < pathLen; i++) {
            Namespace* nspace = path[i];
            IO::writef(stream, AC_BOLD_WHITE "%.*s::", nspace->name.len, nspace->name.buff);
        }

        IO::writef(stream, AC_BOLD_WHITE "%.*s", fcn->name.len, fcn->name.buff);
    }

    // TODO : better name
    void printFlat(Function* fcn) {
        IO::write(stream, AC_BOLD AC_MAGENTA "Function: " AC_RESET);
        printFullFunctionName(fcn);

        IO::write(stream, "  ");
        printSignature(fcn);
        IO::write(stream, '\n');

        print(stream, fcn->exe);
        IO::write(stream, '\n');
    }

    // TODO : print improvements
    //  - jump locations
    //  - annotate folded/magic values
    //  - maybe color only line numbers and not whole lines
    //  - maybe literal colors
    //  - maybe preview AC_DIM
    //  - maybe color variables
    void print(IO::Stream* s, Function* fcn, uint64_t depth) {
        stream = s;
        collectFunctions(fcn->exe, 1, depth);

        IO::write(stream, AC_DIM "[ROOT]\n" AC_RESET);
        printFlat(fcn);
        for (int i = 0; i < functionsArray.size; i++) {
            Function* fcn = *(Function**) DArray::get(&functionsArray, i);
            printFlat(fcn);
        }
    }

    void print(IO::Stream* s, ExeBlock* block) {
        stream = s;
        if (block->rawDataSize > 0) {
            IO::write(stream, AC_BOLD_CYAN "Raw data:\n" AC_RESET);
            printConstants(block->rawData, block->rawDataSize);
        }

        IO::write(stream, AC_BOLD_CYAN "Locals:\n" AC_RESET);
        printLocals(block->localsInfoMap);

        IO::write(stream, '\n');

        IO::write(stream, AC_BOLD_CYAN "Bytecode:\n" AC_RESET);
        printBytecode(block, 1, 0);
    }

    void print(IO::Stream* stream, Reg::Unit* unit) {
        print(stream, unit->exe);
    }

}
