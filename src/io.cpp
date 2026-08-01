#include "io.h"
#include "dynamic_arena.h"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>



namespace IO {

    thread_local uint64_t gBytesWritten;
    thread_local uint8_t  gBuffer[2048];
    thread_local uint64_t gBufferSize = sizeof(gBuffer);



    void write(Stream* stream, const char* const str) {
        write(stream, (const char*) str, (uint32_t) strlen(str));
    }

    void write(Stream* stream, const char* data, uint32_t len) {
        if (len == 0) return;

        switch (stream->kind) {
            case Stream::SK_C_STREAM: {
                fwrite(data, 1, len, (FILE*) stream->cstream);
                break;
            }

            case Stream::SK_BUFFER: {
                uint64_t remaining = stream->buffer.len;
                uint32_t toCopy = len < remaining ? len : remaining;

                if (toCopy > 0) {
                    memcpy(stream->buffer.buff, data, toCopy);

                    stream->buffer.buff += toCopy;
                    stream->buffer.len  -= toCopy;
                }

                break;
            }

            case Stream::SK_ARENA: {
                void* dest = Arena::push(stream->arena, len);
                memcpy(dest, data, len);
                break;
            }

            case Stream:: SK_NULL: {
                gBytesWritten += len;
                return;
            }
        }
    }

    void writef(Stream* stream, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        writefv(stream, fmt, args);
        va_end(args);
    }

    void writefv(Stream* stream, const char* fmt, va_list args) {
        switch (stream->kind) {
            case Stream::SK_C_STREAM: {
                vfprintf((FILE*) stream->cstream, fmt, args);
                break;
            }

            case Stream::SK_BUFFER: {
                int remaining = (int)stream->buffer.len;
                if (remaining <= 0) return;

                int written = vsnprintf(stream->buffer.buff, remaining, fmt, args);

                if (written > 0) {
                    int actualMove = (written < remaining) ? written : (remaining - 1);

                    stream->buffer.buff += actualMove;
                    stream->buffer.len  -= actualMove;
                }

                break;
            }

            case Stream::SK_ARENA: {
                const int guessSize = 1024;
                char* buffer = (char*) Arena::push(stream->arena, guessSize);

                va_list argsCopy;
                va_copy(argsCopy, args);

                int len = vsnprintf(buffer, guessSize, fmt, args);
                if (len < 0) {
                    Arena::rollback(stream->arena, guessSize);
                    va_end(argsCopy);
                    return;
                }

                if (len < guessSize) {
                    Arena::rollback(stream->arena, guessSize - len);
                } else {
                    Arena::rollback(stream->arena, guessSize);

                    buffer = (char*) Arena::push(stream->arena, len + 1);
                    vsnprintf(buffer, len + 1, fmt, argsCopy);

                    Arena::rollback(stream->arena, 1);
                }

                va_end(argsCopy);
                break;
            }

            case Stream::SK_NULL: {
                gBytesWritten += snprintf(NULL, 0, fmt, args);
                return;
            }
        }
    }

    void write(Stream* stream, String str) {
        write(stream, str.buff, str.len);
    }

    void write(Stream* stream, String* str) {
        write(stream, str->buff, str->len);
    }

    void write(Stream* stream, char ch) {
        switch (stream->kind) {
            case Stream::SK_C_STREAM: {
                fputc(ch, (FILE*) stream->cstream);
                break;
            }

            case Stream::SK_BUFFER: {
                if (stream->buffer.len > 0) {
                    *stream->buffer.buff = ch;
                    stream->buffer.buff++;
                    stream->buffer.len--;
                }
                break;
            }

            case Stream::SK_ARENA: {
                *(char*) Arena::push(stream->arena, 1) = ch;
                break;
            }

            case Stream::SK_NULL: {
                gBytesWritten++;
                break;
            }
        }
    }

    void write(Stream* stream, uint8_t ch) {
        write(stream, (char)ch);
    }

    void write(Stream* stream, char ch, uint32_t count) {
        if (count == 0) return;

        switch (stream->kind) {
            case Stream::SK_C_STREAM: {
                // For multiple chars, we use a small loop or a stack buffer for speed
                // but for small counts, fputc in a loop is often faster than a setup for fwrite
                for (uint32_t i = 0; i < count; i++) fputc(ch, (FILE*) stream->cstream);
                break;
            }

            case Stream::SK_BUFFER: {
                uint32_t toWrite = (count < (uint32_t)stream->buffer.len) ? count : (uint32_t)stream->buffer.len;
                if (toWrite > 0) {
                    memset(stream->buffer.buff, ch, toWrite);
                    stream->buffer.buff += toWrite;
                    stream->buffer.len  -= toWrite;
                }
                break;
            }

            case Stream::SK_ARENA: {
                void* dest = Arena::push(stream->arena, count);
                memset(dest, ch, count);
                break;
            }

            case Stream::SK_NULL: {
                gBytesWritten += count;
                break;
            }
        }
    }

    void write(Stream* stream, uint8_t ch, uint32_t count) {
        write(stream, (char) ch, count);
    }

}
