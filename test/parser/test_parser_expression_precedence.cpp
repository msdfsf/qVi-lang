#include "test_parser.h"



static BinaryExpression* asBinary(Variable* var) {
    Test::assertOrDie(var != NULL);
    Test::assertOrDie(var->expression != NULL);
    Test::assert(var->expression->type == EXT_BINARY);
    return (BinaryExpression*)var->expression;
}

static UnaryExpression* asUnary(Variable* var) {
    Test::assertOrDie(var != NULL);
    Test::assertOrDie(var->expression != NULL);
    Test::assert(var->expression->type == EXT_UNARY);
    return (UnaryExpression*)var->expression;
}

#define PARSE_EXPR(src_code, NodeType)                                   \
    initSpanTest(src_code);\
    Variable* var = Ast::Node::makeVariable(); \
    Parser::parseExpression(&ctx, &span, var, INVALID_POS, { Lex::TK_STATEMENT_END }, NULL_FLAG);     \
    NodeType* exprNode = (NodeType*) unwrap(var)->expression;     \
    Test::assertOrDie(exprNode != NULL);



inline Test::Case gPrecedenceParserCases[] = {

    TEST_CASE(test_associativity_subtraction_left_to_right) {
        // `a - b - c` MUST be `(a - b) - c`, NEVER `a - (b - c)`
        PARSE_EXPR("a - b - c;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_SUBTRACTION);

        BinaryExpression* leftSub = asBinary(exprNode->left);
        Test::assert(leftSub->base.opType == OP_SUBTRACTION); // `a - b` on the left
        Test::assert(exprNode->right->expression == NULL); // `c` on the right
    }},

    TEST_CASE(test_associativity_division_left_to_right) {
        // `a / b / c` MUST be `(a / b) / c`
        PARSE_EXPR("a / b / c;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_DIVISION);

        BinaryExpression* leftDiv = asBinary(exprNode->left);
        Test::assert(leftDiv->base.opType == OP_DIVISION); // `a / b`
        Test::assert(exprNode->right->expression == NULL); // `c`
    }},

    TEST_CASE(test_precedence_modulo_and_multiplication) {
        // `a % b * c` -> `(a % b) * c` (Same precedence, left-associative)
        PARSE_EXPR("a % b * c;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_MULTIPLICATION);

        BinaryExpression* leftMod = asBinary(exprNode->left);
        Test::assert(leftMod->base.opType == OP_MODULO);
    }},

    TEST_CASE(test_precedence_bitwise_and_over_xor_over_or) {
        // `a | b ^ c & d` -> `a | (b ^ (c & d))`
        PARSE_EXPR("a | b ^ c & d;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_BITWISE_OR); // `|` is lowest

        BinaryExpression* rightXor = asBinary(exprNode->right);
        Test::assert(rightXor->base.opType == OP_BITWISE_XOR); // `^` is middle

        BinaryExpression* rightAnd = asBinary(rightXor->right);
        Test::assert(rightAnd->base.opType == OP_BITWISE_AND); // `&` is highest
    }},

    TEST_CASE(test_precedence_shift_over_bitwise_and) {
        // `a & b << 2` -> `a & (b << 2)`
        PARSE_EXPR("a & b << 2;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_BITWISE_AND);

        BinaryExpression* rightShift = asBinary(exprNode->right);
        Test::assert(rightShift->base.opType == OP_SHIFT_LEFT);
    }},

    TEST_CASE(test_precedence_addition_over_shift) {
        // `a << 2 + b` -> `a << (2 + b)` (Arithmetic evaluated before shift)
        PARSE_EXPR("a << 2 + b;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_SHIFT_LEFT);

        BinaryExpression* rightAdd = asBinary(exprNode->right);
        Test::assert(rightAdd->base.opType == OP_ADDITION);
    }},

    TEST_CASE(test_precedence_address_of_vs_member_access) {
        // `&packet.header` -> `&(packet.header)` (Member access binds tighter than `&`)
        PARSE_EXPR("&packet.header;", UnaryExpression);

        Test::assert(exprNode->base.opType == OP_GET_ADDRESS);
        Test::assertOrDie(exprNode->operand != NULL);
        Test::assert(cstrcmp((String*)&exprNode->operand->name, (String*)"header") == 0);
    }},

    TEST_CASE(test_precedence_pointer_deref_in_expression) {
        // `ptr^ + 10` -> `(ptr^) + 10`
        PARSE_EXPR("^ptr + 10;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_ADDITION);

        UnaryExpression* leftDeref = asUnary(exprNode->left);
        Test::assert(leftDeref->base.opType == OP_GET_VALUE);
    }},

    TEST_CASE(test_precedence_chained_unary_not_and_bitwise_not) {
        // `!~flags` -> `!(~flags)`
        PARSE_EXPR("!~flags;", UnaryExpression);

        Test::assert(exprNode->base.opType == OP_NEGATION);

        UnaryExpression* innerBitNot = asUnary(exprNode->operand);
        Test::assert(innerBitNot->base.opType == OP_BITWISE_NEGATION);
    }},
    TEST_CASE(test_precedence_cast_over_addition) {
        // `x + y -> f32` -> `x + (y -> f32)` (Cast binds tighter than binary +)
        PARSE_EXPR("x + y -> f32;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_ADDITION);
        Test::assert(exprNode->left->expression == NULL); // `x`

        BinaryExpression* rightCast = asBinary(exprNode->right);
        Test::assert(rightCast->base.opType == OP_CAST_STATIC); // `->` binary op
        Test::assert(rightCast->left->expression == NULL);       // `y`
    }},

    TEST_CASE(test_precedence_parens_override_cast) {
        // `(x + y) -> f32` -> Root is BinaryExpression (OP_CAST_STATIC)
        PARSE_EXPR("(x + y) -> f32;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_CAST_STATIC);

        BinaryExpression* innerAdd = asBinary(unwrap(exprNode->left));
        Test::assert(innerAdd->base.opType == OP_ADDITION);
    }},

    TEST_CASE(test_precedence_bitcast_over_subtraction) {
        // `ptr - offset => usize` -> `ptr - (offset => usize)`
        PARSE_EXPR("ptr - offset => u64;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_SUBTRACTION);

        BinaryExpression* rightBitcast = asBinary(exprNode->right);
        Test::assert(rightBitcast->base.opType == OP_CAST_BIT); // `=>` binary op
    }},

    TEST_CASE(test_precedence_parens_override_bitcast) {
        // `(ptr - offset) => usize` -> Root is BinaryExpression (OP_CAST_BIT)
        PARSE_EXPR("(ptr - offset) => u64;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_CAST_BIT);

        BinaryExpression* innerSub = asBinary(unwrap(exprNode->left));
        Test::assert(innerSub->base.opType == OP_SUBTRACTION);
    }},

    TEST_CASE(test_precedence_equality_vs_bitwise_and) {
        // `a & mask == expected` -> `(a & mask) == expected`
        PARSE_EXPR("a & mask == expected;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_BITWISE_AND);

        BinaryExpression* leftBitAnd = asBinary(unwrap(exprNode->right));
        Test::assert(leftBitAnd->base.opType == OP_EQUAL);
    }},

    TEST_CASE(test_precedence_complex_boolean_ladder) {
        // `!a && b < 10 || c == 0 && d != 5`
        // Expected: `((!a) && (b < 10)) || ((c == 0) && (d != 5))`
        PARSE_EXPR("!a && b < 10 || c == 0 && d != 5;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_BOOL_OR);

        // Left side of || -> `!a && b < 10`
        BinaryExpression* leftAnd = asBinary(exprNode->left);
        Test::assert(leftAnd->base.opType == OP_BOOL_AND);

        UnaryExpression* notA = asUnary(leftAnd->left);
        Test::assert(notA->base.opType == OP_NEGATION);

        BinaryExpression* cmpB = asBinary(leftAnd->right);
        Test::assert(cmpB->base.opType == OP_LESS_THAN);

        // Right side of || -> `c == 0 && d != 5`
        BinaryExpression* rightAnd = asBinary(exprNode->right);
        Test::assert(rightAnd->base.opType == OP_BOOL_AND);

        BinaryExpression* cmpC = asBinary(rightAnd->left);
        Test::assert(cmpC->base.opType == OP_EQUAL);

        BinaryExpression* cmpD = asBinary(rightAnd->right);
        Test::assert(cmpD->base.opType == OP_NOT_EQUAL);
    }},

    TEST_CASE(test_precedence_ultimate_systems_expression) {
        // `base_addr + index * 4 -> u64 < max_limit && !is_locked`
        // ((base_addr + (index * (4 -> u64))) < max_limit) && (!is_locked))
        PARSE_EXPR("base_addr + index * 4 -> u64 < max_limit && !is_locked;", BinaryExpression);

        Test::assert(exprNode->base.opType == OP_BOOL_AND);

        UnaryExpression* rightNot = asUnary(exprNode->right);
        Test::assert(rightNot->base.opType == OP_NEGATION);

        BinaryExpression* leftCmp = asBinary(exprNode->left);
        Test::assert(leftCmp->base.opType == OP_LESS_THAN);

        BinaryExpression* leftAdd = asBinary(leftCmp->left);
        Test::assert(leftAdd->base.opType == OP_ADDITION);

        BinaryExpression* castBin = asBinary(leftAdd->right);
        Test::assert(castBin->base.opType == OP_MULTIPLICATION);

        BinaryExpression* innerMul = asBinary(castBin->right);
        Test::assert(innerMul->base.opType == OP_CAST_STATIC);
    }}
};



extern const Test::Suite gParserSuitePrecedence = {
    "Parser - Expression: Precedence",
    gPrecedenceParserCases,
    sizeof(gPrecedenceParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
