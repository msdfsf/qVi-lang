#include "../src/syntax.h"
#include "../src/lexer.h"
#include "../src/globals.h"
#include "test_core.h"
#include <cstdint>
#include <stdio.h>



#define STR(lit) (String { (char*) lit, sizeof(lit) - 1 })

thread_local Span  span;
thread_local Arena::Container arena;

inline void gLexerPreSuite() {
    initAlloc(&arena);
    alc = &arena;
    initNAlloc(alc);
}

inline void gLexerPostSuite() {
    Arena::release(&arena);
}

inline void gLexerPreCase() {
    Lex::init();
}

inline void gLexerPostCase() {
    Lex::release();
}



static void initSpanTest(Span* span, const char* input) {
    span->str = input;
    span->start = { 0, 0 };
    span->end = { -1, 0 };
    span->fileInfo = NULL;
}

static void assertToken(Lex::Token token, Lex::TokenKind expectedKind, Lex::TokenDetail expectedDetail = Lex::TD_NONE) {
    Test::assert(token.kind, expectedKind);
    if (expectedDetail != Lex::TD_NONE) {
        Test::assert(token.detail, expectedDetail);
    }
}



inline Test::Case gLexerCases[] = {

    TEST_CASE(test_keywords) {
        initSpanTest(&span,
            "i8 i16 i32 i64 u8 u16 u32 u64 f32 f64");

        Lex::Token token;

        while ((token = Lex::nextToken(&span, NULL)).kind != Lex::TK_END) {
            if (!Lex::isDtype(token)) {
                Test::assert(false);
            }
        }

        initSpanTest(&span,
            "const embed muton auton fcn def struct union if else while loop "
            "when case goto enum return continue break using scope namespace "
            "alloc free error catch import from true false as by null");

        while ((token = Lex::nextToken(&span, NULL)).kind != Lex::TK_END) {
            if (token.kind != Lex::TK_KEYWORD) {
                Test::assert(false);
            }
        }
    }},

    TEST_CASE(test_identifiers) {
        initSpanTest(&span,
            "foo foo_boo _private camelCase PascalCase foo123 _123");

        Lex::Token token;
        Lex::TokenValue val;
        const char* expected[] = {
            "foo", "foo_boo", "_private", "camelCase", "PascalCase", "foo123", "_123"
        };

        int i = 0;
        while ((token = Lex::nextToken(&span, &val)).kind != Lex::TK_END) {
            if (token.kind != Lex::TK_IDENTIFIER) {
                Test::assert(false);
            }
            Test::assert(cstrcmp(*val.str, expected[i++]));
        }
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

        Lex::Token token;
        Lex::TokenValue val;
        int count = 0;

        while ((token = Lex::nextToken(&span, &val)).kind != Lex::TK_END) {
            if (token.kind == Lex::TK_NUMBER) {
                ExpectedNumber exp = expected[count++];

                Test::assert(token.detail, exp.detail);

                if (token.detail == Lex::TD_DT_I64) {
                    Test::assert(val.ival, exp.rawIntVal);
                } else if (token.detail == Lex::TD_DT_F64) {
                    double actual = *(double*)&val.ival;
                    Test::assert(actual, exp.floatVal);
                } else if (token.detail == Lex::TD_DT_F32) {
                    Test::assert(token.detail, Lex::TD_DT_F32);
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

        String expected[] = {
            STR("hello"),
            STR("world"),
            STR(""),
            STR("with spaces"),
            STR("with\nescape"),
            STR("raw"),
            STR("path\\to\\file"),
            STR("1234567890"),
            STR("!@#$%^&*()_+-=[]{}|;:,.<>?"),
            STR("   "),
            STR("multiline\ntext\nhere"),
            STR("")
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::Token token;
        Lex::TokenValue val;

        int i = 0;
        while ((token = Lex::nextToken(&span, &val)).kind != Lex::TK_END) {
            if (token.kind == Lex::TK_STRING) {
                StringInitialization* init = (StringInitialization*) val.any;
                Test::assert(cstrcmp(expected[i++], init->rawData));
            }
        }
        Test::assert(i, expectedCount);
    }},

    TEST_CASE(test_string_literals_escape) {
        initSpanTest(&span,
            "\"\\a\""
            "\"\\b\""
            "\"\\f\""
            "\"\\n\""
            "\"\\r\""
            "\"\\t\""
            "\"\\v\""
            "\"\\\\\""
            "\"\\'\""
            "\"\\\"\""
            "\"\\?\""
            "\"\\0\""
            "\"\\x41\""
            "\"\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\\?\\0\\x41\""
        );

        String expected[] = {
            STR("\a"),
            STR("\b"),
            STR("\f"),
            STR("\n"),
            STR("\r"),
            STR("\t"),
            STR("\v"),
            STR("\\"),
            STR("\'"),
            STR("\""),
            STR("\?"),
            STR("\0"),
            STR("A"),          // \x41
            STR("\a\b\f\n\r\t\v\\\'\"\?\0A")
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::Token token;
        Lex::TokenValue val;

        int i = 0;
        while ((token = Lex::nextToken(&span, &val)).kind != Lex::TK_END) {
            if (token.kind == Lex::TK_STRING) {
                StringInitialization* init = (StringInitialization*) val.any;
                Test::assert(cstrcmp(expected[i++], init->rawData));
            }
        }
        Test::assert(i, expectedCount);
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

        const uint64_t expected[] = {
            'a',
            'Z',
            '0',
            ' ',
            '!',
            'BA',
            'CBA',
            'DCBA',
            '\n',
            '\t',
            '\\',
            '\'',
            '\x41',
            '\0'
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::Token token;
        Lex::TokenValue val;

        int i = 0;
        while ((token = Lex::nextToken(&span, &val)).kind != Lex::TK_END) {
            if (token.kind == Lex::TK_CHAR) {
                Test::assert(val.ival == expected[i++]);
            }
        }
        Test::assert(i, expectedCount);
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
            "'\\\\' "
            "'\\'' "
            "'\\\"' "
            "'\\?' "
            "'\\0' "
            "'\\x41' "
        );

        const uint64_t expected[] = {
            '\a',
            '\b',
            '\f',
            '\n',
            '\r',
            '\t',
            '\v',
            '\\',
            '\'',
            '\"',
            '\?',
            '\0',
            '\x41',
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        Lex::Token token;
        Lex::TokenValue val;

        int i = 0;
        while ((token = Lex::nextToken(&span, &val)).kind != Lex::TK_END) {
            if (token.kind == Lex::TK_CHAR) {
                Test::assert(val.ival == expected[i++]);
            }
        }
        Test::assert(i, expectedCount);
    }},

    TEST_CASE(test_operators) {
        initSpanTest(&span, "+ - * / % ++ -- == != < > <= >= && || ! & | ^ ~ << >> .. .");

        Lex::Token token;
        int count = 0;

        Lex::TokenKind expected[] = {
            // Arithmetic
            Lex::TK_OP_PLUS,
            Lex::TK_OP_MINUS,
            Lex::TK_OP_MULTIPLICATION,
            Lex::TK_OP_DIVISION,
            Lex::TK_OP_MODULO,
            Lex::TK_OP_INCREMENT,
            Lex::TK_OP_DECREMENT,

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
            Lex::TK_OP_MEMBER_SELECTION
        };

        const int expectedCount = sizeof(expected) / sizeof(expected[0]);

        while ((token = Lex::nextToken(&span, NULL)).kind != Lex::TK_END) {
            Test::assert(token.kind, expected[count++]);
        }

        // Verify that the lexer produced the exact total number of expected tokens
        Test::assert(count, expectedCount);
    }},

    TEST_CASE(test_punctuation) {
        initSpanTest(&span, "; : :: , ( ) { } [ ]");

        Lex::Token token;
        int count = 0;
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

        while ((token = Lex::nextToken(&span, NULL)).kind != Lex::TK_END) {
            Test::assert(token.kind, expected[count++]);
        }
    }},

    TEST_CASE(test_skip_token) {
        initSpanTest(&span, "_ __ ___ _ _foo _bar");

        Lex::Token token;

        int count = 0;
        while ((token = Lex::nextToken(&span, NULL)).kind != Lex::TK_END) {
            if (token.kind == Lex::TK_SKIP) count++;
        }
        Test::assert(count, 2);
    }},

    TEST_CASE(test_qualified_names) {
        initSpanTest(&span, "foo::bar baz::qux::quux");

        Lex::Token token;
        Lex::TokenValue val;
        int count = 0;

        while ((token = Lex::nextToken(&span, &val)).kind != Lex::TK_END) {
            if (token.kind != Lex::TK_IDENTIFIER) {
                Test::assert(false);
            }
        }
    }},

    TEST_CASE(test_comments_single_line) {
        initSpanTest(&span, "foo // comment\nbar");

        Lex::Token token;
        int count = 0;
        while ((token = Lex::nextToken(&span, NULL)).kind != Lex::TK_END) {
            if (token.kind == Lex::TK_IDENTIFIER) {
                count++;
            }
        }
        Test::assert(count, 2);

        Lex::release();
    }},

    TEST_CASE(test_comments_multi_line) {
        initSpanTest(&span, "foo /{ comment /} bar /{ nested /{ comment /} /} baz");

        Lex::Token token;
        int count = 0;
        while ((token = Lex::nextToken(&span, NULL)).kind != Lex::TK_END) {
            if (token.kind == Lex::TK_IDENTIFIER) {
                count++;
            }
        }
        Test::assert(count, 3);
    }},

    TEST_CASE(test_newlines) {
        initSpanTest(&span, "foo\nbar\n\nbaz");

        Lex::Token token;
        int count = 0;
        while ((token = Lex::nextToken(&span, NULL)).kind != Lex::TK_END) {
            if (token.kind == Lex::TK_IDENTIFIER) {
                count++;
            }
        }
        Test::assert(count, 3);
    }},

    TEST_CASE(test_eof) {
        initSpanTest(&span, "");

        Lex::Token token = Lex::nextToken(&span, NULL);
        Test::assert(token.kind, Lex::TK_END);
    }},

    TEST_CASE(test_large_token_buffers) {
        constexpr int len = 4 * 1024;
        char largeIdentifier[len];

        memset(largeIdentifier, 'a', 2000);
        largeIdentifier[2000] = '\0';

        initSpanTest(&span, largeIdentifier);
        Test::assert(Lex::nextToken(&span, NULL).kind, Lex::TK_IDENTIFIER);
    }},

    TEST_CASE(test_unclosed_literals) {
        //initSpanTest(&span, "\"unclosed string");
        //Test::assert(Lex::nextToken(&span, NULL).kind, Lex::TK_NONE);

        //initSpanTest(&span, "/{ unclosed comment");
        //Test::assert(Lex::nextToken(&span, NULL).kind, Lex::TK_NONE);

        //initSpanTest(&span, "'a");
        //Test::assert(Lex::nextToken(&span, NULL).kind, Lex::TK_NONE);
        Test::assert(false);
    }},

    TEST_CASE(test_try_token) {
        initSpanTest(&span, "foo bar");

        Lex::Token token = Lex::tryToken(&span, Lex::toToken(Lex::TK_IDENTIFIER), NULL);
        Test::assert(token.kind, Lex::TK_IDENTIFIER);

        token = Lex::tryToken(&span, Lex::toToken(Lex::TK_KEYWORD), NULL);
        Test::assert(token.kind, Lex::TK_NONE);
    }},

    TEST_CASE(test_peek_token) {
        initSpanTest(&span, "foo bar");

        Lex::Token peeked = Lex::peekToken(&span, NULL);
        Test::assert(peeked.kind, Lex::TK_IDENTIFIER);

        Lex::Token consumed = Lex::nextToken(&span, NULL);
        Test::assert(consumed.kind, Lex::TK_IDENTIFIER);
    }},

    TEST_CASE(test_sync_token) {
        initSpanTest(&span, "foo bar baz qux");

        Lex::Token token = Lex::syncToken(&span, Lex::toToken(Lex::TK_IDENTIFIER), Lex::toToken(Lex::TK_KEYWORD), NULL);
        Test::assert(token.kind, Lex::TK_IDENTIFIER);

        token = Lex::syncToken(&span, Lex::toToken(Lex::TK_IDENTIFIER), Lex::toToken(Lex::TK_KEYWORD), NULL);
        Test::assert(token.kind, Lex::TK_IDENTIFIER);
    }},

    TEST_CASE(test_try_keyword) {
        initSpanTest(&span, "if else");

        Lex::Token token = Lex::tryKeyword(&span, KW_IF);
        Test::assert(token.kind, Lex::TK_KEYWORD);
        Test::assert(token.detail, KW_IF);

        token = Lex::tryKeyword(&span, KW_WHILE);
        Test::assert(token.kind, Lex::TK_NONE);
    }},

};



extern const Test::Suite gLexerSuite = {
    "Lexer",
    gLexerCases,
    sizeof(gLexerCases) / sizeof(Test::Case),
    gLexerPreCase,
    gLexerPostCase,
    gLexerPreSuite,
    gLexerPostSuite,
};
