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

}
