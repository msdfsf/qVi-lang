#include "test_parser.h"



static void assertBranchCounts(
    Branch* branch,
    uint32_t expectedScopeCount,
    uint32_t expectedExpressionCount
) {
    Test::assertOrDie(branch != NULL);

    Test::assert(branch->scopeCount == expectedScopeCount);
    Test::assert(branch->expressionCount == expectedExpressionCount);

    Test::assert(
        branch->scopeCount == branch->expressionCount ||
        branch->scopeCount == branch->expressionCount + 1
    );
}

static void assertBranchCondition(
    Branch* branch,
    uint32_t index,
    const char* expectedExpName = NULL
) {
    Test::assertOrDie(branch != NULL);
    Test::assertOrDie(index < branch->expressionCount);
    Test::assertOrDie(branch->expressions != NULL);
    Test::assertOrDie(branch->expressions[index] != NULL);

    if (expectedExpName != NULL) {
        Test::assert(cstrcmp((String*)&branch->expressions[index]->name, (String*)expectedExpName) == 0);
    }
}

static void assertBranchScope(Branch* branch, uint32_t index) {
    Test::assertOrDie(branch != NULL);
    Test::assertOrDie(index < branch->scopeCount);
    Test::assertOrDie(branch->scopes != NULL);
    Test::assertOrDie(branch->scopes[index] != NULL);
}

#define PARSE_BRANCH(src_code)                                             \
    initSpanTest(src_code);                                                \
    Parser::parseIfStatement(&ctx, &span);  \
    Branch* branch = *(Branch**)DArray::get(&ctx.nodeStack, 0);            \
    Test::assertOrDie(branch != NULL);



inline Test::Case gIfParserCases[] = {

    TEST_CASE(test_if_basic_block) {
        PARSE_BRANCH(
            " is_ready {\n"
            "    process();\n"
            "}"
        );

        assertBranchCounts(branch, 1 /*scopes*/, 1 /*expressions*/);
        assertBranchCondition(branch, 0, "is_ready");
        assertBranchScope(branch, 0);
    }},

    TEST_CASE(test_if_single_statement_scope) {
        PARSE_BRANCH(" is_valid: do_work();");

        assertBranchCounts(branch, 1, 1);
        assertBranchCondition(branch, 0, "is_valid");
        assertBranchScope(branch, 0);
    }},

    TEST_CASE(test_if_else_blocks) {
        PARSE_BRANCH(
            " is_ready {\n"
            "    start();\n"
            "} else {\n"
            "    wait();\n"
            "}"
        );

        assertBranchCounts(branch, 2 /*scopes*/, 1 /*expressions*/);
        assertBranchCondition(branch, 0, "is_ready");
        assertBranchScope(branch, 0); // If body
        assertBranchScope(branch, 1); // Else body
    }},

    TEST_CASE(test_if_else_single_lines) {
        PARSE_BRANCH(" quick: fast_path(); else: slow_path();");

        assertBranchCounts(branch, 2, 1);
        assertBranchCondition(branch, 0, "quick");
        assertBranchScope(branch, 0);
        assertBranchScope(branch, 1);
    }},

    TEST_CASE(test_if_else_if_else_full_ladder) {
        PARSE_BRANCH(
            " status == 1 {\n"
            "    handle_one();\n"
            "} else if status == 2 {\n"
            "    handle_two();\n"
            "} else {\n"
            "    handle_default();\n"
            "}"
        );

        assertBranchCounts(branch, 3 /*scopes*/, 2 /*expressions*/);
        assertBranchCondition(branch, 0, NULL);
        assertBranchCondition(branch, 1, NULL);
        assertBranchScope(branch, 0); // if body
        assertBranchScope(branch, 1); // else if body
        assertBranchScope(branch, 2); // else body
    }},

    TEST_CASE(test_if_else_if_no_else) {
        PARSE_BRANCH(
            " state == 1 {\n"
            "    step_a();\n"
            "} else if state == 2 {\n"
            "    step_b();\n"
            "}"
        );

        assertBranchCounts(branch, 2 /*scopes*/, 2 /*expressions*/);
        assertBranchCondition(branch, 0, NULL);
        assertBranchCondition(branch, 1, NULL);
        assertBranchScope(branch, 0);
        assertBranchScope(branch, 1);
    }},

    TEST_CASE(test_if_mixed_scope_styles) {
        PARSE_BRANCH(
            " is_fast: return fast_calc();\n"
            "else if is_medium: return medium_calc();\n"
            "else {\n"
            "    log_slow_warning();\n"
            "    return slow_calc();\n"
            "}"
        );

        assertBranchCounts(branch, 3, 2);
        assertBranchCondition(branch, 0, "is_fast");
        assertBranchCondition(branch, 1, "is_medium");
        assertBranchScope(branch, 0);
        assertBranchScope(branch, 1);
        assertBranchScope(branch, 2);
    }}
};



extern const Test::Suite gParserSuiteIf = {
    "Parser - If",
    gIfParserCases,
    sizeof(gIfParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
