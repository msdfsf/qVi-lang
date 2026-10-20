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
    KW_TYPE,
    KW_CONST,
    KW_EMBED,
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
    KW_ERROR,
    KW_CATCH,
    KW_DEFER,
    KW_IMPORT,
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

enum Directive {
    CD_NONE,
    CD_IF,      // #if
    CD_LOOP,    // #loop
    CD_PACKED,  // [#packed]
    CD_REORDER, // [#reorder]
    CD_FORMAT,  // [#format]
    CD_COUNT
};

inline const char* str(Keyword keyword) {
    static const char* const LUT[] = {
        "void",
        "i8",
        "i16",
        "i32",
        "i64",
        "u8",
        "u16",
        "u32",
        "u64",
        "f32",
        "f64",
        "type",
        "const",
        "embed",
        "struct",
        "union",
        "if",
        "else",
        "loop",
        "when",
        "case",
        "enum",
        "return",
        "continue",
        "break",
        "using",
        "alloc",
        "error",
        "catch",
        "defer",
        "import",
        "as",
        "at",
        "true",
        "false",
        "null"
    };

    static_assert(sizeof(LUT) / sizeof(LUT[0]) == KW_COUNT,
                  "Keywords table size mismatch with Keyword enum!");

    if ((unsigned int) keyword < KW_COUNT) {
        return LUT[keyword];
    }

    return "<unknown_keyword>";
}
