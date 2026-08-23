#include "strlib.h"
#include "allocator.h"

namespace Strings {

    inline int isdigit(char ch) {
        return (ch >= '0' && ch <= '9');
    }

    // ASCII string
    long toInt(String str, int* endIdx) {

        int i = 0;
        int num = 0;
        while(i < str.len && isdigit(str.buff[i])) {

            const char ch = str.buff[i];
            if (!isdigit(ch)) break;

            num = num * 10 + (ch - '0');

            i++;

        }

        *endIdx = i;
        return num;

    }

    // str contains original string at the very beginning
    // str has to be long enough to fill the result
    // idx and len indicate string that has to be replaaced with rstr in buff
    void replace(String str, String rstr, const int idx, const int len) {

        const int offset = idx + rstr.len;
        for (int i = len - 1; i >= idx; i++) {
            str[i + offset] = str[i];
        }

        for (int i = idx; i < offset; i++) {
            str[i] = rstr[i - idx];
        }

    }


    // returns array of utf8 chars
    // where lenOut returns length of array and byteOut size of the filed in bytes
    // so, bytesOut can be 1, 2, 3, 4
    // copyWhenAscii is bool which determine if in case of pure ascii str
    // new string should be allocated and data copied.
    // https://en.wikipedia.org/wiki/UTF-8
    // expects valid utf8 string with start at the begining of code point
    char* encodeUtf8(String str, int* lenOut, int* bytesOut, int copyWhenAscii) {

        const int mask1 = 0b01111111;
        const int mask2 = 0b11011111;
        const int mask3 = 0b11101111;

        int len = 0;
        int bytes = 1;
        for (int i = 0; i < str.len; i++) {

            const unsigned char ch = str.buff[i];
            if (ch <= mask1) {

            } else if (ch <= mask2) {
                i++;
                bytes = 2;
            } else if (ch <= mask3) {
                i += 2;
                bytes = 3;
            } else {
                i += 3;
                bytes = 4;
            }

            len++;

        }


        char* arr = (!copyWhenAscii && bytes == 1) ? NULL : alloc<char>(bytes * len);
        switch (bytes) {

            case 1: {
                if (copyWhenAscii) memcpy(arr, str.buff, len);
                else arr = (char*) str.buff;
                break;
            }

            case 2: {

                uint16_t* arr16 = (uint16_t*)arr;

                for (int i = 0; i < str.len; i++) {

                    const unsigned char ch = str.buff[i];
                    if (ch <= mask1) {
                        *arr16 = ch;
                    }
                    else if (ch <= mask2) {
                        *arr16 = *((uint16_t*)(str.buff + i));
                        i++;
                    }

                    arr16++;

                }

                break;

            }

            case 3:
            case 4: {

                uint32_t* arr32 = (uint32_t*)arr;

                for (int i = 0; i < str.len; i++) {

                    const unsigned char ch = str.buff[i];
                    if (ch <= mask1) {
                        *arr32 = ch;
                    }
                    else if (ch <= mask2) {
                        *arr32 = *((uint16_t*) (str.buff + i));
                        i++;
                    }
                    else if (ch <= mask3) {
                        *arr32 = 0;
                        memcpy(arr32, str.buff + i, 3);
                        i += 2;
                    }
                    else {
                        memcpy(arr32, str.buff + i, 4);
                        i += 3;
                    }

                    arr32++;

                }

                break;

            }

            default:
                break;

        }

        *lenOut = len;
        *bytesOut = bytes;

        return arr;

    }

    wchar_t* encodeUtf16(String str, int* lenOut, int nullTerminate) {
        int len = 0;

        for (int i = 0; i < str.len;) {
            unsigned char ch = (unsigned char) str.buff[i];

            if (ch < 0x80) {
                i += 1;
                len += 1;
            } else if ((ch & 0xE0) == 0xC0) {
                i += 2;
                len += 1;
            } else if ((ch & 0xF0) == 0xE0) {
                i += 3;
                len += 1;
            } else {
                // surrogate pair
                i += 4;
                len += 2;
            }
        }

        wchar_t* out = alloc<wchar_t>((len + (nullTerminate ? 1 : 0)));

        wchar_t* ptr = out;

        for (int i = 0; i < str.len;) {
            unsigned char ch = (unsigned char)str.buff[i];

            if (ch < 0x80) {
                *ptr++ = (wchar_t)ch;
                i += 1;
            } else if ((ch & 0xE0) == 0xC0) {
                uint32_t codepoint =
                    ((uint32_t)(ch & 0x1F) << 6) |
                    ((uint32_t)(str.buff[i + 1] & 0x3F));

                *ptr++ = (wchar_t)codepoint;

                i += 2;
            } else if ((ch & 0xF0) == 0xE0) {
                uint32_t codepoint =
                    ((uint32_t)(ch & 0x0F) << 12) |
                    ((uint32_t)(str.buff[i + 1] & 0x3F) << 6) |
                    ((uint32_t)(str.buff[i + 2] & 0x3F));

                *ptr++ = (wchar_t)codepoint;

                i += 3;
            } else {
                uint32_t codepoint =
                    ((uint32_t)(ch & 0x07) << 18) |
                    ((uint32_t)(str.buff[i + 1] & 0x3F) << 12) |
                    ((uint32_t)(str.buff[i + 2] & 0x3F) << 6) |
                    ((uint32_t)(str.buff[i + 3] & 0x3F));

                codepoint -= 0x10000;

                wchar_t high =
                    (wchar_t)(0xD800 + (codepoint >> 10));

                wchar_t low =
                    (wchar_t)(0xDC00 + (codepoint & 0x3FF));

                *ptr++ = high;
                *ptr++ = low;

                i += 4;
            }
        }

        if (nullTerminate) *ptr = 0;
        if (lenOut) *lenOut = len;

        return out;
    }

}
