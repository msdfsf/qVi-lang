#pragma once

#include "globals.h"
#include "ansi_colors.h"
#include <stdint.h>
#include <stdio.h>




// Internaly we use own buffer to write logs to.
// This buffer can be accessed between log calls
// to retrieve output string. This is done because
// compiler may need to store errors in memory to
// provide them to the possible clients.
//
// Therefore logging is divided into two parts.
// At first message got rendered to internal buffer.
// Then, either buffer can be accessed and copied, or
// flush function can be called to deliver buffer to
// all predefined 'streams'.
namespace Logger {

    enum Level : uint8_t {
        PLAIN   = 1 << 0,
        HINT    = 1 << 1,
        INFO    = 1 << 2,
        WARNING = 1 << 3,
        ERROR   = 1 << 4
    };

    enum Style : uint8_t {
        DEFAULT   = 0,
        NO_HEADER = 1 << 0,
        NO_FOOTER = 1 << 1,
    };

    struct Type {
        Level level     = PLAIN;
        Style style     = DEFAULT;
        const char* tag = NULL;
    };

    // TODO : pick good colors
    struct SpanStyle {
        const char* colorGutter    = AC_BRIGHT_BLACK; // Line numbers
        const char* colorText      = AC_BRIGHT_WHITE;        // The source code
        const char* colorHighlight = AC_BRIGHT_MAGENTA;          // The specific highlighted text
        const char* colorUnderline = AC_BRIGHT_CYAN;
        const char* colorPointer   = AC_BRIGHT_RED;

        char pointer   = '^';
        char underline = '~';

        uint8_t contextLines  = 1;    // Lines to show before/after the highlighted text
        bool    showGutter    = true; // Toggle " 10 | " part
        bool    showUnderline = true;
    };

    struct FlushStream {
        enum {
            FS_C_STREAM
        } kind;

        union {
            FILE* cstream;
        };
    };

    extern uint32_t     flushStreamCount;
    extern FlushStream* flushStreams;

    extern SpanStyle defaultSpanStyle;

    extern uint32_t verbosity; // level enum bitmask
    extern uint64_t mute;      // Thread id bitmask

    // Functions to render and auto flush to all flush streams
    void log(Type type, const char* const message, Span* loc, ...);
    void log(Type type, const char* const message);

    // Functions to render to internal buffer only
    void logNoFlush(Type type, const char* const message, Span* loc, ...);
    void logNoFlush(Type type, const char* const message);

    // Functions to render and flush only to the provided stream
    void log(FlushStream* stream, Type type, const char* const message, Span* loc, ...);
    void log(FlushStream* stream, Type type, const char* const message);

    // prints span strictly as defined
    // returns the max digits used for line numbers
    int printSpanStrict(Span* span);
    int printSpanStrictNoFlush(Span* span);
    int printSpanStrict(FlushStream* stream, Span* span);

    // normalizes spans to full line boundaries
    // returns the max digits used for line numbers
    // Note: adds context lines around based on config
    int printSpan(Span* span, SpanStyle* style);
    int printSpanNoFlush(Span* span, SpanStyle* style);
    int printSpan(FlushStream* stream, Span* span, SpanStyle* style);

    // Flushes internal buffer to all streams
    void flush();

    // Flushes only to the provided stream
    void flush(FlushStream* stream);

    // String of last logged string valid till the next
    // log call on the same thread.
    char* getLastString(int* len);

    // String of current buffer un-flushed buffer
    char* getCurrentString(int* len);

    [[noreturn]] void panic(const char* const message, Span* loc, ...);
    [[noreturn]] void panic(const char* const message);

}
