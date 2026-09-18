#include "utils.h"
#include <cstdint>

namespace Utils {

    inline int min(int a, int b) {
        return (a < b) ? a : b;
    }

    inline uint32_t reverse(uint32_t word) {
        uint32_t rev = 0;
        rev |= (word & 0xFF) << 24;
        rev |= ((word >> 8) & 0xFF) << 16;
        rev |= ((word >> 16) & 0xFF) << 8;
        rev |= ((word >> 24) & 0xFF);
        return rev;
    }

    int findLineStart(const char* str, int idx, uint32_t linesBefore) {
        while (idx > 0) {
            if (str[idx] == '\n') {
                if (linesBefore == 0) {
                    idx++;
                    break;
                }
                linesBefore--;
            }
            idx--;
        }

        return idx;
    }

    // foo <x>; boo y;\n
    //
    int findLineEnd(const char* str, int idx, uint32_t linesAfter) {
        while (str[idx] != '\0') {
            if (str[idx] == '\n') {
                if (linesAfter == 0) {
                    idx--;
                    break;
                }
                linesAfter--;
            }
            idx++;
        }

        return idx;
    }

    static inline int64_t sortGetKey(const void* item, uint32_t keyOffset) {
        return *(const int64_t*)((const uint8_t*)item + keyOffset);
    }

    static inline void sortSwap(void* a, void* b, uint32_t stride) {
        if (a == b) return;

        uint8_t* x = (uint8_t*) a;
        uint8_t* y = (uint8_t*) b;

        for (uint32_t i = 0; i < stride; i++) {
            uint8_t tmp = x[i];
            x[i] = y[i];
            y[i] = tmp;
        }
    }

    static void insertionSort(void* data, uint32_t count, uint32_t stride, uint32_t keyOffset) {
        uint8_t* bytes = (uint8_t*) data;

        for (uint32_t i = 1; i < count; i++) {
            uint8_t* current = bytes + i * stride;
            int64_t currentKey = sortGetKey(current, keyOffset);

            uint32_t j = i;

            while (j > 0) {
                uint8_t* previous = bytes + (j - 1) * stride;

                if (sortGetKey(previous, keyOffset) <= currentKey) {
                    break;
                }

                sortSwap(previous, current, stride);
                current = previous;
                j--;
            }
        }
    }

    static void qsortRecursive(uint8_t* data, uint32_t count, uint32_t stride, uint32_t keyOffset) {
        if (count < 2) return;

        uint32_t pivotIdx = count / 2;
        int64_t pivotKey =
            sortGetKey(data + pivotIdx * stride, keyOffset);

        uint32_t left = 0;
        uint32_t right = count - 1;

        while (left <= right) {
            while (sortGetKey(data + left * stride, keyOffset) < pivotKey) {
                left++;
            }

            while (sortGetKey(data + right * stride, keyOffset) > pivotKey) {
                if (right == 0) break;
                right--;
            }

            if (left <= right) {
                sortSwap(data + left * stride, data + right * stride, stride);
                left++;

                if (right > 0) right--;
                else break;
            }
        }

        if (right + 1 > 1) {
            qsortRecursive(data, right + 1, stride, keyOffset);
        }

        if (left < count) {
            qsortRecursive(data + left * stride, count - left, stride, keyOffset);
        }
    }

    static void quickSort(void* data, uint32_t count, uint32_t stride, uint32_t keyOffset) {
        qsortRecursive((uint8_t*) data, count, stride, keyOffset);
    }

    void sort(void* data, uint32_t count, uint32_t stride, uint32_t keyOffset) {
        if (count < 2) return;

        if (count <= 16) {
            insertionSort(data, count, stride, keyOffset);
        } else {
            quickSort(data, count, stride, keyOffset);
        }
    }

}
