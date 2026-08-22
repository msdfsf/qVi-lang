#include "test_parser.h"

static void assertCaseBase(
    SwitchCase* node,
    const char* expectedSwitchVar,
    uint32_t expectedCaseCount,
    bool hasElse
) {
    Test::assertOrDie(node != NULL);
    Test::assertOrDie(node->switchExp != NULL);

    if (expectedSwitchVar != NULL) {
        Test::assert(cstrcmp((String*)&node->switchExp->name, (String*)expectedSwitchVar) == 0);
    }

    Test::assert(node->caseCount == expectedCaseCount);
    Test::assert(node->caseExpCount == expectedCaseCount);

    Test::assert((node->elseCase != NULL) == hasElse);
}

static void assertWhenBranch(
    SwitchCase* node,
    uint32_t index,
    const char* expectedExpName
) {
    Test::assertOrDie(node != NULL);
    Test::assertOrDie(index < node->caseCount);
    Test::assertOrDie(node->casesExp != NULL);
    Test::assertOrDie(node->cases != NULL);

    Test::assertOrDie(node->casesExp[index] != NULL);
    if (expectedExpName != NULL) {
        Test::assert(cstrcmp((String*)&node->casesExp[index]->name, (String*)expectedExpName) == 0);
    }

    Test::assertOrDie(node->cases[index] != NULL);
}

#define PARSE_CASE(src_code)                                              \
    initSpanTest(src_code);                                               \
    Scope* scope = Ast::Node::makeScope();                                \
    ctx.currentScope = scope;                                             \
    Parser::parseCaseStatement(&ctx, &span);                              \
    SwitchCase* caseNode = *(SwitchCase**)DArray::get(&ctx.nodeStack, 0); \
    Test::assertOrDie(caseNode != NULL);



inline Test::Case gCaseParserCases[] = {

    TEST_CASE(test_case_single_line_branches) {
        PARSE_CASE(
            " status {\n"
            "    when 1: handle_one();\n"
            "    when 2: handle_two();\n"
            "}"
        );

        assertCaseBase(caseNode, "status", 2 /*count*/, false /*hasElse*/);
        assertWhenBranch(caseNode, 0, "1");
        assertWhenBranch(caseNode, 1, "2");
    }},

    TEST_CASE(test_case_block_scope_branches) {
        PARSE_CASE(
            " action {\n"
            "    when 10 {\n"
            "        init();\n"
            "        run();\n"
            "    }\n"
            "    when 20 {\n"
            "        stop();\n"
            "    }\n"
            "}"
        );

        assertCaseBase(caseNode, "action", 2 /*count*/, false /*hasElse*/);
        assertWhenBranch(caseNode, 0, "10");
        assertWhenBranch(caseNode, 1, "20");
    }},

    TEST_CASE(test_case_with_else_fallback) {
        PARSE_CASE(
            " x {\n"
            "    when 1: do_a();\n"
            "    when 2: do_b();\n"
            "    else:   do_default();\n"
            "}"
        );

        assertCaseBase(caseNode, "x", 2 /*count*/, true /*hasElse*/);
        assertWhenBranch(caseNode, 0, "1");
        assertWhenBranch(caseNode, 1, "2");
        Test::assertOrDie(caseNode->elseCase != NULL);
    }},

    TEST_CASE(test_case_with_else_block_scope) {
        PARSE_CASE(
            " x {\n"
            "    when 1: do_a();\n"
            "    else {\n"
            "        log_error();\n"
            "        recover();\n"
            "    }\n"
            "}"
        );

        assertCaseBase(caseNode, "x", 1 /*count*/, true /*hasElse*/);
        assertWhenBranch(caseNode, 0, "1");
        Test::assertOrDie(caseNode->elseCase != NULL);
    }},

    TEST_CASE(test_case_dynamic_expressions) {
        PARSE_CASE(
            " input_val {\n"
            "    when get_threshold(): handle_threshold();\n"
            "    when base_limit * 2:  handle_overflow();\n"
            "}"
        );

        assertCaseBase(caseNode, "input_val", 2 /*count*/, false /*hasElse*/);
        // Validates that dynamic expressions are correctly stored in `casesExp`
        Test::assertOrDie(caseNode->casesExp[0] != NULL);
        Test::assertOrDie(caseNode->casesExp[1] != NULL);
    }},

    TEST_CASE(test_case_enum_member_conditions) {
        PARSE_CASE(
            " state {\n"
            "    when State.Idle:    sleep();\n"
            "    when State.Running: tick();\n"
            "    when State.Error:   abort();\n"
            "}"
        );

        assertCaseBase(caseNode, "state", 3 /*count*/, false /*hasElse*/);
        assertWhenBranch(caseNode, 0, NULL);
        assertWhenBranch(caseNode, 1, NULL);
        assertWhenBranch(caseNode, 2, NULL);
    }},

    TEST_CASE(test_case_mixed_branch_styles) {
        PARSE_CASE(
            " event {\n"
            "    when 1: quick_ack();\n"
            "    when 2 {\n"
            "        process_event();\n"
            "        flush();\n"
            "    }\n"
            "    else: log_unhandled();\n"
            "}"
        );

        assertCaseBase(caseNode, "event", 2 /*count*/, true /*hasElse*/);
        assertWhenBranch(caseNode, 0, "1");
        assertWhenBranch(caseNode, 1, "2");
        Test::assertOrDie(caseNode->elseCase != NULL);
    }}
};



extern const Test::Suite gParserSuiteCase = {
    "Parser - Case-When",
    gCaseParserCases,
    sizeof(gCaseParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
