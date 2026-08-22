#include "lexer.h"
#include "data_types.h"
#include "dynamic_arena.h"
#include "strlib.h"
#include "syntax.h"
#include "logger.h"
#include "array_list.h"
#include "diagnostic.h"

#include <cstdint>



// Expects null terminated buffer
namespace Lex {
    constexpr int32_t CHAR_EOS = -1;

    Logger::Type logErr = { .level = Logger::ERROR, .tag = "lexer" };

    // LOOK AT: for now thread local, as I want to keep interface simple
    // doesn't seems like a significant cost. But just feels a bit weird.

    // We will push the offsets of each qualified name part here while parsing,
    // so we can pack the offsets more tightly in the result object,
    // as we will know their total size in advance.
    thread_local DArray::Container qnameStack;

    // for string parsing, as string is identical as in source code as
    // we need to escape stuff
    thread_local Arena::Container stringStack;

    void init() {
        DArray::init(&qnameStack, 32 * 2, sizeof(int));
        Arena::init(&stringStack, 32 * 2);
    }

    void release() {
        DArray::release(&qnameStack);
        Arena::release(&stringStack);
    }

    // TODO: make general str(Token token), also unite these functions to 'str'
    const char* toStr(TokenKind token) {

        switch (token) {
            case TK_NONE: return "none";
            case TK_IDENTIFIER: return "identifier";
            case TK_KEYWORD: return "keyword";
            case TK_CHAR: return "char";
            case TK_STRING: return "string";
            case TK_NUMBER: return "number";
            case TK_FILE: return "file";

            case TK_OP_INCREMENT: return "++";
            case TK_OP_PLUS: return "+";
            case TK_OP_DECREMENT: return "--";
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

            case TK_END: return "EOF";

            default: return "unknown";
        }

    }

    uint32_t hash(const char* str, int len) {
        uint32_t hash = 0x811C9DC5 ^ 0x558355AF;
        int idx = 0;
        while (idx < len) {
            hash ^= (uint8_t) str[idx++];
            hash *= 0x01000193;
        }
        return hash;
    }

    inline Keyword keywordLookup(const char* str, const int len) {
        const unsigned int h = hash(str, len) % KW_TABLE_SIZE;
        Keyword keyword = (Keyword) keywordTable[h];

        const int ans = cstrcmp(keywordStringTable[keyword], String { (char*) str, (uint64_t) len });
        return ans ? keyword : KW_VOID;
    }

    inline Directive directiveLookup(const char* str, const int len) {
        const unsigned int h = hash(str, len) % CD_TABLE_SIZE + 1;
        Directive directive = (Directive) directivesTable[h];

        const int ans = cstrcmp(directivesStringTable[directive], String { (char*) str, (uint64_t) len });
        return ans ? directive : CD_NONE;
    }

    static inline bool isWhitespace(char ch) {
        return (
            ch == ' '  || ch == '\t' ||
            ch == '\v' || ch == '\f' ||
            ch == '\r' || ch == '\n'
        );
    }

    inline int isWhitespaceButNewLine(char ch) {
        return (
            ch == ' '  || ch == '\t' ||
            ch == '\v' || ch == '\f' ||
            ch == '\r'
        );
    }

    inline Pos skipWhitespaces(const char* str, Pos pos) {
        while (str[pos.idx] != '\0' && isWhitespace(str[pos.idx])) {
            if (str[pos.idx] == '\n') {
                pos.ln++;
            }
            pos.idx++;
        }
        return pos;
    }

    // if unterminated comment found returns its start position and marks *err as 1
    // (should be premarked to other value), NULL err ignores and just skips to EOF
    static inline Pos skipWhitespacesAndComments(const char* str, Pos pos, int* err) {

        while (1) {

            const char ch = str[pos.idx];

            if (ch == '\n') {
                pos.ln++;
                pos.idx++;
                continue;
            }

            if (isWhitespaceButNewLine(ch)) {
                pos.idx++;
                continue;
            }

            if (ch == '/' && str[pos.idx + 1] == '/') {

                pos.idx += 2;

                while (1) {

                    if (str[pos.idx] == EOL) {
                        pos.ln++;
                        pos.idx++;
                        break;
                    } else if (str[pos.idx] == EOS) {
                        break;
                    }

                    pos.idx++;

                }

                continue;

            }

            if (ch == '/' && str[pos.idx + 1] == '{') {

                pos.idx += 2;

                Pos startPos = pos;
                int nestedComments = 1;
                while (nestedComments > 0) {

                    if (str[pos.idx] == EOS) {
                        if (err) *err = 1;
                        return startPos;
                    } else if (str[pos.idx] == EOL) {
                        pos.ln++;
                        pos.idx++;
                    } else if (str[pos.idx] == '/' && str[pos.idx + 1] == '{') {
                        nestedComments++;
                        pos.idx += 2;
                    } else if (str[pos.idx] == '/' && str[pos.idx + 1] == '}') {
                        nestedComments--;
                        pos.idx += 2;
                    } else {
                        pos.idx++;
                    }

                }

                continue;

            }

            return pos;

        }

    }

    inline int cmpTwoChars(uint16_t packed, const char* str) {
        return (str[0] == (packed & 0xFF)) && (str[1] == ((packed >> 8) & 0xFF));
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

    inline int parseIdentifier(const char* const str) {

        if (!isIdentifierStart(str[0])) return 0;

        int idx = 1;
        while (isIdentifierChar(str[idx])) {
            idx++;
        }

        return idx;

    }

    inline TokenDetail parseNumber(const char* const str, int* len, uint64_t* out) {

        int idx = 0;

        const int16_t tmp = ((uint8_t)str[0]) | ((uint8_t)str[1] << 8); //((int16_t *) str)[0];
        if (tmp == (('x' << 8) + '0')) {
            // int-hex

            uint64_t num = 0;
            for (int i = 2;; i++) {

                const char ch = str[i];

                if (ch >= '0' && ch <= '9') num = num * 16 + (ch - '0');
                else if (ch >= 'A' && ch <= 'F') num = num * 16 + (10 + ch - 'A');
                else if (ch >= 'a' && ch <= 'f') num = num * 16 + (10 + ch - 'a');
                else if (ch == '_') {
                    if (str[i + 1] == '_') return TD_DT_VOID;
                } else {
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
                else if (ch == '_') {
                    if (str[i + 1] == '_') return TD_DT_VOID;
                } else {
                    *len += i;
                    *out = num;
                    return TD_DT_I64;
                }

            }

        } else {

            uint64_t integer = 0;
            char ch = 0;
            for (int i = 0;; i++) {

                ch = str[i];

                if (ch >= '0' && ch <= '9') integer = integer * 10 + (ch - '0');
                else if (ch == '.') {
                    // float
                    // TODO : precision for 'small' floats

                    double value = (double) integer;
                    double frac = 0.0;
                    double scale = 0.1;

                    while (1) {

                        i++;

                        ch = str[i];
                        if (ch >= '0' && ch <= '9') {
                            frac += (ch - '0') * scale;
                            scale *= 0.1;
                        } else if (ch == '_') {
                            if (str[i + 1] == '_') return TD_DT_VOID;
                        } else {
                            break;
                        }

                    }

                    if (ch == 'f') {
                        *len = i + 1;
                        *((float_t *)out) = (float_t)(value + frac);
                        return TD_DT_F32;
                    }

                    *len = i;
                    *((double_t *)out) = value + frac;
                    return TD_DT_F64;

                } else if (ch == 'f') {

                    *len = i + 1;
                    *((float_t *) out) = (float_t) integer;
                    return TD_DT_F32;

                } else if (ch == '_') {

                    if (str[i + 1] == '_') return TD_DT_VOID;

                } else {

                    *len = i;
                    *out = integer;
                    return TD_DT_I64;

                }

            }

        }

        return TD_DT_VOID;

    }

    uint64_t parseHexInt(const char* const str, int* idx) {

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

    int parseEscapeChar(const char* str, int* idx) {
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
            case '0': {
                if (str[*idx] == '3' && str[*idx + 1] == '3') {
                    *idx += 2;
                    return 0x1B;
                } else {
                    return 0;
                }
            }
            case 'x': {
                int tmp = *idx;
                const char ch = parseHexInt(str, idx);
                if (*idx == tmp) return -1;
                return ch;
            }
            default:
                return -1;
        }
    };

    inline int parseChar(const char* str, int* idx) {
        char ch = str[*idx];

        if (ch == ESCAPE_CHAR) {
            *idx += 1;
            ch = parseEscapeChar(str, idx);
            if (ch == -1) {
                // parseHexInt(str, &idx);
                // Logger::log(Logger::ERROR, ERR_STR(Err::UNSUPPORTED_ESCAPE_SEQUENCE), span, 1);
                return Err::UNSUPPORTED_ESCAPE_SEQUENCE;
            }
        } else if (ch == '\0') {
            return Err::UNEXPECTED_SYMBOL;
        } else {
            *idx += 1;
        }

        return ch;
    }

    int parseCharLiteral(const char* const str, int* len, uint64_t* out) {
        uint64_t tmpOut = 0;

        int size = 0;
        while (1) {
            if (str[*len] == CHAR_LITERAL) {
                (*len)++;
                break;
            }

            char ch = parseChar(str, len);
            if (ch < 0) return ch;

            uint64_t tmp = ch;
            tmp <<= 56;

            tmpOut >>= 8;
            tmpOut |= tmp;

            size++;
        }

        if (size > 8) {
            return Err::DATA_TYPE_SIZE_EXCEEDED;
        }

        tmpOut >>= (8 - size) * 8;
        *out = tmpOut;

        return size;
    }

    int findStringEnd(const char* const str) {
        int idx = 0;

        while (1) {
            if (str[idx] == STRING_LITERAL) {
                idx++;
                break;
            }

            const char ch = parseChar(str, &idx);
            if (ch < 0) return ch;
        }

        idx += (str[idx] == RAW_POSTFIX);
        return idx;
    }

    int parseStringLiteral(const char* const str, int* len, StringInitialization** initOut) {
        Arena::clear(&stringStack);

        while (1) {
            if (str[*len] == STRING_LITERAL) {
                (*len)++;
                break;
            }

            const char ch = parseChar(str, len);
            if (ch < 0 ) return ch;

            char* ptr = (char*) Arena::push(&stringStack, 1, 1);
            *ptr = ch;
        }

        const int rawStringRequired = (str[*len] == RAW_POSTFIX) ? 1 : 0;

        StringInitialization* init = (StringInitialization*) alloc(alc, sizeof(StringInitialization));
        init->base.type = EXT_STRING_INITIALIZATION;


        init->rawData.len = stringStack.logicalPos;
        init->rawData.buff = (char*) alloc(alc, init->rawData.len);

        init->charType = Type::basicTypes + Type::DT_U8;

        // TODO : maybe too slow to use generic arena
        Arena::flatCopy(&stringStack, (uint8_t*) init->rawData.buff);

        // meh but whatever
        if (rawStringRequired) {
            *len += 1;
        } else {
            int utf8Len;
            int utf8BytesPerChar;
            char* utf8Str = Strings::encodeUtf8(init->rawData, &utf8Len, &utf8BytesPerChar, 0);

            if (utf8BytesPerChar != 1) {
                init->rawData.buff = utf8Str;
                init->rawData.len = utf8Len;
                init->charType = Type::basicTypes + (Type::DT_U8 + utf8BytesPerChar - 1);
            }
        }

        *initOut = init;

        return Err::OK;
    }

    // first char already consumed
    Token parseQualifiedName(Span* const span, const char* const str, QualifiedName* const qname, int* outLen) {

        int idx = 0;
        int len = 1;
        int accLen = 0;
        Token token = { .kind = TK_IDENTIFIER, .detail = 0 };

        qname->buff = NULL;
        DArray::clear(&qnameStack);

        while (1) {
            int offset = idx - len + 1;

            while (isIdentifierChar(str[idx + 1])) {
                idx++;
                len++;
            }

            Keyword keyword = keywordLookup(str + offset, len);

            if (keyword > 0) {
                token.kind = TK_KEYWORD;
                token.detail = keyword;
            }

            DArray::push(&qnameStack, &offset);
            DArray::push(&qnameStack, &len);

            if (!cmpTwoChars(SCOPE_RESOLUTION, str + idx + 1)) {
                break;
            }

            if (token.kind == TK_KEYWORD) {
                // ERROR
                *outLen = accLen + len;
                return toToken(Err::UNEXPECTED_SYMBOL);
            }

            accLen += len + 2;
            len = 0;
            idx +=2;
        }

        int* data = (int*) qnameStack.buffer;

        if (qnameStack.size - 2 > 0) {

            qname->pathSize = (qnameStack.size - 2) / 2;
            qname->path = (INamed*) alloc(alc, qname->pathSize * sizeof(INamed));

            for (int i = 0; i < qname->pathSize; i++) {
                qname->path[i].buff = (char*) str + data[i];
                qname->path[i].len = data[i + 1];
            }

        } else {

            qname->pathSize = 0;

        }

        qname->buff = (char*) str + data[2 * qname->pathSize];
        qname->len = data[2 * qname->pathSize + 1];

        DArray::clear(&qnameStack);

        *outLen = accLen + len;
        return token;

    }



    Token tryToken(Span* const span, Token inToken, TokenValue* outVal) {

        Pos posStart = span->start;
        Pos posEnd = span->end;

        Token token = nextToken(span, outVal);
        if (token.encoded == inToken.encoded) return token;

        span->start = posStart;
        span->end = posEnd;

        return { .kind = TK_NONE };

    }

    Token tryKeyword(Span* const span, Keyword keyword) {

        Pos posStart = span->start;
        Pos posEnd = span->end;

        Token token = nextToken(span, NULL);
        if (isKeyword(token, keyword)) return token;

        span->start = posStart;
        span->end = posEnd;

        return { .kind = TK_NONE };

    }



    Token peekToken(Span* const span, TokenValue* val) {

        Pos posStart = span->start;
        Pos posEnd = span->end;

        Token token = nextToken(span, val);

        span->start = posStart;
        span->end = posEnd;

        return token;

    }

    // n should be >0, calling with 1 is equivalent of calling peekToken
    Token peekNthToken(Span* const span, TokenValue* val, unsigned int n) {

        Token token;

        Pos posStart = span->start;
        Pos posEnd = span->end;

        int i = 0;
        while (i < n) {
            token = nextToken(span, val);
            i++;
        }

        span->start = posStart;
        span->end = posEnd;

        return token;

    }



    // span start and end will be set to token boundaries
    // uses span->end as current position, so span->end.idx + 1 is the
    // functions first char to parse
    Token nextToken(Span* const span, TokenValue* val) {

        Token token;
        token.encoded = 0;

        const char* const str = span->str;

        int err = 0;
        span->end.idx++;
        Pos startPos = skipWhitespacesAndComments(str, span->end, &err);
        if (err) {
            span->start = startPos;
            span->end = startPos;
            Logger::log(logErr, "Unterminated comment!", span, 1);
            return toToken(Err::UNTERMINATED_COMMENT);
        }

        int ln = 0;
        int len = 1;

        const char ch = str[startPos.idx];
        switch (ch) {

            case CHAR_LITERAL: {

                len = 0;

                uint64_t ch = 0;
                const int size = parseCharLiteral(str + startPos.idx + 1, &len, &ch);
                if (size < 0) {
                    Logger::log(logErr, "Wrong char literal format!", span, 1);
                    return toToken(size);
                }

                if (val) val->ival = ch;
                token.kind = TK_CHAR;
                token.detail = size;

                len += 1;
                break;

            }

            case STRING_LITERAL: {

                len = 0;

                if (val) {
                    StringInitialization* init;
                    const int err = parseStringLiteral(str + startPos.idx + 1, &len, &init);
                    val->any = init;
                } else {
                    len = findStringEnd(str + startPos.idx + 1);
                }

                if (err < 0) {
                    Logger::log(logErr, "Wrong string literal format!", span, 1);
                    return toToken(err);
                }

                token.kind = TK_STRING;

                len += 1;
                break;

            }

            case '+': {

                if (str[startPos.idx + 1] == '+') {
                    len = 2;
                    token = { .kind = TK_OP_INCREMENT };
                } else {
                    token = { .kind = TK_OP_PLUS };
                }

                break;

            }

            case '-': {

                if (str[startPos.idx + 1] == '-') {
                    len = 2;
                    token = { .kind = TK_OP_DECREMENT };
                } else if (str[startPos.idx + 1] == '>') {
                    len = 2;
                    token = { .kind = TK_OP_ARROW };
                } else {
                    token = { .kind = TK_OP_MINUS };
                }

                break;

            }

            case '*': {

                token = { .kind = TK_OP_MULTIPLICATION };
                break;

            }

            case '!': {

                if (str[startPos.idx + 1] == '=') {
                    len = 2;
                    token = { .kind = TK_OP_BOOL_NOT_EQUAL };
                } else {
                    token = { .kind = TK_OP_BOOL_NEGATION };
                }

                break;

            }

            case '/': {

                token = { .kind = TK_OP_DIVISION };
                break;

            }

            case '%': {

                token = { .kind = TK_OP_MODULO };
                break;

            }

            case '<': {

                if (str[startPos.idx + 1] == '=') {
                    len = 2;
                    token = { .kind = TK_OP_LESS_THAN_OR_EQUAL };
                } else if (str[startPos.idx + 1] == '<') {
                    len = 2;
                    token = { .kind = TK_OP_SHIFT_LEFT };
                } else {
                    token = { .kind = TK_OP_LESS_THAN };
                }

                break;

            }

            case '>': {

                if (str[startPos.idx + 1] == '=') {
                    len = 2;
                    token = { .kind = TK_OP_GREATER_THAN_OR_EQUAL };
                } else if (str[startPos.idx + 1] == '>') {
                    len = 2;
                    token = { .kind = TK_OP_SHIFT_RIGHT };
                } else {
                    token = { .kind = TK_OP_GREATER_THAN };
                }

                break;

            }

            case '=': {

                if (str[startPos.idx + 1] == '=') {
                    len = 2;
                    token = { .kind = TK_OP_BOOL_EQUAL };
                } else if (str[startPos.idx + 1] == '>') {
                    len = 2;
                    token = { .kind = TK_OP_ARROW_FAT };
                } else {
                    token = { .kind = TK_EQUAL };
                }

                break;

            }

            case '&': {

                if (str[startPos.idx + 1] == '&') {
                    len = 2;
                    token = { .kind = TK_OP_BOOL_AND };
                } else {
                    token = { .kind = TK_OP_AND };
                }

                break;

            }

            case '|': {

                if (str[startPos.idx + 1] == '|') {
                    len = 2;
                    token = { .kind = TK_OP_BOOL_OR };
                } else {
                    token = { .kind = TK_OP_OR };
                }

                break;

            }

            case '^': {

                token = { .kind = TK_OP_XOR };
                break;

            }

            case '~': {

                token = { .kind = TK_OP_NEGATION };
                break;

            }

            case '#': {

                const int len = parseIdentifier(str + startPos.idx + 1);
                Directive directive = directiveLookup(str, len);

                if (directive > 0) {
                    token.detail = TD_CD_BEGIN + directive + 1;
                } else {
                    token.detail = TD_NONE;
                }

                token.kind = TK_DIRECTIVE;

                break;

            }

            case '.': {

                if (str[startPos.idx + 1] == '.') {
                    len = 2;
                    token = { .kind = TK_OP_CONCATENATION };
                } else {
                    token = { .kind = TK_OP_MEMBER_SELECTION };
                }

                break;

            }

            case ':': {

                if (str[startPos.idx + 1] == ':') {
                    len = 2;
                    token = { .kind = TK_SCOPE_RESOLUTION };
                } else {
                    token = { .kind = TK_STATEMENT_BEGIN };
                }

                break;

            }

            case ';': {

                token = { .kind = TK_STATEMENT_END };
                break;

            }

            case '{': {

                token = { .kind = TK_SCOPE_BEGIN };
                break;

            }

            case '}': {

                token = { .kind = TK_SCOPE_END };
                break;

            }

            case '[': {

                token = { .kind = TK_ARRAY_BEGIN };
                break;

            }

            case ']': {

                token = { .kind = TK_ARRAY_END };
                break;

            }

            case ',': {

                token = { .kind = TK_LIST_SEPARATOR };
                break;

            }

            case '(': {

                token = { .kind = TK_PARENTHESIS_BEGIN };
                break;

            }

            case ')': {

                token = { .kind = TK_PARENTHESIS_END };
                break;

            }

            case EOS: {

                token = { .kind = TK_END };
                break;

            }

            case '_': {
                if (!isIdentifierChar(str[startPos.idx + 1])) {
                    token = { .kind = TK_SKIP };
                    break;
                }
            }

            default: {

                if (isIdentifierStart(ch)) {

                    QualifiedName stackName;
                    QualifiedName* name = val ? ((QualifiedName*) nalloc(nalc, AT_QUALIFIED_NAME)) : &stackName;
                    token = parseQualifiedName(span, str + startPos.idx, name, &len);

                    if (val) val->any = (void*) name;

                } else if (isNumberStart(ch)) {

                    len = 0;
                    uint64_t num = 0;
                    TokenDetail dtype = parseNumber(str + startPos.idx, &len, &num);

                    if (val) val->ival = num;
                    token.kind = TK_NUMBER;
                    token.detail = dtype;

                }

                break;

            }

        }

        span->start = startPos;
        span->end.ln = startPos.ln + ln;
        span->end.idx = startPos.idx + len - 1;

        return token;

    }

    Token nextFileName(Span* const span, TokenValue* val) {

        const char* const str = span->str;

        int err = 0;
        span->end.idx++;
        Pos startPos = skipWhitespacesAndComments(str, span->end, &err);
        if (err) {
            span->start = startPos;
            Logger::log(logErr, "Unterminated comment!", span, 1);
            return toToken(Err::UNTERMINATED_COMMENT);
        }

        int idx = startPos.idx;

        while (1) {

            const char ch = str[idx];
            if (isIdentifierChar(ch) || ch == '.' || ch == '\\' || ch == '/') {
                idx++;
                continue;
            } else {
                break;
            }

        }

        val->str = (String*) nalloc(nalc, AT_INAMED);
        *(val->str) = String((char*) str + startPos.idx, idx - startPos.idx);

        span->start = startPos;
        span->end.ln = startPos.ln;
        span->end.idx = idx - 1;

        return { .kind = TK_FILE };

    }

    Token peekTokenSkipDecorators(Span* const span, TokenValue* val) {

        Pos posStart = span->start;
        Pos posEnd = span->end;

        const char* const str = span->str;
        int idx = span->end.idx + 1;

        Token token = toToken(TokenKind::TK_NONE);
        while (1) {

            const char ch = str[idx];
            idx++;

            if (ch == POINTER) continue;
            if (ch == ARRAY_BEGIN) {

                // TODO : To a inline function
                int cnt = 1;
                while (1) {

                    const char ch = str[idx];
                    idx++;

                    if (ch == ARRAY_END && cnt <= 1) break;
                    if (ch == ARRAY_BEGIN) cnt++;

                }

                continue;

            }

            if (!isWhitespace(ch)) {

                span->end.idx = idx - 2;
                token = peekToken(span, val);
                break;

            }

        }

        span->start = posStart;
        span->end = posEnd;
        return token;

    }

    // TODO : optimize
    Token syncToken(Span* const span, Token tokenA, Token tokenB, TokenValue* val) {

        Token token;

        while (true) {
            token = nextToken(span, val);
            if (token.encoded == tokenA.encoded ||
                token.encoded == tokenB.encoded ||
                token.kind == Lex::TK_END
            ) {
                break;
            }
        }

        return token;

    }

    // TODO : optimize
    Token syncToken(Span* const span, Token* tokens, uint32_t tokenCount, TokenValue* val) {

        Token token;

        while (true) {
            token = nextToken(span, val);
            if (token.kind == Lex::TK_END) break;

            for (uint32_t i = 0; i < tokenCount; i++) {
                if (token.encoded == tokens[i].encoded) {
                    return token;
                }
            }
        }

        return token;

    }

    // TODO: optimize
    Token syncToken(Span* const span, TokenKind tokenA, TokenKind tokenB, TokenValue* val) {

        Token token;

        while (true) {
            token = nextToken(span, val);
            if (token.kind == tokenA ||
                token.kind == tokenB ||
                token.kind == Lex::TK_END
            ) {
                break;
            }
        }

        return token;

    }

    // TODO
    int findClosureEnd(char* const str, const int endCh) {

        int i = 1;
        int toClose = 1;
        const int bgCh = str[endCh];
        while (1) {

            const int ch = str[i];
            if (ch == bgCh) {
                toClose++;
            } else if (ch == endCh) {
                if (toClose == 1) return i;
                toClose--;
            } else if (ch == EOF) {
                return 0;
            }

            i++;

        }

    }

    int findBlockEnd(Span* const span, const char bCh, const char eCh) {

        int ln = 0;
        int idx = span->end.idx + 1;
        Pos startPos = { .idx = idx, .ln = span->start.ln };
        const char* const str = span->str;

        int toClose = 1;

        while (1) {

            const char ch = str[idx];

            if (ch == '\n') {
                ln++;
            } else if (ch == bCh) {
                toClose++;
            } else if (ch == eCh) {
                toClose--;
                if (toClose <= 0) break;
            } else if (ch == '\0') {
                span->end.ln += ln;
                span->end.idx = idx;
                return Err::UNEXPECTED_END_OF_FILE;
            }

            idx++;

        }

        const int len = idx - span->end.idx - 1;

        span->start = startPos;
        span->end.ln += ln;
        span->end.idx = idx;

        return len;

    }

}
