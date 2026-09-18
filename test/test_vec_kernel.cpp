#include "../src/vec_kernel.h"
#include "test_core.h"



inline void gVecKernelPreSuite() {
}

inline void gVecKernelPostSuite() {
}

inline void gVecKernelPreCase() {
}

inline void gVecKernelPostCase() {
}



template <typename T>
void check_binary(Type::Kind dtype, OperatorEnum op, T a, T b, T expected) {
    VecFunctionBinary fn = vecGetBinary(dtype, op);
    Test::assertOrDie(fn != nullptr);

    T in_a[2] = {a, a};
    T in_b[2] = {b, b};
    T out[2]  = {0, 0};

    fn(out, in_a, in_b, 2);

    Test::assert(out[0], expected);
    Test::assert(out[0], expected);
}

template <typename T>
void check_scalar_r(Type::Kind dtype, OperatorEnum op, T a, T s, T expected) {
    VecFunctionScalar fn = vecGetScalarR(dtype, op);
    Test::assertOrDie(fn != nullptr);

    T in_a[2] = {a, a};
    T out[2]  = {0, 0};
    uint64_t vs = 0;
    *((T*)&vs) = s;

    fn(out, in_a, vs, 2);

    Test::assert(out[0], expected);
    Test::assert(out[0], expected);
}

template <typename T>
void check_scalar_l(Type::Kind dtype, OperatorEnum op, T s, T a, T expected) {
    VecFunctionScalar fn = vecGetScalarL(dtype, op);
    Test::assertOrDie(fn != nullptr);

    T in_a[2] = {a, a};
    T out[2]  = {0, 0};
    uint64_t vs = 0;
    *((T*)&vs) = s;

    fn(out, in_a, vs, 2);

    Test::assert(out[0], expected);
    Test::assert(out[0], expected);
}

template <typename T>
void check_unary(Type::Kind dtype, OperatorEnum op, T a, T expected) {
    VecFunctionUnary fn = vecGetUnary(dtype, op);
    Test::assertOrDie(fn != nullptr);

    T in_a[2] = {a, a};
    T out[2]  = {0, 0};

    fn(out, in_a, 2);

    Test::assert(out[0], expected);
    Test::assert(out[0], expected);
}

template <typename Dest, typename Src>
void check_cast(Type::Kind dest_dtype, Type::Kind src_dtype, Src in_val, Dest expected) {
    VecFunctionCast fn = vecGetCast(dest_dtype, src_dtype);
    Test::assertOrDie(fn != nullptr);

    Src  in[2]  = {in_val, in_val};
    Dest out[2] = {0, 0};

    fn(out, in, 2);

    Test::assert(out[0], expected);
    Test::assert(out[0], expected);
}

template <typename T>
void check_fill(Type::Kind dtype, T val) {
    VecFunctionFill fn = vecGetFill(dtype);
    Test::assertOrDie(fn != nullptr);

    T out[3] = {0, 0, 0};
    uint64_t vval = 0;
    *((T*)&vval) = val;

    fn(out, vval, 3);

    Test::assert(out[0], val);
    Test::assert(out[0], val);
    Test::assert(out[0], val);
}




inline Test::Case gVecKernelCases[] = {

    TEST_CASE(test_vec_binary_arithmetic) {
        // Addition (+) across signed, unsigned, and floats
        check_binary<int8_t>  (Type::DT_I8,  OP_ADDITION, 10, 20, 30);
        check_binary<int16_t> (Type::DT_I16, OP_ADDITION, 100, 200, 300);
        check_binary<int32_t> (Type::DT_I32, OP_ADDITION, 1000, 2000, 3000);
        check_binary<int64_t> (Type::DT_I64, OP_ADDITION, 10000LL, 20000LL, 30000LL);
        check_binary<uint8_t> (Type::DT_U8,  OP_ADDITION, 50, 50, 100);
        check_binary<uint16_t>(Type::DT_U16, OP_ADDITION, 500, 500, 1000);
        check_binary<uint32_t>(Type::DT_U32, OP_ADDITION, 5000, 5000, 10000);
        check_binary<uint64_t>(Type::DT_U64, OP_ADDITION, 50000ULL, 50000ULL, 100000ULL);
        check_binary<float>   (Type::DT_F32, OP_ADDITION, 1.5f, 2.5f, 4.0f);
        check_binary<double>  (Type::DT_F64, OP_ADDITION, 10.25, 20.75, 31.0);

        // Subtraction (-)
        check_binary<int32_t>(Type::DT_I32, OP_SUBTRACTION, 50, 20, 30);
        check_binary<float>  (Type::DT_F32, OP_SUBTRACTION, 5.5f, 2.0f, 3.5f);

        // Multiplication (*)
        check_binary<int32_t>(Type::DT_I32, OP_MULTIPLICATION, 6, 7, 42);
        check_binary<double> (Type::DT_F64, OP_MULTIPLICATION, 2.5, 4.0, 10.0);

        // Division (/)
        check_binary<int32_t>(Type::DT_I32, OP_DIVISION, 100, 4, 25);
        check_binary<float>  (Type::DT_F32, OP_DIVISION, 9.0f, 2.0f, 4.5f);

        // Modulo (%) - Integer Only
        check_binary<int32_t>(Type::DT_I32, OP_MODULO, 23, 10, 3);
        check_binary<uint8_t>(Type::DT_U8,  OP_MODULO, 15, 4, 3);
    }},

    TEST_CASE(test_vec_binary_bitwise_and_shifts) {
        // Bitwise AND (&)
        check_binary<uint8_t> (Type::DT_U8,  OP_BITWISE_AND, 0b11110000, 0b10101010, 0b10100000);
        check_binary<int32_t> (Type::DT_I32, OP_BITWISE_AND, 0xFF00, 0x0FF0, 0x0F00);

        // Bitwise OR (|)
        check_binary<uint16_t>(Type::DT_U16, OP_BITWISE_OR, 0xF000, 0x000F, 0xF00F);

        // Bitwise XOR (^)
        check_binary<uint32_t>(Type::DT_U32, OP_BITWISE_XOR, 0xAAAA, 0x5555, 0xFFFF);

        // Shifts (<<, >>)
        check_binary<int32_t> (Type::DT_I32, OP_SHIFT_LEFT,  1, 4, 16);
        check_binary<int32_t> (Type::DT_I32, OP_SHIFT_RIGHT, 32, 2, 8);
    }},

    TEST_CASE(test_vec_binary_comparisons) {
        // Equality (==, !=)
        check_binary<int32_t>(Type::DT_I32, OP_EQUAL, 10, 10, 1);
        check_binary<int32_t>(Type::DT_I32, OP_EQUAL, 10, 20, 0);
        check_binary<float>  (Type::DT_F32, OP_NOT_EQUAL, 1.5f, 2.5f, 1.0f);

        // Relational (<, <=, >, >=)
        check_binary<int32_t>(Type::DT_I32, OP_LESS_THAN, 5, 10, 1);
        check_binary<int32_t>(Type::DT_I32, OP_LESS_THAN, 10, 5, 0);
        check_binary<int32_t>(Type::DT_I32, OP_LESS_THAN_OR_EQUAL, 10, 10, 1);
        check_binary<float>  (Type::DT_F32, OP_GREATER_THAN, 3.5f, 2.0f, 1.0f);
        check_binary<double> (Type::DT_F64, OP_GREATER_THAN_OR_EQUAL, 5.0, 5.0, 1.0);

        // Logical (&&, ||)
        check_binary<int32_t>(Type::DT_I32, OP_BOOL_AND, 1, 0, 0);
        check_binary<int32_t>(Type::DT_I32, OP_BOOL_AND, 1, 1, 1);
        check_binary<int32_t>(Type::DT_I32, OP_BOOL_OR,  1, 0, 1);
    }},

    TEST_CASE(test_vec_scalar_right_and_left) {
        // Vector-Scalar Right (VS): out = a[i] OP s
        check_scalar_r<int32_t>(Type::DT_I32, OP_ADDITION,       10, 5, 15);
        check_scalar_r<int32_t>(Type::DT_I32, OP_SUBTRACTION,    10, 3, 7);
        check_scalar_r<int32_t>(Type::DT_I32, OP_MULTIPLICATION, 4, 5, 20);
        check_scalar_r<float>  (Type::DT_F32, OP_DIVISION,       10.0f, 2.0f, 5.0f);
        check_scalar_r<int32_t>(Type::DT_I32, OP_SHIFT_LEFT,     1, 3, 8);

        // Scalar-Vector Left (SV): out = s OP a[i] (Tests non-commutative ops!)
        check_scalar_l<int32_t>(Type::DT_I32, OP_SUBTRACTION, 100, 30, 70);   // 100 - 30 = 70
        check_scalar_l<int32_t>(Type::DT_I32, OP_DIVISION,    100, 20, 5);    // 100 / 20 = 5
        check_scalar_l<int32_t>(Type::DT_I32, OP_SHIFT_LEFT,  1, 4, 16);      // 1 << 4 = 16
    }},

    TEST_CASE(test_vec_unary_operations) {
        // Unary Plus (+) and Minus (-)
        check_unary<int32_t>(Type::DT_I32, OP_UNARY_PLUS,  25, 25);
        check_unary<int32_t>(Type::DT_I32, OP_UNARY_MINUS, 42, -42);
        check_unary<float>  (Type::DT_F32, OP_UNARY_MINUS, 3.14f, -3.14f);

        // Logical Not (!) and Bitwise Not (~)
        check_unary<int32_t>(Type::DT_I32, OP_NEGATION, 0, 1);
        check_unary<int32_t>(Type::DT_I32, OP_NEGATION, 5, 0);
        check_unary<uint8_t>(Type::DT_U8,  OP_BITWISE_NEGATION, (uint8_t)0x0F, (uint8_t)0xF0);

        // Increment (++) and Decrement (--)
        check_unary<int32_t>(Type::DT_I32, OP_INCREMENT, 10, 11);
        check_unary<int32_t>(Type::DT_I32, OP_DECREMENT, 10, 9);
    }},

    TEST_CASE(test_vec_cast_matrix) {
        // Int-to-Int upcast and downcast
        check_cast<int32_t, int8_t>  (Type::DT_I32, Type::DT_I8,  (int8_t)-5, (int32_t)-5);
        check_cast<int8_t,  int32_t> (Type::DT_I8,  Type::DT_I32, (int32_t)120, (int8_t)120);
        check_cast<uint64_t, uint8_t>(Type::DT_U64, Type::DT_U8,  (uint8_t)250, (uint64_t)250);

        // Float-to-Int truncation and Int-to-Float
        check_cast<int32_t, float>  (Type::DT_I32, Type::DT_F32, 9.75f, 9);
        check_cast<float,   int32_t>(Type::DT_F32, Type::DT_I32, 42, 42.0f);

        // Float-to-Double and Double-to-Float
        check_cast<double, float> (Type::DT_F64, Type::DT_F32, 1.25f, 1.25);
        check_cast<float,  double>(Type::DT_F32, Type::DT_F64, 2.5, 2.5f);
    }},

    TEST_CASE(test_vec_fill_all_types) {
        check_fill<int8_t>  (Type::DT_I8,  (int8_t)-12);
        check_fill<int16_t> (Type::DT_I16, (int16_t)-300);
        check_fill<int32_t> (Type::DT_I32, (int32_t)12345);
        check_fill<int64_t> (Type::DT_I64, (int64_t)987654321LL);
        check_fill<uint8_t> (Type::DT_U8,  (uint8_t)255);
        check_fill<uint16_t>(Type::DT_U16, (uint16_t)65000);
        check_fill<uint32_t>(Type::DT_U32, (uint32_t)1000000);
        check_fill<uint64_t>(Type::DT_U64, (uint64_t)0xFFFFFFFFFFFFULL);
        check_fill<float>   (Type::DT_F32, 3.1415f);
        check_fill<double>  (Type::DT_F64, 2.718281828);
    }},

    TEST_CASE(test_vec_safety_and_invalid_dispatches) {
        // Integer-only operators on Floats/Doubles must return nullptr
        Test::assert(vecGetBinary(Type::DT_F32, OP_MODULO) == nullptr);
        Test::assert(vecGetBinary(Type::DT_F64, OP_MODULO) == nullptr);
        Test::assert(vecGetBinary(Type::DT_F32, OP_BITWISE_AND) == nullptr);
        Test::assert(vecGetBinary(Type::DT_F64, OP_BITWISE_OR) == nullptr);
        Test::assert(vecGetBinary(Type::DT_F32, OP_SHIFT_LEFT) == nullptr);

        // Bitwise Negation on Floats must return nullptr
        Test::assert(vecGetUnary(Type::DT_F32, OP_BITWISE_NEGATION) == nullptr);
        Test::assert(vecGetUnary(Type::DT_F64, OP_BITWISE_NEGATION) == nullptr);

        // Invalid Operator Enum
        Test::assert(vecGetBinary(Type::DT_I32, OP_INVALID) == nullptr);
        Test::assert(vecGetScalarR(Type::DT_I32, OP_INVALID) == nullptr);
        Test::assert(vecGetUnary(Type::DT_I32, OP_INVALID) == nullptr);
    }}

};

extern const Test::Suite gVecKernel = {
    "VecKernel",
    NULL,
    gVecKernelCases,
    sizeof(gVecKernelCases) / sizeof(Test::Case),
    gVecKernelPreCase,
    gVecKernelPostCase,
    gVecKernelPreSuite,
    gVecKernelPostSuite,
};
