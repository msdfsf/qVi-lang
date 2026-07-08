#pragma once
#include <bit>
#include <cstdint>

namespace Utils {

    inline uint32_t reverse(uint32_t word);

    int findLineStart(const char* str, int idx, uint32_t linesBefore = 0);
    int findLineEnd(const char* str, int idx, uint32_t linesAfter = 0);

    inline uint64_t getPow2Ceil(size_t val) {
        return std::bit_ceil(val);
    }

    // Returns the next address >= ptr that is divisible by align
    inline uintptr_t alignForward(uintptr_t ptr, size_t align) {
        return (ptr + (align - 1)) & ~(align - 1);
    }

    // Returns the number of bytes needed to pad 'ptr' to 'align'
    inline size_t getPadding(uintptr_t ptr, size_t align) {
        return alignForward(ptr, align) - ptr;
    }

    inline int countDigits(uint64_t num) {
        if (num == 0) return 1;

        // pray to unroll (but even so, /= 10 may not endup as cmptime)
        int count = 0;
        while (num != 0) {
            num /= 10;
            count++;
        }

        return count;

    }

}
