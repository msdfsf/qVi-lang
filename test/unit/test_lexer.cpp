#include "../../src/syntax.h"
#include "../../src/lexer.h"
#include "../../src/globals.h"
#include "../test_core.h"
#include <cstdint>
#include <stdio.h>



#define STR(lit) (String { (char*) lit, sizeof(lit) - 1 })

static thread_local Span span;

inline void gLexerPreSuite(bool visualize) {
    nallocInit();
}

inline void gLexerPostSuite(bool visualize) {
}

inline void gLexerPreCase(bool visualize) {
    Lex::init();
}

inline void gLexerPostCase(bool visualize) {
    Lex::release();
}



static void initSpanTest(Span* span, const char* input) {
    span->str = input;
    span->start = { 0, 0 };
    span->end = { 0, 0 };
    span->fileInfo = NULL;
}

// Asserts that token text (as pointed to by 'value') equals expected
static void assertText(Lex::TokenEx tex, const char* expected) {
    Test::assert(tex.token.size, (uint32_t) strlen(expected));
    Test::assert(strncmp(tex.value.str, expected, tex.token.size), 0);
}

// Asserts that raw content of a string token equals expected;
// 'overhead' is the count of syntax bytes the token spans besides its content
static void assertStringContent(Lex::TokenEx tex, const char* expected, uint32_t overhead) {
    const uint32_t len = (uint32_t) strlen(expected);
    Test::assert(tex.token.size, len + overhead);
    Test::assert(strncmp(tex.value.str, expected, len), 0);
}



inline Test::Case gLexerCases[] = {

    TEST_CASE(test_keywords_type) {
        initSpanTest(&span, "i8 i16 i32 i64 u8 u16 u32 u64 f32 f64");

        Lex::TokenEx tex;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            if (!Lex::isType(tex.token)) {
                Test::assert(false);
            }
        }
    }},

    TEST_CASE(test_keywords_regular) {
        initSpanTest(&span,
            "type const embed alloc struct union if else loop when case enum "
            "return continue break using error catch defer import as at true false null");

        Lex::TokenEx tex;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            Test::assert(tex.token.kind, Lex::TK_KEYWORD);
        }
    }},

    TEST_CASE(test_void_is_identifier) {
        // 'void' is an internal sentinel and is not spelled in source
        initSpanTest(&span, "void");
        Test::assert(Lex::nextToken(&span).token.kind, Lex::TK_IDENTIFIER);
    }},

    TEST_CASE(test_identifiers) {
        initSpanTest(&span, "foo foo_boo _private camelCase PascalCase foo123 _123");

        Lex::TokenEx tex;
        const char* expected[] = {
            "foo", "foo_boo", "_private", "camelCase", "PascalCase", "foo123", "_123"
        };

        int i = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            Test::assert(tex.token.kind, Lex::TK_IDENTIFIER);
            assertText(tex, expected[i++]);
        }

        Test::assert(i, sizeof(expected) / sizeof(expected[0]));
    }},

    TEST_CASE(test_numeric_literals) {
        initSpanTest(&span,
            // Integers
            "0 42 000042 1_000_000 9223372036854775807 "

            // Hexadecimal
            "0x0 0x0000_00FF 0xdead_beef 0x1a2B_3c4D 0xFFFFFFFFFFFFFFFF "

            // Binary
            "0b0 0b0000_0001 0b1010_1010 0b1111_1111_1111_1111 "

            // Double Floats
            "0.0 0.5 3.14159 123_456.789_012 "

            // Single Floats
            "0.0f 0.5f 3.14f 42f"
        );

        struct ExpectedNumber {
            Lex::TokenDetail detail;
            uint64_t         rawIntVal;
            double           floatVal;
        };

        ExpectedNumber expected[] = {
            // Integers
            { Lex::TD_DT_I64, 0 },
            { Lex::TD_DT_I64, 42 },
            { Lex::TD_DT_I64, 42 },                     // 000042 -> 42
            { Lex::TD_DT_I64, 1000000 },                // 1_000_000
            { Lex::TD_DT_I64, 9223372036854775807ULL }, // Max I64

            // Hexadecimal
            { Lex::TD_DT_I64, 0x0 },
            { Lex::TD_DT_I64, 0xFF },                  // 0x0000_00FF
            { Lex::TD_DT_I64, 0xDEADBEEFULL },         // 0xdead_beef (lowercase)
            { Lex::TD_DT_I64, 0x1A2B3C4DULL },         // 0x1a2B_3c4D (mixed case)
            { Lex::TD_DT_I64, 0xFFFFFFFFFFFFFFFFULL }, // Max U64 hex

            // Binary
            { Lex::TD_DT_I64, 0b0 },
            { Lex::TD_DT_I64, 0b1 },                // 0b0000_0001
            { Lex::TD_DT_I64, 0b10101010 },         // 0b1010_1010
            { Lex::TD_DT_I64, 0b1111111111111111 }, // 0b1111_1111_1111_1111

            // Double Floats (f64)
            { Lex::TD_DT_F64, 0, 0.0 },
            { Lex::TD_DT_F64, 0, 0.5 },
            { Lex::TD_DT_F64, 0, 3.14159 },
            { Lex::TD_DT_F64, 0, 123456.789012 }, // 123_456.789_012

            // Single Floats (f32)
            { Lex::TD_DT_F32, 0, 0.0 },  // 0.0f
            { Lex::TD_DT_F32, 0, 0.5 },  // 0.5f
            { Lex::TD_DT_F32, 0, 3.14 }, // 3.14f
            { Lex::TD_DT_F32, 0, 42.0 }  // 42f
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::TokenEx tex;
        int count = 0;

        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            if (tex.token.kind == Lex::TK_NUMBER) {
                ExpectedNumber exp = expected[count++];

                Test::assert(tex.token.detail, exp.detail);

                if (tex.token.detail == Lex::TD_DT_I64) {
                    Test::assert(tex.value.i64, exp.rawIntVal);
                } else if (tex.token.detail == Lex::TD_DT_F64) {
                    Test::assert(tex.value.f64, exp.floatVal);
                } else if (tex.token.detail == Lex::TD_DT_F32) {
                    Test::assert((float) tex.value.f32, (float) exp.floatVal);
                }
            }
        }

        Test::assert(count, expectedCount);
    }},

    TEST_CASE(test_string_literals) {
        initSpanTest(&span,
            "\"hello\""
            "\"world\""
            "\"\""
            "\"with spaces\""
            "\"with\\nescape\""
            "\"raw\"b"
            "\"path\\\\to\\\\file\"b"
            "\"1234567890\""
            "\"!@#$%^&*()_+-=[]{}|;:,.<>?\""
            "\"   \""
            "\"multiline\\ntext\\nhere\""
            "\"\""
        );

        // Content of a string literal token is raw: escapes are not
        // resolved by the lexer and are preserved as written
        const char* expected[] = {
            "hello",
            "world",
            "",
            "with spaces",
            "with\\nescape",
            "raw",
            "path\\\\to\\\\file",
            "1234567890",
            "!@#$%^&*()_+-=[]{}|;:,.<>?",
            "   ",
            "multiline\\ntext\\nhere",
            ""
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::TokenEx tex;
        int i = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            if (tex.token.kind == Lex::TK_STRING) {
                assertStringContent(tex, expected[i++], 2);
            }
        }

        Test::assert(i, expectedCount);
    }},

    TEST_CASE(test_string_literal_escapes_are_raw) {
        initSpanTest(&span, "\"\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\\?\\0\\x41\"");

        Lex::TokenEx tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_STRING);
        assertStringContent(tex, "\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\\?\\0\\x41", 2);
    }},

    TEST_CASE(test_string_infer) {
        initSpanTest(&span, ".\"hello\" u16.\"\xC4\x8Dus\"");

        Lex::TokenEx tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_STRING_INFER);
        assertStringContent(tex, "hello", 3);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_KEYWORD);
        Test::assert(tex.token.detail, KW_U16);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_STRING_INFER);
        assertStringContent(tex, "\xC4\x8Dus", 3);

        Test::assert(Lex::nextToken(&span).token.kind, Lex::TK_END);
    }},

    TEST_CASE(test_char_literals) {
        initSpanTest(&span,
            "'a' "
            "'Z' "
            "'0' "
            "' ' "
            "'!' "
            "'AB' "
            "'ABC' "
            "'ABCD' "
            "'\\n' "
            "'\\t' "
            "'\\\\' "
            "'\\'' "
            "'\\x41' "
            "'\\0'"
        );

        struct ExpectedChar {
            uint64_t value;
            uint16_t width;
        };

        // Code points pack big-endian, by the byte width of every
        // code point ('AB' -> 0x4142, 'ABCD' -> 0x41424344)
        const ExpectedChar expected[] = {
            { 'a', 1 },
            { 'Z', 1 },
            { '0', 1 },
            { ' ', 1 },
            { '!', 1 },
            { 0x4142, 2 },
            { 0x414243, 3 },
            { 0x41424344, 4 },
            { '\n', 1 },
            { '\t', 1 },
            { '\\', 1 },
            { '\'', 1 },
            { '\x41', 1 },
            { '\0', 1 }
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::TokenEx tex;
        int i = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            if (tex.token.kind == Lex::TK_CHAR) {
                Test::assert(tex.value.i64, expected[i].value);
                Test::assert(tex.token.detail, (Lex::TokenDetail) expected[i].width);
                i++;
            }
        }

        Test::assert(i, expectedCount);
    }},

    TEST_CASE(test_char_literals_multibyte) {
        // Multi-byte code points are decoded and span accordingly wide
        initSpanTest(&span, "'\xC4\x8D'");
        Lex::TokenEx tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_CHAR);
        Test::assert(tex.value.i64, 0x10D);
        Test::assert(tex.token.detail, 2);
    }},

    TEST_CASE(test_char_escape_sequences) {
        initSpanTest(&span,
            "'\\a' "
            "'\\b' "
            "'\\f' "
            "'\\n' "
            "'\\r' "
            "'\\t' "
            "'\\v' "
            "'\\e' "
            "'\\\\' "
            "'\\'' "
            "'\\\"' "
            "'\\?' "
            "'\\0' "
            "'\\x41' "
        );

        const uint64_t expected[] = {
            0x07,
            '\b',
            '\f',
            '\n',
            '\r',
            '\t',
            '\v',
            0x1B,
            '\\',
            '\'',
            '\"',
            '\?',
            '\0',
            '\x41',
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::TokenEx tex;
        int i = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            if (tex.token.kind == Lex::TK_CHAR) {
                Test::assert(tex.value.i64, expected[i++]);
            }
        }

        Test::assert(i, expectedCount);
    }},

    TEST_CASE(test_operators) {
        initSpanTest(&span, "+ - * / % == != < > <= >= && || ! & | ^ ~ << >> .. . ^( -> =>");

        Lex::TokenKind expected[] = {
            // Arithmetic
            Lex::TK_OP_PLUS,
            Lex::TK_OP_MINUS,
            Lex::TK_OP_MULTIPLICATION,
            Lex::TK_OP_DIVISION,
            Lex::TK_OP_MODULO,

            // Comparison
            Lex::TK_OP_BOOL_EQUAL,
            Lex::TK_OP_BOOL_NOT_EQUAL,
            Lex::TK_OP_LESS_THAN,
            Lex::TK_OP_GREATER_THAN,
            Lex::TK_OP_LESS_THAN_OR_EQUAL,
            Lex::TK_OP_GREATER_THAN_OR_EQUAL,

            // Logical
            Lex::TK_OP_BOOL_AND,
            Lex::TK_OP_BOOL_OR,
            Lex::TK_OP_BOOL_NEGATION,

            // Bitwise
            Lex::TK_OP_AND,
            Lex::TK_OP_OR,
            Lex::TK_OP_XOR,
            Lex::TK_OP_NEGATION,

            // Shifts
            Lex::TK_OP_SHIFT_LEFT,
            Lex::TK_OP_SHIFT_RIGHT,

            // Concat & Member
            Lex::TK_OP_CONCATENATION,
            Lex::TK_OP_MEMBER_SELECTION,

            // Callable signature, return & lambda
            Lex::TK_SIGNATURE_BEGIN,
            Lex::TK_OP_ARROW,
            Lex::TK_OP_ARROW_FAT,
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::TokenEx tex;
        int count = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            Test::assert(tex.token.kind, expected[count++]);
        }

        Test::assert(count, expectedCount);
    }},

    TEST_CASE(test_punctuation) {
        initSpanTest(&span, "; : :: , ( ) { } [ ]");

        Lex::TokenKind expected[] = {
            Lex::TK_STATEMENT_END,
            Lex::TK_STATEMENT_BEGIN,
            Lex::TK_SCOPE_RESOLUTION,
            Lex::TK_LIST_SEPARATOR,
            Lex::TK_PARENTHESIS_BEGIN,
            Lex::TK_PARENTHESIS_END,
            Lex::TK_SCOPE_BEGIN,
            Lex::TK_SCOPE_END,
            Lex::TK_ARRAY_BEGIN,
            Lex::TK_ARRAY_END
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::TokenEx tex;
        int count = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            Test::assert(tex.token.kind, expected[count++]);
        }

        Test::assert(count, expectedCount);
    }},

    TEST_CASE(test_colon_equal_separates) {
        // ':=' is not a token, it lexes as ':' followed by '='
        initSpanTest(&span, ":=");
        Test::assert(Lex::nextToken(&span).token.kind, Lex::TK_STATEMENT_BEGIN);
        Test::assert(Lex::nextToken(&span).token.kind, Lex::TK_EQUAL);
    }},

    TEST_CASE(test_compound_tokens) {
        initSpanTest(&span, ".{ .[ .\"x\" ^( ... -> =>");

        Lex::TokenEx tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_SCOPE_LITERAL_BEGIN);
        Test::assert(tex.token.size, 2);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_ARRAY_LITERAL_BEGIN);
        Test::assert(tex.token.size, 2);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_STRING_INFER);
        Test::assert(tex.token.size, 4);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_SIGNATURE_BEGIN);
        Test::assert(tex.token.size, 2);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_VARIADIC);
        Test::assert(tex.token.size, 3);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_OP_ARROW);
        Test::assert(tex.token.size, 2);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_OP_ARROW_FAT);
        Test::assert(tex.token.size, 2);

        Test::assert(Lex::nextToken(&span).token.kind, Lex::TK_END);
    }},

    TEST_CASE(test_directives) {
        initSpanTest(&span, "#if #loop #");

        Lex::TokenEx tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_DIRECTIVE);
        Test::assert(tex.token.detail, Lex::TD_NONE);
        Test::assert(tex.token.size, 1);

        // The word after '#' lexes as a regular token; the parser
        // is the one resolving directive names
        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_KEYWORD);
        Test::assert(tex.token.detail, KW_IF);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_DIRECTIVE);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_KEYWORD);
        Test::assert(tex.token.detail, KW_LOOP);

        tex = Lex::nextToken(&span);
        Test::assert(tex.token.kind, Lex::TK_DIRECTIVE);

        Test::assert(Lex::nextToken(&span).token.kind, Lex::TK_END);

        // Directive lookup packs a TK_DIRECTIVE core on a hit
        Test::assert(Lex::lookupDirective("if", 2).kind,   Lex::TK_DIRECTIVE);
        Test::assert(Lex::lookupDirective("if", 2).detail, Lex::TD_CD_IF);
        Test::assert(Lex::lookupDirective("loop", 4).detail,   Lex::TD_CD_LOOP);
        Test::assert(Lex::lookupDirective("packed", 6).detail, Lex::TD_CD_PACKED);
        Test::assert(Lex::lookupDirective("reorder", 7).detail, Lex::TD_CD_REORDER);
        Test::assert(Lex::lookupDirective("format", 6).detail,  Lex::TD_CD_FORMAT);

        // ... and falls back to an identifier on a miss
        Test::assert(Lex::lookupDirective("qwe", 3).kind,   Lex::TK_IDENTIFIER);
        Test::assert(Lex::lookupDirective("qwe", 3).detail, Lex::TD_NONE);
    }},

    TEST_CASE(test_skip_token) {
        initSpanTest(&span, "_ __ ___ _ _foo _bar");

        Lex::TokenEx tex;
        int count = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            if (tex.token.kind == Lex::TK_SKIP) count++;
        }
        Test::assert(count, 2);
    }},

    TEST_CASE(test_qualified_names) {
        initSpanTest(&span, "foo::bar baz::qux::quux");

        Lex::TokenKind expected[] = {
            Lex::TK_IDENTIFIER,
            Lex::TK_SCOPE_RESOLUTION,
            Lex::TK_IDENTIFIER,
            Lex::TK_IDENTIFIER,
            Lex::TK_SCOPE_RESOLUTION,
            Lex::TK_IDENTIFIER,
            Lex::TK_SCOPE_RESOLUTION,
            Lex::TK_IDENTIFIER
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::TokenEx tex;
        int count = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            Test::assert(tex.token.kind, expected[count++]);
        }

        Test::assert(count, expectedCount);
    }},

    TEST_CASE(test_comments_single_line) {
        initSpanTest(&span, "foo // comment\nbar");

        Lex::TokenEx tex;
        int count = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            if (tex.token.kind == Lex::TK_IDENTIFIER) {
                count++;
            }
        }
        Test::assert(count, 2);

        Lex::release();
    }},

    TEST_CASE(test_comments_multi_line) {
        initSpanTest(&span, "foo /{ comment /} bar /{ nested /{ comment /} /} baz");

        Lex::TokenEx tex;
        int count = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            if (tex.token.kind == Lex::TK_IDENTIFIER) {
                count++;
            }
        }
        Test::assert(count, 3);
    }},

    TEST_CASE(test_newlines) {
        initSpanTest(&span, "foo\nbar\n\nbaz");

        Lex::TokenEx tex;
        int count = 0;
        while ((tex = Lex::nextToken(&span)).token.kind != Lex::TK_END) {
            if (tex.token.kind == Lex::TK_IDENTIFIER) {
                count++;
            }
        }
        Test::assert(count, 3);
    }},

    TEST_CASE(test_eof) {
        initSpanTest(&span, "");

        Test::assert(Lex::nextToken(&span).token.kind, Lex::TK_END);
    }},

    TEST_CASE(test_large_token_buffers) {
        constexpr int len = 4 * 1024;
        char largeIdentifier[len];

        memset(largeIdentifier, 'a', 2000);
        largeIdentifier[2000] = '\0';

        initSpanTest(&span, largeIdentifier);
        Test::assert(Lex::nextToken(&span).token.kind, Lex::TK_IDENTIFIER);
    }},

    TEST_CASE(test_try_token) {
        initSpanTest(&span, "foo if");

        // Matching compares both kind and detail, so detail matters
        // even for kinds that do not carry any
        Lex::TokenEx tex = Lex::tryToken(&span, { .kind = Lex::TK_IDENTIFIER, .detail = Lex::TD_NONE });
        Test::assert(tex.token.kind, Lex::TK_IDENTIFIER);

        tex = Lex::tryToken(&span, Lex::toToken(Lex::TK_KEYWORD));
        Test::assert(tex.token.kind, Lex::TK_NONE);

        tex = Lex::tryToken(&span, { .kind = Lex::TK_KEYWORD, .detail = KW_LOOP });
        Test::assert(tex.token.kind, Lex::TK_NONE);

        tex = Lex::tryToken(&span, { .kind = Lex::TK_KEYWORD, .detail = KW_IF });
        Test::assert(tex.token.kind, Lex::TK_KEYWORD);
        Test::assert(tex.token.detail, KW_IF);
    }},

    TEST_CASE(test_peek_token) {
        initSpanTest(&span, "foo bar baz");

        Lex::TokenEx peeked = Lex::peekToken(&span);
        Test::assert(peeked.token.kind, Lex::TK_IDENTIFIER);
        assertText(peeked, "foo");

        Lex::TokenEx second = Lex::peekNthToken(&span, 2);
        Test::assert(second.token.kind, Lex::TK_IDENTIFIER);
        assertText(second, "bar");

        Lex::TokenEx consumed = Lex::nextToken(&span);
        Test::assert(consumed.token.kind, Lex::TK_IDENTIFIER);
        assertText(consumed, "foo");
    }},

    TEST_CASE(test_sync_token) {
        initSpanTest(&span, "foo bar if baz");

        Lex::TokenEx tex = Lex::syncToken(&span, Lex::TK_KEYWORD, Lex::TK_STATEMENT_END);
        Test::assert(tex.token.kind, Lex::TK_KEYWORD);
        Test::assert(tex.token.detail, KW_IF);
    }},

    TEST_CASE(test_sync_token_match) {
        initSpanTest(&span, "42 if");

        Lex::TokenEx tex = Lex::syncToken(&span,
            Lex::Token { .kind = Lex::TK_IDENTIFIER, .detail = Lex::TD_NONE },
            Lex::Token { .kind = Lex::TK_KEYWORD,    .detail = KW_IF });
        Test::assert(tex.token.kind, Lex::TK_KEYWORD);
        Test::assert(tex.token.detail, KW_IF);
    }},

    TEST_CASE(test_sync_token_until_end) {
        initSpanTest(&span, "foo bar");

        Lex::TokenEx tex = Lex::syncToken(&span, Lex::TK_STATEMENT_END, Lex::TK_SCOPE_END);
        Test::assert(tex.token.kind, Lex::TK_END);
    }},

    TEST_CASE(test_try_keyword) {
        initSpanTest(&span, "if else");

        Lex::TokenEx tex = Lex::tryKeyword(&span, KW_IF);
        Test::assert(tex.token.kind, Lex::TK_KEYWORD);
        Test::assert(tex.token.detail, KW_IF);

        tex = Lex::tryKeyword(&span, KW_LOOP);
        Test::assert(tex.token.kind, Lex::TK_NONE);

        tex = Lex::tryKeyword(&span, KW_ELSE);
        Test::assert(tex.token.kind, Lex::TK_KEYWORD);
        Test::assert(tex.token.detail, KW_ELSE);
    }},

};



extern const Test::Suite gLexerSuite = {
    "Lexer",
    NULL,
    gLexerCases,
    sizeof(gLexerCases) / sizeof(Test::Case),
    gLexerPreCase,
    gLexerPostCase,
    gLexerPreSuite,
    gLexerPostSuite,
};
