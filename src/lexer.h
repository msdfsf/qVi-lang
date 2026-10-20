// Constrains:
//  - TokenKind and TokenDetail actual range, one used by
//    enums should not exceed full range of 31 bit, so
//    negative encoded value can represent invalid state.

#pragma once

#include "globals.h"
#include "operators.h"
#include "keywords.h"
#include "data_types.h"
#include "array_list.h"
#include <array>
#include <cstdint>

namespace Lex {
    // NOTE: if token kind is error, size contains error value

    constexpr uint64_t INVALID_INDEX = -1LL;

    enum TokenKind   : int16_t;
    enum TokenDetail : int16_t;

    struct TokenCore {
        TokenKind   kind;
        TokenDetail detail;
    };

    union Token {
        struct {
            union {
                struct {
                    int16_t  kind;
                    int16_t  detail;
                };
                TokenCore core;
            };
            uint32_t size;
        };
        uint64_t encoded;
    };

    union Value {
        uint64_t i64;
        float_t  f32;
        double_t f64;
        char*    str;
        void*    any;
    };

    struct TokenEx {
        Token token;
        Value value;
    };
    static_assert(sizeof(TokenEx) <= 16, "TokenEx shall be max 16 bytes as we want to use it as return value.");



    constexpr TokenCore packCore(int16_t kind, int16_t detail) {
        return (TokenCore) { .kind = (TokenKind) kind, .detail = (TokenDetail) detail };
    }

    // Compile-time helpers to pack ASCII characters into 16, 32, 64 bit 'wchars'
    constexpr uint16_t W16(char a, char b) {
        return (uint16_t) ((uint8_t) a | ((uint8_t) b << 8));
    }

    constexpr uint32_t W32(char a, char b, char c, char d) {
        return (uint32_t) ((uint8_t) a | ((uint8_t) b << 8) | ((uint8_t) c << 16) | ((uint8_t) d << 24));
    }

    constexpr uint64_t W64(char a, char b, char c, char d, char e, char f, char g, char h) {
        return (uint64_t) W32(a, b, c, d) | ((uint64_t) W32(e, f, g, h) << 32);
    }



    inline const char EOS = '\0';
    inline const char EOL = '\n';

    // As there are a lot of long names
    // group-less literals will be defined without
    // prefixes and other identification stuff...
    inline const char POINTER        = '^';
    inline const char ADDRESS        = '&';
    inline const char STATEMENT_END  = ';';
    inline const char STRING_LITERAL = '"';
    inline const char RAW_POSTFIX    = 'b';
    inline const char UNI_POSTFIX    = 'u';
    inline const char CHAR_LITERAL   = '\'';
    inline const char ESCAPE_CHAR    = '\\';
    inline const char ARRAY_BEGIN    = '[';
    inline const char ARRAY_END      = ']';
    inline const char SCOPE_BEGIN    = '{';
    inline const char SCOPE_END      = '}';
    inline const char LABEL_END      = ':';
    inline const char EQUAL          = '=';
    inline const char SKIP           = '_';
    inline const char LIST_SEPARATOR = ',';
    inline const uint16_t SCOPE_RESOLUTION = W16(':', ':');

    inline const char* KWS_ARRAY_LENGTH = "length";
    inline const char* KWS_ARRAY_SIZE   = "size";

    inline const char* KWS_VOID     = "void";
    inline const char* KWS_I8       = "i8";
    inline const char* KWS_I16      = "i16";
    inline const char* KWS_I32      = "i32";
    inline const char* KWS_I64      = "i64";
    inline const char* KWS_U8       = "u8";
    inline const char* KWS_U16      = "u16";
    inline const char* KWS_U32      = "u32";
    inline const char* KWS_U64      = "u64";
    inline const char* KWS_F32      = "f32";
    inline const char* KWS_F64      = "f64";
    inline const char* KWS_TYPE     = "type";
    inline const char* KWS_CONST    = "const";
    inline const char* KWS_EMBED    = "embed";
    inline const char* KWS_STRUCT   = "struct";
    inline const char* KWS_UNION    = "union";
    inline const char* KWS_IF       = "if";
    inline const char* KWS_ELSE     = "else";
    inline const char* KWS_LOOP     = "loop";
    inline const char* KWS_WHEN     = "when";
    inline const char* KWS_CASE     = "case";
    inline const char* KWS_ENUM     = "enum";
    inline const char* KWS_RETURN   = "return";
    inline const char* KWS_CONTINUE = "continue";
    inline const char* KWS_BREAK    = "break";
    inline const char* KWS_USING    = "using";
    inline const char* KWS_ALLOC    = "alloc";
    inline const char* KWS_ERROR    = "error";
    inline const char* KWS_CATCH    = "catch";
    inline const char* KWS_DEFER    = "defer";
    inline const char* KWS_IMPORT   = "import";
    inline const char* KWS_AS       = "as";
    inline const char* KWS_AT       = "at";
    inline const char* KWS_TRUE     = "true";
    inline const char* KWS_FALSE    = "false";
    inline const char* KWS_NULL     = "null";

    inline const char* CDS_IF      = "if";
    inline const char* CDS_LOOP    = "loop";
    inline const char* CDS_PACKED  = "packed";
    inline const char* CDS_REORDER = "reorder";
    inline const char* CDS_FORMAT  = "format";



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
        KWS_TYPE,
        KWS_CONST,
        KWS_EMBED,
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
        KWS_ERROR,
        KWS_CATCH,
        KWS_DEFER,
        KWS_IMPORT,
        KWS_AS,
        KWS_AT,
        KWS_TRUE,
        KWS_FALSE,
        KWS_NULL,
    };

    inline const char* directiveStringTable[CD_COUNT] = {
        NULL,       // CD_NONE
        CDS_IF,
        CDS_LOOP,
        CDS_PACKED,
        CDS_REORDER,
        CDS_FORMAT,
    };



    enum Error {
        ERR_UNTERMINATED_COMMENT,
        ERR_UNTERMINATED_STRING,
        ERR_INVALID_CHAR_ENCODING,
        ERR_UNEXPECTED_SYMBOL,
    };

    enum TokenKind : int16_t {
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

        // Compound tokens. Their trailing part is already a single
        // char token, so the fused form gets its own kind:
        TK_SIGNATURE_BEGIN,      // "^("
        TK_SCOPE_LITERAL_BEGIN,  // ".{"
        TK_ARRAY_LITERAL_BEGIN,  // ".["
        TK_STRING_INFER,         // ".\"...\"", alternative to TK_STRING, spans the whole literal
        TK_VARIADIC,             // "..."

        TK_SKIP,
        TK_FILE,
        TK_COMMENT,
        TK_ERROR,

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
    enum TokenDetail : int16_t {
        TD_NONE     = -1,

        TD_KW_BEGIN = KW_VOID,
        // Keyword enum values
        TD_CD_BEGIN = KW_COUNT,
        // Directive enum values (CD_NONE lands on TD_CD_BEGIN)
        TD_CD_IF,
        TD_CD_LOOP,
        TD_CD_PACKED,
        TD_CD_REORDER,
        TD_CD_FORMAT,
        TD_CD_END,

        TD_DT_VOID,
        TD_DT_I64,
        TD_DT_U64,
        TD_DT_F32,
        TD_DT_F64
    };



    // Core functions
    // ---

    // Core function that is used internally to fill batch buffer
    // Receives a pos relative to given str and aligns it to the next token start
    TokenEx nextToken(const char* const str, Pos* const pos);

    // 'next' functions will commit changes to span
    TokenEx nextToken   (Span* const span);
    TokenEx nextFileName(Span* const span);

    // Resets per-thread batch state; 'release' is there for symmetry
    void init();
    void release();

    // 'try' functions will not commit changes to span if token is not found
    TokenEx tryToken  (Span* const span, Token token);
    TokenEx tryKeyword(Span* const span, Keyword keyword);

    // 'peek' functions will not commit changes to span
    TokenEx peekToken   (Span* const span);
    TokenEx peekNthToken(Span* const span, const uint32_t n);

    // 'sync' functions will search the input stream for the first occurrence
    // of any specified token. It skips all preceding tokens and commits changes
    // Order of arguments dictates the priority of the search.
    TokenEx syncToken(Span* const span, const Token tokenA,     const Token tokenB,        const bool stopOnNewline = false);
    TokenEx syncToken(Span* const span, const Token* tokens,    const uint32_t tokenCount, const bool stopOnNewline = false);
    TokenEx syncToken(Span* const span, const TokenKind tokenA, const TokenKind tokenB,    const bool stopOnNewline = false);

    TokenCore lookupKeyword  (const char* str, int len);
    TokenCore lookupDirective(const char* str, int len);



    // Conversion helpers
    // ---

    inline Error toError(Token token) {
        return (Error) token.size;
    }

    inline Token toToken(uint64_t val) {
        return Token { .encoded = val };
    }

    inline Token toToken(TokenKind val) {
        return Token { .kind = val };
    }

    inline Token toToken(TokenDetail val) {
        return Token { .detail = val };
    }

    inline TokenEx toTokenEx(TokenKind val) {
        return TokenEx { .token { .kind = val } };
    }

    inline Token toTokenAsBinaryOperator(OperatorEnum val) {
        return Token{ .kind = TK_BINARY_OPERATOR, .detail = val };
    }

    inline Type::Kind toType(Keyword val) {
        return (Type::Kind) (val - KW_I8 + Type::DT_I8);
    }

    inline Type::Kind toType(Token val) {
        if (val.kind == TK_KEYWORD) {
            return toType((Keyword)val.detail);
        }

        switch (val.detail) {
            case TD_DT_F32: return Type::DT_F32;
            case TD_DT_F64: return Type::DT_F64;
            case TD_DT_I64: return Type::DT_I64;
            case TD_DT_U64: return Type::DT_U64;
            default: return Type::DT_VOID;
        }
    }

    inline OperatorEnum toOperator(Token token) {
        return (OperatorEnum) (token.kind - TK_OP_BEGIN + 1);
    }

    // Converts a Token to its corresponding unary OperatorEnum.
    // Returns OP_NONE if the token cannot represent a unary operator.
    inline OperatorEnum toUnaryOperator(Token val) {
        switch (val.kind) {

            case TK_OP_PLUS             : return OP_UNARY_PLUS;
            case TK_OP_MINUS            : return OP_UNARY_MINUS;
            case TK_POINTER             : return OP_GET_VALUE;
            case TK_ADDRESS             : return OP_GET_ADDRESS;
            case TK_OP_NEGATION         : return OP_BITWISE_NEGATION;
            case TK_OP_BOOL_NEGATION    : return OP_NEGATION;
            default                     : return OP_NONE;

        }
    }

    inline OperatorEnum toPostfixOperator(Token val) {
        switch (val.kind) {
            case TK_OP_SUBSCRIPT        : return OP_SUBSCRIPT;
            case TK_PARENTHESIS_BEGIN   : return OP_CALL;
            case TK_OP_MEMBER_SELECTION : return OP_MEMBER_SELECTION;
            default                     : return OP_NONE;
        }
    }

    inline OperatorEnum toBinaryOperator(Token val) {
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

    inline Type::Qualifier toQualifier(Keyword val) {
        switch (val) {
            case KW_EMBED: return Type::Qualifier::Q_EMBED;
            case KW_CONST: return Type::Qualifier::Q_CONST;
            case KW_ALLOC: return Type::Qualifier::Q_ALLOC;
            default: return Type::Qualifier::Q_NONE;
        }
    }

    template <typename T>
    static inline uint64_t toIntStr(T ch) {
        uint64_t tmp = 0;
        memcpy(&tmp, &ch, sizeof(T));
        return tmp;
    }



    // Checks helpers
    // ---

    static inline bool isKeyword(Token token, Keyword keyword) {
        return (token.kind == TK_KEYWORD && token.detail == keyword);
    }

    static inline bool isType(TokenDetail val) {
        return (val >= KW_I8 && val <= KW_F64);
    }

    static inline bool isType(Token val) {
        return ((val.kind == Lex::TK_KEYWORD) && isType((TokenDetail) val.detail));
    }

    static inline bool isInt(Keyword val) {
        return (val >= KW_I8 && val <= KW_U64);
    }

    inline bool isQualifier(Token val) {
        return val.detail == KW_EMBED ||
               val.detail == KW_CONST ||
               val.detail == KW_ALLOC;
    }

    static inline bool isOperator(Token val) {
        return (val.kind > TK_OP_BEGIN && val.kind < TK_OP_END);
    }

    inline bool isUnicodeEscape(const char* const str) {
        return str[0] == Lex::ESCAPE_CHAR && (str[1] == 'u' || str[1] == 'U');
    }

    inline bool match(Token a, Token b) {
        return a.kind == b.kind && a.detail == b.detail;
    }



    // Parse helpers
    // ---

    TokenDetail parseNumber(const char* const str, uint32_t* len, uint64_t* out);

    uint64_t parseInt   (const char* const str, uint32_t* idx);
    uint64_t parseIntHex(const char* const str, uint32_t* idx);

    int parseChar       (const char* const str, uint32_t* idx);
    int parseEscapeChar (const char* const str, uint32_t* idx);
    int parseCharLiteral(const char* const str, uint32_t* len, uint64_t* out);

    int32_t findBlockEnd       (Span* const span, const char bCh, const char eCh);
    int32_t findBlockEndEscaped(Span* const span, const char bCh, const char eCh);

    int32_t findStringEnd(const char* const str);



    // MISC
    // ---

    const char* str(TokenKind token);

    void write(IO::Stream* const stream, Token token);
    void write(IO::Stream* const stream, TokenEx token);

}
