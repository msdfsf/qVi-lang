// Constrains:
//  - TokenKind and TokenDetail actual range, one used by
//    enums should not exceed full range of 31 bit, so
//    negative encoded value can represent invalid state.

#pragma once

#include "globals.h"
#include "operators.h"
#include "keywords.h"
#include "data_types.h"
#include <array>
#include <cstdint>

namespace Lex {

    enum TokenKind : int32_t;
    enum TokenDetail : int32_t;

    union Token {
        struct {
            int32_t kind;
            int32_t detail;
        };
        int64_t encoded;
    };

    union TokenValue {
        uint64_t ival;
        float_t  f32;
        double_t f64;
        String*  str;
        void*    any;
    };

    static inline uint64_t packTokens(TokenKind a, TokenKind b) {
        return (b | (a << 8));
    }

    static inline uint16_t packChars(unsigned char a, unsigned char b) {
        return (b | (a << 8));
    }

    static const char EOS = '\0';
    static const char EOL = '\n';

    // As there are a lot of long names
    // group-less literals will be defined without
    // prefixes and other identification stuff...
    static const char POINTER = '^';
    static const char ADDRESS = '&';
    static const char STATEMENT_END = ';';
    static const char STRING_LITERAL = '"';
    static const char RAW_POSTFIX = 'b';
    static const char CHAR_LITERAL = '\'';
    static const char ESCAPE_CHAR = '\\';
    static const char ARRAY_BEGIN = '[';
    static const char ARRAY_END = ']';
    static const char SCOPE_BEGIN = '{';
    static const char SCOPE_END = '}';
    static const char LABEL_END = ':';
    static const char EQUAL = '=';
    static const char SKIP = '_';
    static const char LIST_SEPARATOR = ',';
    static const uint16_t SCOPE_RESOLUTION = packChars(':', ':');

    static const char* KWS_ARRAY_LENGTH = "length";
    static const char* KWS_ARRAY_SIZE = "size";

    inline const char* KWS_VOID = "void";
    inline const char* KWS_I8 = "i8";
    inline const char* KWS_I16 = "i16";
    inline const char* KWS_I32 = "i32";
    inline const char* KWS_I64 = "i64";
    inline const char* KWS_U8 = "u8";
    inline const char* KWS_U16 = "u16";
    inline const char* KWS_U32 = "u32";
    inline const char* KWS_U64 = "u64";
    inline const char* KWS_F32 = "f32";
    inline const char* KWS_F64 = "f64";
    inline const char* KWS_CONST = "const";
    inline const char* KWS_EMBED = "embed";
    inline const char* KWS_FLUID = "fluid";
    inline const char* KWS_STRUCT = "struct";
    inline const char* KWS_UNION = "union";
    inline const char* KWS_IF = "if";
    inline const char* KWS_ELSE = "else";
    inline const char* KWS_LOOP = "loop";
    inline const char* KWS_WHEN = "when";
    inline const char* KWS_CASE = "case";
    inline const char* KWS_ENUM = "enum";
    inline const char* KWS_RETURN = "return";
    inline const char* KWS_CONTINUE = "continue";
    inline const char* KWS_BREAK = "break";
    inline const char* KWS_USING = "using";
    inline const char* KWS_ALLOC = "alloc";
    inline const char* KWS_FREE = "free";
    inline const char* KWS_ERROR = "error";
    inline const char* KWS_CATCH = "catch";
    inline const char* KWS_IMPORT = "import";
    inline const char* KWS_FROM = "from";
    inline const char* KWS_AS = "as";
    inline const char* KWS_AT = "at";
    inline const char* KWS_TRUE = "true";
    inline const char* KWS_FALSE = "false";
    inline const char* KWS_NULL = "null";

    inline const char* keywordStringTable[KW_COUNT] = {
        KWS_VOID,
        KWS_I8,
        KWS_I16,
        KWS_I32,
        KWS_I64,
        KWS_U8,
        KWS_U16,
        KWS_U32,
        KWS_U64,
        KWS_F32,
        KWS_F64,
        KWS_CONST,
        KWS_EMBED,
        KWS_FLUID,
        KWS_STRUCT,
        KWS_UNION,
        KWS_IF,
        KWS_ELSE,
        KWS_LOOP,
        KWS_WHEN,
        KWS_CASE,
        KWS_ENUM,
        KWS_RETURN,
        KWS_CONTINUE,
        KWS_BREAK,
        KWS_USING,
        KWS_ALLOC,
        KWS_FREE,
        KWS_ERROR,
        KWS_CATCH,
        KWS_IMPORT,
        KWS_FROM,
        KWS_AS,
        KWS_AT,
        KWS_TRUE,
        KWS_FALSE,
        KWS_NULL,
    };

    constexpr int KW_TABLE_SIZE = 51;
    constexpr std::array<int, KW_TABLE_SIZE> makeKeywordTable() {
        std::array<int, KW_TABLE_SIZE> table = {};

        table[0] = KW_AT;
        table[1] = KW_NULL;
        table[2] = KW_F64;
        table[3] = KW_U8;
        table[4] = KW_I8;
        table[6] = KW_U64;
        table[7] = KW_FROM;
        table[8] = KW_I64;
        table[9] = KW_TRUE;
        table[10] = KW_BREAK;
        table[11] = KW_ERROR;
        table[12] = KW_AS;
        table[13] = KW_ELSE;
        table[14] = KW_ALLOC;
        table[16] = KW_IMPORT;
        table[17] = KW_U16;
        table[20] = KW_FREE;
        table[23] = KW_LOOP;
        table[24] = KW_UNION;
        table[28] = KW_WHEN;
        table[29] = KW_EMBED;
        table[30] = KW_RETURN;
        table[31] = KW_FALSE;
        table[33] = KW_CATCH;
        table[34] = KW_ENUM;
        table[36] = KW_IF;
        table[38] = KW_I16;
        table[39] = KW_F32;
        table[40] = KW_U32;
        table[42] = KW_CONST;
        table[43] = KW_CONTINUE;
        table[44] = KW_I32;
        table[46] = KW_FLUID;
        table[47] = KW_STRUCT;
        table[48] = KW_VOID;
        table[49] = KW_USING;
        table[50] = KW_CASE;

        return table;
    }

    constexpr auto keywordTable = makeKeywordTable();

    static const char* CDS_NONE = "none";
    static const char* CDS_TEST = "test";

    enum Directive {
        CD_NONE,
        CD_TEST,
        CD_COUNT
    };

    static const char* directivesStringTable[CD_COUNT] = {
        CDS_NONE,
        CDS_TEST,
    };

    constexpr int CD_TABLE_SIZE = 1;
    static const int directivesTable[CD_TABLE_SIZE] = {
        CD_TEST,
    };

    enum TokenKind : int32_t {
        TK_NONE,
        TK_STATEMENT_BEGIN,
        TK_STATEMENT_END,
        TK_KEYWORD,
        TK_DIRECTIVE,
        TK_IDENTIFIER,
        TK_NUMBER,
        TK_STRING,
        TK_CHAR,
        TK_EQUAL,
        TK_SCOPE_RESOLUTION,
        TK_SCOPE_BEGIN,
        TK_SCOPE_END,
        TK_ARRAY_END,
        TK_LIST_SEPARATOR,
        TK_RAW,
        TK_PARENTHESIS_BEGIN,
        TK_PARENTHESIS_END,
        TK_SKIP,
        TK_FILE,

        // If used, detail should be valid OperatorEnum
        TK_BINARY_OPERATOR,
        // Not used as TokenDetail, since some operators may
        // represent other tokens. Such tokens are more
        // convenient to access via TokenKind, so storing
        // operators in this field is preferred.
        // Colliding tokens will be represented by constants
        // with the same value, defined outside of this enum.
        TK_OP_BEGIN,

        TK_OP_PLUS,
        TK_OP_MINUS,
        TK_OP_INCREMENT,
        TK_OP_DECREMENT,
        TK_OP_MULTIPLICATION,
        TK_OP_DIVISION,
        TK_OP_MODULO,
        TK_OP_AND,
        TK_OP_OR,
        TK_OP_XOR,
        TK_OP_NEGATION,
        TK_OP_SHIFT_RIGHT,
        TK_OP_SHIFT_LEFT,
        TK_OP_BOOL_AND,
        TK_OP_BOOL_OR,
        TK_OP_BOOL_NEGATION,
        TK_OP_BOOL_EQUAL,
        TK_OP_BOOL_NOT_EQUAL,
        TK_OP_LESS_THAN,
        TK_OP_LESS_THAN_OR_EQUAL,
        TK_OP_GREATER_THAN,
        TK_OP_GREATER_THAN_OR_EQUAL,
        TK_OP_CONCATENATION,
        TK_OP_MEMBER_SELECTION,
        TK_OP_SUBSCRIPT,
        TK_OP_ARROW,
        TK_OP_ARROW_FAT,

        TK_OP_END,

        TK_END,
    };

    constexpr TokenKind TK_LABEL_END = TK_STATEMENT_BEGIN;
    constexpr TokenKind TK_POINTER = TK_OP_XOR;
    constexpr TokenKind TK_ADDRESS = TK_OP_AND;
    constexpr TokenKind TK_ARRAY_BEGIN = TK_OP_SUBSCRIPT;
    constexpr TokenKind TK_THE_REST = TK_OP_CONCATENATION;
    constexpr TokenKind TK_SLICE = TK_STATEMENT_BEGIN;
    constexpr TokenKind TK_RANGE = TK_STATEMENT_BEGIN;

    // CAUTION: Keyword enum must be aligned with
    //          corresponding token representations
    enum TokenDetail : int32_t {
        TD_NONE = -1,

        TD_KW_BEGIN = KW_VOID,
        // Keyword enum values
        TD_CD_BEGIN = KW_COUNT,
        TD_CD_TEST,
        TD_CD_END,

        TD_DT_VOID,
        TD_DT_I64,
        TD_DT_U64,
        TD_DT_F32,
        TD_DT_F64
    };



    // call in each thread
    void init();
    void release();

    const char* toStr(TokenKind token);



    // 'next' functions will commit changes to span
    Token nextToken(Span* const span, TokenValue* val = NULL);
    Token nextFileName(Span* const span, TokenValue* val = NULL);

    // 'try' functions will not commit changes to span if token is not found
    Token tryToken(Span* const span, Token token, TokenValue* val = NULL);
    Token tryKeyword(Span* const span, Keyword keyword);

    // 'peek' functions will not commit changes to span
    Token peekToken(Span* const span, TokenValue* val = NULL);
    Token peekNthToken(Span* const span, TokenValue* val, unsigned int n);
    Token peekTokenSkipDecorators(Span* const span, TokenValue* val = NULL);

    // 'sync' functions will search the input stream for the first occurrence
    // of any specified token. It skips all preceding tokens and commits changes
    // Order of arguments dictates the priority of the search.
    Token syncToken(Span* const span, Token tokenA, Token tokenB, TokenValue* val = NULL);
    Token syncToken(Span* const span, Token* tokens, uint32_t tokenCount, TokenValue* val = NULL);
    Token syncToken(Span* const span, TokenKind tokenA, TokenKind tokenB, TokenValue* val = NULL);

    unsigned int hash(const char* str);

    int findBlockEnd(Span* const span, const char bCh, const char eCh);



    // Keep them in the header to prevent compiler yelling
    static inline Token toToken(int64_t val) {
        return Token { .encoded = val };
    }

    static inline Token toToken(TokenKind val) {
        return Token { .kind = val };
    }

    static inline Token toToken(TokenDetail val) {
        return Token { .detail = val };
    }

    static inline Token toTokenAsBinaryOperator(OperatorEnum val) {
        return Token{ .kind = TK_BINARY_OPERATOR,.detail = val };
    }

    static inline Type::Kind toDtype(Keyword val) {
        return (Type::Kind) (val - KW_VOID);
    }

    // TODO : make keyword work, and move this to separate
    static inline Type::Kind toDtype(Token val) {

        if (val.kind == TK_KEYWORD) {
            return toDtype((Keyword)val.detail);
        }

        switch (val.detail) {
            case TD_DT_F32: return Type::DT_F32;
            case TD_DT_F64: return Type::DT_F64;
            case TD_DT_I64: return Type::DT_I64;
            case TD_DT_U64: return Type::DT_U64;
            default: return Type::DT_VOID;
        }

    }

    // TODO: clear this operator madness
    static inline OperatorEnum toOperator(TokenValue val) {
        return (OperatorEnum) val.ival;
    }

    static inline OperatorEnum toOperator(Token val) {
        return (OperatorEnum) (val.kind - TK_OP_BEGIN + 1);
    }

    // Converts a Token to its corresponding unary OperatorEnum.
    // Returns OP_NONE if the token cannot represent a unary operator.
    static inline OperatorEnum toUnaryOperator(Token val) {

        switch (val.kind) {

            case TK_OP_PLUS             : return OP_UNARY_PLUS;
            case TK_OP_MINUS            : return OP_UNARY_MINUS;
            case TK_POINTER             : return OP_GET_VALUE;
            case TK_ADDRESS             : return OP_GET_ADDRESS;
            case TK_OP_INCREMENT        : return OP_INCREMENT;
            case TK_OP_DECREMENT        : return OP_DECREMENT;
            case TK_OP_NEGATION         : return OP_BITWISE_NEGATION;
            case TK_OP_BOOL_NEGATION    : return OP_NEGATION;
            default                     : return OP_NONE;

        }

    }

    static inline OperatorEnum toPostfixOperator(Token val) {

        switch (val.kind) {
            case TK_OP_SUBSCRIPT        : return OP_SUBSCRIPT;
            case TK_PARENTHESIS_BEGIN   : return OP_CALL;
            case TK_OP_MEMBER_SELECTION : return OP_MEMBER_SELECTION;
            case TK_OP_INCREMENT        : return OP_INCREMENT;
            case TK_OP_DECREMENT        : return OP_DECREMENT;
            default                     : return OP_NONE;
        }

    }

    static inline OperatorEnum toBinaryOperator(Token val) {

        if (val.kind == TK_BINARY_OPERATOR) return (OperatorEnum) val.detail;

        switch (val.kind) {

            case TK_OP_PLUS                     : return OP_ADDITION;
            case TK_OP_MINUS                    : return OP_SUBTRACTION;
            case TK_OP_MULTIPLICATION           : return OP_MULTIPLICATION;
            case TK_OP_DIVISION                 : return OP_DIVISION;
            case TK_OP_MODULO                   : return OP_MODULO;
            case TK_OP_LESS_THAN                : return OP_LESS_THAN;
            case TK_OP_GREATER_THAN             : return OP_GREATER_THAN;
            case TK_OP_LESS_THAN_OR_EQUAL       : return OP_LESS_THAN_OR_EQUAL;
            case TK_OP_GREATER_THAN_OR_EQUAL    : return OP_GREATER_THAN_OR_EQUAL;
            case TK_OP_BOOL_EQUAL               : return OP_EQUAL;
            case TK_OP_BOOL_NOT_EQUAL           : return OP_NOT_EQUAL;
            case TK_OP_BOOL_AND                 : return OP_BOOL_AND;
            case TK_OP_BOOL_OR                  : return OP_BOOL_OR;
            case TK_OP_CONCATENATION            : return OP_CONCATENATION;
            case TK_OP_MEMBER_SELECTION         : return OP_MEMBER_SELECTION;
            case TK_OP_AND                      : return OP_BITWISE_AND;
            case TK_OP_OR                       : return OP_BITWISE_OR;
            case TK_OP_XOR                      : return OP_BITWISE_XOR;
            case TK_OP_NEGATION                 : return OP_BITWISE_NEGATION;
            case TK_OP_SHIFT_RIGHT              : return OP_SHIFT_RIGHT;
            case TK_OP_SHIFT_LEFT               : return OP_SHIFT_LEFT;
            case TK_ARRAY_BEGIN                 : return OP_SUBSCRIPT;
            case TK_OP_ARROW                    : return OP_CAST_STATIC;
            case TK_OP_ARROW_FAT                : return OP_CAST_BIT;
            default                             : return OP_NONE;

        }

    }

    static inline int isKeyword(Token token, Keyword keyword) {
        return (token.kind == TK_KEYWORD && token.detail == keyword);
    }

    static inline int isDtype(TokenDetail val) {
        return (val >= KW_VOID && val <= KW_F64);
    }

    static inline int isDtype(Token val) {
        return ((val.kind == Lex::TK_KEYWORD) && isDtype((TokenDetail) val.detail));
    }

    inline bool isQualifier(Token val) {
        return val.detail == KW_EMBED ||
               val.detail == KW_CONST ||
               val.detail == KW_FLUID ||
               val.detail == KW_ALLOC;
    }

    inline Type::Qualifier toQualifier(Keyword val) {
        switch (val) {
            case KW_EMBED: return Type::Qualifier::Q_EMBED;
            case KW_CONST: return Type::Qualifier::Q_CONST;
            case KW_FLUID: return Type::Qualifier::Q_FLUID;
            case KW_ALLOC: return Type::Qualifier::Q_ALLOC;
            default: return Type::Qualifier::Q_NONE;
        }
    }

    static inline int isInt(Keyword val) {
        return (val >= KW_I8 && val <= KW_U64);
    }
    static inline int isOperator(Token val) {
        return (val.kind > TK_OP_BEGIN && val.kind < TK_OP_END);
    }

    static inline int isPostfixOperator(Token val) {
        return val.kind == TK_OP_INCREMENT || val.kind == TK_OP_DECREMENT;
    }

    static inline int compareChars(const char* str, uint16_t ch) {
        return (str[0] == (char)(ch & 0xFF)) && (str[1] == (char)(ch >> 8));
    }

    template <typename T>
    static inline uint64_t toIntStr(T ch) {
        uint64_t tmp = 0;
        memcpy(&tmp, &ch, sizeof(T));
        return tmp;
    }

}
