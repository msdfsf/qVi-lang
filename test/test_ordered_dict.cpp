#include "test_core.h"
#include "../src/ordered_dict.h"



const Test::Case gOrderedDictCases[] = {

    TEST_CASE(test_ordered_dict_uint_keys) {
        OrderedDict::Container dict;
        OrderedDict::init(&dict, 4);

        int val1 = 100, val2 = 200;
        OrderedDict::set(&dict, (uint64_t) 10, &val1);
        OrderedDict::set(&dict, (uint64_t) 20, &val2);

        Test::assert(*(int*) OrderedDict::get(&dict, (uint64_t) 10), 100);
        Test::assert(*(int*) OrderedDict::get(&dict, (uint64_t) 20), 200);

        Test::assert(OrderedDict::get(&dict, (uint64_t) 999) == NULL);

        int valUpdated = 999;
        Test::assert(OrderedDict::set(&dict, (uint64_t) 10, &valUpdated), 0);
        Test::assert(*(int*) OrderedDict::get(&dict, (uint64_t) 10), 100);
    }},

    TEST_CASE(test_ordered_dict_string_keys) {
        OrderedDict::Container dict;
        OrderedDict::init(&dict, 4);

        String keyAlpha   = { (char*) "alpha", 5 };
        String keyBeta    = { (char*) "beta",  4 };
        String keyMissing = { (char*) "missing", 7 };

        int v1 = 111, v2 = 222;
        OrderedDict::set(&dict, keyAlpha, &v1);
        OrderedDict::set(&dict, keyBeta,  &v2);

        Test::assert(*(int*) OrderedDict::get(&dict, keyAlpha), 111);
        Test::assert(*(int*) OrderedDict::get(&dict, keyBeta),  222);
        Test::assert(OrderedDict::get(&dict, keyMissing) == NULL);
    }},

    TEST_CASE(test_ordered_dict_iterator) {
        OrderedDict::Container dict;
        OrderedDict::init(&dict, 4);

        int v1 = 1, v2 = 2, v3 = 3;
        OrderedDict::set(&dict, (uint64_t) 10, &v1);
        OrderedDict::set(&dict, (uint64_t) 30, &v2);
        OrderedDict::set(&dict, (uint64_t) 20, &v3);

        OrderedDict::resetIterator(&dict);

        OrderedDict::Pair* p1 = OrderedDict::getNext(&dict);
        Test::assert(p1 != NULL);
        Test::assert(p1->key.idx, 10);

        OrderedDict::Pair* p2 = OrderedDict::getNext(&dict);
        Test::assert(p2 != NULL);
        Test::assert(p2->key.idx, 20);

        OrderedDict::Pair* p3 = OrderedDict::getNext(&dict);
        Test::assert(p3 != NULL);
        Test::assert(p3->key.idx, 30);

        OrderedDict::Pair* p4 = OrderedDict::getNext(&dict);
        Test::assert(p4 != NULL);
        Test::assert(p4->key.idx, 10);
    }},

    TEST_CASE(test_ordered_dict_copy_and_clear) {
        OrderedDict::Container dict;
        OrderedDict::init(&dict, 4);

        int v1 = 55;
        OrderedDict::set(&dict, (uint64_t) 100, &v1);

        // Perform tightCopy
        OrderedDict::Container* copyDict = OrderedDict::tightCopy(&dict);
        Test::assert(copyDict != NULL);
        Test::assert(*(int*) OrderedDict::get(copyDict, (uint64_t) 100), 55);

        // Clear original dict
        OrderedDict::clear(&dict);
        Test::assert(OrderedDict::get(&dict, (uint64_t) 100) == NULL);

        // Copy should remain valid and independent
        Test::assert(*(int*) OrderedDict::get(copyDict, (uint64_t) 100), 55);
    }}

};



extern const Test::Suite gOrderedDictSuite = {
    "OrderedDict Suite",
    gOrderedDictCases,
    sizeof(gOrderedDictCases) / sizeof(Test::Case)
};
