#pragma once
#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <type_traits>
#define CONFIG_DISABLE_LOGGING
#define CONFIG_ERROR_RECOVERY


#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "../src/ansi_colors.h"



inline jmp_buf gJumpBuffer;

#define min(a, b) ((a) < (b) ? (a) : (b))

#define TEST_CASE(name) \
    Test::Case{ #name, [](Test::Result* res)

#if defined(_WIN32)
#include <io.h>
#define DUP    _dup
#define DUP2   _dup2
#define FILENO _fileno
#define CLOSE  _close
#define READ   _read
#define PIPE(fds) _pipe(fds, 4096, 0)
#else
#include <unistd.h>
#define DUP    dup
#define DUP2   dup2
#define FILENO fileno
#define CLOSE  close
#define READ   read
#define PIPE(fds) pipe(fds)
#endif

namespace Test {
    constexpr uint32_t maxAssertToDisplay = 16;

    struct String {
        char*    buf;
        uint64_t len;
    };

    struct Result {
        uint32_t assertPassed = 0;
        uint32_t assertFailed = 0;
        uint32_t testsPassed  = 0;
        uint32_t testsFailed  = 0;

        uint32_t currentAssertFailed   = 0;
        uint32_t currentAssertPassed   = 0;
        uint32_t currentAssertMask     = 0;
        bool     currentTestFailed     = false;
        bool     currentTestFailedHard = false;
    };

    using CaseFcn = void (*)(Result* res);
    using FileFcn = void (*)(const char* file, const char* output, Result* res);

    using PreFcn  = void (*) ();
    using PostFcn = void (*) ();

    struct Case {
        const char* name;
        CaseFcn     fcn;
    };

    struct FileCase {
        const char* fname;
        FileFcn     fcn;
    };

    struct Suite {
        const char*       name;

        FileCase*         file = NULL;

        const Test::Case* cases;
        size_t            caseCount;

        PreFcn        preCaseFcn   = NULL;
        PostFcn       postCaseFcn  = NULL;
        PreFcn        preSuiteFcn  = NULL;
        PreFcn        postSuiteFcn = NULL;
    };

    struct File {
        String name;
        char*  buffer;

        struct Test {
            String      name;
            const char* data;
            const char* result;
        }* tests;
        uint64_t testCount;
    };

    inline thread_local Result gResult;

    inline int gOgStdout = -1;
    inline int gPipe[2] = { -1, -1 };
    inline thread_local char gStdoutBuffer[1024 * 10];



    inline int _writeStatusLine(const char* status, String name, const char* color) {
        printf("%s[%-5s]%s %.*s\n", color, status, AC_RESET, (int) name.len, name.buf);
        return 1 + 5 + 2 + name.len;
    }

    inline int _writeStatusLine(const char* status, const char* name, const char* color) {
        return _writeStatusLine(status, String { (char*) name, strlen(name) }, color);
    }

    inline void _writeAssertLine(Result* result) {
        uint32_t mask = result->currentAssertMask;

        const int assertCount = result->currentAssertFailed + result->currentAssertPassed;
        const int count = min(maxAssertToDisplay, assertCount);

        printf(AC_BOLD_RED "[%2i|%-2i] " AC_RESET, result->currentAssertPassed, count);

        for (int i = 0; i < count; i++, mask >>= 1) {
            if (mask & 1) {
                printf(AC_BOLD_RED "%i " AC_RESET, i + 1);
            } else {
                printf(AC_BOLD_GREEN "%i " AC_RESET, i + 1);
            }
        }

        if (result->currentTestFailedHard) {
            printf(AC_BRIGHT_BLACK "?" AC_RESET);
        } else if (count < assertCount) {
            printf(AC_BRIGHT_BLACK "..." AC_RESET);
        }

        printf("\n");
    }

    inline void _writeResult(const Test::Result* res) {
        uint32_t testCount = res->testsPassed + res->testsFailed;

        printf(AC_BRIGHT_BLACK "==================================================\n");
        if (res->testsFailed == 0) {
            printf(AC_BOLD_GREEN "WE CHILL: " AC_RESET "%u|%u tests passed!\n",
                    res->testsPassed, testCount);
        } else {
            printf(AC_BOLD_RED "WE WORK: " AC_RESET "%u|%u tests passed.\n",
                    res->testsPassed, testCount);
        }
    }



    inline bool assertTrue(Test::Result* result, bool condition) {
        if (condition) {
            result->assertPassed++;
            result->currentAssertPassed++;

            return false;
        } else {
            const int idx = result->currentAssertPassed + result->currentAssertFailed;
            if (idx < maxAssertToDisplay) {
                result->currentAssertMask |= 1 << idx;
            }

            result->assertFailed++;
            result->currentAssertFailed++;
            result->currentTestFailed = true;

            return true;
        }
    }

    inline bool assert(bool condition) {
        return assertTrue(&gResult, condition);
    }

    inline void assertOrDie(bool condition) {
        if (assertTrue(&gResult, condition)) {
            gResult.currentTestFailedHard = true;
            longjmp(gJumpBuffer, 1);
        }
    }

    template<typename T1, typename T2>
    inline bool assertEqual(Test::Result* result, T1 actual, T2 expected) {
        if constexpr (std::is_floating_point_v<T1> || std::is_floating_point_v<T2>) {
            using CommonFloat = std::conditional_t<
                std::is_same_v<T1, double> || std::is_same_v<T2, double>, double, float>;

            CommonFloat diff = std::abs(static_cast<CommonFloat>(actual) - static_cast<CommonFloat>(expected));
            CommonFloat eps  = std::is_same_v<CommonFloat, double> ? 1e-9 : 1e-5f;

            return assertTrue(result, diff <= eps);
        }

        if constexpr (std::is_same_v<std::decay_t<T1>, const char*> &&
                            std::is_same_v<std::decay_t<T2>, const char*>) {
            if (actual == nullptr || expected == nullptr) {
                return assertTrue(result, actual == expected);
            }
            return assertTrue(result, std::strcmp(actual, expected) == 0);
        }

        return assertTrue(result, actual == expected);
    }

    template<typename T1, typename T2>
    inline bool assert(T1 actual, T2 expected) {
        return assertEqual(&gResult, actual, expected);
    }

    template<typename T1, typename T2>
    inline void assertOrDie(T1 actual, T2 expected) {
        if (assertEqual(&gResult, actual, expected)) {
            gResult.currentTestFailedHard = true;
            longjmp(gJumpBuffer, 1);
        }
    }



    inline bool captureStdoutBegin() {
        std::fflush(stdout);

        gOgStdout = DUP(FILENO(stdout));
        if (gOgStdout < 0) return false;

        if (PIPE(gPipe) != 0) {
            CLOSE(gOgStdout);
            return false;
        }

        DUP2(gPipe[1], FILENO(stdout));
        CLOSE(gPipe[1]);

        return true;
    }

    inline bool captureStdoutEnd() {
        std::fflush(stdout);

        DUP2(gOgStdout, FILENO(stdout));
        CLOSE(gOgStdout);

        size_t idx = 0;
        char buffer[1024];

        while (true) {
            const int bytesRead = READ(gPipe[0], buffer, sizeof(buffer) - 1);
            if (bytesRead <= 0) {
                break;
            }

            if (idx + bytesRead >= sizeof(gStdoutBuffer) - 1) {
                CLOSE(gPipe[0]);
                gStdoutBuffer[idx] = '\0';
                return false;
            }

            std::memcpy(gStdoutBuffer + idx, buffer, bytesRead);
            idx += bytesRead;
        }

        gStdoutBuffer[idx] = '\0';

        CLOSE(gPipe[0]);
        return true;
    }



    inline void _unescapeInPlace(char* str, size_t len) {
        auto hexValue = [](char ch) -> uint8_t {
            if (ch >= '0' && ch <= '9') return (uint8_t) (ch - '0');
            if (ch >= 'a' && ch <= 'f') return (uint8_t) (ch - 'a' + 10);
            if (ch >= 'A' && ch <= 'F') return (uint8_t) (ch - 'A' + 10);
            return 0xFF;
        };

        char* reader = str;
        char* writer = str;
        char* end = str + len;

        while (reader < end) {
            if (*reader == '\\' && reader + 1 < end) {
                reader++;

                switch (*reader) {
                    case 'n':
                        *writer++ = '\n';
                        reader++;
                        break;

                    case 't':
                        *writer++ = '\t';
                        reader++;
                        break;

                    case 'r':
                        *writer++ = '\r';
                        reader++;
                        break;

                    case '"':
                        *writer++ = '"';
                        reader++;
                        break;

                    case '\\':
                        *writer++ = '\\';
                        reader++;
                        break;

                    case 'x': {
                        if (reader + 2 < end) {
                            uint8_t hi = hexValue(reader[1]);
                            uint8_t lo = hexValue(reader[2]);

                            if (hi != 0xFF && lo != 0xFF) {
                                *writer++ = (char)((hi << 4) | lo);
                                reader += 3;
                                break;
                            }
                        }

                        // Invalid/incomplete \x escape
                        *writer++ = '\\';
                        *writer++ = *reader++;
                        break;
                    }

                    default:
                        *writer++ = *reader++;
                        break;
                }
            } else {
                *writer++ = *reader++;
            }
        }

        *writer = '\0';
    }

    // TODO: handle errors
    inline File parseTestFile(const char* filepath) {
        File result = { NULL, NULL, NULL, 0 };

        FILE* file = std::fopen(filepath, "rb");
        if (!file) return result;

        std::fseek(file, 0, SEEK_END);
        long fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        if (fileSize <= 0) {
            std::fclose(file);
            return result;
        }

        char* buffer = (char*) std::malloc(fileSize + 1);
        size_t bytesRead = std::fread(buffer, 1, fileSize, file);
        buffer[bytesRead] = '\0';
        std::fclose(file);

        result.buffer = buffer;

        // First, count number of `#test` directives to pre-allocate memory
        uint64_t count = 0;
        char* ptr = buffer;
        while (*ptr) {
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            if (std::strncmp(ptr, "#test", 5) == 0) {
                count++;
            }

            while (*ptr && *ptr != '\n') ptr++;
            if (*ptr == '\n') ptr++;
        }

        if (count == 0) return result;

        result.testCount = count;
        result.tests     = (File::Test*) std::malloc(sizeof(File::Test) * count);

        // Second, populate 'arrays'
        ptr = buffer;
        char* currentSourceStart = buffer;
        uint64_t testIdx = 0;

        String name;
        bool hasName = false;
        bool recomputeNameLen = false;

        while (*ptr && testIdx < count) {
            char* lineStart = ptr;

            while (*ptr == ' ' || *ptr == '\t') ptr++;

            if (ptr && ptr[0] == '/' && ptr[1] == '/') {
                // We record last comment line before test, so we
                // can annotate it...
                hasName = true;
                name.buf = ptr + 2;
                recomputeNameLen = true;
            }

            if (std::strncmp(ptr, "#test", 5) == 0) {
                *lineStart = '\0';

                if (hasName) {
                    result.tests[testIdx].name = name;

                    hasName = false;
                    name.buf = NULL;
                    name.len = 0;
                }

                char* quoteOpen = std::strchr(ptr + 5, '"');
                if (quoteOpen) {
                    quoteOpen++;

                    // Looking for 'quoteEnd' backwards from line end
                    // TODO: any number of lines
                    char* lineEnd = quoteOpen;
                    while (*lineEnd && *lineEnd != '\n' && *lineEnd != '\r') lineEnd++;

                    char* quoteClose = lineEnd - 1;
                    while (quoteClose >= quoteOpen && *quoteClose != '"') quoteClose--;

                    if (quoteClose >= quoteOpen) {
                        size_t resultLen = quoteClose - quoteOpen;
                        _unescapeInPlace(quoteOpen, resultLen);

                        result.tests[testIdx].data   = currentSourceStart;
                        result.tests[testIdx].result = quoteOpen;
                        testIdx++;

                        ptr = lineEnd;
                        if (*ptr == '\r') ptr++;
                        if (*ptr == '\n') ptr++;
                        currentSourceStart = ptr;

                        continue;
                    }
                }
            }

            while (*ptr && *ptr != '\n') ptr++;
            if (*ptr == '\n') {
                if (recomputeNameLen) {
                    name.len = ptr - name.buf;
                    recomputeNameLen = false;
                }
                ptr++;
            }
        }

        return result;
    }

    inline void freeTestFile(File* file) {
        if (!file) return;

        std::free(file->buffer);
        std::free(file->tests);

        file->buffer    = NULL;
        file->tests     = NULL;
        file->testCount = 0;
    }



    inline void _clearCurrentResult(Test::Result* result) {
        result->currentAssertMask   = 0;
        result->currentTestFailed   = 0;
        result->currentAssertFailed = 0;
        result->currentAssertPassed = 0;
        gResult.currentTestFailedHard = false;
    }

    inline void runTestCase(const Test::Case* testCase, Test::Result* result) {
        _clearCurrentResult(result);

        if (setjmp(gJumpBuffer) == 0) {
            testCase->fcn(result);
        }

        if (result->currentTestFailed) {
            _writeStatusLine(":FAIL", testCase->name, AC_BOLD_RED);
            _writeAssertLine(result);
            result->testsFailed++;
        } else {
            _writeStatusLine(":OKAY", testCase->name, AC_BOLD_GREEN);
            result->testsPassed++;
        }
    }

    inline void runTestFile(const Test::File* testFile, FileFcn fcn, const int idx, Test::Result* result) {
        _clearCurrentResult(result);

        const String name = testFile->tests[idx].name;

        const char* input  = testFile->tests[idx].data;
        const char* output = testFile->tests[idx].result;

        if (setjmp(gJumpBuffer) == 0) {
            // TODO: do something if these fails
            captureStdoutBegin();
            fcn(input, output, result);
            captureStdoutEnd();
            assert((const char*) gStdoutBuffer, (const char*) output);
        }

        if (result->currentTestFailed) {
            _writeStatusLine(":FAIL", name, AC_BOLD_RED);
            _writeAssertLine(result);
            result->testsFailed++;
        } else {
            _writeStatusLine(":OKAY", name, AC_BOLD_GREEN);
            result->testsPassed++;
        }
    }

    inline void runTestSuite(const Test::Suite* suite, Test::Result* result) {
        int lineLen = _writeStatusLine("SUITE", suite->name, AC_BOLD_CYAN);

        if (suite->preSuiteFcn) suite->preSuiteFcn();

        if (suite->file) {
            File file = parseTestFile(suite->file->fname);
            file.name.buf = (char*) suite->file->fname;
            file.name.len = strlen(suite->file->fname);

            for (size_t i = 0; i < file.testCount; i++) {
                if (suite->preCaseFcn) suite->preCaseFcn();
                runTestFile(&file, suite->file->fcn, i, result);
                if (suite->postCaseFcn) suite->postCaseFcn();
            }
            freeTestFile(&file);
        }

        for (size_t i = 0; i < suite->caseCount; i++) {
            if (suite->preCaseFcn) suite->preCaseFcn();
            runTestCase(suite->cases + i, result);
            if (suite->postCaseFcn) suite->postCaseFcn();
        }

        if (suite->postSuiteFcn) suite->postSuiteFcn();

        putchar('\n');
    }

}
