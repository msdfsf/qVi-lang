// Unit tests for src/tmp_string_allocator.h
//
// The allocator is a thread local singleton (gTmpStringAllocator), so all
// cases run against shared state. The suite installs pre/post case hooks
// that tsRelease() the allocator, guaranteeing the pristine state (static
// buffer, pos = 0, no stashed heap buffer) before and after every case.

#include <stdint.h>
#include <string.h>
#include <thread>

#include "../test_core.h"
#include "../../src/tmp_string_allocator.h"



// Suite lifecycle: reset the thread local allocator around every case
// ---

static void gTmpStringPreCase(bool)  { tsRelease(); }
static void gTmpStringPostCase(bool) { tsRelease(); }



const Test::Case gTmpStringCases[] = {

    // Note: the pre-case hook already called tsRelease(), so this verifies
    // both the pristine startup state and the full reset contract.
    TEST_CASE(test_tmp_string_initial_state) {
        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, TMP_STRING_STATIC_CAPACITY);
        Test::assert(gTmpStringAllocator.pos, 0);
        Test::assert(gTmpStringAllocator.size, 0);
        Test::assert(gTmpStringAllocator.tmp == NULL);
        Test::assert(gTmpStringAllocator.tmpCapacity, 0);
    }},

    TEST_CASE(test_tmp_string_basic_alloc) {
        char* p1 = tsAlloc(16);

        Test::assert(p1 == gTmpStringBuffer); // first alloc starts at the static buffer
        Test::assert(gTmpStringAllocator.pos, 16);
        Test::assert(gTmpStringAllocator.size, 16);
        Test::assert(gTmpStringAllocator.capacity, TMP_STRING_STATIC_CAPACITY);

        memset(p1, 'a', 16);

        char* p2 = tsAlloc(5); // rounds up to 8
        Test::assert(p2 == gTmpStringBuffer + 16);
        Test::assert(gTmpStringAllocator.pos, 24);
        Test::assert(gTmpStringAllocator.size, 24);

        // previous allocations are not corrupted by new ones
        bool p1Intact = true;
        for (int i = 0; i < 16; i++) {
            if (p1[i] != 'a') { p1Intact = false; break; }
        }
        Test::assert(p1Intact);

        // small allocations never leave the static buffer
        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);
    }},

    TEST_CASE(test_tmp_string_zero_byte_alloc) {
        char* p1 = tsAlloc(32);
        Test::assert(gTmpStringAllocator.pos, 32);

        char* zero = tsAlloc(0);

        // zero-sized allocation is a valid pointer that does not advance position
        Test::assert(zero == gTmpStringBuffer + 32);
        Test::assert(gTmpStringAllocator.pos, 32);
        Test::assert(gTmpStringAllocator.size, 32);

        // next allocation reuses the same position
        char* p2 = tsAlloc(4);
        Test::assert(p2 == zero);
        Test::assert(gTmpStringAllocator.pos, 40);
    }},

    TEST_CASE(test_tmp_string_alignment_rounding) {
        // size is always rounded up to the requested granularity
        Test::assert(tsAlloc(1, 1)  == gTmpStringBuffer);          // pos 0   -> 1
        Test::assert(gTmpStringAllocator.pos, 1);

        Test::assert(tsAlloc(1, 2)  == gTmpStringBuffer + 1);     // pos 1   -> 3
        Test::assert(tsAlloc(1, 4)  == gTmpStringBuffer + 3);      // pos 3   -> 7
        Test::assert(tsAlloc(1, 8)  == gTmpStringBuffer + 7);      // pos 7   -> 15
        Test::assert(tsAlloc(1, 16) == gTmpStringBuffer + 15);     // pos 15  -> 31
        Test::assert(tsAlloc(1, 32) == gTmpStringBuffer + 31);     // pos 31  -> 63
        Test::assert(tsAlloc(1, 64) == gTmpStringBuffer + 63);     // pos 63  -> 127
        Test::assert(gTmpStringAllocator.pos, 127);

        // exact fit of the static buffer must not trigger growth
        Test::assert(tsAlloc(1, 1) == gTmpStringBuffer + 127);     // pos 127 -> 128
        Test::assert(gTmpStringAllocator.pos, 128);
        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, TMP_STRING_STATIC_CAPACITY);
    }},

    TEST_CASE(test_tmp_string_pointer_alignment) {
        // with consistent default alignment every returned pointer is 8-aligned
        char* p1 = tsAlloc(3); // rounds to 8
        char* p2 = tsAlloc(9); // rounds to 16
        char* p3 = tsAlloc(1); // rounds to 8

        Test::assert(p1 == gTmpStringBuffer);
        Test::assert(p2 == gTmpStringBuffer + 8);
        Test::assert(p3 == gTmpStringBuffer + 24);

        Test::assert((uintptr_t) p1 % 8, 0);
        Test::assert((uintptr_t) p2 % 8, 0);
        Test::assert((uintptr_t) p3 % 8, 0);

        Test::assert(gTmpStringAllocator.pos, 32);
    }},

    TEST_CASE(test_tmp_string_growth_static_to_heap) {
        uint8_t* p = (uint8_t*) tsAlloc(120);
        for (size_t i = 0; i < 120; i++) {
            p[i] = (uint8_t) (i * 7 + 3);
        }
        Test::assert(gTmpStringAllocator.pos, 120);
        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);

        // 120 + 16 exceeds the static capacity -> first growth.
        // Policy: capacity = max(capacity * 2, requested), at least 512.
        char* fresh = tsAlloc(16);

        Test::assert(gTmpStringAllocator.buffer != gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, 512);
        Test::assert(gTmpStringAllocator.pos, 136);
        Test::assert(gTmpStringAllocator.size, 136);

        // contents of the static buffer were copied over
        bool copiedIntact = true;
        for (size_t i = 0; i < 120; i++) {
            if ((uint8_t) gTmpStringAllocator.buffer[i] != (uint8_t) (i * 7 + 3)) {
                copiedIntact = false;
                break;
            }
        }
        Test::assert(copiedIntact);

        // the newly allocated region is usable heap memory
        Test::assert(fresh == gTmpStringAllocator.buffer + 120);
        memset(fresh, 0xAB, 16);
        bool freshIntact = true;
        for (int i = 0; i < 16; i++) {
            if ((uint8_t) fresh[i] != 0xAB) { freshIntact = false; break; }
        }
        Test::assert(freshIntact);
    }},

    TEST_CASE(test_tmp_string_heap_realloc_growth) {
        // grow #1: static -> heap (512)
        tsAlloc(120);                              // pos 120 (static)
        tsAlloc(16);                               // forces growth, pos 136
        Test::assert(gTmpStringAllocator.capacity, 512);

        for (size_t i = 0; i < 136; i++) {
            gTmpStringAllocator.buffer[i] = (char) (uint8_t) (i * 31 + 17);
        }

        // fill the heap buffer exactly to capacity, no growth yet
        char* tail = tsAlloc(376);                 // 136 + 376 == 512
        Test::assert(gTmpStringAllocator.pos, 512);
        Test::assert(gTmpStringAllocator.capacity, 512); // exact fit does not grow
        Test::assert(tail == gTmpStringAllocator.buffer + 136);

        for (size_t i = 136; i < 512; i++) {
            gTmpStringAllocator.buffer[i] = (char) (uint8_t) (i * 31 + 17);
        }

        // grow #2: heap -> bigger heap via realloc (512 * 2 == 1024)
        tsAlloc(8);                                // 520 > 512
        Test::assert(gTmpStringAllocator.buffer != gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, 1024);
        Test::assert(gTmpStringAllocator.pos, 520);

        // realloc preserved every byte written so far
        bool reallocIntact = true;
        for (size_t i = 0; i < 512; i++) {
            if ((uint8_t) gTmpStringAllocator.buffer[i] != (uint8_t) (i * 31 + 17)) {
                reallocIntact = false;
                break;
            }
        }
        Test::assert(reallocIntact);
    }},

    TEST_CASE(test_tmp_string_large_single_request) {
        char* big = tsAlloc(5000);

        // the requested capacity wins over the doubling policy
        Test::assert(gTmpStringAllocator.buffer != gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, 5000);
        Test::assert(gTmpStringAllocator.pos, 5000);
        Test::assert(big == gTmpStringAllocator.buffer);

        memset(big, 0x11, 5000);
        bool bigIntact = true;
        for (size_t i = 0; i < 5000; i += 97) {
            if ((uint8_t) big[i] != 0x11) { bigIntact = false; break; }
        }
        Test::assert(bigIntact);
    }},

    TEST_CASE(test_tmp_string_string_usage_pattern) {
        // mirrors the intended usage: temporary strings alive within one scope
        const char* words[] = { "Hello", "temporary", "world!" };
        char* buffers[3];

        size_t expectedPos = 0;
        for (int i = 0; i < 3; i++) {
            size_t len = strlen(words[i]) + 1;
            buffers[i] = tsAlloc(len);
            strcpy(buffers[i], words[i]);
            expectedPos += (len + 7) & ~(size_t) 7;
        }

        Test::assert(gTmpStringAllocator.pos, expectedPos);
        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);

        // all strings stay readable and untouched while in scope
        for (int i = 0; i < 3; i++) {
            Test::assert(strcmp(buffers[i], words[i]), 0);
        }
    }},

    TEST_CASE(test_tmp_string_mark_rollback_static) {
        char* p1 = tsAlloc(16);
        Test::assert(gTmpStringAllocator.pos, 16);

        TmpStringMarker marker = tsMark();
        Test::assert(marker.pos, 16);
        Test::assert(marker.isStatic);

        char* p2 = tsAlloc(40);
        memset(p2, 'x', 40);
        Test::assert(gTmpStringAllocator.pos, 56);

        tsRollback(marker);

        Test::assert(gTmpStringAllocator.pos, 16);
        Test::assert(gTmpStringAllocator.size, 16);
        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);

        // rolled back region is handed out again
        char* p3 = tsAlloc(8);
        Test::assert(p3 == p1 + 16);
    }},

    TEST_CASE(test_tmp_string_nested_markers) {
        tsAlloc(8);

        TmpStringMarker outer = tsMark();
        Test::assert(outer.pos, 8);

        tsAlloc(16);

        TmpStringMarker inner = tsMark();
        Test::assert(inner.pos, 24);

        tsAlloc(64);
        Test::assert(gTmpStringAllocator.pos, 88);

        tsRollback(inner);
        Test::assert(gTmpStringAllocator.pos, 24);

        tsRollback(outer);
        Test::assert(gTmpStringAllocator.pos, 8);

        char* p = tsAlloc(4); // rounds to 8
        Test::assert(p == gTmpStringBuffer + 8);
        Test::assert(gTmpStringAllocator.pos, 16);
    }},

    TEST_CASE(test_tmp_string_rollback_across_growth) {
        tsAlloc(16);

        TmpStringMarker marker = tsMark(); // marked on the static buffer
        Test::assert(marker.isStatic);

        tsAlloc(200); // forces growth to the heap
        Test::assert(gTmpStringAllocator.buffer != gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, 512);

        tsRollback(marker);

        // the heap buffer is dropped, allocator falls back to the static buffer
        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, TMP_STRING_STATIC_CAPACITY);
        Test::assert(gTmpStringAllocator.pos, 16);
        Test::assert(gTmpStringAllocator.size, 16);

        // allocator remains fully usable
        char* p = tsAlloc(100); // rounds to 104
        Test::assert(p == gTmpStringBuffer + 16);
        Test::assert(gTmpStringAllocator.pos, 120);
    }},

    TEST_CASE(test_tmp_string_rollback_on_heap_keeps_buffer) {
        tsAlloc(200); // pos 200, heap (512)
        Test::assert(gTmpStringAllocator.buffer != gTmpStringBuffer);

        TmpStringMarker marker = tsMark();
        Test::assert(marker.pos, 200);
        Test::assert(!marker.isStatic);

        tsAlloc(400); // 600 > 512 -> realloc to 1024
        Test::assert(gTmpStringAllocator.capacity, 1024);
        Test::assert(gTmpStringAllocator.pos, 600);

        tsRollback(marker);

        // heap buffer survives the rollback (only static markers free the heap)
        Test::assert(gTmpStringAllocator.buffer != gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, 1024);
        Test::assert(gTmpStringAllocator.pos, 200);
        Test::assert(gTmpStringAllocator.size, 200);

        // allocation continues on the kept heap buffer
        char* p = tsAlloc(64);
        Test::assert(p == gTmpStringAllocator.buffer + 200);
    }},

    TEST_CASE(test_tmp_string_clear_stash_and_reuse) {
        tsAlloc(200); // heap, capacity 512
        char* heapBuffer = gTmpStringAllocator.buffer;
        size_t heapCapacity = gTmpStringAllocator.capacity;

        tsClear();

        // reset back to static, heap buffer stashed for later release
        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, TMP_STRING_STATIC_CAPACITY);
        Test::assert(gTmpStringAllocator.pos, 0);
        Test::assert(gTmpStringAllocator.size, 0);
        Test::assert(gTmpStringAllocator.tmp == heapBuffer);
        Test::assert(gTmpStringAllocator.tmpCapacity, heapCapacity);

        // allocator is immediately usable on the static buffer
        char* p = tsAlloc(50); // rounds to 56
        Test::assert(p == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.pos, 56);

        // second tsClear releases the previously stashed heap buffer
        // (current buffer is static, so the static one gets stashed instead)
        tsClear();
        Test::assert(gTmpStringAllocator.tmp == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.tmpCapacity, TMP_STRING_STATIC_CAPACITY);
        Test::assert(gTmpStringAllocator.pos, 0);
    }},

    TEST_CASE(test_tmp_string_release_resets_everything) {
        tsAlloc(300); // heap, capacity 512
        tsClear();   // stash the heap buffer
        Test::assert(gTmpStringAllocator.tmp != NULL);

        tsAlloc(64); // static again, pos 64

        tsRelease(); // frees the stash, full reset

        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, TMP_STRING_STATIC_CAPACITY);
        Test::assert(gTmpStringAllocator.pos, 0);
        Test::assert(gTmpStringAllocator.size, 0);
        Test::assert(gTmpStringAllocator.tmp == NULL);
        Test::assert(gTmpStringAllocator.tmpCapacity, 0);

        // allocator stays usable after release
        char* p = tsAlloc(16);
        Test::assert(p == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.pos, 16);
    }},

    TEST_CASE(test_tmp_string_init_behaves_like_clear) {
        tsAlloc(200); // heap, capacity 512
        char* heapBuffer = gTmpStringAllocator.buffer;

        tsInit();

        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.pos, 0);
        Test::assert(gTmpStringAllocator.size, 0);
        Test::assert(gTmpStringAllocator.tmp == heapBuffer);
        Test::assert(gTmpStringAllocator.tmpCapacity, 512);
    }},

    TEST_CASE(test_tmp_string_thread_local_isolation) {
        char* mainPtr = tsAlloc(64);
        memset(mainPtr, 0x5A, 64);
        size_t mainPos = gTmpStringAllocator.pos;
        Test::assert(mainPos, 64);

        bool workerChecksPassed = false;

        std::thread worker([&]() {
            bool ok = true;

            // a fresh thread starts with its own pristine allocator
            ok = ok && gTmpStringAllocator.buffer == gTmpStringBuffer;
            ok = ok && gTmpStringAllocator.pos == 0;
            ok = ok && gTmpStringAllocator.capacity == TMP_STRING_STATIC_CAPACITY;

            // ... and allocates into its own thread local static buffer
            char* workerPtr = tsAlloc(32);
            ok = ok && workerPtr == gTmpStringBuffer;
            ok = ok && gTmpStringAllocator.pos == 32;

            tsRelease();
            workerChecksPassed = ok;
        });
        worker.join();

        Test::assert(workerChecksPassed);

        // the main thread's state was completely unaffected
        Test::assert(gTmpStringAllocator.pos, mainPos);
        Test::assert(gTmpStringAllocator.buffer == gTmpStringBuffer);
        Test::assert(gTmpStringAllocator.capacity, TMP_STRING_STATIC_CAPACITY);

        bool mainDataIntact = true;
        for (int i = 0; i < 64; i++) {
            if ((uint8_t) mainPtr[i] != 0x5A) { mainDataIntact = false; break; }
        }
        Test::assert(mainDataIntact);
    }},

};



extern const Test::Suite gTmpStringSuite = {
    "Tmp String Allocator Suite",
    NULL,
    gTmpStringCases,
    sizeof(gTmpStringCases) / sizeof(Test::Case),
    gTmpStringPreCase,
    gTmpStringPostCase,
};
