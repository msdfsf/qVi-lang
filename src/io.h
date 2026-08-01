#pragma once

#include "string.h"
#include <cstdint>



namespace Arena {
    struct Container;
};

namespace IO {

    struct Stream {
        enum Kind : uint8_t {
            SK_NULL,     // Discard output
            SK_C_STREAM, // C-style FILE*
            SK_BUFFER,   // Fixed memory buffer
            SK_ARENA     // Internal arena representation
        } kind;

        union {
            void*  cstream;
            String buffer;
            Arena::Container* arena;
        };
    };

    void write  (Stream* stream, const char* const str);
    void write  (Stream* stream, const char* str, uint32_t len);
    void writef (Stream* stream, const char* fmt, ...);
    void writefv(Stream* stream, const char* fmt, va_list args);

    void write(Stream* stream, String str);
    void write(Stream* stream, String* str);

    void write(Stream* stream, char ch);
    void write(Stream* stream, uint8_t ch);
    void write(Stream* stream, char ch, uint32_t count);
    void write(Stream* stream, uint8_t ch, uint32_t count);

}
