#include "json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR(str) ((const JsonString) { str, sizeof(str) - 1 })



// === LEXER ===
// ==

const char* jsonTypeName(JsonType type) {
    switch (type) {
        case JSON_OBJECT_OPEN:  return "Object Open '{'";
        case JSON_OBJECT_CLOSE: return "Object Close '}'";
        case JSON_ARRAY_OPEN:   return "Array Open '['";
        case JSON_ARRAY_CLOSE:  return "Array Close ']'";
        case JSON_KEY:          return "Key";
        case JSON_STRING:       return "String";
        case JSON_NUMBER:       return "Number";
        case JSON_BOOL:         return "Boolean";
        case JSON_NULL:         return "Null";
        case JSON_ERROR:        return "Syntax Error";
        case JSON_EOF:          return "End of File";
        default:                return "Unknown";
    }
}

static bool match(JsonString str, const char* lit) {
    return (str.len == (int) strlen(lit) && strncmp(str.data, lit, str.len) == 0);
}

static uint64_t stringToUint(JsonString str) {
    uint64_t ans = 0;
    for (int i = 0; i < str.len; i++) {
        if (str.data[i] >= '0' && str.data[i] <= '9') {
            ans = ans * 10 + (str.data[i] - '0');
        }
    }
    return ans;
}

void jsonSkip(JsonLex* ctx) {

    while (ctx->pos < ctx->src.len) {

        const char ch = ctx->src.data[ctx->pos];
        if (isspace(ch) || ch == ',') {
            ctx->pos++;
            continue;
        }

        // skip comments
        if (ch == '/' && ctx->pos + 1 < ctx->src.len) {

            // line comment
            if (ctx->src.data[ctx->pos + 1] == '/') {
                while (ctx->pos < ctx->src.len &&
                       ctx->src.data[ctx->pos] != '\n'
                ) ctx->pos++;
                continue;
            }

            // block comment
            if (ctx->src.data[ctx->pos + 1] == '*') {
                ctx->pos += 2;
                while (ctx->pos + 1 < ctx->src.len &&
                       !(ctx->src.data[ctx->pos] == '*' &&
                       ctx->src.data[ctx->pos+1] == '/')
                ) ctx->pos++;
                ctx->pos += 2;
                continue;
            }

            break;
        }

        break;

    }

}

bool jsonIsDelimiter(char ch) {
    return isspace((unsigned char)ch) || ch == ',' || ch == ']' ||
           ch == '}' || ch == ':' || ch == '/' || ch == '\0';
}

// TODO : for now we inplace escape string in src
//.dataer, maybe add a flag to disable this behavior
JsonString jsonParseString(JsonLex* ctx) {

    // skip '"'
    ctx->pos++;

    int start = ctx->pos;
    int writeIdx = start;
    while (ctx->pos < ctx->src.len &&
           ctx->src.data[ctx->pos] != '"'
    ) {

        if (ctx->src.data[ctx->pos] == '\\' && ctx->pos + 1 < ctx->src.len) {
            ctx->pos++;
            const char ch = ctx->src.data[ctx->pos];
            switch (ch) {
                case '"':  ctx->src.data[writeIdx] = '"';  break;
                case '\\': ctx->src.data[writeIdx] = '\\'; break;
                case '/':  ctx->src.data[writeIdx] = '/';  break;
                case 'b':  ctx->src.data[writeIdx] = '\b'; break;
                case 'f':  ctx->src.data[writeIdx] = '\f'; break;
                case 'n':  ctx->src.data[writeIdx] = '\n'; break;
                case 'r':  ctx->src.data[writeIdx] = '\r'; break;
                case 't':  ctx->src.data[writeIdx] = '\t'; break;
                default:   ctx->src.data[writeIdx] = ch; break;
            }

        } else {
            ctx->src.data[writeIdx] = ctx->src.data[ctx->pos];
        }

        writeIdx++;
        (ctx->pos)++;

    }

    // TODO : not a fan of
    ctx->src.data[writeIdx] = '\0';

    JsonString res = { ctx->src.data + start, (size_t) (writeIdx - start) };
    if (ctx->pos < ctx->src.len) ctx->pos++;

    return res;

}

// Logic to handle true, false, null, and numbers
JsonType jsonParseBareWord(JsonLex* ctx) {
    int start = ctx->pos;
    while (ctx->pos < ctx->src.len && !jsonIsDelimiter(ctx->src.data[ctx->pos])) {
        ctx->pos++;
    }

    JsonString word = { ctx->src.data + start, ctx->pos - start };

    if (match(word, "true"))  {
        ctx->value.b = true;
        return JSON_BOOL;
    }

    if (match(word, "false")) {
        ctx->value.b = false;
        return JSON_BOOL;
    }

    if (match(word, "null"))  {
        return JSON_NULL;
    }

    const char ch = word.data[0];
    if (isdigit(ch) || ch == '-') {
        if (ch == '-') {
            word.data += 1;
            word.len -= 1;
            ctx->value.n = -stringToUint(word);
        }else {
            ctx->value.n = stringToUint(word);
        }
        return JSON_NUMBER;
    }

    ctx->value.s = word;
    return JSON_ERROR;
}

JsonType jsonNext(JsonLex* ctx) {

    jsonSkip(ctx);
    if (ctx->pos >= ctx->src.len) return JSON_EOF;

    char ch = ctx->src.data[ctx->pos];
    switch (ch) {
        case '{': ctx->pos++; return JSON_OBJECT_OPEN;
        case '}': ctx->pos++; return JSON_OBJECT_CLOSE;
        case '[': ctx->pos++; return JSON_ARRAY_OPEN;
        case ']': ctx->pos++; return JSON_ARRAY_CLOSE;
        case '"': {
            ctx->value.s = jsonParseString(ctx);
            jsonSkip(ctx);
            if (ctx->pos < ctx->src.len && ctx->src.data[ctx->pos] == ':') {
                ctx->pos++;
                return JSON_KEY;
            }
            return JSON_STRING;
        }
        default: return jsonParseBareWord(ctx);
    }

}

JsonType jsonPeek(JsonLex* ctx) {

    size_t startPos = ctx->pos;

    jsonSkip(ctx);
    if (ctx->pos >= ctx->src.len) return JSON_EOF;

    char ch = ctx->src.data[ctx->pos];
    switch (ch) {
        case '{': return JSON_OBJECT_OPEN;
        case '}': return JSON_OBJECT_CLOSE;
        case '[': return JSON_ARRAY_OPEN;
        case ']': return JSON_ARRAY_CLOSE;
        case '"': return JSON_STRING;
        default: return jsonParseBareWord(ctx);
    }

    ctx->pos = startPos;

}

void jsonSkipValue(JsonLex* ctx, JsonType type) {
    if (type == JSON_OBJECT_OPEN || type == JSON_ARRAY_OPEN) {
        int depth = 1;
        while (depth > 0) {
            JsonType t = jsonNext(ctx);
            if (t == JSON_OBJECT_OPEN || t == JSON_ARRAY_OPEN) depth++;
            else if (t == JSON_OBJECT_CLOSE || t == JSON_ARRAY_CLOSE) depth--;
            else if (t == JSON_EOF) break;
        }
    } else if (type == JSON_KEY) {
        jsonSkipValue(ctx, jsonNext(ctx));
    }
}

void jsonPrintError(JsonLex* ctx, FILE* stream) {

    fprintf(stream, "\n\033[1;31mJSON Error:\033[0m Expected \033[1;32m%s\033[0m, but got \033[1;31m%s\033[0m\n",
            jsonTypeName((JsonType) ctx->errExpected),
            jsonTypeName((JsonType) ctx->errProvided));

    fprintf(stream, "\033[1;34m  --> \033[0mLine %d, Column %d\n",
        ctx->errRow,
        ctx->errCol);
    fprintf(stream, "%4d \033[1;34m|\033[0m ", ctx->errRow);

    for (int i = ctx->errCol; i < ctx->errCol + ctx->errLen; i++) {
        fputc(ctx->src.data[i], stream);
    }
    fprintf(stream, "\n");

    fprintf(stream, "       ");
    for (int i = 0; i < ctx->errCol - 1; i++) fputc(' ', stream);
    fprintf(stream, "\033[1;31m^\033[0m\n\n");
    fprintf(stream, "in file: %.*s", ctx->fname.len, ctx->fname.data);

}

static void noteError(JsonLex* ctx, JsonType provided, JsonType expected) {

    ctx->errProvided = (uint8_t) provided;
    ctx->errExpected = (uint8_t) expected;
    ctx->errIdx      = (int) ctx->pos;

    ctx->errLen = (provided == JSON_STRING || provided == JSON_KEY)
        ? (int) ctx->value.s.len : 1;

    if (!ctx->errDontResolveSpan) {
        int row = 1;
        int col = 1;

        for (size_t i = 0; i < ctx->pos && i < ctx->src.len; i++) {
            if (ctx->src.data[i] == '\n') {
                row++;
                col = 1;
            } else {
                col++;
            }
        }

        ctx->errRow = row;
        ctx->errCol = col;
    }

}

void jsonAssert(JsonLex* ctx, JsonType provided, JsonType expected) {
    if (provided == expected) return;
    noteError(ctx, provided, expected);
    jsonPrintError(ctx, stderr);
    exit(1);
}

bool jsonMatch(JsonLex* ctx, JsonType provided, JsonType expected) {
    if (provided == expected) return true;
    noteError(ctx, provided, expected);
    return false;
}

void jsonLexInit(JsonLex* ctx, JsonString src, JsonString fname) {
    ctx->src = src;
    ctx->pos = 0;
    ctx->value.s = (JsonString) { 0, 0 };
    ctx->fname = fname;
}



// === WRITER ===
// ==

static inline void jsonPushChar(JsonWriter* ctx, const char ch) {
    if (ctx->size < ctx->capacity) {
        ctx->buffer[ctx->size++] = ch;
    }
}

static inline void jsonPush(JsonWriter* ctx, JsonString s) {
    if (ctx->size + s.len < ctx->capacity) {
        memcpy(ctx->buffer + ctx->size, s.data, s.len);
        ctx->size += s.len;
    }
}

static inline void jsonSeparator(JsonWriter* ctx) {
    if (!(ctx->firstItemMask & (1U << ctx->depth))) {
        jsonPushChar(ctx, ',');
    }
    ctx->firstItemMask &= ~(1U << ctx->depth);
}

void jsonWriterInit(JsonWriter* ctx, char* buf, size_t cap) {
    ctx->buffer = buf;
    ctx->capacity = cap;
    ctx->size = 0;
    ctx->depth = 0;
    ctx->firstItemMask = 0xFFFFFFFF;
}

void jsonWriteObjectStart(JsonWriter* ctx) {
    jsonSeparator(ctx);
    jsonPushChar(ctx, '{');
    ctx->depth++;
    ctx->firstItemMask |= (1u << ctx->depth);
}

void jsonWriteObjectEnd(JsonWriter* ctx) {
    ctx->depth--;
    jsonPushChar(ctx, '}');
}

void jsonWriteArrayStart(JsonWriter* ctx) {
    jsonSeparator(ctx);
    jsonPushChar(ctx, '[');
    ctx->depth++;
    ctx->firstItemMask |= (1U << ctx->depth);
}

void jsonWriteArrayEnd(JsonWriter* ctx) {
    ctx->depth--;
    jsonPushChar(ctx, ']');
}

void jsonWriteKey(JsonWriter* ctx, JsonString key) {
    jsonSeparator(ctx);
    jsonPushChar(ctx, '\"');
    jsonPush(ctx, key);
    jsonPush(ctx, STR("\":"));

    ctx->firstItemMask |= (1U << ctx->depth);
}

void jsonWriteStr(JsonWriter* ctx, JsonString val) {
    jsonSeparator(ctx);
    jsonPushChar(ctx, '\"');

    // We must account for escapes
    size_t start = 0;
    for (size_t i = 0; i < val.len; ++i) {
        char c = val.data[i];
        if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t') {
            if (i > start) {
                jsonPush(ctx, (JsonString) { val.data + start, i - start });
            }

            if      (c == '"')  jsonPush(ctx, STR("\\\""));
            else if (c == '\\') jsonPush(ctx, STR("\\\\"));
            else if (c == '\n') jsonPush(ctx, STR("\\n"));
            else if (c == '\r') jsonPush(ctx, STR("\\r"));
            else if (c == '\t') jsonPush(ctx, STR("\\t"));

            start = i + 1;
        }
    }

    if (start < val.len) {
        jsonPush(ctx, (JsonString) { val.data + start, val.len - start });
    }

    jsonPushChar(ctx, '\"');
}

void jsonWriteInt(JsonWriter* ctx, long long val) {
    jsonSeparator(ctx);
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", val);
    jsonPush(ctx, (JsonString) { buf, (size_t) len });
}

void jsonWriteBool(JsonWriter* ctx, bool val) {
    jsonSeparator(ctx);
    if (val) jsonPush(ctx, STR("true"));
    else     jsonPush(ctx, STR("false"));
}

void jsonWriteNull(JsonWriter* ctx) {
    jsonSeparator(ctx);
    jsonPush(ctx, STR("null"));
}

JsonString jsonWriterCommit(JsonWriter* ctx) {
    return (JsonString) { ctx->buffer, ctx->size };
}
