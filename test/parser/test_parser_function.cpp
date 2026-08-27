#include "test_parser.h"



static void assertFunctionBase(
    Function* fn,
    const char* expectedName,
    uint32_t expectedInArgCount,
    bool hasReturn,
    bool hasBody
) {
    Test::assertOrDie(fn != NULL);

    if (expectedName != NULL) {
        Test::assert(cstrcmp((String*)&fn->name, (String*)expectedName) == 0);
    }

    Test::assert(fn->prototype.inArgCount == expectedInArgCount);
    Test::assert((fn->prototype.outArg != NULL) == hasReturn);
    Test::assert((fn->bodyScope != NULL) == hasBody);
}

static void assertInArg(
    Function* fn,
    uint32_t index,
    const char* expectedParamName,
    Type::Kind expectedType = Type::DT_UNDEFINED
) {
    Test::assertOrDie(fn != NULL);
    Test::assertOrDie(index < fn->prototype.inArgCount);
    Test::assertOrDie(fn->prototype.inArgs != NULL);
    Test::assertOrDie(fn->prototype.inArgs[index] != NULL);
    Test::assertOrDie(fn->prototype.inArgs[index]->var != NULL);

    if (expectedParamName != NULL) {
        Test::assert(cstrcmp((String*)&fn->prototype.inArgs[index]->var->name, (String*)expectedParamName) == 0);
    }

    if (expectedType != Type::DT_UNDEFINED) {
        Test::assert(fn->prototype.inArgs[index]->type.baseType == expectedType);
    }
}

static void assertReturnType(
    Function* fn,
    Type::Kind expectedType,
    int expectedDecoratorCount = 0
) {
    Test::assertOrDie(fn != NULL);
    Test::assertOrDie(fn->prototype.outArg != NULL);

    Test::assert(fn->prototype.outArg->type.baseType == expectedType);
    Test::assert(fn->prototype.outArg->type.decoratorCount == expectedDecoratorCount);
}

#define PARSE_FUNCTION(fn_name, src_code)                                           \
    initQName(fn_name);                                                             \
    initSpanTest(src_code);                                                         \
    Parser::parseFunction(&ctx, &span, &name, NULL_FLAG); \
    Function* fn = *(Function**) DArray::get(&ctx.nodeStack, 0);                     \
    Test::assertOrDie(fn != NULL);



inline Test::Case gFunctionParserCases[] = {

    TEST_CASE(test_function_standard_with_return) {
        PARSE_FUNCTION(
            "add",
            "a: i32, b: i32) -> i32 {\n"
            "    return a + b;\n"
            "}"
        );

        assertFunctionBase(fn, "add", 2 /*inArgs*/, true /*hasReturn*/, true /*hasBody*/);
        assertInArg(fn, 0, "a", Type::DT_I32);
        assertInArg(fn, 1, "b", Type::DT_I32);
        assertReturnType(fn, Type::DT_I32);
    }},

    TEST_CASE(test_function_void_no_return) {
        PARSE_FUNCTION(
            "log_message",
            "id: u64) {\n"
            "    print(id);\n"
            "}"
        );

        assertFunctionBase(fn, "log_message", 1 /*inArgs*/, false /*hasReturn*/, true /*hasBody*/);
        assertInArg(fn, 0, "id", Type::DT_U64);
        Test::assert(fn->prototype.outArg == NULL);
    }},

    TEST_CASE(test_function_zero_parameters) {
        PARSE_FUNCTION(
            "get_tick_count",
            ") -> u64 {\n"
            "    return 1000;\n"
            "}"
        );

        assertFunctionBase(fn, "get_tick_count", 0 /*inArgs*/, true /*hasReturn*/, true /*hasBody*/);
        assertReturnType(fn, Type::DT_U64);
    }},

    TEST_CASE(test_function_prototype_declaration_only) {
        ctx.foreignContext = true;
        PARSE_FUNCTION(
            "draw_pixel",
            "x: f32, y: f32) -> u8;"
        );

        assertFunctionBase(fn, "draw_pixel", 2 /*inArgs*/, true /*hasReturn*/, false /*hasBody*/);
        assertInArg(fn, 0, "x", Type::DT_F32);
        assertInArg(fn, 1, "y", Type::DT_F32);
        assertReturnType(fn, Type::DT_U8);
        Test::assert(fn->bodyScope == NULL);
    }},

    TEST_CASE(test_function_return_pointer) {
        PARSE_FUNCTION(
            "find_node",
            "id: u64) -> Node^ {\n"
            "    return null;\n"
            "}"
        );

        assertFunctionBase(fn, "find_node", 1, true, true);
        assertInArg(fn, 0, "id", Type::DT_U64);
        assertReturnType(fn, Type::DT_UNDEFINED, 1 /*decoratorCount*/);
        Test::assert(fn->prototype.outArg->type.decorators[0]->kind == TypeDecorator::DEC_POINTER);
    }},

    TEST_CASE(test_function_with_error_set) {
        PARSE_FUNCTION(
            "read_packet",
            "buffer: u8^) using NetError -> u64 {\n"
            "    return 64;\n"
            "}"
        );

        assertFunctionBase(fn, "read_packet", 1, true, true);
        assertInArg(fn, 0, "buffer", Type::DT_U8);
        assertReturnType(fn, Type::DT_U64);

        Test::assertOrDie(fn->errorSetName != NULL);
    }},

    TEST_CASE(test_function_single_statement_body) {
        PARSE_FUNCTION(
            "square",
            "x: i32) -> i32: return x * x;"
        );

        assertFunctionBase(fn, "square", 1, true, true);
        assertInArg(fn, 0, "x", Type::DT_I32);
        assertReturnType(fn, Type::DT_I32);
        Test::assert(fn->bodyScope != NULL);
    }}
};



extern const Test::Suite gParserSuiteFunction = {
    "Parser - Function",
    gFunctionParserCases,
    sizeof(gFunctionParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
