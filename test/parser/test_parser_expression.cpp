#include "test_parser.h"



static void assertBinaryExpr(
    BinaryExpression* bin,
    OperatorEnum expectedOp
) {
    Test::assertOrDie(bin != NULL);
    Test::assert(bin->base.opType == expectedOp);
    Test::assertOrDie(bin->left != NULL);
    Test::assertOrDie(bin->right != NULL);
}

static void assertUnaryExpr(
    UnaryExpression* un,
    OperatorEnum expectedOp
) {
    Test::assertOrDie(un != NULL);
    Test::assert(un->base.opType == expectedOp);
    Test::assertOrDie(un->operand != NULL);
}

static void assertArrayInit(
    ArrayInitialization* aex,
    uint32_t expectedCount
) {
    Test::assertOrDie(aex != NULL);
    Test::assert(aex->attributeCount == expectedCount);
    if (expectedCount > 0) {
        Test::assertOrDie(aex->attributes != NULL);
    }
}

static void assertTypeInit(
    TypeInitialization* tex,
    uint32_t expectedCount,
    bool hasFillVar = false
) {
    Test::assertOrDie(tex != NULL);
    Test::assert(tex->attributeCount == expectedCount);
    if (expectedCount > 0) {
        Test::assertOrDie(tex->attributes != NULL);
        Test::assertOrDie(tex->idxs != NULL);
    }
    Test::assert((tex->fillVar != NULL) == hasFillVar);
}

static void assertCastExpr(
    Cast* castNode,
    Cast::Kind expectedKind
) {
    Test::assertOrDie(castNode != NULL);
    Test::assertOrDie(castNode->operand != NULL);
    Test::assertOrDie(castNode->target != NULL);
    Test::assert(castNode->kind == expectedKind);
}

static void assertCatchExpr(
    Catch* catchNode,
    bool expectsScope
) {
    Test::assertOrDie(catchNode != NULL);

    Test::assertOrDie(catchNode->call != NULL);

    if (expectsScope) {
        Test::assertOrDie(catchNode->scope != NULL);
        Test::assert(catchNode->err == NULL);
    } else {
        Test::assertOrDie(catchNode->err != NULL);
        Test::assert(catchNode->scope == NULL);
    }
}

#define PARSE_EXPR(src_code, NodeType)                                   \
    initSpanTest(src_code);\
    Variable* var = Ast::Node::makeVariable();    \
    Parser::parseExpression(&ctx, &span, var, INVALID_POS, { Lex::TK_STATEMENT_END }, NULL_FLAG);     \
    NodeType* exprNode = (NodeType*) unwrap(var)->expression;     \
    Test::assertOrDie(exprNode != NULL);



inline Test::Case gExpressionParserCases[] = {

    TEST_CASE(test_expr_binary_arithmetic) {
        PARSE_EXPR("a + b;", BinaryExpression);
        assertBinaryExpr(exprNode, OP_ADDITION);
    }},

    TEST_CASE(test_expr_binary_array_concat) {
        PARSE_EXPR("arr1 .. arr2;", BinaryExpression);
        assertBinaryExpr(exprNode, OP_CONCATENATION);
    }},

    TEST_CASE(test_expr_unary_negation) {
        PARSE_EXPR("!is_ready;", UnaryExpression);
        assertUnaryExpr(exprNode, OP_NEGATION);
    }},

    /*
    TEST_CASE(test_expr_ternary) {
        PARSE_EXPR("is_ok ? 100 : 200;", TernaryExpression);
        Test::assertOrDie(exprNode->condition != NULL);
        Test::assertOrDie(exprNode->trueExp != NULL);
        Test::assertOrDie(exprNode->falseExp != NULL);
    }},
    */

    TEST_CASE(test_expr_value_cast) {
        PARSE_EXPR("pi -> i32;", BinaryExpression);
        assertBinaryExpr(exprNode, OP_CAST_STATIC);
    }},

    TEST_CASE(test_expr_bitcast_reinterpretation) {
        PARSE_EXPR("raw_bytes => ^Packet;", BinaryExpression);
        assertBinaryExpr(exprNode, OP_CAST_BIT);
    }},

    TEST_CASE(test_expr_array_initialization) {
        PARSE_EXPR("[10, 20, 30];", ArrayInitialization);
        assertArrayInit(exprNode, 3 /*attributeCount*/);
    }},

    TEST_CASE(test_expr_type_initialization_struct) {
        PARSE_EXPR("{ x: 1.0, y: 2.0 };", TypeInitialization);
        assertTypeInit(exprNode, 2 /*attributeCount*/, false /*hasFillVar*/);
    }},

    TEST_CASE(test_expr_type_initialization_with_fill) {
        PARSE_EXPR("{ count: 0, ...: 0 };", TypeInitialization);
        assertTypeInit(exprNode, 1 /*attributeCount*/, true /*hasFillVar*/);
    }},

    TEST_CASE(test_expr_string_initialization) {
        const char* str = "\"hello qVi\";";
        PARSE_EXPR(str, StringInitialization);
        Test::assert(exprNode->rawData.len == 9);
    }},

    TEST_CASE(test_expr_catch_with_fallback_value) {
        PARSE_EXPR("read_file(path) catch default_buffer;", Catch);
        assertCatchExpr(exprNode, false /*expectsScope*/);
        Test::assert(cstrcmp((String*)&exprNode->err->name, (String*)"default_buffer") == 0);
    }},

    // TEST_CASE(test_expr_catch_with_block_scope) {
    //     PARSE_EXPR(
    //         "open_socket(port) catch {\n"
    //         "    log_error();\n"
    //         "    return false;\n"
    //         "};",
    //         Catch
    //     );
    //     assertCatchExpr(exprNode, true /*expectsScope*/);
    //     Test::assertOrDie(exprNode->scope != NULL);
    // }}
};



extern const Test::Suite gParserSuiteExpression = {
    "Parser - Expression",
    gExpressionParserCases,
    sizeof(gExpressionParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
