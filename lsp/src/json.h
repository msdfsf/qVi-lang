#pragma once

#ifndef JSON_H
#define JSON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>



typedef struct JsonString {
    char*  data;
    size_t len;
} JsonString;



// === LEXER ===
// ==

typedef enum {
    JSON_NONE = 0,
    JSON_OBJECT_OPEN,
    JSON_OBJECT_CLOSE,
    JSON_ARRAY_OPEN,
    JSON_ARRAY_CLOSE,
    JSON_KEY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOL,
    JSON_NULL,
    JSON_ERROR,
    JSON_EOF
} JsonType;

typedef struct JsonLex {
    JsonString src;
    size_t     pos;
    union {
        JsonString s;
        double     n;
        bool       b;
    } value;

    JsonString fname;

    int errCol;
    int errRow;
    int errIdx; // actual index in the buffer
    int errLen; // length of error segment from errIdx

    uint8_t errExpected;
    uint8_t errProvided;
    uint8_t errCode;

    bool errDontResolveSpan;
} JsonLex;

void     jsonLexInit   (JsonLex* ctx, JsonString src, JsonString fname);
JsonType jsonNext      (JsonLex* ctx);
JsonType jsonPeek      (JsonLex* ctx);
void     jsonSkipValue (JsonLex* ctx, JsonType type);
void     jsonAssert    (JsonLex* js, JsonType provided, JsonType expected);
bool     jsonMatch     (JsonLex* ctx, JsonType provided, JsonType expected);
void     jsonPrintError(JsonLex* ctx, FILE* stream);

const char* jsonTypeName  (JsonType type);



// === WRITER ===
// ==

typedef struct {
    char*  buffer;
    size_t size;
    size_t capacity;

    void* (*realloc) (void* ptr, size_t size);

    int      depth;
    uint32_t firstItemMask; // Bitmask to track commas for 32 levels
} JsonWriter;

void jsonWriterInit(JsonWriter* ctx, char* buffer, size_t capacity);

void jsonWriteObjectStart(JsonWriter* ctx);
void jsonWriteObjectEnd  (JsonWriter* ctx);
void jsonWriteArrayStart (JsonWriter* ctx);
void jsonWriteArrayEnd   (JsonWriter* ctx);
void jsonWriteKey        (JsonWriter* ctx, JsonString key);
void jsonWriteStr        (JsonWriter* ctx, JsonString val);
void jsonWriteInt        (JsonWriter* ctx, long long val);
void jsonWriteDouble     (JsonWriter* ctx, double val);
void jsonWriteBool       (JsonWriter* ctx, bool val);
void jsonWriteNull       (JsonWriter* ctx);

JsonString jsonWriterCommit(JsonWriter* ctx);

#ifdef __cplusplus
}
#endif

#endif
