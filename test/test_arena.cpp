#include <stdlib.h>
#include <string.h>
#include "test_core.h"
#include "../src/dynamic_arena.h"

const Test::Case gArenaCases[] = {

    TEST_CASE(test_arena_push_and_alignment) {
        Arena::Container arena;
        Arena::init(&arena, 128);

        void* ptr1 = Arena::push(&arena, 1, 1);
        void* ptr2 = Arena::push(&arena, 4, 8);
        void* ptr3 = Arena::push(&arena, 10, 16);

        Test::assert(ptr1 != NULL);
        Test::assert(ptr2 != NULL);
        Test::assert(ptr3 != NULL);

        Test::assert((uintptr_t) ptr2 % 8, 0);
        Test::assert((uintptr_t) ptr3 % 16, 0);

        Test::assert(Arena::getFlatSize(&arena), arena.logicalPos);
    }},

    TEST_CASE(test_arena_multi_block_overflow) {
        Arena::Container arena;
        Arena::init(&arena, 64);

        uint8_t* p1 = (uint8_t*) Arena::push(&arena, 40, 8);
        memset(p1, 'A', 40);

        uint8_t* p2 = (uint8_t*) Arena::push(&arena, 40, 8);
        memset(p2, 'B', 40);

        Test::assert(arena.blockCount >= 2);

        uint8_t* lookupInBlock0 = Arena::getPointerToLogicalOffset(&arena, 10);
        uint8_t* lookupInBlock1 = Arena::getPointerToLogicalOffset(&arena, 50);

        Test::assert(lookupInBlock0 != NULL);
        Test::assert(lookupInBlock1 != NULL);

        Test::assert(*lookupInBlock0, 'A');
        Test::assert(*lookupInBlock1, 'B');
    }},

    TEST_CASE(test_arena_flat_copy_accuracy) {
        Arena::Container arena;
        Arena::init(&arena, 32);

        // fill arena with known data patterns across 10 different blocks
        for (int i = 0; i < 10; ++i) {
            uint8_t* blockData = (uint8_t*) Arena::push(&arena, 12, 4);
            memset(blockData, (uint8_t)(i + 1), 12);
        }

        uint64_t totalFlatSize = Arena::getFlatSize(&arena);
        Test::assert(totalFlatSize > 0);

        // allocate a flat destination buffer
        uint8_t* flatBuffer = (uint8_t*) malloc(totalFlatSize);
        memset(flatBuffer, 0x00, totalFlatSize);

        // flatten all arena blocks into destination buffer
        Arena::flatCopy(&arena, flatBuffer);

        // verify that flatBuffer[offset] matches getPointerToLogicalOffset
        for (uint64_t offset = 0; offset < totalFlatSize; ++offset) {
            uint8_t* directPtr = Arena::getPointerToLogicalOffset(&arena, offset);

            if (directPtr != NULL) {
                Test::assert(flatBuffer[offset], *directPtr);
            }
        }

        free(flatBuffer);
    }},

    TEST_CASE(test_arena_rollback_by_marker) {
        Arena::Container arena;
        Arena::init(&arena, 64);

        uint8_t* p1 = (uint8_t*) Arena::push(&arena, 20, 8);
        memset(p1, 0x11, 20);

        Arena::Marker marker1 = Arena::getMarker(&arena);
        uint64_t expectedLogicalPos1 = arena.logicalPos;

        // forces allocation of two additional blocks
        uint8_t* p2 = (uint8_t*) Arena::push(&arena, 50, 8);
        memset(p2, 'A', 50);

        uint8_t* p3 = (uint8_t*) Arena::push(&arena, 50, 8);
        memset(p3, 'B', 50);

        Test::assert(arena.logicalPos > expectedLogicalPos1);

        // Rollback to 20, 8 allocation
        Arena::rollback(&arena, marker1);

        Test::assert(arena.logicalPos, expectedLogicalPos1);
        Test::assert(Arena::getFlatSize(&arena), expectedLogicalPos1);

        // logicalPos - 1 must be valid, but logicalPos must be out of bounds
        Test::assert(Arena::getPointerToLogicalOffset(&arena, expectedLogicalPos1 - 1) != NULL);
        Test::assert(Arena::getPointerToLogicalOffset(&arena, expectedLogicalPos1) == NULL);

        // push new data after rollback
        uint8_t* p4 = (uint8_t*) Arena::push(&arena, 15, 8);
        memset(p4, 'C', 15);

        // We previously rollbacked to 20 with align of 8, so 24
        Test::assert(arena.logicalPos, 24 + 15);
        Test::assert(Arena::getFlatSize(&arena), 24 + 15);

        Arena::release(&arena);
    }},

    TEST_CASE(test_arena_rollback_by_pointer) {
        Arena::Container arena;
        Arena::init(&arena, 48);

        uint8_t* p1 = (uint8_t*) Arena::push(&arena, 16, 8);
        uint64_t logicalPosBeforeP2 = arena.logicalPos;

        uint8_t* p2 = (uint8_t*) Arena::push(&arena, 32, 8);
        uint8_t* p3 = (uint8_t*) Arena::push(&arena, 32, 8);

        Arena::rollback(&arena, p2);

        Test::assert(arena.logicalPos, logicalPosBeforeP2);
        Test::assert(Arena::getFlatSize(&arena), logicalPosBeforeP2);

        Test::assert(Arena::getPointerToLogicalOffset(&arena, logicalPosBeforeP2) == NULL);

        Arena::release(&arena);
    }},

    TEST_CASE(test_arena_rollback_by_size) {
        Arena::Container arena;
        Arena::init(&arena, 64);

        Arena::push(&arena, 24, 8);
        uint64_t posBeforeSecondPush = arena.logicalPos;

        Arena::push(&arena, 25, 8);
        Test::assert(arena.logicalPos, posBeforeSecondPush + 25);

        Arena::rollback(&arena, (uint64_t) 25);

        Test::assert(arena.logicalPos, posBeforeSecondPush);
        Test::assert(Arena::getFlatSize(&arena), posBeforeSecondPush);

        // logicalPos - 1 must be valid, but logicalPos must be out of bounds
        Test::assert(Arena::getPointerToLogicalOffset(&arena, posBeforeSecondPush - 1) != NULL);
        Test::assert(Arena::getPointerToLogicalOffset(&arena, posBeforeSecondPush) == NULL);

        Arena::release(&arena);
    }},

    TEST_CASE(test_arena_rollback_free_memory) {
        Arena::Container arena;
        Arena::init(&arena, 32);

        Arena::Marker mStart = Arena::getMarker(&arena);

        // push data across 4 blocks
        for (int i = 0; i < 4; ++i) {
            Arena::push(&arena, 28, 4);
        }

        uint64_t blockCountBefore = arena.blockCount;
        Test::assert(blockCountBefore == 4);

        Arena::rollback(&arena, mStart, true);

        Test::assert(arena.head, arena.tail);
        Test::assert(arena.blockCount, 1);
        Test::assert(arena.logicalPos, 0);

        Arena::release(&arena);
    }},

    TEST_CASE(test_arena_out_of_bounds_query) {
        Arena::Container arena;
        Arena::init(&arena, 64);

        Arena::push(&arena, 20, 8);

        // querying past logicalPos must return NULL
        uint8_t* oobPtr1 = Arena::getPointerToLogicalOffset(&arena, 20);
        uint8_t* oobPtr2 = Arena::getPointerToLogicalOffset(&arena, 9999);

        Test::assert(oobPtr1 == NULL);
        Test::assert(oobPtr2 == NULL);
    }}

};



extern const Test::Suite gArenaSuite = {
    "Arena Allocator Suite",
    gArenaCases,
    sizeof(gArenaCases) / sizeof(Test::Case)
};
