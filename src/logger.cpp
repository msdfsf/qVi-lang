#include "logger.h"
#include "ansi_colors.h"
#include "globals.h"
#include "io.h"
#include "utils.h"

#include <cstdarg>
#include <cstdint>
#include <stdarg.h>
#include <cstdlib>



// TODO : to config
constexpr int     LoggerContextLines = 1;


constexpr int DEFAULT_MESSAGE_LENGTH = 256;
constexpr int TAB_SIZE = 4;

constexpr    size_t   gBufferSize = 1024;
thread_local char     gBuffer[gBufferSize];
thread_local uint32_t gBufferIndex = 0;
thread_local uint32_t gBufferPreviousLength = 0;

thread_local IO::Stream gBufferStream = {
    .kind = IO::Stream::SK_BUFFER,
    .buffer = String(gBuffer, gBufferSize)
};

inline uint32_t getBufferIndex() {
    return (uint32_t) (gBufferStream.buffer.buff - gBuffer);
}

void resetBuffer() {
    gBufferStream.buffer.buff = gBuffer;
    gBufferStream.buffer.len  = gBufferSize;
}



namespace Logger {

    uint32_t    flushStreamCount = 0;
    IO::Stream* flushStreams = NULL;

    SpanStyle defaultSpanStyle = SpanStyle();

    uint32_t verbosity = PLAIN | HINT | INFO | WARNING | ERROR;
    uint64_t mute = 0;


    int getEndClosure(const char ch) {
        switch (ch) {
            case '(' : return ')';
            case '<' : return '>';
            case '[' : return ']';
            case '{' : return '}';
            default: return ')';
        }
    }



    // Low level routines that gather all
    // needed info from configuration
    void write(const char* data, uint32_t len) {
        IO::write(&gBufferStream, data, len);
    }

    void write(const char* const str) {
        IO::write(&gBufferStream, str);
    }

    void writef(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        IO::writefv(&gBufferStream, fmt, args);
        va_end(args);
    }

    void writefv(const char* fmt, va_list args) {
        IO::writefv(&gBufferStream, fmt, args);
    }



    void flush(IO::Stream* stream) {
        uint32_t currentLen = getBufferIndex();
        if (currentLen > 0) {
            IO::write(stream, gBuffer, currentLen);
        }

        gBufferPreviousLength = currentLen;
        resetBuffer();
    }

    void flush() {
        uint32_t currentLen = getBufferIndex();
        if (currentLen == 0) return;

        for (uint32_t i = 0; i < flushStreamCount; i++) {
            IO::write(&flushStreams[i], gBuffer, currentLen);
        }

        gBufferPreviousLength = currentLen;
        resetBuffer();
    }


    String getInternalBuffer() {
        return gBuffer;
    }

    char* getLastString(int* len) {
        *len = gBufferPreviousLength;
        return gBuffer;
    }

    char* getCurrentString(int* len) {
        *len = gBufferIndex;
        return gBuffer;
    }



    int printSpanStrict(IO::Stream* stream, Span* span) {
        const char* str = span->str;
        const uint64_t size = span->end.idx - span->start.idx;
        const uint64_t endIdx = span->end.idx;

        const int maxLineDigits = Utils::countDigits(span->end.ln);

        uint64_t idx = span->start.idx;
        uint64_t prevIdx = idx;
        uint64_t lineNum = span->start.ln;
        while (idx < endIdx) {
            const char ch = str[idx];
            if (ch == '\n') {
                IO::writef(stream, AC_BOLD_CYAN "%*llu | ", maxLineDigits, lineNum);
                IO::write(stream, str + prevIdx, idx - prevIdx + 1);
                lineNum++;
                prevIdx = idx + 1;
            }
            idx++;
        }

        if (idx - prevIdx > 0) {
            IO::writef(stream, AC_BOLD_CYAN "%*llu | ", maxLineDigits, lineNum);
            IO::write(stream, str + prevIdx, idx - prevIdx + 1);
        }

        IO::writef(stream, AC_RESET "\n");

        return maxLineDigits;
    };

    int printSpanStrict(Span* span) {
        const int tmp = printSpanStrict(&gBufferStream, span);
        flush();
        return tmp;
    }

    int printSpanStrictNoFlush(Span* span) {
        return printSpanStrict(&gBufferStream, span);
    }



    void printUnderline(uint32_t maxLineDigits, uint32_t offset, uint32_t length, SpanStyle* style) {
        writef("%s%*s | ", style->colorGutter, maxLineDigits, " ");

        for (int i = 0; i < offset; i++) write(" ");
        writef("%s%c%s", style->colorPointer, style->pointer, style->colorUnderline);
        for (int i = 1; i < length; i++) writef("%c", style->underline);

        write("\n");
    }

    int printSpan(IO::Stream* stream, Span* span, SpanStyle* style) {
        if (!span || !span->str || !style) return 0;

        const char* str = span->str;

        uint32_t startIdx = Utils::findLineStart(str, span->start.idx, style->contextLines);
        uint32_t endIdx   = Utils::findLineEnd(str, span->end.idx, style->contextLines);

        const int maxLineDigits = Utils::countDigits(span->end.ln + style->contextLines);

        uint32_t idx = startIdx;
        uint32_t lastCommitedIdx = startIdx - 1;
        uint32_t lineStartIdx = idx;

        uint32_t currentLine = span->start.ln - style->contextLines;;

        if (idx <= endIdx) {
            IO::writef(stream, "%s%*u | %s", style->colorGutter, maxLineDigits, currentLine, style->colorText);
        }

        bool lineInSpan = false;
        while (idx <= endIdx) {
            if (str[idx] == '\n' || idx == endIdx) {
                IO::write(stream, str + (lastCommitedIdx + 1), idx - lastCommitedIdx);

                if (style->showUnderline && lineInSpan) {
                    const uint32_t start = span->start.idx > lineStartIdx ?
                        (span->start.idx - lineStartIdx) : 0;
                    const uint32_t end = span->end.idx < idx ?
                        (span->end.idx - lineStartIdx) : (idx - lineStartIdx);
                    const uint32_t length = end > start ? (end - start) : 1;

                    printUnderline(maxLineDigits, start, length, style);
                }

                if (str[idx] != '\n') break;

                currentLine++;
                IO::writef(stream, "%s%*u | %s", style->colorGutter, maxLineDigits, currentLine, style->colorText);

                lineInSpan = idx < span->end.idx;

                lineStartIdx = idx + 1;
                lastCommitedIdx = idx;

                idx++;
                continue;
            }

            if (idx == span->start.idx) {
                lineInSpan = true;
                IO::write(stream, str + (lastCommitedIdx + 1), idx - lastCommitedIdx - 1);
                IO::write(stream, style->colorHighlight);
                lastCommitedIdx = idx - 1;
            }

            if (idx == span->end.idx) {
                IO::write(stream, str + (lastCommitedIdx + 1), idx - lastCommitedIdx);
                IO::write(stream, style->colorText);
                lastCommitedIdx = idx;
            }

            idx++;
        }

        IO::write(stream, AC_RESET);

        return maxLineDigits;
    }

    int printSpan(Span* span, SpanStyle* style) {
        return printSpan(&gBufferStream, span, style);
    }

    int printSpanNoFlush(Span* span, SpanStyle* style) {
        const int tmp = printSpan(&gBufferStream, span, style);
        flush();
        return tmp;
    }



    void vlogNoFlush(Type type, const char* const message, Span* span, va_list args) {
        if (mute || !(verbosity & type.level)) return;

        if (!(type.style & NO_HEADER)) {
            switch (type.level) {
                case INFO: {
                    write("INFO");
                    break;
                }

                case WARNING: {
                    writef(AC_WARNING "\nWARNING" AC_RESET);
                    break;
                }

                case ERROR: {
                    writef(AC_ERROR "\nERROR" AC_RESET);
                    break;
                }

                default: {
                    break;
                }
            }

            if (type.tag) {
                writef("[%s]", type.tag);
            }

            if (span) {
                uint32_t lineStart = Utils::findLineStart(span->str, span->start.idx);
                writef("(%i, %i) : ", span->start.ln, span->start.idx - lineStart + 1);
            } else if (type.level != PLAIN) {
                write(" : ");
            }

        }

        writefv(message, args);
        write("\n");

        if (span) {
            printSpanNoFlush(span, &Logger::defaultSpanStyle);
        }

        if (!span || type.style & NO_FOOTER) return;

        write("\n");
        writef(" in file: %.*s\n", span->fileInfo->name.len, span->fileInfo->name.buff);
        write("\n");
    }

    void logNoFlush(Type type, const char* const message, Span* span, ...) {
        va_list args;
        va_start(args, span);

        vlogNoFlush(type, message, span, args);

        va_end(args);
    }

    void logNoFlush(Type type, const char* const message) {
        logNoFlush(type, message, NULL);
    }

    void vlog(Type type, const char* const message, Span* span, va_list args) {
        vlogNoFlush(type, message, span, args);
        flush();
    }

    void log(Type type, const char* const message, Span* span, ...) {
        va_list args;
        va_start(args, span);

        vlogNoFlush(type, message, span, args);
        flush();

        va_end(args);
    }

    void log(Type type, const char* const message) {
        log(type, message, NULL);
    }

    void vlog(IO::Stream* stream, Type type, const char* const message, Span* span, va_list args) {
        vlogNoFlush(type, message, span, args);
        flush(stream);
    }

    void log(IO::Stream* stream, Type type, const char* const message, Span* span, ...) {
        va_list args;
        va_start(args, span);

        vlogNoFlush(type, message, span, args);
        flush(stream);

        va_end(args);
    }

    void log(IO::Stream* stream, Type type, const char* const message) {
        log(stream, type, message, NULL);
    }



    [[noreturn]] void panic(const char* const message, Span* span, ...) {
        va_list args;
        va_start(args, span);

        Type type = {.level = ERROR, .tag = "PANIC"};
        vlogNoFlush(type, message, span, args);
        flush();

        va_end(args);
        exit(1);
    }

    [[noreturn]] void panic(const char* const message) {
        Type type = {.level = ERROR, .tag = "PANIC"};
        log(type, message);
    }

}
