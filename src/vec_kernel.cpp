#include "vec_kernel.h"
#include <cstdint>

// Here we generate some stuff to generate dispatch table dtype x operator
//

// --- Core function macros
//

#define GEN_VEC_FCN_B(name, dtype, op) \
    void name(void* vout, void* va, void* vb, const int len) { \
        dtype* out = (dtype*)vout; dtype* a = (dtype*)va; dtype* b = (dtype*)vb; \
        for (int i = 0; i < len; i++) out[i] = a[i] op b[i]; \
    }

#define GEN_VEC_FCN_VS(name, dtype, op) \
    void name(void* vout, void* va, uint64_t vs, const int len) { \
        dtype* out = (dtype*)vout; dtype* a = (dtype*)va; \
        dtype s = *((dtype*)&vs); \
        for (int i = 0; i < len; i++) out[i] = a[i] op s; \
    }

#define GEN_VEC_FCN_SV(name, dtype, op) \
    void name(void* vout, void* va, uint64_t vs, const int len) { \
        dtype* out = (dtype*)vout; dtype* a = (dtype*)va; \
        dtype s = *((dtype*)&vs); \
        for (int i = 0; i < len; i++) out[i] = s op a[i]; \
    }

#define GEN_VEC_FCN_U(name, dtype, op) \
    void name(void* vout, void* va, const int len) { \
        dtype* out = (dtype*)vout; dtype* a = (dtype*)va; \
        for (int i = 0; i < len; i++) out[i] = op a[i]; \
    }

#define GEN_VEC_FCN_C(name, dest, src) \
    void name(void* vout, void* va, const int len) { \
        dest* out = (dest*)vout; src* a = (src*)va; \
        for(int i = 0; i < len; i++) out[i] = (dest)a[i]; \
    }

#define GEN_VEC_FCN_F(name, dtype) \
    void name(void* vout, uint64_t vval, const int len) { \
        dtype* out = (dtype*)vout; dtype val = *((dtype*) &vval);\
        for(int i = 0; i < len; i++) out[i] = val; \
    }

// Just a helper to unite table and function generation
#define GEN_VEC_FCN_NAME(ftag, tname, oname) vec##ftag##oname##tname



// --- Expand each function by type
//

#define GEN_VEC_FCN_BY_TYPE(ftag, oname, dtype, tname, op) \
    GEN_VEC_FCN_##ftag(GEN_VEC_FCN_NAME(ftag, tname, oname), dtype, op)



// --- Expand each typed function by operator
//

#define GEN_VEC_FCN_BY_OPER_INT(ftag, oname, op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, int8_t,   I8,  op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, int16_t,  I16, op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, int32_t,  I32, op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, int64_t,  I64, op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, uint8_t,  U8,  op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, uint16_t, U16, op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, uint32_t, U32, op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, uint64_t, U64, op)

#define GEN_VEC_FCN_BY_OPER(ftag, oname, op) \
    GEN_VEC_FCN_BY_OPER_INT(ftag, oname, op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, float,  F32, op) \
    GEN_VEC_FCN_BY_TYPE(ftag, oname, double, F64, op)

#define GEN_VEC_FCN_BY_TAG(ftag) \
    GEN_VEC_FCN_BY_OPER(ftag, Add, +) \
    GEN_VEC_FCN_BY_OPER(ftag, Sub, -) \
    GEN_VEC_FCN_BY_OPER(ftag, Mul, *) \
    GEN_VEC_FCN_BY_OPER(ftag, Div, /) \
    GEN_VEC_FCN_BY_OPER_INT(ftag, Mod, %) \
    GEN_VEC_FCN_BY_OPER_INT(ftag, And, &) \
    GEN_VEC_FCN_BY_OPER_INT(ftag, Or, |) \
    GEN_VEC_FCN_BY_OPER_INT(ftag, Xor, ^) \
    GEN_VEC_FCN_BY_OPER(ftag, BoolAnd, &&) \
    GEN_VEC_FCN_BY_OPER(ftag, BoolOr, ||) \
    GEN_VEC_FCN_BY_OPER_INT(ftag, Shr, >>) \
    GEN_VEC_FCN_BY_OPER_INT(ftag, Shl, <<) \
    GEN_VEC_FCN_BY_OPER(ftag, Eq, ==) \
    GEN_VEC_FCN_BY_OPER(ftag, Neq, !=) \
    GEN_VEC_FCN_BY_OPER(ftag, Lte, <=) \
    GEN_VEC_FCN_BY_OPER(ftag, Gte, >=) \
    GEN_VEC_FCN_BY_OPER(ftag, Lt, <) \
    GEN_VEC_FCN_BY_OPER(ftag, Gt, >)
    //GEN_VEC_FCN_BY_OPER(ftag, Dot, .) \
    //GEN_VEC_FCN_BY_OPER(ftag, Arrow, ->)



// --- For cast we have to have own case
//

#define GEN_VEC_CAST_BY_DEST(dest, name) \
    GEN_VEC_FCN_C(vecCI8##name,  dest, int8_t)   \
    GEN_VEC_FCN_C(vecCI16##name, dest, int16_t)  \
    GEN_VEC_FCN_C(vecCI32##name, dest, int32_t)  \
    GEN_VEC_FCN_C(vecCI64##name, dest, int64_t)  \
    GEN_VEC_FCN_C(vecCU8##name,  dest, uint8_t)  \
    GEN_VEC_FCN_C(vecCU16##name, dest, uint16_t) \
    GEN_VEC_FCN_C(vecCU32##name, dest, uint32_t) \
    GEN_VEC_FCN_C(vecCU64##name, dest, uint64_t) \
    GEN_VEC_FCN_C(vecCF32##name, dest, float)    \
    GEN_VEC_FCN_C(vecCF64##name, dest, double)



// --- Generate all functions
//

GEN_VEC_FCN_BY_TAG(B)
GEN_VEC_FCN_BY_TAG(VS)
GEN_VEC_FCN_BY_TAG(SV)

GEN_VEC_FCN_BY_OPER(U, Add, +)
GEN_VEC_FCN_BY_OPER(U, Sub, -)
//GEN_VEC_BY_OPER(U, Addr, &)
//GEN_VEC_BY_OPER(U, Value, *)
GEN_VEC_FCN_BY_OPER_INT(U, Neg, ~)
GEN_VEC_FCN_BY_OPER(U, Inc, ++)
GEN_VEC_FCN_BY_OPER(U, Dec, --)
GEN_VEC_FCN_BY_OPER(U, BoolNeg, !)

GEN_VEC_CAST_BY_DEST(int8_t,   I8)
GEN_VEC_CAST_BY_DEST(int16_t,  I16)
GEN_VEC_CAST_BY_DEST(int32_t,  I32)
GEN_VEC_CAST_BY_DEST(int64_t,  I64)
GEN_VEC_CAST_BY_DEST(uint8_t,  U8)
GEN_VEC_CAST_BY_DEST(uint16_t, U16)
GEN_VEC_CAST_BY_DEST(uint32_t, U32)
GEN_VEC_CAST_BY_DEST(uint64_t, U64)
GEN_VEC_CAST_BY_DEST(float,    F32)
GEN_VEC_CAST_BY_DEST(double,   F64)

GEN_VEC_FCN_F(vecFillI8,  int8_t)
GEN_VEC_FCN_F(vecFillI16, int16_t)
GEN_VEC_FCN_F(vecFillI32, int32_t)
GEN_VEC_FCN_F(vecFillI64, int64_t)
GEN_VEC_FCN_F(vecFillU8,  uint8_t)
GEN_VEC_FCN_F(vecFillU16, uint16_t)
GEN_VEC_FCN_F(vecFillU32, uint32_t)
GEN_VEC_FCN_F(vecFillU64, uint64_t)
GEN_VEC_FCN_F(vecFillF32, float)
GEN_VEC_FCN_F(vecFillF64, double)



// --- Actual dispatching
//

#define RETURN_TYPE_ALL(ftag, oname, dtype) \
    switch (dtype) { \
        case Type::DT_I8:  return GEN_VEC_FCN_NAME(ftag, I8,  oname); \
        case Type::DT_I16: return GEN_VEC_FCN_NAME(ftag, I16, oname); \
        case Type::DT_I32: return GEN_VEC_FCN_NAME(ftag, I32, oname); \
        case Type::DT_I64: return GEN_VEC_FCN_NAME(ftag, I64, oname); \
        case Type::DT_U8:  return GEN_VEC_FCN_NAME(ftag, U8,  oname); \
        case Type::DT_U16: return GEN_VEC_FCN_NAME(ftag, U16, oname); \
        case Type::DT_U32: return GEN_VEC_FCN_NAME(ftag, U32, oname); \
        case Type::DT_U64: return GEN_VEC_FCN_NAME(ftag, U64, oname); \
        case Type::DT_F32: return GEN_VEC_FCN_NAME(ftag, F32, oname); \
        case Type::DT_F64: return GEN_VEC_FCN_NAME(ftag, F64, oname); \
        default: return nullptr; \
    }

#define RETURN_TYPE_INT(ftag, oname, dtype) \
    switch (dtype) { \
        case Type::DT_I8:  return GEN_VEC_FCN_NAME(ftag, I8,  oname); \
        case Type::DT_I16: return GEN_VEC_FCN_NAME(ftag, I16, oname); \
        case Type::DT_I32: return GEN_VEC_FCN_NAME(ftag, I32, oname); \
        case Type::DT_I64: return GEN_VEC_FCN_NAME(ftag, I64, oname); \
        case Type::DT_U8:  return GEN_VEC_FCN_NAME(ftag, U8,  oname); \
        case Type::DT_U16: return GEN_VEC_FCN_NAME(ftag, U16, oname); \
        case Type::DT_U32: return GEN_VEC_FCN_NAME(ftag, U32, oname); \
        case Type::DT_U64: return GEN_VEC_FCN_NAME(ftag, U64, oname); \
        default: return nullptr; \
    }

#define RETURN_CAST_SRC(destName, src) \
    switch (src) { \
        case Type::DT_I8:  return GEN_VEC_FCN_NAME(C, destName, I8);  \
        case Type::DT_I16: return GEN_VEC_FCN_NAME(C, destName, I16); \
        case Type::DT_I32: return GEN_VEC_FCN_NAME(C, destName, I32); \
        case Type::DT_I64: return GEN_VEC_FCN_NAME(C, destName, I64); \
        case Type::DT_U8:  return GEN_VEC_FCN_NAME(C, destName, U8);  \
        case Type::DT_U16: return GEN_VEC_FCN_NAME(C, destName, U16); \
        case Type::DT_U32: return GEN_VEC_FCN_NAME(C, destName, U32); \
        case Type::DT_U64: return GEN_VEC_FCN_NAME(C, destName, U64); \
        case Type::DT_F32: return GEN_VEC_FCN_NAME(C, destName, F32); \
        case Type::DT_F64: return GEN_VEC_FCN_NAME(C, destName, F64); \
        default: return nullptr; \
    }

VecFunctionBinary vecGetBinary(Type::Kind dtype, OperatorEnum oper) {
    switch (oper) {
        case OP_ADDITION:              RETURN_TYPE_ALL(B, Add, dtype);
        case OP_SUBTRACTION:           RETURN_TYPE_ALL(B, Sub, dtype);
        case OP_MULTIPLICATION:        RETURN_TYPE_ALL(B, Mul, dtype);
        case OP_DIVISION:              RETURN_TYPE_ALL(B, Div, dtype);
        case OP_MODULO:                RETURN_TYPE_INT(B, Mod, dtype);
        case OP_BITWISE_AND:           RETURN_TYPE_INT(B, And, dtype);
        case OP_BITWISE_OR:            RETURN_TYPE_INT(B, Or,  dtype);
        case OP_BITWISE_XOR:           RETURN_TYPE_INT(B, Xor, dtype);
        case OP_SHIFT_LEFT:            RETURN_TYPE_INT(B, Shl, dtype);
        case OP_SHIFT_RIGHT:           RETURN_TYPE_INT(B, Shr, dtype);
        case OP_EQUAL:                 RETURN_TYPE_ALL(B, Eq,  dtype);
        case OP_NOT_EQUAL:             RETURN_TYPE_ALL(B, Neq, dtype);
        case OP_LESS_THAN:             RETURN_TYPE_ALL(B, Lt,  dtype);
        case OP_LESS_THAN_OR_EQUAL:    RETURN_TYPE_ALL(B, Lte, dtype);
        case OP_GREATER_THAN:          RETURN_TYPE_ALL(B, Gt,  dtype);
        case OP_GREATER_THAN_OR_EQUAL: RETURN_TYPE_ALL(B, Gte, dtype);
        case OP_BOOL_AND:              RETURN_TYPE_ALL(B, BoolAnd, dtype);
        case OP_BOOL_OR:               RETURN_TYPE_ALL(B, BoolOr,  dtype);
        default: return NULL;
    }
}

VecFunctionScalar vecGetScalarR(Type::Kind dtype, OperatorEnum oper) {
    switch (oper) {
        case OP_ADDITION:              RETURN_TYPE_ALL(VS, Add, dtype);
        case OP_SUBTRACTION:           RETURN_TYPE_ALL(VS, Sub, dtype);
        case OP_MULTIPLICATION:        RETURN_TYPE_ALL(VS, Mul, dtype);
        case OP_DIVISION:              RETURN_TYPE_ALL(VS, Div, dtype);
        case OP_MODULO:                RETURN_TYPE_INT(VS, Mod, dtype);
        case OP_BITWISE_AND:           RETURN_TYPE_INT(VS, And, dtype);
        case OP_BITWISE_OR:            RETURN_TYPE_INT(VS, Or,  dtype);
        case OP_BITWISE_XOR:           RETURN_TYPE_INT(VS, Xor, dtype);
        case OP_SHIFT_LEFT:            RETURN_TYPE_INT(VS, Shl, dtype);
        case OP_SHIFT_RIGHT:           RETURN_TYPE_INT(VS, Shr, dtype);
        case OP_EQUAL:                 RETURN_TYPE_ALL(VS, Eq,  dtype);
        case OP_NOT_EQUAL:             RETURN_TYPE_ALL(VS, Neq, dtype);
        case OP_LESS_THAN:             RETURN_TYPE_ALL(VS, Lt,  dtype);
        case OP_LESS_THAN_OR_EQUAL:    RETURN_TYPE_ALL(VS, Lte, dtype);
        case OP_GREATER_THAN:          RETURN_TYPE_ALL(VS, Gt,  dtype);
        case OP_GREATER_THAN_OR_EQUAL: RETURN_TYPE_ALL(VS, Gte, dtype);
        case OP_BOOL_AND:              RETURN_TYPE_ALL(VS, BoolAnd, dtype);
        case OP_BOOL_OR:               RETURN_TYPE_ALL(VS, BoolOr,  dtype);
        default: return nullptr;
    }
}

VecFunctionScalar vecGetScalarL(Type::Kind dtype, OperatorEnum oper) {
    switch (oper) {
        case OP_ADDITION:              RETURN_TYPE_ALL(SV, Add, dtype);
        case OP_SUBTRACTION:           RETURN_TYPE_ALL(SV, Sub, dtype);
        case OP_MULTIPLICATION:        RETURN_TYPE_ALL(SV, Mul, dtype);
        case OP_DIVISION:              RETURN_TYPE_ALL(SV, Div, dtype);
        case OP_MODULO:                RETURN_TYPE_INT(SV, Mod, dtype);
        case OP_BITWISE_AND:           RETURN_TYPE_INT(SV, And, dtype);
        case OP_BITWISE_OR:            RETURN_TYPE_INT(SV, Or,  dtype);
        case OP_BITWISE_XOR:           RETURN_TYPE_INT(SV, Xor, dtype);
        case OP_SHIFT_LEFT:            RETURN_TYPE_INT(SV, Shl, dtype);
        case OP_SHIFT_RIGHT:           RETURN_TYPE_INT(SV, Shr, dtype);
        case OP_EQUAL:                 RETURN_TYPE_ALL(SV, Eq,  dtype);
        case OP_NOT_EQUAL:             RETURN_TYPE_ALL(SV, Neq, dtype);
        case OP_LESS_THAN:             RETURN_TYPE_ALL(SV, Lt,  dtype);
        case OP_LESS_THAN_OR_EQUAL:    RETURN_TYPE_ALL(SV, Lte, dtype);
        case OP_GREATER_THAN:          RETURN_TYPE_ALL(SV, Gt,  dtype);
        case OP_GREATER_THAN_OR_EQUAL: RETURN_TYPE_ALL(SV, Gte, dtype);
        case OP_BOOL_AND:              RETURN_TYPE_ALL(SV, BoolAnd, dtype);
        case OP_BOOL_OR:               RETURN_TYPE_ALL(SV, BoolOr,  dtype);
        default: return nullptr;
    }
}

VecFunctionUnary vecGetUnary(Type::Kind dtype, OperatorEnum oper) {
    switch (oper) {
        case OP_UNARY_PLUS:       RETURN_TYPE_ALL(U, Add, dtype);
        case OP_UNARY_MINUS:      RETURN_TYPE_ALL(U, Sub, dtype);
        case OP_NEGATION:         RETURN_TYPE_ALL(U, BoolNeg, dtype);
        case OP_BITWISE_NEGATION: RETURN_TYPE_INT(U, Neg, dtype);
        case OP_INCREMENT:        RETURN_TYPE_ALL(U, Inc, dtype);
        case OP_DECREMENT:        RETURN_TYPE_ALL(U, Dec, dtype);
        default: return nullptr;
    }
}

VecFunctionCast vecGetCast(Type::Kind dest, Type::Kind src) {
    switch (dest) {
        case Type::DT_I8:  RETURN_CAST_SRC(I8,  src);
        case Type::DT_I16: RETURN_CAST_SRC(I16, src);
        case Type::DT_I32: RETURN_CAST_SRC(I32, src);
        case Type::DT_I64: RETURN_CAST_SRC(I64, src);
        case Type::DT_U8:  RETURN_CAST_SRC(U8,  src);
        case Type::DT_U16: RETURN_CAST_SRC(U16, src);
        case Type::DT_U32: RETURN_CAST_SRC(U32, src);
        case Type::DT_U64: RETURN_CAST_SRC(U64, src);
        case Type::DT_F32: RETURN_CAST_SRC(F32, src);
        case Type::DT_F64: RETURN_CAST_SRC(F64, src);
        default: return nullptr;
    }
}

VecFunctionFill vecGetFill(Type::Kind dtype) {
    switch (dtype) {
        case Type::DT_I8:  return vecFillI8;
        case Type::DT_I16: return vecFillI16;
        case Type::DT_I32: return vecFillI32;
        case Type::DT_I64: return vecFillI64;
        case Type::DT_U8:  return vecFillU8;
        case Type::DT_U16: return vecFillU16;
        case Type::DT_U32: return vecFillU32;
        case Type::DT_U64: return vecFillU64;
        case Type::DT_F32: return vecFillF32;
        case Type::DT_F64: return vecFillF64;
        default: return nullptr;
    }
}
