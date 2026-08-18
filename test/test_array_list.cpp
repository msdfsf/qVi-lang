#include <string.h>
#include "test_core.h"
#include "../src/array_list.h"

const Test::Case gDArrayCases[] = {

    TEST_CASE(test_darray_init_push_growth) {
        DArray::Container arr;
        DArray::init(&arr, 2, sizeof(int));

        Test::assert(arr.size, 0);
        Test::assert(arr.elementSize, sizeof(int));

        // forces multiple reallocations
        for (int i = 1; i <= 5; ++i) {
            DArray::push(&arr, &i);
        }

        Test::assert(arr.size, 5);

        for (size_t i = 0; i < 5; ++i) {
            int val = *(int*) DArray::get(&arr, i);
            Test::assert(val, (int)(i + 1));
        }

        DArray::release(&arr);
    }},

    TEST_CASE(test_darray_get_set_get_last) {
        DArray::Container arr;
        DArray::init(&arr, 4, sizeof(uint64_t));

        uint64_t v1 = 100, v2 = 200, v3 = 300;
        DArray::push(&arr, &v1);
        DArray::push(&arr, &v2);
        DArray::push(&arr, &v3);

        Test::assert(arr.size, 3);

        Test::assert(*(uint64_t*) DArray::get(&arr, 0), 100);
        Test::assert(*(uint64_t*) DArray::get(&arr, 2), 300);

        Test::assert(*(uint64_t*) DArray::getLast(&arr), 300);

        uint64_t outVal = 0;
        DArray::get(&arr, 1, &outVal);
        Test::assert(outVal, 200);

        uint64_t vNew = 999;
        DArray::set(&arr, 1, &vNew);
        Test::assert(*(uint64_t*) DArray::get(&arr, 1), 999);

        DArray::release(&arr);
    }},

    TEST_CASE(test_darray_push_front) {
        DArray::Container arr;
        DArray::init(&arr, 4, sizeof(int));

        int a = 10, b = 20;
        DArray::push(&arr, &a);
        DArray::push(&arr, &b);

        // [10, 20]
        int frontVal = 5;
        DArray::pushFront(&arr, &frontVal);

        // should be [5, 10, 20]
        Test::assert(arr.size, 3);
        Test::assert(*(int*) DArray::get(&arr, 0), 5);
        Test::assert(*(int*) DArray::get(&arr, 1), 10);
        Test::assert(*(int*) DArray::get(&arr, 2), 20);

        DArray::release(&arr);
    }},

    TEST_CASE(test_darray_pop_and_clear) {
        DArray::Container arr;
        DArray::init(&arr, 4, sizeof(int));

        int v1 = 1, v2 = 2, v3 = 3;
        DArray::push(&arr, &v1);
        DArray::push(&arr, &v2);
        DArray::push(&arr, &v3);

        Test::assert(arr.size, 3);

        DArray::pop(&arr);
        Test::assert(arr.size, 2);
        Test::assert(*(int*) DArray::getLast(&arr), 2);

        DArray::clear(&arr);
        Test::assert(arr.size, 0);

        DArray::release(&arr);
    }},

    TEST_CASE(test_darray_struct_payload) {
        struct Point { int x; int y; };

        DArray::Container arr;
        DArray::init(&arr, 2, sizeof(Point));

        Point p1 = { 10, 20 };
        Point p2 = { 30, 40 };

        DArray::push(&arr, &p1);
        DArray::push(&arr, &p2);

        Test::assert(arr.size, 2);

        Point* readP1 = (Point*) DArray::get(&arr, 0);
        Point* readP2 = (Point*) DArray::getLast(&arr);

        Test::assert(readP1->x, 10);
        Test::assert(readP1->y, 20);
        Test::assert(readP2->x, 30);
        Test::assert(readP2->y, 40);

        DArray::release(&arr);
    }}

};



extern const Test::Suite gDArraySuite = {
    "DArray (Dynamic Array) Suite",
    gDArrayCases,
    sizeof(gDArrayCases) / sizeof(Test::Case)
};
