#include "test_parser.h"

static void assertLoopBase(Loop* loop, bool hasBody = true) {
    Test::assertOrDie(loop != NULL);
    if (hasBody) {
        Test::assertOrDie(loop->bodyScope != NULL);
    }
}

static void assertLoopArgKind(Loop* loop, Loop::Arg::Kind expectedKind) {
    assertLoopBase(loop);
    Test::assert(loop->arg.kind == expectedKind);
}

static void assertRangeBounds(RangeExpression* range, bool hasStart, bool hasStep, bool hasEnd) {
    Test::assertOrDie(range != NULL);
    Test::assert((range->bidx != NULL) == hasStart);
    Test::assert((range->step != NULL) == hasStep);
    Test::assert((range->eidx != NULL) == hasEnd);
}

static void assertItemAlias(Loop* loop, const char* expectedName) {
    assertLoopBase(loop);
    Test::assertOrDie(loop->item != NULL);
    Test::assert(cstrcmp((String*)&loop->item->name, (String*)expectedName) == 0);
}

static void assertIndexDef(
    Loop* loop,
    const char* expectedName,
    Type::Kind expectedType = Type::DT_U64
) {
    assertLoopBase(loop);
    Test::assertOrDie(loop->index.def != NULL);
    Test::assertOrDie(loop->index.def->var != NULL);

    Test::assert(cstrcmp((String*)&loop->index.def->var->name, (String*)expectedName) == 0);
    Test::assert(loop->index.def->type.baseType == expectedType);
}

static void assertIndexRef(Loop* loop, const char* expectedName) {
    assertLoopBase(loop);
    Test::assertOrDie(loop->index.var != NULL);
    Test::assert(cstrcmp((String*) &loop->index.var->name, (String*) expectedName) == 0);
}

#define PARSE_LOOP(src_code)                                             \
    initSpanTest(src_code);                                              \
    Parser::parseLoop(&ctx, &span);  \
    Loop* loop = *(Loop**) DArray::get(&ctx.nodeStack, 0);               \
    Test::assertOrDie(loop != NULL);



inline Test::Case gLoopParserCases[] = {

    TEST_CASE(test_loop_infinite_bare) {
        PARSE_LOOP(" { tick(); }");
        assertLoopBase(loop);
        Test::assert(loop->item == NULL);
        Test::assert(loop->index.var == NULL && loop->index.def == NULL);
    }},

    TEST_CASE(test_loop_condition_while) {
        PARSE_LOOP(" is_running { update(); }");
        assertLoopArgKind(loop, Loop::Arg::EXPRESSION);
        Test::assertOrDie(loop->arg.exp != NULL);
        Test::assert(cstrcmp((String*)&loop->arg.exp->name, (String*)"is_running") == 0);
    }},

    TEST_CASE(test_loop_collection_target) {
        PARSE_LOOP(" buffer { process(); }");
        assertLoopArgKind(loop, Loop::Arg::EXPRESSION);
        Test::assertOrDie(loop->arg.exp != NULL);
        Test::assert(cstrcmp((String*)&loop->arg.exp->name, (String*)"buffer") == 0);
    }},

    TEST_CASE(test_loop_range_basic) {
        PARSE_LOOP(" 0:10 { ... }");
        assertLoopArgKind(loop, Loop::Arg::RANGE);
        assertRangeBounds(loop->arg.range, true /*start*/, false /*step*/, true /*end*/);
    }},

    TEST_CASE(test_loop_range_strided) {
        PARSE_LOOP(" 0:2:10 { ... }");
        assertLoopArgKind(loop, Loop::Arg::RANGE);
        assertRangeBounds(loop->arg.range, true /*start*/, true /*step*/, true /*end*/);
    }},

    TEST_CASE(test_loop_range_reverse_step) {
        PARSE_LOOP(" 10:-1:0 { ... }");
        assertLoopArgKind(loop, Loop::Arg::RANGE);
        assertRangeBounds(loop->arg.range, true /*start*/, true /*step*/, true /*end*/);
    }},

    TEST_CASE(test_loop_range_implicit_start) {
        PARSE_LOOP(" :10 { ... }");
        assertLoopArgKind(loop, Loop::Arg::RANGE);
        assertRangeBounds(loop->arg.range, false /*start*/, false /*step*/, true /*end*/);
    }},

    TEST_CASE(test_loop_item_alias_only) {
        PARSE_LOOP(" items as item { print(item); }");
        assertLoopArgKind(loop, Loop::Arg::EXPRESSION);
        assertItemAlias(loop, "item");
        Test::assert(loop->index.var == NULL && loop->index.def == NULL);
    }},

    TEST_CASE(test_loop_index_default_u64) {
        PARSE_LOOP(" 0:10 at i { print(i); }");
        assertLoopArgKind(loop, Loop::Arg::RANGE);
        assertIndexDef(loop, "i", Type::DT_U64);
        Test::assert(loop->item == NULL);
    }},

    TEST_CASE(test_loop_index_explicit_type) {
        PARSE_LOOP(" 0:10 at i: i32 { print(i); }");
        assertLoopArgKind(loop, Loop::Arg::RANGE);
        assertIndexDef(loop, "i", Type::DT_I32);
    }},

    TEST_CASE(test_loop_index_outer_borrow) {
        PARSE_LOOP(" items at &found_idx { break; }");
        assertLoopArgKind(loop, Loop::Arg::EXPRESSION);
        assertIndexRef(loop, "found_idx");
    }},

    TEST_CASE(test_loop_full_combination) {
        PARSE_LOOP(" buffer as byte at idx: u32 { process(byte, idx); }");
        assertLoopArgKind(loop, Loop::Arg::EXPRESSION);
        assertItemAlias(loop, "byte");
        assertIndexDef(loop, "idx", Type::DT_U32);
    }},

    TEST_CASE(test_loop_single_statement_scope) {
        PARSE_LOOP(" 0:10 at i: tick();");
        assertLoopArgKind(loop, Loop::Arg::RANGE);
        assertIndexDef(loop, "i", Type::DT_U64);
        assertLoopBase(loop, true /*hasBody*/);
    }}
};



extern const Test::Suite gParserSuiteLoop = {
    "Parser - Loop",
    gLoopParserCases,
    sizeof(gLoopParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
