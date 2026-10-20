#include "lexer.h"
#include "data_types.h"
#include "globals.h"
#include "io.h"
#include "keywords.h"
#include "logger.h"
#include "diagnostic.h"
#include "config.h"

#include <cstdint>

//


// Expects null terminated buffer
namespace Lex {
    constexpr int32_t CHAR_EOS = -1;

    Logger::Type logErr = { .level = Logger::ERROR, .tag = "lexer" };

    struct BatchToken {
        TokenEx base;
        Pos     pos;
    };

    thread_local int batchSize = 0;
    thread_local int batchRead = 0;
    thread_local BatchToken batchBuffer[Config::TOKEN_BATCH_BUFFER_SIZE] = { 0 };

    // Have to be >= 200 so they don't collide with single-char tokens
    enum DispatchTag : uint8_t {
        DT_IDENT = 200, // Starts 'a'-'z', 'A'-'Z', '_'
        DT_NUM   = 201, // Starts '0'-'9'
        DT_STR   = 202, // Starts '"'
        DT_CH    = 203, // Starts '\''
        DT_OP    = 204, // Multi-char operators: '=', '+', '-', ':', etc.
        DT_SPC   = 205, // Whitespace: ' ', '\t', '\r', '\n'
        DT_EOF   = 206, // '\0'
        DT_ERR   = 207, // Illegal character
    };

    constexpr std::array<uint8_t, 256> makeDispatchTable() {
        std::array<uint8_t, 256> table = {};

        for (int i = 0; i < 256; i++) {
            table[i] = DT_ERR;
        }

        table[0] = DT_EOF;

        // Whitespace
        table[' ']  = DT_SPC;
        table['\t'] = DT_SPC;
        table['\r'] = DT_SPC;
        table['\n'] = DT_SPC;
        table['\v'] = DT_SPC;
        table['\f'] = DT_SPC;

        // Identifiers
        for (int ch = 'a'; ch <= 'z'; ch++) table[ch] = DT_IDENT;
        for (int ch = 'A'; ch <= 'Z'; ch++) table[ch] = DT_IDENT;
        table['_'] = DT_IDENT;

        // Numbers
        for (int ch = '0'; ch <= '9'; ch++) table[ch] = DT_NUM;

        // Strings
        table['"']  = DT_STR;
        table['\''] = DT_CH;

        // Multi-char symbols
        table['='] = DT_OP;
        table['+'] = DT_OP;
        table['-'] = DT_OP;
        table['*'] = DT_OP;
        table['/'] = DT_OP;
        table['%'] = DT_OP;
        table['<'] = DT_OP;
        table['>'] = DT_OP;
        table['!'] = DT_OP;
        table['&'] = DT_OP;
        table['|'] = DT_OP;
        table['^'] = DT_OP;
        table['~'] = DT_OP;
        table['.'] = DT_OP;
        table[':'] = DT_OP;

        // Single char tokens
        table[';'] = Lex::TK_STATEMENT_END;
        table[','] = Lex::TK_LIST_SEPARATOR;
        table['('] = Lex::TK_PARENTHESIS_BEGIN;
        table[')'] = Lex::TK_PARENTHESIS_END;
        table['{'] = Lex::TK_SCOPE_BEGIN;
        table['}'] = Lex::TK_SCOPE_END;
        table['['] = Lex::TK_ARRAY_BEGIN;
        table[']'] = Lex::TK_ARRAY_END;

        return table;
    }

    static const auto CHAR_DISPATCH = makeDispatchTable();



    const char* str(TokenKind token) {
        switch (token) {
            case TK_NONE: return "none";
            case TK_IDENTIFIER: return "identifier";
            case TK_KEYWORD: return "keyword";
            case TK_CHAR: return "char";
            case TK_STRING: return "string";
            case TK_NUMBER: return "number";
            case TK_FILE: return "file";

            case TK_OP_PLUS: return "+";
            case TK_OP_ARROW: return "->";
            case TK_OP_ARROW_FAT: return "=>";
            case TK_OP_MINUS: return "-";
            case TK_OP_MULTIPLICATION: return "*";
            case TK_OP_DIVISION: return "/";
            case TK_OP_MODULO: return "%";

            case TK_OP_BOOL_NOT_EQUAL: return "!=";
            case TK_OP_BOOL_NEGATION: return "!";
            case TK_OP_BOOL_EQUAL: return "==";
            case TK_EQUAL: return "=";

            case TK_OP_LESS_THAN: return "<";
            case TK_OP_LESS_THAN_OR_EQUAL: return "<=";
            case TK_OP_GREATER_THAN: return ">";
            case TK_OP_GREATER_THAN_OR_EQUAL: return ">=";

            case TK_OP_SHIFT_LEFT: return "<<";
            case TK_OP_SHIFT_RIGHT: return ">>";

            case TK_OP_AND: return "&";
            case TK_OP_BOOL_AND: return "&&";
            case TK_OP_OR: return "|";
            case TK_OP_BOOL_OR: return "||";
            case TK_OP_XOR: return "^";
            case TK_OP_NEGATION: return "~";

            case TK_DIRECTIVE: return "#";

            case TK_OP_CONCATENATION: return "..";
            case TK_OP_MEMBER_SELECTION: return ".";

            case TK_SCOPE_RESOLUTION: return "::";
            case TK_STATEMENT_BEGIN: return ":";
            case TK_STATEMENT_END: return ";";

            case TK_SCOPE_BEGIN: return "{";
            case TK_SCOPE_END: return "}";

            case TK_ARRAY_BEGIN: return "[";
            case TK_ARRAY_END: return "]";

            case TK_SKIP: return "_";
            case TK_LIST_SEPARATOR: return ",";

            case TK_PARENTHESIS_BEGIN: return "(";
            case TK_PARENTHESIS_END: return ")";

            case TK_SIGNATURE_BEGIN: return "^(";
            case TK_SCOPE_LITERAL_BEGIN: return ".{";
            case TK_ARRAY_LITERAL_BEGIN: return ".[";
            case TK_STRING_INFER: return "string-infer";
            case TK_VARIADIC: return "...";

            case TK_END: return "EOF";

            default: return "unknown";
        }
    }

    void write(IO::Stream* const stream, Token token) {
        switch (token.kind) {
            case TK_KEYWORD: {
                IO::write(stream, str((Keyword) token.detail));
                break;
            }

            case TK_NUMBER: {
                IO::writef(stream, "number<%s>", str(toType(token)));
                break;
            }

            default: {
                IO::write(stream, str(token.core.kind));
                break;
            }
        }
    }

    void write(IO::Stream* const stream, TokenEx token) {
        switch (token.token.kind) {
            case TK_KEYWORD: {
                IO::write(stream, str((Keyword) token.token.detail));
                break;
            }

            case TK_NUMBER: {
                switch (token.token.detail) {
                    case TD_DT_U64: {
                        IO::writef(stream, "<%s:%llu>", str(toType(token.token)), token.value.i64);
                        break;
                    }

                    case TD_DT_I64: {
                        IO::writef(stream, "<%s:%lli>", str(toType(token.token)), token.value.i64);
                        break;
                    }

                    case TD_DT_F32: {
                        IO::writef(stream, "<%s:%f>", str(toType(token.token)), token.value.f32);
                        break;
                    }

                    case TD_DT_F64: {
                        IO::writef(stream, "<%s:%f>", str(toType(token.token)), token.value.f64);
                        break;
                    }
                }
                break;
            }

            case TK_IDENTIFIER: {
                IO::write(stream, token.value.str, token.token.size);
            }

            default: {
                IO::write(stream, str(token.token.core.kind));
                break;
            }
        }
    }

    inline TokenCore lookupKeyword(const char* str, int len) {
        if (len < 2 || len > 8) return { TK_IDENTIFIER, TD_NONE };

        switch (len) {
            case 2: {
                uint16_t w = *(const uint16_t*) str;
                if (w == W16('i', '8')) return packCore(TK_KEYWORD, KW_I8);
                if (w == W16('u', '8')) return packCore(TK_KEYWORD, KW_U8);
                if (w == W16('i', 'f')) return packCore(TK_KEYWORD, KW_IF);
                if (w == W16('a', 's')) return packCore(TK_KEYWORD, KW_AS);
                if (w == W16('a', 't')) return packCore(TK_KEYWORD, KW_AT);
                break;
            }

            case 3: {
                uint16_t suffix = *(const uint16_t*) (str + 1);
                char prefix = str[0];

                if (prefix == 'i') {
                    if (suffix == W16('1', '6')) return packCore(TK_KEYWORD, KW_I16);
                    if (suffix == W16('3', '2')) return packCore(TK_KEYWORD, KW_I32);
                    if (suffix == W16('6', '4')) return packCore(TK_KEYWORD, KW_I64);
                } else if (prefix == 'u') {
                    if (suffix == W16('1', '6')) return packCore(TK_KEYWORD, KW_U16);
                    if (suffix == W16('3', '2')) return packCore(TK_KEYWORD, KW_U32);
                    if (suffix == W16('6', '4')) return packCore(TK_KEYWORD, KW_U64);
                } else if (prefix == 'f') {
                    if (suffix == W16('3', '2')) return packCore(TK_KEYWORD, KW_F32);
                    if (suffix == W16('6', '4')) return packCore(TK_KEYWORD, KW_F64);
                }
                break;
            }

            case 4: {
                uint32_t w = *(const uint32_t*) str;
                switch (str[0]) {
                    case 'e':
                        if (w == W32('e', 'l', 's', 'e')) return packCore(TK_KEYWORD, KW_ELSE);
                        if (w == W32('e', 'n', 'u', 'm')) return packCore(TK_KEYWORD, KW_ENUM);
                        break;
                    case 'l': if (w == W32('l', 'o', 'o', 'p')) return packCore(TK_KEYWORD, KW_LOOP); break;
                    case 'w': if (w == W32('w', 'h', 'e', 'n')) return packCore(TK_KEYWORD, KW_WHEN); break;
                    case 'c': if (w == W32('c', 'a', 's', 'e')) return packCore(TK_KEYWORD, KW_CASE); break;
                    case 't':
                        if (w == W32('t', 'r', 'u', 'e')) return packCore(TK_KEYWORD, KW_TRUE);
                        if (w == W32('t', 'y', 'p', 'e')) return packCore(TK_KEYWORD, KW_TYPE);
                        break;
                    case 'n': if (w == W32('n', 'u', 'l', 'l')) return packCore(TK_KEYWORD, KW_NULL); break;
                    default: break;
                }
                break;
            }

            case 5: {
                uint32_t suffix = *(const uint32_t*) (str + 1);
                switch (str[0]) {
                    case 'c':
                        if (suffix == W32('o', 'n', 's', 't')) return packCore(TK_KEYWORD, KW_CONST);
                        if (suffix == W32('a', 't', 'c', 'h')) return packCore(TK_KEYWORD, KW_CATCH);
                        break;
                    case 'e':
                        if (suffix == W32('m', 'b', 'e', 'd')) return packCore(TK_KEYWORD, KW_EMBED);
                        if (suffix == W32('r', 'r', 'o', 'r')) return packCore(TK_KEYWORD, KW_ERROR);
                        break;
                    case 'd':
                        if (suffix == W32('e', 'f', 'e', 'r')) return packCore(TK_KEYWORD, KW_DEFER);
                        break;
                    case 'f':
                        if (suffix == W32('a', 'l', 's', 'e')) return packCore(TK_KEYWORD, KW_FALSE);
                        break;
                    case 'u':
                        if (suffix == W32('n', 'i', 'o', 'n')) return packCore(TK_KEYWORD, KW_UNION);
                        if (suffix == W32('s', 'i', 'n', 'g')) return packCore(TK_KEYWORD, KW_USING);
                        break;
                    case 'b':
                        if (suffix == W32('r', 'e', 'a', 'k')) return packCore(TK_KEYWORD, KW_BREAK);
                        break;
                    case 'a':
                        if (suffix == W32('l', 'l', 'o', 'c')) return packCore(TK_KEYWORD, KW_ALLOC);
                        break;
                    default:
                        break;
                }
                break;
            }

            case 6: {
                switch (str[0]) {
                    case 's':
                        if (str[1] == 't' && *(const uint32_t*) (str + 2) == W32('r', 'u', 'c', 't'))
                            return packCore(TK_KEYWORD, KW_STRUCT);
                        break;
                    case 'r':
                        if (str[1] == 'e' && *(const uint32_t*) (str + 2) == W32('t', 'u', 'r', 'n'))
                            return packCore(TK_KEYWORD, KW_RETURN);
                        break;
                    case 'i':
                        if (str[1] == 'm' && *(const uint32_t*) (str + 2) == W32('p', 'o', 'r', 't'))
                            return packCore(TK_KEYWORD, KW_IMPORT);
                        break;
                    default:
                        break;
                }
                break;
            }

            case 8: {
                uint64_t w = *(const uint64_t*) str;
                if (w == W64('c', 'o', 'n', 't', 'i', 'n', 'u', 'e')) {
                    return packCore(TK_KEYWORD, KW_CONTINUE);
                }
                break;
            }

            default: {
                break;
            }
        }

        return packCore(TK_IDENTIFIER, TD_NONE);
    }

    TokenCore lookupDirective(const char* str, int len) {
        switch (len) {
            case 2: {
                if (*(const uint16_t*) str == W16('i', 'f')) return packCore(TK_DIRECTIVE, TD_CD_IF);
                break;
            }

            case 4: {
                if (*(const uint32_t*) str == W32('l', 'o', 'o', 'p')) return packCore(TK_DIRECTIVE, TD_CD_LOOP);
                break;
            }

            case 6: {
                if (*(const uint32_t*) str == W32('p', 'a', 'c', 'k') && *(const uint16_t*) (str + 4) == W16('e', 'd'))
                    return packCore(TK_DIRECTIVE, TD_CD_PACKED);
                if (*(const uint32_t*) str == W32('f', 'o', 'r', 'm') && *(const uint16_t*) (str + 4) == W16('a', 't'))
                    return packCore(TK_DIRECTIVE, TD_CD_FORMAT);
                break;
            }

            case 7: {
                if (*(const uint32_t*) str == W32('r', 'e', 'o', 'r') && *(const uint32_t*) (str + 3) == W32('r', 'd', 'e', 'r'))
                    return packCore(TK_DIRECTIVE, TD_CD_REORDER);
                break;
            }

            default: {
                break;
            }
        }

        return packCore(TK_IDENTIFIER, TD_NONE);
    }

    inline bool isWhitespace(const char ch) {
        return (
            ch == ' '  || ch == '\t' ||
            ch == '\v' || ch == '\f' ||
            ch == '\r' || ch == '\n'
        );
    }

    inline int isWhitespaceButNewLine(const char ch) {
        return (
            ch == ' '  || ch == '\t' ||
            ch == '\v' || ch == '\f' ||
            ch == '\r'
        );
    }

    inline int isIdentifierStart(const char ch) {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
    }

    inline int isIdentifierChar(const char ch) {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
    }

    inline int isNumberStart(const char ch) {
        return (ch >= '0' && ch <= '9') || ch == '.' || ch == '-';
    }



    inline int parseCommentLine(const char* const str) {
        const char* p = str;
        while (*p != EOS && *p != EOL) {
            p++;
        }
        return p - str;
    }

    inline int parseCommentBlock(const char* const str, uint32_t* ln, int* err) {
        int idx = 2;

        int nestedComments = 1;
        while (nestedComments > 0) {
            if (str[idx] == EOS) {
                if (err) *err = 1;
                break;
            } else if (str[idx] == EOL) {
                (*ln)++;
                idx++;
            } else if (str[idx] == '/' && str[idx + 1] == '{') {
                nestedComments++;
                idx += 2;
            } else if (str[idx] == '/' && str[idx + 1] == '}') {
                nestedComments--;
                idx += 2;
            } else {
                idx++;
            }
        }

        return idx;
    }

    inline int parseIdentifier(const char* const str) {
        const char* p = str;
        while (true) {
            const uint8_t tag = CHAR_DISPATCH[(uint8_t) *p];
            if (tag != DT_IDENT && tag != DT_NUM) {
                break;
            }
            p++;
        }
        return (int) (p - str);
    }



    // Core
    // ---

    void init() {
        batchSize = 0;
        batchRead = 0;
    }

    void release() {
    }

    void batchRefill(const char* const str) {
        uint32_t unreadCount = 0;

        Pos pos;
        if (batchSize > 0) {
            BatchToken token = batchBuffer[batchSize - 1];
            if (token.base.token.kind == TK_END) {
                pos = token.pos;
            } else {
                pos = { token.pos.idx + token.base.token.size, token.pos.ln };
            }
        } else [[unlikely]] {
            pos = { .idx = 0, .ln = 1 };
        }

        // Shift remaining tokens
        if (batchRead < batchSize) {
            unreadCount = batchSize - batchRead;
            for (uint32_t i = 0; i < unreadCount; i++) {
                batchBuffer[i] = batchBuffer[batchRead + i];
            }
        }

        batchRead = 0;
        batchSize = unreadCount;

        while (batchSize < Config::TOKEN_BATCH_BUFFER_SIZE) {
            const TokenEx token = nextToken(str, &pos);
            batchBuffer[batchSize++] = { token, pos };
            if (token.token.kind == TK_END) {
                break;
            }

            pos.idx += (token.token.size > 0) ? token.token.size : 1;
        }
    }

    inline void commit(Span* const span, Pos pos, Token token) {
        batchRead++;
        span->start = pos;
        span->end   = { pos.idx + token.size, pos.ln };
    }

    // TODO: to be used by the parser to report lexer errors
    inline TokenEx toErrorToken(const Error err) {
        TokenEx token = {};
        token.token.kind   = TK_ERROR;
        token.token.detail = TD_NONE;
        token.token.size   = (uint32_t) err;

        return token;
    }

    // Receives a pos relative to given str and aligns it to the next token start
    TokenEx nextToken(const char* const str, Pos* const pos) {
        Token token;
        Value value;
        token.encoded = 0;

        // We want to update idx to the start of a token and
        // use len to obtain its size..
        uint32_t idx = pos->idx;
        uint32_t ln  = pos->ln;
        uint32_t len = 1;

        while (1) {
            while (isWhitespace(str[idx])) {
                if (str[idx] == '\n') ln++;
                idx++;
            }

            if (str[idx] == '/' && str[idx + 1] == '/') {
                idx += parseCommentLine(str + idx);
            } else if (str[idx] == '/' && str[idx + 1] == '{') {
                int err = 0;
                const int commentLen = parseCommentBlock(str + idx, &ln, &err);
                if (err) {
                    *pos = { idx, ln };
                    return toErrorToken(ERR_UNTERMINATED_COMMENT);
                }
                idx += commentLen;
            } else {
                break;
            }
        }

        switch (str[idx]) {
            case CHAR_LITERAL: {
                len = 0;

                uint64_t ch = 0;
                const int size = parseCharLiteral(str + idx + 1, &len, &ch);
                if (size < 0) {
                    *pos = { idx, ln };
                    return toErrorToken(size == Err::UNEXPECTED_SYMBOL ? ERR_UNEXPECTED_SYMBOL : ERR_INVALID_CHAR_ENCODING);
                }

                value.i64 = ch;
                token.kind = TK_CHAR;
                token.detail = size;

                len += 1;
                break;
            }

            case STRING_LITERAL: {
                const int32_t end = findStringEnd(str + idx + 1);
                if (end < 0) {
                    *pos = { idx, ln };
                    return toErrorToken(ERR_UNTERMINATED_STRING);
                }

                len = (uint32_t) end + 1;
                value.str = (char*) (str + idx + 1);
                token.kind = TK_STRING;
                break;
            }

            case '+': {
                token.kind = TK_OP_PLUS;
                break;
            }

            case '-': {
                if (str[idx + 1] == '>') {
                    len = 2;
                    token.kind = TK_OP_ARROW;
                } else {
                    token.kind = TK_OP_MINUS;
                }

                break;
            }

            case '*': {
                token.kind = TK_OP_MULTIPLICATION;
                break;
            }

            case '!': {
                if (str[idx + 1] == '=') {
                    len = 2;
                    token.kind = TK_OP_BOOL_NOT_EQUAL;
                } else {
                    token.kind = TK_OP_BOOL_NEGATION;
                }

                break;
            }

            case '/': {
                token.kind = TK_OP_DIVISION;
                break;
            }

            case '%': {
                token.kind = TK_OP_MODULO;
                break;
            }

            case '<': {
                if (str[idx + 1] == '=') {
                    len = 2;
                    token.kind = TK_OP_LESS_THAN_OR_EQUAL;
                } else if (str[idx + 1] == '<') {
                    len = 2;
                    token.kind = TK_OP_SHIFT_LEFT;
                } else {
                    token.kind = TK_OP_LESS_THAN;
                }

                break;
            }

            case '>': {
                if (str[idx + 1] == '=') {
                    len = 2;
                    token.kind = TK_OP_GREATER_THAN_OR_EQUAL;
                } else if (str[idx + 1] == '>') {
                    len = 2;
                    token.kind = TK_OP_SHIFT_RIGHT;
                } else {
                    token.kind = TK_OP_GREATER_THAN;
                }

                break;
            }

            case '=': {
                if (str[idx + 1] == '=') {
                    len = 2;
                    token.kind = TK_OP_BOOL_EQUAL;
                } else if (str[idx + 1] == '>') {
                    len = 2;
                    token.kind = TK_OP_ARROW_FAT;
                } else {
                    token.kind = TK_EQUAL;
                }

                break;
            }

            case '&': {
                if (str[idx + 1] == '&') {
                    len = 2;
                    token.kind = TK_OP_BOOL_AND;
                } else {
                    token.kind = TK_OP_AND;
                }

                break;
            }

            case '|': {
                if (str[idx + 1] == '|') {
                    len = 2;
                    token.kind = TK_OP_BOOL_OR;
                } else {
                    token.kind = TK_OP_OR;
                }

                break;
            }

            case '^': {
                if (str[idx + 1] == '(') {
                    len = 2;
                    token.kind = TK_SIGNATURE_BEGIN;
                } else {
                    token.kind = TK_OP_XOR;
                }

                break;
            }

            case '~': {
                token.kind = TK_OP_NEGATION;
                break;
            }

            case '#': {
                token.core = packCore(TK_DIRECTIVE, TD_NONE);
                break;
            }

            case '.': {
                if (str[idx + 1] == '{') {
                    len = 2;
                    token.kind = TK_SCOPE_LITERAL_BEGIN;
                } else if (str[idx + 1] == '[') {
                    len = 2;
                    token.kind = TK_ARRAY_LITERAL_BEGIN;
                } else if (str[idx + 1] == STRING_LITERAL) {
                    const int32_t end = findStringEnd(str + idx + 2);
                    if (end < 0) {
                        *pos = { idx, ln };
                        return toErrorToken(ERR_UNTERMINATED_STRING);
                    }

                    len = 2 + (uint32_t) end;
                    value.str = (char*) (str + idx + 2);
                    token.kind = TK_STRING_INFER;
                } else if (str[idx + 1] == '.') {
                    if (str[idx + 2] == '.') {
                        len = 3;
                        token.kind = TK_VARIADIC;
                    } else {
                        len = 2;
                        token.kind = TK_OP_CONCATENATION;
                    }
                } else {
                    token.kind = TK_OP_MEMBER_SELECTION;
                }

                break;
            }

            case ':': {
                if (str[idx + 1] == ':') {
                    len = 2;
                    token.kind = TK_SCOPE_RESOLUTION;
                } else {
                    token.kind = TK_STATEMENT_BEGIN;
                }

                break;
            }

            case ';': {
                token.kind = TK_STATEMENT_END;
                break;
            }

            case '{': {
                token.kind = TK_SCOPE_BEGIN;
                break;
            }

            case '}': {
                token.kind = TK_SCOPE_END;
                break;
            }

            case '[': {
                token.kind = TK_ARRAY_BEGIN;
                break;
            }

            case ']': {
                token.kind = TK_ARRAY_END;
                break;
            }

            case ',': {
                token.kind = TK_LIST_SEPARATOR;
                break;
            }

            case '(': {
                token.kind = TK_PARENTHESIS_BEGIN;
                break;
            }

            case ')': {
                token.kind = TK_PARENTHESIS_END;
                break;
            }

            case EOS: {
                token.kind = TK_END;
                break;
            }

            case '_': {
                const uint8_t tag = CHAR_DISPATCH[(uint8_t) str[idx + 1]];
                if (tag != DT_IDENT && tag != DT_NUM) {
                    token.kind = TK_SKIP;
                    break;
                }
            }

            default: {
                const uint8_t tag = CHAR_DISPATCH[(uint8_t) str[idx]];
                if (tag == DT_IDENT) {
                    len = parseIdentifier(str + idx + 1) + 1;
                    token.core = lookupKeyword(str + idx, len);
                    value.str = (char*) (str + idx);
                } else if (tag == DT_NUM) {
                    len = 0;
                    uint64_t num = 0;
                    TokenDetail type = parseNumber(str + idx, &len, &num);

                    value.i64 = num;
                    token.kind = TK_NUMBER;
                    token.detail = type;
                } else {
                    *pos = { idx, ln };
                    return toErrorToken(ERR_UNEXPECTED_SYMBOL);
                }

                break;
            }
        }

        *pos = { idx, ln };
        token.size = len;
        return { token, value };
    }

    TokenEx nextToken(Span* const span) {
        if (batchRead >= batchSize) [[unlikely]] {
            batchRefill(span->str);
        }

        BatchToken token = batchBuffer[batchRead];
        commit(span, token.pos, token.base.token);

        return token.base;
    }

    TokenEx nextFileName(Span* const span) {
        char* str = (char*)span->str + span->end.idx + 1;
        uint32_t ln = span->end.ln;

        // Whitespaces and comments
        while (true) {
            while (CHAR_DISPATCH[(uint8_t)*str] == DT_SPC) {
                if (*str == '\n') ln++;
                str++;
            }

            // Skip comments before the import path if any
            if (str[0] == '/' && str[1] == '/') {
                ln++;
                str += parseCommentLine(str + 2);
                continue;
            }

            break;
        }

        const uint32_t sIdx = (int)(str - span->str);

        // Path scanning
        if (*str == '#') {
            // TODO: maybe later we will use them for 'modules', path bindings etc..
        }

        str++;
        const char* const pathStart = str;

        while (*str != EOS && *str != EOL) {
            str++;
        }

        while (str != pathStart) {
            str--;
            if (!isWhitespace(*str)) break;
        }

        const int pathLen  = (int) (str - pathStart) + 1;

        // Update Span And Return
        span->start = { .idx = sIdx,           .ln = ln };
        span->end   = { .idx = sIdx + pathLen, .ln = ln };

        TokenEx token;
        token.token.core = packCore(Lex::TK_FILE, 0);
        token.token.size = pathLen;
        token.value.str  = (char*) pathStart;

        return token;
    }



    // Try Functions
    // ---

    TokenEx tryToken(Span* const span, const Token inToken) {
        if (batchRead >= batchSize) [[unlikely]] {
            batchRefill(span->str);
        }

        const BatchToken token = batchBuffer[batchRead];
        if (match(inToken, token.base.token)) {
            commit(span, token.pos, token.base.token);
            return token.base;
        }

        return toTokenEx(TK_NONE);
    }

    TokenEx tryKeyword(Span* const span, Keyword keyword) {
        if (batchRead >= batchSize) [[unlikely]] {
            batchRefill(span->str);
        }

        const BatchToken token = batchBuffer[batchRead];
        if (isKeyword(token.base.token, keyword)) {
            commit(span, token.pos, token.base.token);
            return token.base;
        }

        return toTokenEx(TK_NONE);
    }



    // Peek Functions
    // ---

    TokenEx peekToken(Span* const span) {
        if (batchRead >= batchSize) [[unlikely]] {
            batchRefill(span->str);
        }

        return batchBuffer[batchRead].base;
    }

    // n should be >0, calling with 1 is equivalent of calling peekToken
    TokenEx peekNthToken(Span* const span, const uint32_t n) {
        if (batchRead + n > batchSize) [[unlikely]] {
            batchRefill(span->str);
        }

        return batchRead + n > batchSize ? toTokenEx(TK_END) : batchBuffer[batchRead + (n - 1)].base;
    }



    // Sync Functions
    // ---

    // TODO : optimize
    TokenEx syncToken(Span* const span, Token tokenA, Token tokenB, bool stopOnNewline) {
        TokenEx token = toTokenEx(TK_END);

        uint32_t startLine = span->end.ln;
        while (true) {
            token = nextToken(span);
            if (match(token.token, tokenA) ||
                match(token.token, tokenB) ||
                token.token.kind == Lex::TK_END
            ) {
                break;
            }

            if (stopOnNewline && span && span->start.ln > startLine) {
                return token;
            }
        }

        return token;
    }

    // TODO : optimize
    TokenEx syncToken(Span* const span, const Token* tokens, uint32_t tokenCount, bool stopOnNewline) {
        TokenEx token;

        uint32_t startLine = span->start.ln;
        while (true) {
            token = nextToken(span);
            if (token.token.kind == Lex::TK_END) break;

            for (uint32_t i = 0; i < tokenCount; i++) {
                if (match(token.token, tokens[i])) {
                    return token;
                }
            }

            if (stopOnNewline && span && span->start.ln > startLine) {
                return token;
            }
        }

        return token;
    }

    // TODO: optimize
    TokenEx syncToken(Span* const span, TokenKind tokenA, TokenKind tokenB, bool stopOnNewline) {
        TokenEx token;

        uint32_t startLine = span->start.ln;
        while (true) {
            token = nextToken(span);
            if (token.token.kind == tokenA ||
                token.token.kind == tokenB ||
                token.token.kind == Lex::TK_END
            ) {
                break;
            }

            if (stopOnNewline && span && span->start.ln > startLine) {
                return token;
            }
        }

        return token;
    }



    // Parse helpers
    // ---

    TokenDetail parseNumber(const char* const str, uint32_t* len, uint64_t* out) {
        const int16_t tmp = ((uint8_t)str[0]) | ((uint8_t)str[1] << 8); //((int16_t *) str)[0];
        if (tmp == (('x' << 8) + '0')) {
            // int-hex

            uint64_t num = 0;
            for (int i = 2;; i++) {
                const char ch = str[i];

                if (ch >= '0' && ch <= '9') num = num * 16 + (ch - '0');
                else if (ch >= 'A' && ch <= 'F') num = num * 16 + (10 + ch - 'A');
                else if (ch >= 'a' && ch <= 'f') num = num * 16 + (10 + ch - 'a');
                else if (ch != '_') {
                    *len = i;
                    *out = num;
                    return TD_DT_I64;
                }
            }
        } else if (tmp == (('b' << 8) + '0')) {
            // int-bin

            uint64_t num = 0;
            for (int i = 2;; i++) {
                const char ch = str[i];
                if (ch == '0' || ch == '1') num = num * 2 + (ch - '0');
                else if (ch != '_') {
                    *len = i;
                    *out = num;
                    return TD_DT_I64;
                }
            }
        } else {

            uint64_t integer = 0;
            char ch = 0;
            int i = 0;
            for (;; i++) {
                ch = str[i];

                if (ch >= '0' && ch <= '9') integer = integer * 10 + (ch - '0');
                else if (ch == '_') {
                    // digit separator
                } else {
                    break;
                }
            }

            bool isFloat = false;
            double value = (double) integer;

            if (ch == '.') {
                // float
                // TODO : precision for 'small' floats

                double frac = 0.0;
                double scale = 0.1;

                i++;
                while (1) {
                    ch = str[i];

                    if (ch >= '0' && ch <= '9') {
                        frac += (ch - '0') * scale;
                        scale *= 0.1;
                        i++;
                    } else if (ch == '_') {
                        i++;
                    } else {
                        break;
                    }
                }

                value += frac;
                isFloat = true;
            }

            if (ch == 'e' || ch == 'E') {
                // exponent, ex 1.5e-4

                int expIdx = i + 1;
                const int expSign = (str[expIdx] == '-') ? -1 : 1;

                if (str[expIdx] == '+' || str[expIdx] == '-') {
                    expIdx++;
                }

                int exponent = 0;
                bool hasDigits = false;
                for (;; expIdx++) {
                    const char ech = str[expIdx];

                    if (ech >= '0' && ech <= '9') {
                        exponent = exponent * 10 + (ech - '0');
                        hasDigits = true;
                    } else if (ech != '_') {
                        break;
                    }
                }

                if (hasDigits) {
                    while (exponent-- > 0) {
                        value = (expSign > 0) ? value * 10.0 : value / 10.0;
                    }

                    i = expIdx;
                    ch = str[i];
                    isFloat = true;
                }
            }

            if (ch == 'f' || ch == 'F') {
                *len = i + 1;
                *((float_t *) out) = (float_t) value;
                return TD_DT_F32;
            }

            if (isFloat) {
                *len = i;
                *((double_t *) out) = value;
                return TD_DT_F64;
            }

            *len = i;
            *out = integer;
            return TD_DT_I64;
        }

        return TD_DT_VOID;
    }

    uint64_t parseIntHex(const char* const str, uint32_t* idx) {
        const int pivot = *idx;
        uint64_t num = 0;
        for (int i = 0;; i++) {

            const char ch = str[pivot + i];

            if (ch >= '0' && ch <= '9') num = num * 16 + (ch - '0');
            else if (ch >= 'A' && ch <= 'F') num = num * 16 + (10 + ch - 'A');
            else if (ch >= 'a' && ch <= 'f') num = num * 16 + (10 + ch - 'a');
            else {
                *idx += i;
                return num;
            }

        }
    }

    uint64_t parseInt(const char* const str, uint32_t* idx) {
        uint64_t num = 0;
        while (1) {
            const char ch = str[*idx];
            if (ch >= '0' && ch <= '9') num = num * 10 + (ch - '0');
            else break;
            (*idx)++;
        }

        return num;
    }

    inline int hexDigit(const char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        return -1;
    }

    int parseEscapeChar(const char* const str, uint32_t* idx) {
        const char ch = str[*idx];
        *idx += 1;

        switch(ch) {
            case 'a':
                return 0x07;
            case 'b':
                return 0x08;
            case 'f':
                return 0x0C;
            case 'n':
                return 0x0A;
            case 'r':
                return 0x0D;
            case 't':
                return 0x09;
            case 'v':
                return 0x0B;
            case 'e':
                return 0x1B;
            case '\\':
                return 0x5C;
            case '\'':
                return 0x27;
            case '\"':
                return 0x22;
            case '?':
                return 0x3F;
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7': {
                int value = ch - '0';
                for (int i = 0; i < 2; i++) {
                    const char oct = str[*idx];
                    if (oct < '0' || oct > '7') break;
                    value = value * 8 + (oct - '0');
                    (*idx)++;
                }
                return value;
            }
            case 'x': {
                const int tmp = *idx;
                const int hexValue = (int) parseIntHex(str, idx);
                if (*idx == tmp) return -1;
                return hexValue;
            }
            case 'u': case 'U': {
                const int digits = ch == 'u' ? 4 : 8;
                uint32_t codepoint = 0;

                // Fixed-width unicode escape, returns a code point
                for (int i = 0; i < digits; i++) {
                    const int digit = hexDigit(str[*idx]);
                    if (digit < 0) return -1;

                    codepoint = (codepoint << 4) | (uint32_t) digit;
                    (*idx)++;
                }

                if (codepoint > 0x10FFFF) return -1;
                return (int) codepoint;
            }
            default:
                return -1;
        }
    };

    // Lightweight single-char parser, UTF-8 decoding is left to the caller)
    int parseChar(const char* const str, uint32_t* idx) {
        const uint8_t ch = (uint8_t) str[*idx];

        if (ch == ESCAPE_CHAR) {
            *idx += 1;
            const int escaped = parseEscapeChar(str, idx);
            if (escaped == -1) {
                return Err::UNSUPPORTED_ESCAPE_SEQUENCE;
            }

            return escaped;
        }

        if (ch == '\0') {
            return Err::UNEXPECTED_SYMBOL;
        }

        *idx += 1;
        return ch;
    }

    // Decodes a single UTF-8 code point, advancing *idx by its byte width
    inline int decodeChar(const char* const str, uint32_t* idx) {
        const uint8_t ch = (uint8_t) str[*idx];

        if (ch == '\0') {
            return Err::UNEXPECTED_SYMBOL;
        }

        int extra = 0;
        int32_t codepoint = 0;

        if (ch < 0x80) {
            extra = 0;
            codepoint = ch;
        } else if ((ch & 0xE0) == 0xC0) {
            extra = 1;
            codepoint = ch & 0x1F;
        } else if ((ch & 0xF0) == 0xE0) {
            extra = 2;
            codepoint = ch & 0x0F;
        } else if ((ch & 0xF8) == 0xF0) {
            extra = 3;
            codepoint = ch & 0x07;
        } else {
            return Err::UNEXPECTED_SYMBOL;
        }

        for (int i = 1; i <= extra; i++) {
            const uint8_t cont = (uint8_t) str[*idx + i];
            if ((cont & 0xC0) != 0x80) {
                return Err::UNEXPECTED_SYMBOL;
            }
            codepoint = (codepoint << 6) | (cont & 0x3F);
        }

        *idx += extra + 1;
        return codepoint;
    }

    int parseCharLiteral(const char* const str, uint32_t* len, uint64_t* out) {
        uint64_t value = 0;

        int size = 0;
        while (1) {
            if (str[*len] == CHAR_LITERAL) {
                (*len)++;
                break;
            }

            int codepoint = 0;
            if (str[*len] == ESCAPE_CHAR) {
                (*len)++;
                codepoint = parseEscapeChar(str, len);
                if (codepoint < 0) return Err::UNSUPPORTED_ESCAPE_SEQUENCE;
            } else {
                codepoint = decodeChar(str, len);
                if (codepoint < 0) return codepoint;
            }

            // Pack big-endian by the byte width of every code point
            // 'abcd' -> 0x61626364, 'č' -> 0x10D
            int width = 4;
            if (codepoint <= 0xFF) {
                width = 1;
            } else if (codepoint <= 0xFFFF) {
                width = 2;
            }

            size += width;
            if (size > 8) {
                return Err::DATA_TYPE_SIZE_EXCEEDED;
            }

            value = (value << (width * 8)) | (uint64_t) codepoint;
        }

        *out = value;

        return size;
    }

    int32_t findBlockEnd(Span* const span, const char bCh, const char eCh) {
        const char* const start = span->str + span->end.idx + 1;
        const char* p           = start;
        int ln                  = 0;
        int toClose             = 1;

        const Pos startPos = {
            .idx = (uint32_t) (start - span->str),
            .ln  = span->start.ln
        };

        while (true) {
            char ch = *p;

            if (ch == '\n') {
                ln++;
            } else if (ch == bCh) {
                toClose++;
            } else if (ch == eCh) {
                toClose--;
                if (toClose <= 0) break; // Reached matching end character
            } else if (ch == '\0') {
                span->end.ln  += ln;
                span->end.idx  = (int)(p - span->str);
                return Err::UNEXPECTED_END_OF_FILE;
            }

            p++;
        }

        const int32_t len = (int32_t) (p - start);

        span->start   = startPos;
        span->end.ln += ln;
        span->end.idx = (int) (p - span->str);

        return len;
    }

    int32_t findBlockEndEscaped(Span* const span, const char bCh, const char eCh) {
        const char* const start = span->str + span->end.idx + 1;
        const char* p           = start;
        int ln                  = 0;
        int toClose             = 1;

        const Pos startPos = {
            .idx = (uint32_t) (start - span->str),
            .ln  = span->start.ln
        };

        while (true) {
            char ch = *p;

            if (ch == '\\') {
                if (p[1] == '\n') ln++;

                if (p[1] != '\0') {
                    p += 2;
                    continue;
                }
            }

            if (ch == '\n') {
                ln++;
            } else if (ch == bCh) {
                toClose++;
            } else if (ch == eCh) {
                toClose--;
                if (toClose <= 0) break;
            } else if (ch == '\0') {
                span->end.ln  += ln;
                span->end.idx  = (int)(p - span->str);
                return Err::UNEXPECTED_END_OF_FILE;
            }

            p++;
        }

        const int32_t len = (int32_t) (p - start);

        span->start   = startPos;
        span->end.ln += ln;
        span->end.idx = (int)(p - span->str);

        return len;
    }

    // Returns the length up to and including the closing quote,
    // or -1 when the end of the buffer is reached first
    int32_t findStringEnd(const char* const str) {
        const char* p = str;

        while (*p != EOS) {
            if (*p == ESCAPE_CHAR) {
                if (p[1] == EOS) return -1;
                p += 2;
                continue;
            }

            if (*p == STRING_LITERAL) {
                return (int32_t) (p - str) + 1;
            }

            p++;
        }

        return -1;
    }

}
