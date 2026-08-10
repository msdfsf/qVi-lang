#pragma once

#include "globals.h"
#include <cstdint>

struct Operator {

    // precedence of operator, zero->positive-whatever, high->low *(precedence seems too long for usage)
    int rank;
    uint64_t flag;

};

enum OperatorEnum : uint8_t {
    OP_NONE = (uint8_t) -1,

    OP_UNARY_BEGIN = 0,
    OP_UNARY_PLUS = OP_UNARY_BEGIN,
    OP_UNARY_MINUS,
    OP_GET_ADDRESS,
    OP_GET_VALUE,
    OP_NEGATION,
    OP_BITWISE_NEGATION,
    OP_INCREMENT,
    OP_DECREMENT,
    OP_UNARY_END = OP_DECREMENT,

    OP_BINARY_BEGIN,
    OP_SUBSCRIPT = OP_BINARY_BEGIN,
    OP_CALL,
    OP_MEMBER_SELECTION,
    OP_DEREFERENCE_MEMBER_SELECTION,

    OP_MULTIPLICATION,
    OP_DIVISION,
    OP_MODULO,

    OP_ADDITION,
    OP_SUBTRACTION,
    OP_CONCATENATION,

    OP_SHIFT_RIGHT,
    OP_SHIFT_LEFT,

    OP_LESS_THAN,
    OP_GREATER_THAN,
    OP_LESS_THAN_OR_EQUAL,
    OP_GREATER_THAN_OR_EQUAL,

    OP_EQUAL,
    OP_NOT_EQUAL,

    OP_BITWISE_AND,
    OP_BITWISE_XOR,
    OP_BITWISE_OR,

    OP_BOOL_AND,
    OP_BOOL_OR,
    OP_BINARY_END = OP_BOOL_OR,

    // OP_CAST // to tie cast to dtype, not sure about it as operator, but lets see
    OP_COUNT,
    OP_INVALID
};

// indexed by OperatorEnum
constexpr Operator operators[] {
    // --- UNARY ---
    { .rank = 1, .flag = IS_UNARY | IS_ONE_CHAR }, // OP_UNARY_PLUS
    { .rank = 1, .flag = IS_UNARY | IS_ONE_CHAR }, // OP_UNARY_MINUS
    { .rank = 1, .flag = IS_UNARY | IS_ONE_CHAR }, // OP_GET_ADDRESS
    { .rank = 1, .flag = IS_UNARY | IS_ONE_CHAR }, // OP_GET_VALUE
    { .rank = 1, .flag = IS_UNARY | IS_ONE_CHAR }, // OP_NEGATION
    { .rank = 1, .flag = IS_UNARY | IS_ONE_CHAR }, // OP_BITWISE_NEGATION
    { .rank = 1, .flag = IS_UNARY | IS_TWO_CHAR }, // OP_INCREMENT
    { .rank = 1, .flag = IS_UNARY | IS_TWO_CHAR }, // OP_DECREMENT

    // --- BINARY ---
    { .rank = 0, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_SUBSCRIPT
    { .rank = 0, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_CALL
    { .rank = 0, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_MEMBER_SELECTION
    { .rank = 0, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_DEREF_MEMBER_SELECTION

    { .rank = 2, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_MULTIPLICATION
    { .rank = 2, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_DIVISION
    { .rank = 2, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_MODULO

    { .rank = 3, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_ADDITION
    { .rank = 3, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_SUBTRACTION
    { .rank = 3, .flag = IS_BINARY | IS_TWO_CHAR }, // OP_CONCATENATION

    { .rank = 4, .flag = IS_BINARY | IS_TWO_CHAR }, // OP_SHIFT_RIGHT
    { .rank = 4, .flag = IS_BINARY | IS_TWO_CHAR }, // OP_SHIFT_LEFT

    { .rank = 5, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_LESS_THAN
    { .rank = 5, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_GREATER_THAN
    { .rank = 5, .flag = IS_BINARY | IS_TWO_CHAR }, // OP_LESS_THAN_OR_EQUAL
    { .rank = 5, .flag = IS_BINARY | IS_TWO_CHAR }, // OP_GREATER_THAN_OR_EQUAL

    { .rank = 6, .flag = IS_BINARY | IS_TWO_CHAR }, // OP_EQUAL
    { .rank = 6, .flag = IS_BINARY | IS_TWO_CHAR }, // OP_NOT_EQUAL

    { .rank = 7, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_BITWISE_AND
    { .rank = 8, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_BITWISE_XOR
    { .rank = 9, .flag = IS_BINARY | IS_ONE_CHAR }, // OP_BITWISE_OR

    { .rank = 10, .flag = IS_BINARY | IS_TWO_CHAR }, // OP_BOOL_AND
    { .rank = 11, .flag = IS_BINARY | IS_TWO_CHAR }  // OP_BOOL_OR
};
static_assert(sizeof(operators) / sizeof(Operator) == OP_COUNT, "Operator array size mismatch!");

inline int isMemberSelection(OperatorEnum op) {
    return op == OP_MEMBER_SELECTION || op == OP_DEREFERENCE_MEMBER_SELECTION;
}

// TODO : to namespace
const char* OperatorToStr(OperatorEnum op);
