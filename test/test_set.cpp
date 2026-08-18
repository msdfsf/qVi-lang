#include "test_core.h"
#include "../src/set.h"
#include <cstdint>

const Test::Case gSetCases[] = {

    TEST_CASE(test_set_basic_insert_find) {
        Set::Container set;
        Set::init(&set, 16);
        set.hashMethod = Set::HM_IDENTITY;
        set.keyStorage = Set::KS_VALUE;
        set.keyOffset  = 0;

        uint64_t data1 = 10;
        uint64_t data2 = 20;

        bool inserted1 = Set::insert(&set, (uint8_t*) data1);
        bool inserted2 = Set::insert(&set, (uint8_t*) data2);

        Test::assert(inserted1);
        Test::assert(inserted2);

        bool insertedDup = Set::insert(&set, (uint8_t*) data1);
        Test::assert(!insertedDup);

        uint8_t* found1 = Set::find(&set, 10);
        uint8_t* found2 = Set::find(&set, 20);
        uint8_t* foundMissing = Set::find(&set, 69);

        Test::assert(found1 != NULL);
        Test::assert(found2 != NULL);
        Test::assert(foundMissing == NULL);

        Set::release(&set);
    }},

    TEST_CASE(test_set_remove_and_probing) {
        Set::Container set;
        Set::init(&set, 16);
        set.hashMethod = Set::HM_IDENTITY;
        set.keyStorage = Set::KS_VALUE;

        uint64_t data1 = { 100 };
        uint64_t data2 = { 200 };
        uint64_t data3 = { 250 };

        Set::insert(&set, (uint8_t*) data1);
        Set::insert(&set, (uint8_t*) data2);
        Set::insert(&set, (uint8_t*) data3);

        bool removed = Set::remove(&set, 200);
        Test::assert(removed);

        Test::assert(Set::find(&set, 200) == NULL);

        Test::assert(!Set::remove(&set, 200));

        Test::assert(Set::find(&set, 100) != nullptr);
        Test::assert(Set::find(&set, 250) != nullptr);

        Set::release(&set);
    }},

    TEST_CASE(test_set_string_interface) {
        Set::Container set;
        Set::init(&set, 16);
        set.hashMethod = Set::HM_STRING_STRUCT_FNV1A;

        String keyFoo = { (char*) "foo", 3 };
        String keyBoo = { (char*) "boo", 3 };
        String keyGoo = { (char*) "goo", 3 };

        Test::assert(Set::insert(&set, (uint8_t*) &keyFoo));
        Test::assert(Set::insert(&set, (uint8_t*) &keyBoo));
        Test::assert(!Set::insert(&set, (uint8_t*) &keyFoo));

        uint8_t* foundFoo = Set::find(&set, keyFoo);
        uint8_t* foundBoo = Set::find(&set, keyBoo);
        uint8_t* foundGoo = Set::find(&set, keyGoo);

        Test::assert(foundFoo != NULL);
        Test::assert(foundBoo != NULL);
        Test::assert(foundGoo == NULL);

        Test::assert(Set::remove(&set, keyBoo));
        Test::assert(Set::find(&set, keyBoo) == NULL);

        Set::release(&set);
    }},

    TEST_CASE(test_set_clear_and_reinsert) {
        Set::Container set;
        Set::init(&set, 16);
        set.hashMethod = Set::HM_IDENTITY;

        uint64_t data1 = 50;
        Set::insert(&set, (uint8_t*) data1);

        Test::assert(Set::find(&set, 50) != NULL);

        Set::clear(&set);

        Test::assert(Set::find(&set, 50) == NULL);

        Test::assert(Set::insert(&set, (uint8_t*) data1));

        Set::release(&set);
    }}

};



extern const Test::Suite gSetSuite = {
    "Set / Hash Table Suite",
    gSetCases,
    sizeof(gSetCases) / sizeof(Test::Case)
};
