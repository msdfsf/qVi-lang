#pragma once
#include "stdint.h"

struct Span;
struct AstContext;

namespace Err {
    enum Err : int64_t {
        OK                              = 0,
        MALLOC                          = -1,
        UNEXPECTED_END_OF_FILE          = -2,
        UNEXPECTED_END_OF_EXPRESSION    = -3,
        INVALID_OPERATOR                = -4,
        VARIABLE_ALREADY_DEFINED        = -5,
        FUNCTION_ALREADY_DEFINED        = -6,
        UNEXPECTED_SYMBOL               = -7,
        UNKNOWN_VARIABLE                = -8,
        UNKNOWN_FUNCTION                = -9,
        INVALID_NUMBER_LITERAL          = -10,
        INVALID_TYPE_CONVERSION         = -11,
        MISSING_VARIABLE_NAME           = -12,
        INVALID_TYPE_UNARY_OPERATOR     = -13,
        INVALID_TYPE_BINARY_OPERATOR    = -14,
        CANNOT_ASSIGN_TO_CONST          = -15,
        UNKNOWN_DATA_TYPE               = -16,
        INVALID_DATA_TYPE               = -17,
        COMPILE_TIME_KNOWN_EXPRESSION_REQUIRED = -18,
        TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH    = -19,
        INVALID_ATTRIBUTE_NAME          = -20,
        OPERATOR_CANNOT_ACT_AS_UNARY    = -21,
        OPERATOR_CANNOT_ACT_AS_BINARY   = -22,
        CANNOT_GET_ADDRESS_OF_ADDRESS   = -23,
        INVALID_USAGE_OF_OPERATOR       = -24,
        CANNOT_EVALUATE                 = -25,
        INVALID_EMBED_ARRAY_SIZE        = -26,
        UNKNOWN_QUALIFIER               = -27,
        ARRAY_EXPECTED                  = -28,
        TOO_MANY_ARGUMENTS              = -29,
        APPROPRIATE_TAG_DOES_NOT_EXISTS   = -30,
        BUFFER_OVERRUN                  = -31,
        SYSTEM_COMMAND_EXECUTION_FAILED    = -32,
        INVALID_LVALUE                  = -33,
        INVALID_VARIABLE_NAME           = -34,
        NOT_ENOUGH_ARGUMENTS            = -35,
        INVALID_DECLARATION_PLACE       = -36,
        UNKNOWN_NAMESPACE               = -37,
        INVALID_NUMBER_OF_ATTRIBUTES    = -38,
        UNSUPPORTED_ESCAPE_SEQUENCE     = -39,
        DATA_TYPE_SIZE_EXCEEDED         = -40,
        UNEXPECTED_ERROR                = -41,
        ARRAY_SIZE_MISMATCH             = -42,
        NO_MATCHING_FUNCTION_FOUND      = -43,
        MORE_THAN_ONE_OVERLOAD_MATCH    = -44,
        UNEXPECTED_RVALUE               = -45,
        UNKNOWN_ERROR_SET               = -46,
        INVALID_RVALUE                  = -47,
        SYMBOL_ALREADY_DEFINED          = -48,
        TCC_ERROR                       = -49,
        UNTERMINATED_COMMENT            = -50,
        INVALID_ARRAY_LENGTH            = -51,
        GLOBAL_SCOPE_REQUIRED           = -52,
        INVALID_DECLARATION_ORDER       = -53,
        CIRCULAR_IMPORT                 = -54,
        FILE_DOES_NOT_EXISTS            = -55,
        UNKNOWN_DIRECTIVE               = -56,
        INVALID_ARGUMENTS               = -57,
        NOT_YET_IMPLEMENTED             = -58,
        IO_ERROR                        = -59,
        MAX_FILE_PATH_EXCEEDED          = -60,
        FILE_LOAD_FAILED                = -61,
        DIVISION_BY_ZERO                = -62,
        IMPORT_NOT_GLOBAL               = -63,
        NAMESPACE_NOT_GLOBAL            = -64,
        QUALIFIED_NAME_NOT_ALLOWED      = -65,
        DECLARATION_AFTER_USE           = -66,
        FOREIGN_LIBRARY_NOT_FOUND       = -67,
        PATH_TOO_LONG                   = -68,
        SYMBOL_NOT_FOUND                = -69,
        LIBRARY_LOAD_FAILED             = -70,
        COUNT                           = 71,
    };
    const char* const str(Err code);
    bool isFatal(Err err);
}

namespace Wrn {
    enum Wrn :int64_t {
        UNUSED_VARIABLE   = -1,
        SHADOWED_VARIABLE = -2,
        DIVISION_BY_ZERO  = -3,
        SMALLER_DTYPE_CAN_BE_USED = -4,
    };
    const char* const str(Wrn code);
}

namespace Inf {
    enum Inf : int64_t {
        TMP_INFO = -1,
    };
    const char* const str(Inf code);
}

namespace Diag {

    struct Format {
        const char* fmt;
    };

    void report(AstContext* ast, Span* span, Err::Err code, ...);
    void report(AstContext* ast, Span* span, Wrn::Wrn code, ...);
    void report(AstContext* ast, Span* span, Inf::Inf code, ...);

    void report(AstContext* ast, Span* span, Err::Err code, Format fmt, ...);
    void report(AstContext* ast, Span* span, Wrn::Wrn code, Format fmt, ...);
    void report(AstContext* ast, Span* span, Inf::Inf code, Format fmt, ...);

    void report(AstContext* ast, Span** span, Err::Err code, Format* fmt, ...);
    void report(AstContext* ast, Span** span, Wrn::Wrn code, Format* fmt, ...);
    void report(AstContext* ast, Span** span, Inf::Inf code, Format* fmt, ...);

    void commit(AstContext* ast, Span* span, Err::Err code);
    void commit(AstContext* ast, Span* span, Wrn::Wrn code);
    void commit(AstContext* ast, Span* span, Inf::Inf code);

}
