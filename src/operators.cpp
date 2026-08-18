#include "operators.h"

const char* OperatorToStr(OperatorEnum op) {
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
        case OP_SLICE:                         return "slice";
        case OP_MEMBER_SELECTION:              return "member_selection";
        case OP_DEREFERENCE_MEMBER_SELECTION:  return "dereference_member_selection";
        case OP_NEGATION:                      return "neg";
        case OP_CONCATENATION:                 return "cat";
        case OP_COUNT:                         return "count";
        case OP_INVALID:                       return "invalid";
        default:                               return "nop";
    }
}
