#pragma once

#include <cstdint>
#include <cstring>


constexpr std::size_t cstrlen(const char* str) {
    if (!str) return 0;

    std::size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

struct String {

    char* buff;
    uint64_t len;

    inline char& operator [] (const int idx) {
        return buff[idx];
    }

    inline operator char* () {
        return buff;
    }

    inline void operator = (char* str) {
        buff = str;
        len = cstrlen(str);
    }

    inline void operator += (int offset) {
        buff += offset;
        len -= offset;
    }

    inline bool operator==(const String& str) noexcept {
        if (str.len != this->len) return false;
        return std::memcmp(this->buff, str.buff, str.len) == 0;
    }

    inline bool operator<(const String& str) noexcept {
        const int na = this->len;
        const int nb = str.len;
        const int n = na < nb ? na : nb;
        int cmp = 0;
        if (n > 0) cmp = std::memcmp(this->buff, str.buff, static_cast<size_t>(n));
        if (cmp != 0) return cmp < 0;
        return na < nb;
    }

    constexpr String() : buff(NULL), len(0) {}

    constexpr String(char* buff, uint64_t len) : buff(buff), len(len) {}

    constexpr String(const char* cstr) : buff((char*) cstr), len(cstrlen(cstr)) {}

};

inline bool operator==(const String& a, const String& b) noexcept {
    if (a.len != b.len) return false;
    return std::memcmp(a.buff, b.buff, a.len) == 0;
}

inline bool operator<(const String& a, const String& b) noexcept {
    const int na = a.len;
    const int nb = b.len;
    const int n = na < nb ? na : nb;
    int cmp = 0;
    if (n > 0) cmp = std::memcmp(a.buff, b.buff, static_cast<size_t>(n));
    if (cmp != 0) return cmp < 0;
    return na < nb;
}

constexpr int cstrcmp(const String strA, const String strB) {

    if (strA.len != strB.len) return 0;

    for (int i = 0; i < strA.len; i++) {
        if (strA.buff[i] != strB.buff[i]) {
            return 0;
        }
    }

    return 1;

}

constexpr int cstrcmp(String* const strA, String* const strB) {
    return cstrcmp(*strA, *strB);
}

// Alphabetically compares strings
// Returns negative value if strA comes before strB
// Returns positive value if strA comes after strB
// Returns 0 if strings are equal
constexpr int cstracmp(const String strA, const String strB) {

    int minLen = (strA.len < strB.len) ? strA.len : strB.len;

    for (int i = 0; i < minLen; i++) {
        const int diff = (unsigned char) strA.buff[i] - (unsigned char) strB.buff[i];
        if (diff != 0) {
            return diff;
        }
    }

    return strA.len - strB.len;

}
