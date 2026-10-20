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



    // decodeUtf8
    // https://en.wikipedia.org/wiki/UTF-8
    //
    // USAGE MODES:
    // 1. Dry-Run Only (out == NULL):
    //    Calculates required buffer length, optimal integer width (elementBytesOut),
    //    and fully validates the UTF-8 sequence. Does not write anything.
    //
    // 2. Safe Decode (out != NULL, dryRun == true):
    //    Executes the dry-run first to validate bounds and catch errors. If successful,
    //    it proceeds to decode. Perfect for untrusted inputs.
    //
    // 3. Fast Decode (out != nullptr, dryRun == false):
    //    Skips the pre-validation pass and decodes directly into the buffer.
    //    Use this when you have pre-validated text and a guaranteed large enough buffer.
    //
    UTFInfo decodeUtf8(UTFInfo info, String str, void* out) {
        UTFInfo result = info;
        result.err = UTFInfo::E_NONE;
        result.length = 0;
        result.elementBytesOut = 1; // Assume ASCII minimum until proven otherwise

        if (str.len <= 0 || !str.buff) return result;

        // --- PHASE 1: DRY RUN ---
        if (info.dryRun || !out) {
            uint32_t i = 0;
            while (i < str.len) {
                uint8_t ch = str.buff[i];

                if (ch < 0x80) {
                    // 1-byte
                    i += 1;
                } else if ((ch & 0xE0) == 0xC0) {
                    // 2-byte
                    if (i + 1 >= str.len) {
                        result.length = i;
                        result.err = UTFInfo::E_INVALID_UTF8;
                        return result;
                    }

                    // Any 2-byte UTF-8 sequence demands at least a 2-byte integer array
                    if (result.elementBytesOut < 2) {
                        result.elementBytesOut = 2;
                    }

                    i += 2;
                } else if ((ch & 0xF0) == 0xE0) {
                    // 3-byte
                    if (i + 2 >= str.len) {
                        result.length = i;
                        result.err = UTFInfo::E_INVALID_UTF8;
                        return result;
                    }

                    // 3-byte UTF-8 sequences demand at least a 2-byte integer array
                    if (result.elementBytesOut < 2) {
                        result.elementBytesOut = 2;
                    }

                    i += 3;
                } else if ((ch & 0xF8) == 0xF0) {
                    // 4-byte
                    if (i + 3 >= str.len) {
                        result.length = i;
                        result.err = UTFInfo::E_INVALID_UTF8;
                        return result;
                    }

                    // 4-byte UTF-8 sequences always result in Codepoints > 0xFFFF
                    // Demands a 4-byte integer array
                    result.elementBytesOut = 4;
                    i += 4;
                } else {
                    result.err = UTFInfo::E_INVALID_UTF8;
                    return result;
                }

                result.length++;
            }

            if (!out) return result;
        }

        // --- PHASE 2: DECODING ---
        uint32_t i = 0;

        // Reset length to 0 so we can use it as our array index during decoding
        result.length = 0;

        switch (info.elementBytesIn) {
            case 1: {
                // 8-bit output stride
                uint8_t* const arr = (uint8_t*) out;

                while (i < str.len) {
                    uint8_t ch = str.buff[i];
                    if (ch < 0x80) {
                        // ASCII
                        arr[result.length++] = ch;
                        i += 1;
                    } else {
                        // Any multibyte UTF-8 sequence requires at least 16-bit integers
                        result.err = UTFInfo::E_MISMATCH_ALIGN;
                        return result;
                    }
                }
                break;
            }

            case 2: {
                // 16-bit output stride
                uint16_t* const arr = (uint16_t*) out;
                result.elementBytesOut = 2;

                while (i < str.len) {
                    const uint8_t ch = str.buff[i];
                    if (ch < 0x80) {
                        arr[result.length++] = ch;
                        i += 1;
                    } else if ((ch & 0xE0) == 0xC0) {
                        arr[result.length++] = ((ch & 0x1F) << 6) | (str.buff[i + 1] & 0x3F);
                        i += 2;
                    } else if ((ch & 0xF0) == 0xE0) {
                        arr[result.length++] = ((ch & 0x0F) << 12) | ((str.buff[i + 1] & 0x3F) << 6) | (str.buff[i + 2] & 0x3F);
                        i += 3;
                    } else {
                        // Cannot fit a 4-byte UTF-8 character (>0xFFFF) into a 16-bit stride
                        result.err = UTFInfo::E_MISMATCH_ALIGN;
                        return result;
                    }
                }
                break;
            }

            case 4: { // 32-bit output stride
                uint32_t* const arr = (uint32_t*) out;
                result.elementBytesOut = 4;

                while (i < str.len) {
                    const uint8_t ch = str.buff[i];
                    if (ch < 0x80) {
                        arr[result.length++] = ch;
                        i += 1;
                    } else if ((ch & 0xE0) == 0xC0) {
                        arr[result.length++] = ((ch & 0x1F) << 6) | (str.buff[i + 1] & 0x3F);
                        i += 2;
                    } else if ((ch & 0xF0) == 0xE0) {
                        arr[result.length++] = ((ch & 0x0F) << 12) | ((str.buff[i + 1] & 0x3F) << 6) | (str.buff[i + 2] & 0x3F);
                        i += 3;
                    } else if ((ch & 0xF8) == 0xF0) {
                        arr[result.length++] = ((ch & 0x07) << 18) | ((str.buff[i + 1] & 0x3F) << 12) | ((str.buff[i + 2] & 0x3F) << 6) | (str.buff[i + 3] & 0x3F);
                        i += 4;
                    } else {
                        result.err = UTFInfo::E_INVALID_UTF8;
                        return result;
                    }
                }
                break;
            }

            default: {
                result.err = UTFInfo::E_MISMATCH_ALIGN;
                break;
            }
        }

        return result;
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
