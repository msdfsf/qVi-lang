#include "test_parser.h"

static void assertEnumBase(
    Enumerator* eNode,
    const char* expectedName,
    uint32_t expectedVarCount,
    Type::Kind expectedMemberTypeKind = Type::DT_UNDEFINED
) {
    Test::assertOrDie(eNode != NULL);

    if (expectedName != NULL) {
        Test::assert(cstrcmp((String*)&eNode->name, (String*)expectedName) == 0);
    }

    Test::assert(eNode->varCount == expectedVarCount);

    if (expectedMemberTypeKind != Type::DT_UNDEFINED) {
        Test::assertOrDie(eNode->memberType != NULL);
        Test::assert(eNode->memberType->kind == expectedMemberTypeKind);
    }
}

static void assertEnumMember(
    Enumerator* eNode,
    uint32_t index,
    const char* expectedMemberName,
    bool hasInitializer = false
) {
    Test::assertOrDie(eNode != NULL);
    Test::assertOrDie(index < eNode->varCount);
    Test::assertOrDie(eNode->vars != NULL);
    Test::assertOrDie(eNode->vars[index] != NULL);

    Test::assert(cstrcmp((String*)&eNode->vars[index]->name, (String*)expectedMemberName) == 0);

    if (hasInitializer) {
        Test::assert(
            eNode->vars[index]->expression != NULL ||
            eNode->vars[index]->value.hasValue
        );
    } else {
        Test::assert(eNode->vars[index]->expression == NULL);
    }
}

#define PARSE_ENUM(enum_name, src_code)                                    \
    initQName(enum_name);                                                  \
    initSpanTest(src_code);                                                \
    Parser::parseEnumDefinition(&ctx, &span, &name); \
    Enumerator* enumNode = *(Enumerator**)DArray::get(&ctx.nodeStack, 0);  \
    Test::assertOrDie(enumNode != NULL);



inline Test::Case gEnumParserCases[] = {

    TEST_CASE(test_enum_basic_untyped_backing) {
        PARSE_ENUM(
            "Status",
            "enum {\n"
            "    Ready;\n"
            "    Pending;\n"
            "    Failed;\n"
            "}"
        );

        assertEnumBase(enumNode, "Status", 3 /*varCount*/);
        assertEnumMember(enumNode, 0, "Ready",   false /*hasInit*/);
        assertEnumMember(enumNode, 1, "Pending", false /*hasInit*/);
        assertEnumMember(enumNode, 2, "Failed",  false /*hasInit*/);
    }},

    TEST_CASE(test_enum_explicit_u8_backing) {
        PARSE_ENUM(
            "Color",
            "enum u8 {\n"
            "    Red;\n"
            "    Green;\n"
            "    Blue;\n"
            "}"
        );

        assertEnumBase(enumNode, "Color", 3 /*varCount*/, Type::DT_U8);
        assertEnumMember(enumNode, 0, "Red");
        assertEnumMember(enumNode, 1, "Green");
        assertEnumMember(enumNode, 2, "Blue");
    }},

    TEST_CASE(test_enum_explicit_i64_backing) {
        PARSE_ENUM(
            "BigFlags",
            "enum i64 {\n"
            "    FlagA;\n"
            "    FlagB;\n"
            "}"
        );

        assertEnumBase(enumNode, "BigFlags", 2 /*varCount*/, Type::DT_I64);
        assertEnumMember(enumNode, 0, "FlagA");
        assertEnumMember(enumNode, 1, "FlagB");
    }},

    TEST_CASE(test_enum_explicit_values) {
        PARSE_ENUM(
            "HttpStatus",
            "enum u32 {\n"
            "    Ok = 200;\n"
            "    NotFound = 404;\n"
            "    InternalError = 500;\n"
            "}"
        );

        assertEnumBase(enumNode, "HttpStatus", 3 /*varCount*/, Type::DT_U32);
        assertEnumMember(enumNode, 0, "Ok",            true /*hasInit*/);
        assertEnumMember(enumNode, 1, "NotFound",      true /*hasInit*/);
        assertEnumMember(enumNode, 2, "InternalError", true /*hasInit*/);
    }},

    TEST_CASE(test_enum_mixed_values) {
        PARSE_ENUM(
            "Permissions",
            "enum {\n"
            "    None = 0;\n"
            "    Read = 1;\n"
            "    Write = 2;\n"
            "    All;\n" // Automatically assigned next sequential value in sema
            "}"
        );

        assertEnumBase(enumNode, "Permissions", 4 /*varCount*/);
        assertEnumMember(enumNode, 0, "None",  true /*hasInit*/);
        assertEnumMember(enumNode, 1, "Read",  true /*hasInit*/);
        assertEnumMember(enumNode, 2, "Write", true /*hasInit*/);
        assertEnumMember(enumNode, 3, "All",   false /*hasInit*/);
    }},

    TEST_CASE(test_enum_single_variant) {
        PARSE_ENUM(
            "Single",
            "enum {\n"
            "    OnlyOne;\n"
            "}"
        );

        assertEnumBase(enumNode, "Single", 1 /*varCount*/);
        assertEnumMember(enumNode, 0, "OnlyOne", false);
    }}
};



extern const Test::Suite gParserSuiteEnum = {
    "Parser - Enumerator",
    gEnumParserCases,
    sizeof(gEnumParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
