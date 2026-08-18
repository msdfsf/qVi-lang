#include "../src/data_types.h"
#include "../src/globals.h"
#include "test_core.h"



inline Test::Case gTypeSystemCases[] = {

    TEST_CASE(test_primitive_types) {
        Type::init();

        Type::TypeInfo* i32Type = Type::basicTypes + Type::DT_I32;
        Type::TypeInfo* f64Type = Type::basicTypes + Type::DT_F64;

        Test::assert(i32Type->kind == Type::DT_I32);

        Type::release();
    }},

    TEST_CASE(test_make_pointer) {
        Type::init();

        Type::TypeInfo* i32Type = Type::basicTypes + Type::DT_I32;

        Type::TypeInfo* ptr1 = Type::makePointer(i32Type);
        Type::TypeInfo* ptr2 = Type::makePointer(i32Type);

        Test::assert(ptr1, ptr2);

        Test::assert(ptr1->kind, Type::DT_POINTER);
        Test::assert(ptr1->size, 8);
        Test::assert(ptr1->align, 8);

        Type::PointerInfo* pInfo = (Type::PointerInfo*) ptr1;
        Test::assert(pInfo->element, i32Type);
    }},

    TEST_CASE(test_array_sizing) {
        Type::init();

        Type::TypeInfo* f64Type = Type::basicTypes + Type::DT_F64;

        Type::TypeInfo* arr1 = Type::makeArray(f64Type, 20);
        Type::TypeInfo* arr2 = Type::makeArray(f64Type, 20);

        Test::assert(arr1, arr2);

        Test::assert(arr1->kind, Type::DT_ARRAY);
        Test::assert(arr1->size, 160);
        Test::assert(arr1->align, 8);

        Type::ArrayInfo* aInfo = (Type::ArrayInfo*) arr1;
        Test::assert(aInfo->elementCount, 20);
        Test::assert(aInfo->element, f64Type);

        Type::release();
    }},

    TEST_CASE(test_make_array_unknown_length) {
        Type::init();

        Type::TypeInfo* u8Type = Type::basicTypes + Type::DT_U8;

        Type::TypeInfo* arrPlaceholder = Type::makeArray(u8Type, Type::ARRAY_LEN_UNKNOWN);

        Test::assert(arrPlaceholder != NULL);
        Test::assert(arrPlaceholder->kind, Type::DT_ARRAY);

        Type::release();
    }},

    TEST_CASE(test_make_slice) {
        Type::init();

        Type::TypeInfo* u8Type = Type::basicTypes + Type::DT_U8;

        Type::TypeInfo* slice = Type::makeSlice(u8Type, IS_CONST);

        Test::assert(slice != NULL);
        Test::assert(slice->kind, Type::DT_SLICE);
        Test::assert(slice->size, 16);
        Test::assert(slice->align, 8);

        Type::SliceInfo* sInfo = (Type::SliceInfo*) slice;
        Test::assert(sInfo->element, u8Type);
        Test::assert(sInfo->flags, IS_CONST);

        Type::release();
    }},

    TEST_CASE(test_nested_type_composition) {
        Type::init();

        Type::TypeInfo* i32Type = Type::basicTypes + Type::DT_I32;

        // Construct int[10]^
        Type::TypeInfo* arr      = Type::makeArray(i32Type, 10);
        Type::TypeInfo* ptrToArr = Type::makePointer(arr);

        Test::assert(ptrToArr->kind, Type::DT_POINTER);
        Test::assert(ptrToArr->size, 8);

        Type::PointerInfo* pInfo = (Type::PointerInfo*) ptrToArr;
        Test::assert(pInfo->element, arr);
        Test::assert(pInfo->element->size, 40);

        Type::release();
    }}

};



extern const Test::Suite gTypeSystemSuite = {
    "Type System",
    gTypeSystemCases,
    sizeof(gTypeSystemCases) / sizeof(Test::Case)
};
