#pragma once

#include "string.h"

namespace Strings {

    long toInt(String str, int* endIdx);
    void replace(String str, String rstr, const int idx, const int len);

    char* encodeUtf8(String str, int* lenOut, int* bytesOut, int copyWhenAscii);
    wchar_t* encodeUtf16(String str, int* lenOut, int nullTerminate);

    inline constexpr bool isLowercase(const char ch) {
        return ('a' <= ch && ch <= 'z');
    }

    inline constexpr bool isUppercase(const char ch) {
        return ('A' <= ch && ch <= 'Z');
    }

    inline constexpr bool isAlpha(const char ch) {
        return isLowercase(ch) || isUppercase(ch);
    }

    inline constexpr const char toLowercase(const char ch) {
        return isUppercase(ch) ? ch + ('a' - 'A') : ch;
    }

    inline constexpr int compare(const String strA, const String strB) {

        if (strA.len != strB.len) return 0;

        for (int i = 0; i < strA.len; i++) {
            if (strA.buff[i] != strB.buff[i]) {
                return 0;
            }
        }

        return 1;

    }

    inline constexpr int compare(const String* strA, const String strB) {

        if (strA->len != strB.len) return 0;

        for (int i = 0; i < strA->len; i++) {
            if (strA->buff[i] != strB.buff[i]) {
                return 0;
            }
        }

        return 1;

    }

    inline constexpr int compare(const String* strA, const String* strB) {

        if (strA->len != strB->len) return 0;

        for (int i = 0; i < strA->len; i++) {
            if (strA->buff[i] != strB->buff[i]) {
                return 0;
            }
        }

        return 1;

    }



    inline constexpr bool icompare(const String strA, const String strB) {
        if (strA.len != strB.len) return 0;

        for (int i = 0; i < strA.len; i++) {
            if (toLowercase(strA.buff[i]) != toLowercase(strB.buff[i])) {
                return false;
            }
        }

        return true;
    }



    // Alphabetically compares strings
    // Returns negative value if strA comes before strB
    // Returns positive value if strA comes after strB
    // Returns 0 if strings are equal
    constexpr int acompare(const String strA, const String strB) {

        int minLen = (strA.len < strB.len) ? strA.len : strB.len;

        for (int i = 0; i < minLen; i++) {
            const int diff = (unsigned char) strA.buff[i] - (unsigned char) strB.buff[i];
            if (diff != 0) {
                return diff;
            }
        }

        return strA.len - strB.len;

    }

    inline void copy(const String source, const String target) {
        std::memcpy(target.buff, source.buff, target.len);
    }

    // TODO: better arg names?
    // TODO: unite
    inline bool contains(String source, String target) {
        if (target.len == 0)         return true;
        if (source.len == 0)         return false;
        if (target.len > source.len) return false;

        for (size_t i = 0; i <= source.len - target.len; i++) {
            if (compare({ source.buff + i, target.len }, target)) {
                return true;
            }
        }

        return false;
    }

    // If source contains target; source being null-terminated string
    inline bool contains(const char* source, String target) {
        if (target.len == 0) return true;
        if (!source)         return false;

        size_t sourceLen = strlen(source);
        if (target.len > sourceLen) return false;

        for (size_t i = 0; i <= sourceLen - target.len; i++) {
            if (compare({(char*) source + i, target.len }, target)) {
                return true;
            }
        }

        return false;
    }

    inline bool icontains(String source, String target) {
        if (target.len == 0)         return true;
        if (source.len == 0)         return false;
        if (target.len > source.len) return false;

        for (size_t i = 0; i <= source.len - target.len; i++) {
            if (icompare({ source.buff + i, target.len }, target)) {
                return true;
            }
        }

        return false;
    }

    inline bool icontains(const char* source, String target) {
        if (target.len == 0) return true;
        if (!source)         return false;

        size_t sourceLen = strlen(source);
        if (target.len > sourceLen) return false;

        for (size_t i = 0; i <= sourceLen - target.len; i++) {
            if (icompare({(char*) source + i, target.len }, target)) {
                return true;
            }
        }

        return false;
    }
}
