#pragma once

enum Keyword {
    KW_VOID,
    KW_I8,
    KW_I16,
    KW_I32,
    KW_I64,
    KW_U8,
    KW_U16,
    KW_U32,
    KW_U64,
    KW_F32,
    KW_F64,
    KW_CONST,
    KW_EMBED,
    KW_FLUID,
    KW_STRUCT,
    KW_UNION,
    KW_IF,
    KW_ELSE,
    KW_LOOP,
    KW_WHEN,
    KW_CASE,
    KW_ENUM,
    KW_RETURN,
    KW_CONTINUE,
    KW_BREAK,
    KW_USING,
    KW_ALLOC,
    KW_FREE,
    KW_ERROR,
    KW_CATCH,
    KW_IMPORT,
    KW_FROM,
    KW_AS,
    KW_AT,
    KW_TRUE,
    KW_FALSE,
    KW_NULL,
    KW_COUNT
};

enum TypedefKeyword {
    TKW_STRUCT,
    TKW_UNION,
};

enum DirectiveKeyword {
    DKW_LANG_DEF,
    DKW_IMPORT
};
