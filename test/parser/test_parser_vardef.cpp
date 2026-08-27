#include "test_parser.h"

static void assertVardef(
    VariableDefinition* def,
    const char* expectedName,
    Type::Kind expectedBaseType,
    Type::Qualifier expectedQual = Type::Q_NONE,
    int expectedDecoratorCount = 0
) {
    Test::assertOrDie(def != NULL);
    Test::assertOrDie(def->var != NULL);

    Test::assert(cstrcmp((String*) &def->var->name, (String*) expectedName) == 0);

    Test::assert(def->type.baseType == expectedBaseType);
    Test::assert(def->type.qualifier == expectedQual);
    Test::assert(def->type.decoratorCount == expectedDecoratorCount);
}

static void assertCustomType(VariableDefinition* def, const char* expectedTypeName) {
    Test::assertOrDie(def != NULL);
    Test::assert(def->type.baseType == Type::DT_UNDEFINED);
    Test::assertOrDie(def->type.baseName != NULL);
    Test::assert(cstrcmp((String*)def->type.baseName, (String*)expectedTypeName) == 0);
}

template <typename... Kinds>
static void assertDecorators(TypeSpecifier* type, Kinds... expectedKinds) {
    TypeDecorator::Kind kinds[] = { expectedKinds... };
    int count = sizeof...(Kinds);

    Test::assertOrDie(type->decoratorCount == count);
    for (int i = 0; i < count; i++) {
        Test::assert(type->decorators[i]->kind == kinds[i]);
    }
}

#define PARSE_VARDEF(var_name, src_code)                                             \
    initQName(var_name);    \
    initSpanTest(src_code); \
    Lex::TokenValue tokenVal; \
    Lex::Token token = Lex::nextToken(&span, &tokenVal); \
    Parser::parseVarDefinition(&ctx, &span, { token, tokenVal } , &name, { Lex::TK_STATEMENT_END });       \
    VariableDefinition* def = *(VariableDefinition**)DArray::get(&ctx.nodeStack, 0); \
    Test::assertOrDie(def != NULL);



inline Test::Case gVardefParserCases[] = {

    TEST_CASE(test_vardef_simple_initialized) {
        PARSE_VARDEF("count", "i32 = 42;");
        assertVardef(def, "count", Type::DT_I32, Type::Q_NONE, 0);
        Test::assertOrDie(def->type.span);
        assertSpan(*def->type.span, Pos{ 0, 1 }, Pos{ 4, 1 });
    }},

    TEST_CASE(test_vardef_uninitialized) {
        PARSE_VARDEF("total", "u64;");
        assertVardef(def, "total", Type::DT_U64, Type::Q_NONE, 0);
    }},

    TEST_CASE(test_vardef_custom_type) {
        PARSE_VARDEF("player", "Entity = 1;");
        assertVardef(def, "player", Type::DT_UNDEFINED, Type::Q_NONE, 0);
        assertCustomType(def, "Entity");
    }},

    TEST_CASE(test_vardef_embed_qualifier) {
        PARSE_VARDEF("MAX_BUFFER", "embed u64 = 4096;");
        assertVardef(def, "MAX_BUFFER", Type::DT_U64, Type::Q_EMBED, 0);
    }},

    TEST_CASE(test_vardef_single_pointer) {
        PARSE_VARDEF("ptr", "i32^ = 1;");
        assertVardef(def, "ptr", Type::DT_I32, Type::Q_NONE, 1);
        assertDecorators(&def->type, TypeDecorator::DEC_POINTER);
    }},

    TEST_CASE(test_vardef_double_pointer) {
        PARSE_VARDEF("handle", "u8^^;");
        assertVardef(def, "handle", Type::DT_U8, Type::Q_NONE, 2);
        assertDecorators(&def->type, TypeDecorator::DEC_POINTER, TypeDecorator::DEC_POINTER);
    }},

    TEST_CASE(test_vardef_array_inferred) {
        PARSE_VARDEF("arr", "i32[] = 1;");
        assertVardef(def, "arr", Type::DT_I32, Type::Q_NONE, 1);
        assertDecorators(&def->type, TypeDecorator::DEC_ARRAY);
    }},

    TEST_CASE(test_vardef_array_explicit_size) {
        PARSE_VARDEF("buffer", "u8[1024];");
        assertVardef(def, "buffer", Type::DT_U8, Type::Q_NONE, 1);
        assertDecorators(&def->type, TypeDecorator::DEC_ARRAY);
        Test::assert(unwrap(def->type.decorators[0]->len)->value.u64 == 1024);
    }},

    TEST_CASE(test_vardef_slice_const) {
        PARSE_VARDEF("view", "i32[const] = 1;");
        assertVardef(def, "view", Type::DT_I32, Type::Q_NONE, 1);
        assertDecorators(&def->type, TypeDecorator::DEC_ARRAY);
    }},

    TEST_CASE(test_vardef_slice_fluid) {
        PARSE_VARDEF("view", "i32[fluid] = 1;");
        assertVardef(def, "view", Type::DT_I32, Type::Q_NONE, 1);
        assertDecorators(&def->type, TypeDecorator::DEC_ARRAY);
    }},

    TEST_CASE(test_vardef_slice_alloc_vector) {
        PARSE_VARDEF("list", "i32[alloc] = 1;");
        assertVardef(def, "list", Type::DT_I32, Type::Q_NONE, 1);
        assertDecorators(&def->type, TypeDecorator::DEC_ARRAY);
    }},

    TEST_CASE(test_vardef_pointer_to_array) {
        PARSE_VARDEF("grid_ptr", "f32[16]^;");
        assertVardef(def, "grid_ptr", Type::DT_F32, Type::Q_NONE, 2);
        assertDecorators(&def->type, TypeDecorator::DEC_ARRAY, TypeDecorator::DEC_POINTER);
    }},

    TEST_CASE(test_vardef_alloc_assignment) {
        PARSE_VARDEF("dynamic_ptr", "i32^ = 1;");
        assertVardef(def, "dynamic_ptr", Type::DT_I32, Type::Q_NONE, 1);
        assertDecorators(&def->type, TypeDecorator::DEC_POINTER);
    }},
};



extern const Test::Suite gParserSuiteVardef = {
    "Parser - Variable Definitions",
    gVardefParserCases,
    sizeof(gVardefParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
