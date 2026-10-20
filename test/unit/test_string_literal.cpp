#include "../../src/parser.h"
#include "../test_core.h"
#include <cstdint>



#define STR(lit)   (String { (char*) lit, sizeof(lit) - 1 })
#define STR16(lit) (String { (char*) u##lit, sizeof(u##lit) - sizeof(char16_t) })
#define STR32(lit) (String { (char*) U##lit, sizeof(U##lit) - sizeof(char32_t) })

// requestedAlign: 0 = auto, 1 = raw UTF-8 bytes, 2/4 = decoded element width
#define INIT(x) \
    const char str[] = x; \
    const int len = sizeof(str) - 1; \
    StringInitialization* init = NULL;

#define BASIC_ASSERT(type) \
    Test::assert(ans, (int64_t) Err::OK); \
    Test::assertOrDie(init != NULL); \
    Test::assert(init->charType, Type::basicTypes + type);

void match(const char* str, const int len, String out) {
    if (Test::assert(out.len == (uint64_t) len)) return;

    for (int i = 0; i < len && i < (int) out.len; i++) {
        Test::assert(str[i], out.buff[i]);
    }
}

inline Test::Case gStringLiteralCases[] = {

    TEST_CASE(test_parse_string_raw) {
        INIT("hello\"");
        int64_t ans = Parser::parseStringLiteral(str, len, 1, &init);
        BASIC_ASSERT(Type::DT_U8);
        match(str, len - 1, init->rawData);
    }},

    TEST_CASE(test_parse_string_raw_unicode) {
        INIT("Манты\"");
        int64_t ans = Parser::parseStringLiteral(str, len, 1, &init);
        BASIC_ASSERT(Type::DT_U8);
        match(str, len - 1, init->rawData);
    }},

    TEST_CASE(test_parse_string_auto) {
        INIT("hello\"");
        int64_t ans = Parser::parseStringLiteral(str, len, 0, &init);
        BASIC_ASSERT(Type::DT_U8);
        match(str, len - 1, init->rawData);
    }},

    TEST_CASE(test_parse_string_auto_unicode) {
        INIT("Манты\"");
        int64_t ans = Parser::parseStringLiteral(str, len, 0, &init);
        BASIC_ASSERT(Type::DT_U16);
        Test::assert(cstrcmp(STR16("Манты"), init->rawData));
    }},

    TEST_CASE(test_parse_string_empty) {
        INIT("\"");
        int64_t ans = Parser::parseStringLiteral(str, len, 1, &init);
        BASIC_ASSERT(Type::DT_U8);
        match(str, len - 1, init->rawData);
    }},

    TEST_CASE(test_parse_string_u16) {
        const char* input[] = {
            "hello\"",
            "café\"",
            "Илюха Мэд БАЗА\"",
            "αβγδε\"",
            "世界\"",
            "100 €\""
        };

        String expected[] = {
            STR16("hello"),
            STR16("café"),
            STR16("Илюха Мэд БАЗА"),
            STR16("αβγδε"),
            STR16("世界"),
            STR16("100 €")
        };

        const int n = sizeof(expected) / sizeof(expected[0]);
        for (int i = 0; i < n; i++) {
            const char* str = input[i];
            const int len = strlen(str);

            StringInitialization* init = NULL;
            int64_t ans = Parser::parseStringLiteral(str, len, 2, &init);

            BASIC_ASSERT(Type::DT_U16);
            Test::assert(cstrcmp(expected[i], init->rawData));
        }
    }},

    TEST_CASE(test_parse_string_u32) {
        const char* input[] = {
            "hello\"",
            "café\"",
            "世界\"",
            "!RPG Марафон, едем 😎😎😎\"",
            "Богдан 🚀 Rocket\"",
            "Math: 𝐀𝐁𝐂\""
        };

        String expected[] = {
            STR32("hello"),
            STR32("café"),
            STR32("世界"),
            STR32("!RPG Марафон, едем 😎😎😎"),
            STR32("Богдан 🚀 Rocket"),
            STR32("Math: 𝐀𝐁𝐂")
        };

        const int n = sizeof(expected) / sizeof(expected[0]);
        for (int i = 0; i < n; i++) {
            const char* str = input[i];
            const int len = strlen(str);

            StringInitialization* init = NULL;
            int64_t ans = Parser::parseStringLiteral(str, len, 4, &init);

            BASIC_ASSERT(Type::DT_U32);
            Test::assert(cstrcmp(expected[i], init->rawData));
        }
    }},

    TEST_CASE(test_string_alignment_rejections) {
        StringInitialization* init = NULL;

        // 1. '😎' requires 4 bytes, but u16 allows only 2
        {
            const char* str = "😎\"";
            int64_t ans = Parser::parseStringLiteral(str, strlen(str), 2, &init);
            Test::assert(ans < 0);
        }

        // 2. u64 is not a valid string element width
        {
            const char* str = "hello\"";
            int64_t ans = Parser::parseStringLiteral(str, strlen(str), 8, &init);
            Test::assert(ans < 0);
        }

        // 3. decoded strings reject invalid UTF-8, while raw bytes keep it
        {
            const char* str = "\\xFF\"";
            int64_t ans = Parser::parseStringLiteral(str, strlen(str), 2, &init);
            Test::assert(ans < 0);

            ans = Parser::parseStringLiteral(str, strlen(str), 1, &init);
            BASIC_ASSERT(Type::DT_U8);
            match("\xFF", 1, init->rawData);
        }
    }},

    TEST_CASE(test_parse_string_escape) {
        const char* input[] = {
            "\\a\"",
            "\\b\"",
            "\\f\"",
            "\\n\"",
            "\\r\"",
            "\\t\"",
            "\\v\"",
            "\\\\\"",
            "\\'\"",
            "\\\"\"",
            "\\?\"",
            "\\0\"",
            "\\x41\"",
            "\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\\?\\0\\x41\""
        };

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

        const int n = sizeof(expected) / sizeof(expected[0]);
        for (int i = 0; i < n; i++) {
            const char* str = input[i];
            const int len = strlen(str);

            StringInitialization* init;
            int64_t ans = Parser::parseStringLiteral(str, len, 1, &init);

            BASIC_ASSERT(Type::DT_U8);
            Test::assert(cstrcmp(expected[i], init->rawData));
        }
    }},

    TEST_CASE(test_parse_string_unicode_escape) {
        // Raw bytes keep the UTF-8 encoding of the code point
        {
            INIT("\\u0416\"");
            int64_t ans = Parser::parseStringLiteral(str, len, 1, &init);
            BASIC_ASSERT(Type::DT_U8);
            match(STR("Ж"), 2, init->rawData);
        }

        // Decoded arrays get the code point itself
        {
            INIT("\\u0416\"");
            int64_t ans = Parser::parseStringLiteral(str, len, 2, &init);
            BASIC_ASSERT(Type::DT_U16);
            Test::assert(cstrcmp(STR16("\u0416"), init->rawData));
        }

        // Same for astral code points
        {
            INIT("\\U0001F60E\"");
            int64_t ans = Parser::parseStringLiteral(str, len, 4, &init);
            BASIC_ASSERT(Type::DT_U32);
            Test::assert(cstrcmp(STR32("\U0001F60E"), init->rawData));
        }
    }}

};

extern const Test::Suite gStringLiteralSuite = {
    "String Literal Parser",
    NULL,
    gStringLiteralCases,
    sizeof(gStringLiteralCases) / sizeof(Test::Case),
};
