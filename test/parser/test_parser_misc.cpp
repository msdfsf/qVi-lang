#include "test_parser.h"



static void assertReturn(
    ReturnStatement* retNode,
    bool hasValue,
    bool hasError
) {
    Test::assertOrDie(retNode != NULL);

    if (hasValue) {
        Test::assert(retNode->var != NULL);
    }

    if (hasError) {
        Test::assert(retNode->err != NULL);
    }
}

static void assertBreak(
    BreakStatement* breakNode,
    const char* expectedTargetLabel = NULL
) {
    Test::assertOrDie(breakNode != NULL);

    if (expectedTargetLabel != NULL) {
        Test::assertOrDie(breakNode->target != NULL);
        Test::assert(cstrcmp((String*)&((Variable*)breakNode->target)->name, (String*)expectedTargetLabel) == 0);
    } else {
        Test::assert(breakNode->target == NULL);
    }
}

static void assertContinue(
    ContinueStatement* contNode,
    const char* expectedTargetLabel = NULL
) {
    Test::assertOrDie(contNode != NULL);

    if (expectedTargetLabel != NULL) {
        Test::assertOrDie(contNode->target != NULL);
        Test::assert(cstrcmp((String*)&((Variable*)contNode->target)->name, (String*)expectedTargetLabel) == 0);
    } else {
        Test::assert(contNode->target == NULL);
    }
}

#define PARSE_RETURN(src_code)                                              \
    initSpanTest(src_code);           \
    Function* fcn = Ast::Node::makeFunction(); \
    ctx.currentFunction = fcn; \
    Parser::parseReturnStatement(&ctx, &span);   \
    ReturnStatement* retNode = *(ReturnStatement**)DArray::get(&ctx.nodeStack, 0); \
    Test::assertOrDie(retNode != NULL);

#define PARSE_BREAK(src_code)                                               \
    initSpanTest(src_code);                                                 \
    Parser::parseBreakStatement(&ctx, &span);    \
    BreakStatement* breakNode = *(BreakStatement**)DArray::get(&ctx.nodeStack, 0); \
    Test::assertOrDie(breakNode != NULL);

#define PARSE_CONTINUE(src_code)                                            \
    initSpanTest(src_code);                                                 \
    Parser::parseContinueStatement(&ctx, &span); \
    ContinueStatement* contNode = *(ContinueStatement**)DArray::get(&ctx.nodeStack, 0); \
    Test::assertOrDie(contNode != NULL);



inline Test::Case gMiscParserCases[] = {

    TEST_CASE(test_return_bare_void) {
        PARSE_RETURN(";");
        assertReturn(retNode, false /*hasValue*/, false /*hasError*/);
    }},

    TEST_CASE(test_return_value) {
        PARSE_RETURN(" a + b;");
        assertReturn(retNode, true /*hasValue*/, false /*hasError*/);
        Test::assert(retNode->var != NULL);
    }},

    TEST_CASE(test_return_value_and_error) {
        PARSE_RETURN(" a + b * 2, FileError::NotFound;");
        assertReturn(retNode, true /*hasValue*/, true /*hasError*/);
        Test::assert(retNode->err->name.pathSize == 1);
    }},

    TEST_CASE(test_return_only_error) {
        PARSE_RETURN(" _, FileError::NotFound;");
        assertReturn(retNode, false /*hasValue*/, true /*hasError*/);
        Test::assert(retNode->err->name.pathSize == 1);
    }},



    TEST_CASE(test_break_bare_unlabeled) {
        PARSE_BREAK(";");
        // Target is NULL for bare break
        assertBreak(breakNode, NULL);
    }},

    TEST_CASE(test_break_labeled_target) {
        PARSE_BREAK(" outer;");
        // Target captures the label identifier
        assertBreak(breakNode, "outer");
    }},

    TEST_CASE(test_break_labeled_scope_target) {
        PARSE_BREAK(" parse_packet;");
        assertBreak(breakNode, "parse_packet");
    }},



    TEST_CASE(test_continue_bare_unlabeled) {
        PARSE_CONTINUE("continue;");
        // Target is NULL for bare continue
        assertContinue(contNode, NULL);
    }},

    TEST_CASE(test_continue_labeled_target) {
        PARSE_CONTINUE(" outer_loop;");
        // Target captures the loop label identifier
        assertContinue(contNode, "outer_loop");
    }}
};



extern const Test::Suite gParserSuiteMisc = {
    "Parser - Return, Break, Continue",
    gMiscParserCases,
    sizeof(gMiscParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
