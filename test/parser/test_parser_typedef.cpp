#include "test_parser.h"



static void assertTypeDefinitionBase(
    TypeDefinition* node,
    const char* expectedName,
    NodeType expectedNodeType,
    uint32_t expectedVarCount
) {
    Test::assertOrDie(node != NULL);

    Test::assert(node->base.type == expectedNodeType);

    if (expectedName != NULL) {
        Test::assert(cstrcmp((String*)&node->name, (String*)expectedName) == 0);
    }

    Test::assert(node->varCount == expectedVarCount);
}

static void assertField(
    TypeDefinition* node,
    uint32_t index,
    const char* expectedFieldName,
    Type::Kind expectedFieldKind = Type::DT_UNDEFINED
) {
    Test::assertOrDie(node != NULL);
    Test::assertOrDie(index < node->varCount);
    Test::assertOrDie(node->vars != NULL);
    Test::assertOrDie(node->vars[index] != NULL);

    Test::assert(cstrcmp((String*)&node->vars[index]->var->name, (String*)expectedFieldName) == 0);

    if (node->vars[index] != NULL && expectedFieldKind != Type::DT_UNDEFINED) {
        Test::assert(node->vars[index]->type.baseType == expectedFieldKind);
    }
}

#define PARSE_TYPEDEF(type, type_name, src_code)                                           \
    initQName(type_name);                                                            \
    initSpanTest(src_code);                                                          \
    Parser::parseTypeDefinition(&ctx, &span, &name, type);      \
    TypeDefinition* typeNode = *(TypeDefinition**)DArray::get(&ctx.nodeStack, 0);    \
    Test::assertOrDie(typeNode != NULL);



inline Test::Case gTypedefParserCases[] = {

    TEST_CASE(test_struct_basic_primitives) {
        PARSE_TYPEDEF(
            Type::DT_STRUCT,
            "Vector2",
            " {\n"
            "    x: f32;\n"
            "    y: f32;\n"
            "}"
        );

        assertTypeDefinitionBase(typeNode, "Vector2", NT_TYPE_DEFINITION, 2 /*varCount*/);
        assertField(typeNode, 0, "x", Type::DT_F32);
        assertField(typeNode, 1, "y", Type::DT_F32);
    }},

    TEST_CASE(test_struct_single_member_newtype) {
        PARSE_TYPEDEF(
            Type::DT_STRUCT,
            "Meters",
            " {\n"
            "    val: f32;\n"
            "}"
        );

        assertTypeDefinitionBase(typeNode, "Meters", NT_TYPE_DEFINITION, 1 /*varCount*/);
        assertField(typeNode, 0, "val", Type::DT_F32);
    }},

    TEST_CASE(test_struct_with_pointer_fields) {
        PARSE_TYPEDEF(
            Type::DT_STRUCT,
            "Node",
            " {\n"
            "    id: u64;\n"
            "    next: Node^;\n"
            "}"
        );

        assertTypeDefinitionBase(typeNode, "Node", NT_TYPE_DEFINITION, 2 /*varCount*/);
        assertField(typeNode, 0, "id", Type::DT_U64);
        assertField(typeNode, 1, "next", Type::DT_UNDEFINED);

        Test::assertOrDie(typeNode->vars[1]!= NULL);
        Test::assert(typeNode->vars[1]->type.decoratorCount == 1);
        Test::assert(typeNode->vars[1]->type.decorators[0]->kind == TypeDecorator::DEC_POINTER);
    }},

    TEST_CASE(test_struct_with_fixed_array_fields) {
        PARSE_TYPEDEF(
            Type::DT_STRUCT,
            "Packet",
            " {\n"
            "    header_id: u32;\n"
            "    payload: u8[1024];\n"
            "}"
        );

        assertTypeDefinitionBase(typeNode, "Packet", NT_TYPE_DEFINITION, 2 /*varCount*/);
        assertField(typeNode, 0, "header_id", Type::DT_U32);
        assertField(typeNode, 1, "payload", Type::DT_U8);

        // Verify fixed array decorator on 'payload'
        Test::assertOrDie(typeNode->vars[1]!= NULL);
        Test::assert(typeNode->vars[1]->type.decoratorCount == 1);
        Test::assert(typeNode->vars[1]->type.decorators[0]->kind == TypeDecorator::DEC_ARRAY);
    }},

    TEST_CASE(test_union_basic_primitives) {
        PARSE_TYPEDEF(
            Type::DT_UNION,
            "Variant",
            " {\n"
            "    as_int: i32;\n"
            "    as_float: f32;\n"
            "}"
        );

        assertTypeDefinitionBase(typeNode, "Variant", NT_UNION, 2 /*varCount*/);
        assertField(typeNode, 0, "as_int", Type::DT_I32);
        assertField(typeNode, 1, "as_float", Type::DT_F32);
    }},

    TEST_CASE(test_union_with_raw_bytes_overlay) {
        PARSE_TYPEDEF(
            Type::DT_UNION,
            "FloatBits",
            " {\n"
            "    value: f32;\n"
            "    raw_bytes: u8[4];\n"
            "}"
        );

        assertTypeDefinitionBase(typeNode, "FloatBits", NT_UNION, 2 /*varCount*/);
        assertField(typeNode, 0, "value", Type::DT_F32);
        assertField(typeNode, 1, "raw_bytes", Type::DT_U8);
    }},

    TEST_CASE(test_struct_empty) {
        PARSE_TYPEDEF(
            Type::DT_STRUCT,
            "EmptyMarker",
            " {\n"
            "}"
        );

        assertTypeDefinitionBase(typeNode, "EmptyMarker", NT_TYPE_DEFINITION, 0 /*varCount*/);
    }}
};



extern const Test::Suite gParserSuiteTypedef = {
    "Parser - Struct, Union",
    gTypedefParserCases,
    sizeof(gTypedefParserCases) / sizeof(Test::Case),
    gParserPreCase,
    gParserPostCase,
    gParserPreSuite,
    gParserPostSuite,
};
