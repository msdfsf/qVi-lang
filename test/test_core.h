#pragma once
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <ostream>
#include <type_traits>


#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <chrono>
#include <algorithm>
#include <stdlib.h>
#include <setjmp.h>
#include "../src/ansi_colors.h"
#include "../src/strlib.h"



inline jmp_buf gJumpBuffer;

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

    struct ExpectedDiagnostic {
        int  code;
        int  line;
        enum Kind : uint8_t {
            DK_ERROR,
            DK_WARNING
        }    kind;
        bool hasLine;
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

    union TestArgument {
        const char* output;
        struct {
            int                 count;
            ExpectedDiagnostic* data;
        } diag;
        FILE* outStream; // For snapshots
    };

    using CaseFcn = void (*)(Result* res);
    using FileFcn = void (*)(const char* file, TestArgument arg, Result* res);

    using PreFcn  = void (*) (bool visualize);
    using PostFcn = void (*) (bool visualize);

    using GoldenFcn = void (*) (FILE* file, const char* input);

    struct Case {
        const char* name;
        CaseFcn     fcn;
    };

    struct FileCase {
        const char* fname;
        FileFcn     fcn;
    };

    enum FileCaseKind : uint8_t {
        CK_RUN_PASS = 0,
        CK_RUN_FAIL = 1,

        CK_SNAPSHOT,
    };

    struct Suite {
        const char*       name;

        FileCase*         file = NULL;

        const Test::Case* cases;
        size_t            caseCount;

        PreFcn        preCaseFcn   = NULL;
        PostFcn       postCaseFcn  = NULL;
        PreFcn        preSuiteFcn  = NULL;
        PostFcn       postSuiteFcn = NULL;

        FileCaseKind  fileKind;
        bool          useGolden = false;
    };

    struct File {
        String name;
        char*  buffer;
        char*  bufferGolden;

        struct Test {
            String      name;
            const char* data;
            const char* result; // points to buffer or bufferGolden chunk
        }* tests;
        uint64_t testCount;
    };

    // Operators:
    //   A & B - name has to contain A and B
    //   A | B - name has to contain A or B
    //   A ^ B - name has to contain either A or B
    enum FilterOp : uint8_t {
        NONE, // Has to be set on last element
        AND,  // &
        OR,   // |
        XOR   // ^
    };

    struct FilterEntry {
        String   str;
        FilterOp op;  // Operator joining this with the next result
    };

    struct Filter {
        static constexpr int MAX_QUERY_ENTRIES = 16;

        FilterEntry qFile [MAX_QUERY_ENTRIES];
        FilterEntry qSuite[MAX_QUERY_ENTRIES];
        FilterEntry qTest [MAX_QUERY_ENTRIES];

        int32_t qFileCount  = 0;
        int32_t qSuiteCount = 0;
        int32_t qTestCount  = 0;
    };

    struct Context {
        Filter* filter;
        bool    visualize;
        bool    useGolden;
    };

    struct BenchData {
        void*    data;
        uint32_t size;
    };

    struct BenchCase {
        const char* name;
        int         iterCount;
        void        (*run)     (BenchData* data);
        BenchData   (*prep)    ();
        void        (*release) (BenchData* data);
    };

    struct BenchResult {
        Result result;

        const char* name;
        int64_t     iterCount;
        double      secElapsed; // Total elapsed time in seconds

        // Timing stats in nanoseconds
        double nsMin;
        double nsMax;
        double nsMed;
        double nsAvg;

        double mbPerSec;
    };

    using BenchFcn = void (void*);

    inline thread_local Result gResult;

    constexpr int gExpectedDiagnosticCount = 32;
    inline thread_local ExpectedDiagnostic gExpectedDiagnostics[gExpectedDiagnosticCount];

    inline int gOgStdout = -1;
    inline int gPipe[2] = { -1, -1 };
    inline thread_local char gStdoutBuffer[1024 * 10];

    inline bool gGenerateGoldens = false;
    inline bool gAutoGenerateGoldens = false; // Auto-generate if _golden.txt is missing


    inline void _clearCurrentResult(Test::Result* result) {
        result->currentAssertMask   = 0;
        result->currentTestFailed   = 0;
        result->currentAssertFailed = 0;
        result->currentAssertPassed = 0;
        gResult.currentTestFailedHard = false;
    }

    inline int _writeStatusLine(const char* status, String name, const char* color) {
        printf("%s[%-5s]%s %.*s\n", color, status, AC_RESET, (int) name.len, name.buff);
        return 1 + 5 + 2 + name.len;
    }

    inline int _writeStatusLine(const char* status, const char* name, const char* color) {
        return _writeStatusLine(status, String { (char*) name, strlen(name) }, color);
    }

    inline void _writeAssertLine(Result* result) {
        uint32_t mask = result->currentAssertMask;

        const int assertCount = result->currentAssertFailed + result->currentAssertPassed;

        if (result->currentAssertPassed == assertCount) {
            printf(AC_BOLD_GREEN "[%2i|%-2i] " AC_RESET, result->currentAssertPassed, assertCount);
        } else {
            printf(AC_BOLD_RED "[%2i|%-2i] " AC_RESET, result->currentAssertPassed, assertCount);
        }

        const int count = maxAssertToDisplay < assertCount ? maxAssertToDisplay : assertCount;
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

    inline void _formatTime(char* out, size_t outSize, double ns) {
        if (ns < 1000.0) {
            snprintf(out, outSize, "%.2f ns", ns);
        } else if (ns < 1000000.0) {
            snprintf(out, outSize, "%.2f us", ns / 1000.0);
        } else {
            snprintf(out, outSize, "%.2f ms", ns / 1000000.0);
        }
    }

    inline void _writeBenchResult(const Test::BenchResult* res) {
        char bufMed[32], bufMin[32], bufMax[32], bufAvg[32];
        _formatTime(bufMed, sizeof(bufMed), res->nsMed);
        _formatTime(bufMin, sizeof(bufMin), res->nsMin);
        _formatTime(bufMax, sizeof(bufMax), res->nsMax);
        _formatTime(bufAvg, sizeof(bufAvg), res->nsAvg);

        _writeStatusLine("BENCH", res->name, AC_BOLD_CYAN);
        printf("  Iterations : %lld\n", res->iterCount);
        printf("  Total Time : %.4f s\n", res->secElapsed);
        printf("  Median     : \033[1;36m%s\033[0m\n", bufMed);
        printf("  Best (Min) : %s\n", bufMin);
        printf("  Worst (Max): %s\n", bufMax);
        printf("  Average    : %s\n", bufAvg);
        printf("  Rate (Med) : %.0f iters/sec\n", 1e9 / res->nsMed);

        if (res->mbPerSec > 0.0) {
            if (res->mbPerSec >= 1024.0) {
                printf("  Data Rate  : \033[1;32m%.2f GB/s\033[0m (%.1f MB/s)\n", res->mbPerSec / 1024.0, res->mbPerSec);
            } else {
                printf("  Data Rate  : \033[1;32m%.2f MB/s\033[0m\n", res->mbPerSec);
            }
        }
        _writeAssertLine(&gResult);
        printf("\n");
    }



    inline const int32_t _skipWhitespaces(String str) {
        int idx = 0;
        while (idx < str.len && isspace(str.buff[idx])) {
            idx++;
        }
        return idx;
    }

    // Parses a single section expression like "array & pointers | math"
    inline bool parseFilterSection(String str, FilterEntry* entries, int32_t* count) {
        FilterOp nextOp    = FilterOp::NONE;

        int idx = 0;
        int entryIdx = 0;
        while (idx < str.len) {
            idx += _skipWhitespaces({ str.buff + idx, str.len - idx});
            if (idx >= str.len) break;

            const char* token = str.buff + idx;

            int tokenLen = 0;
            while (idx + tokenLen < str.len
                && token[tokenLen] != '&'
                && token[tokenLen] != '|'
                && token[tokenLen] != '^') {
                tokenLen++;
            }
            if (tokenLen == 0) return false;
            idx += tokenLen;


            idx += _skipWhitespaces({ str.buff + idx, str.len - idx });

            const char op = str.buff[idx++];
            if      (op == '&') nextOp = FilterOp::AND;
            else if (op == '|') nextOp = FilterOp::OR;
            else if (op == '^') nextOp = FilterOp::XOR;
            else                nextOp = FilterOp::NONE;

            if (entryIdx >= Filter::MAX_QUERY_ENTRIES) {
                return false;
            }

            FilterEntry* entry = entries + entryIdx;
            entry->str = { (char*) token, (uint64_t) tokenLen };
            entry->op  = nextOp;

            entryIdx++;
        }

        if ((entries + entryIdx - 1)->op != FilterOp::NONE) {
            // Last entry shall have no operator
            return false;
        }

        *count = entryIdx;
        return true;
    }

    enum FilterKind {
        FK_FILE,
        FK_SUITE,
        FK_CASE,
    };

    inline bool filterMatches(Filter* filter, FilterKind kind, String str) {
        if (!filter) return true;

        int32_t count = 0;
        const FilterEntry* entries = NULL;

        switch (kind) {
            case FK_FILE:
                count   = filter->qFileCount;
                entries = filter->qFile;
                break;
            case FK_SUITE:
                count   = filter->qSuiteCount;
                entries = filter->qSuite;
                break;
            case FK_CASE:
                count   = filter->qTestCount;
                entries = filter->qTest;
                break;
            default:
                return true;
        }

        if (count == 0) return true;
        if (!str) return false;

        bool result = Strings::icontains(str, entries[0].str);

        FilterOp op = entries[0].op;
        for (int32_t i = 1; i < count; i++) {
            bool currentMatch = Strings::icontains(str, entries[i].str);

            switch (op) {
                case FilterOp::AND:
                    result = result && currentMatch;
                    break;
                case FilterOp::OR:
                    result = result || currentMatch;
                    break;
                case FilterOp::XOR:
                    result = (result != currentMatch); // != is boolean XOR
                    break;
                default:
                    break;
            }

            op = entries[i].op;
        }

        return result;
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


    inline BenchResult run(const Test::BenchCase* const benchCase, uint64_t iterCount = 0, uint64_t warmupIter = 3) {
        BenchData data = benchCase->prep();
        uint64_t bytesPerIter = data.size;
        iterCount = iterCount == 0 ? benchCase->iterCount : iterCount;

        double* nsTimes = new double[iterCount];

        // Warmup
        for (int64_t i = 0; i < warmupIter; i++) {
            benchCase->run(&data);
        }

        _clearCurrentResult(&gResult); // TODO

        // Collect times
        auto totalStart = std::chrono::high_resolution_clock::now();

        for (int64_t i = 0; i < iterCount; i++) {
            auto start = std::chrono::high_resolution_clock::now();

            benchCase->run(&data);

            auto end = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::nano>(end - start).count();

            nsTimes[i] = elapsed;
        }

        auto totalEnd = std::chrono::high_resolution_clock::now();
        double totalElapsedSec = std::chrono::duration<double>(totalEnd - totalStart).count();

        // Resolve result
        std::sort(nsTimes, nsTimes + iterCount);

        double nsMin = nsTimes[0];
        double nsMax = nsTimes[iterCount - 1];

        double nsMed = 0.0;
        if (iterCount % 2 == 1) {
            nsMed = nsTimes[iterCount / 2];
        } else {
            nsMed = (nsTimes[(iterCount / 2) - 1] + nsTimes[iterCount / 2]) / 2.0;
        }

        double nsSum = 0.0;
        for (int64_t i = 0; i < iterCount; i++) {
            nsSum += nsTimes[i];
        }
        double nsAvg = nsSum / (double) (iterCount);

        if (gResult.assertFailed > 0) gResult.testsFailed += 1;
        else gResult.testsPassed += 1;

        // Populate Result
        BenchResult res;
        res.result     = gResult;
        res.name       = benchCase->name;
        res.iterCount  = iterCount;
        res.secElapsed = totalElapsedSec;
        res.nsMin      = nsMin;
        res.nsMax      = nsMax;
        res.nsMed      = nsMed;
        res.nsAvg      = nsAvg;
        res.mbPerSec   = 0.0;

        if (bytesPerIter > 0) {
            double bytesPerSec = ((double) (bytesPerIter) * 1e9) / nsMed;
            res.mbPerSec       = bytesPerSec / (1024.0 * 1024.0);
        }

        benchCase->release(&data);
        delete[] nsTimes;
        return res;
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

    // The value after '#test' is the expected diagnostic list: comma separated
    // 'E<code>' / 'W<code>', order matters. Optional '@line' asserts the 1-based
    // line number counted from the first line of the section.
    // TODO: handle errors
    inline File parseTestFile(const char* filepath) {
        File result = { { 0 }, NULL, NULL, NULL, 0 };

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
                name.buff = ptr + 2;
                recomputeNameLen = true;
            }

            if (std::strncmp(ptr, "#test", 5) == 0) {
                *lineStart = '\0';

                if (hasName) {
                    result.tests[testIdx].name = name;

                    hasName = false;
                    name.buff = NULL;
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
                    name.len = ptr - name.buff;
                    recomputeNameLen = false;
                }
                ptr++;
            }
        }

        return result;
    }

    // '#test "code[@line][,code[@line]]..."'
    inline bool parseExpectedErrors(const char* spec, ExpectedDiagnostic* out, int* count) {
        *count = 0;
        if (!spec) return false;

        const char* ptr = spec;
        while (*ptr && *count < gExpectedDiagnosticCount) {
            while (*ptr == ' ' || *ptr == ',') ptr++;
            if (!*ptr) break;

            ExpectedDiagnostic entry = { 0, -1, ExpectedDiagnostic::DK_ERROR, false };

            if (*ptr == 'E') {
                entry.kind = ExpectedDiagnostic::DK_ERROR;
            } else if (*ptr == 'W') {
                entry.kind = ExpectedDiagnostic::DK_WARNING;
            } else {
                return false;
            }
            ptr++;

            if (*ptr < '0' || *ptr > '9') return false;

            while (*ptr >= '0' && *ptr <= '9') {
                entry.code = entry.code * 10 + (*ptr - '0');
                ptr++;
            }

            if (*ptr == '@') {
                ptr++;

                if (*ptr < '0' || *ptr > '9') return false;

                entry.line = 0;
                while (*ptr >= '0' && *ptr <= '9') {
                    entry.line = entry.line * 10 + (*ptr - '0');
                    ptr++;
                }
                entry.hasLine = true;
            }

            out[*count] = entry;
            (*count)++;

            while (*ptr == ' ') ptr++;
            if (*ptr == ',') {
                ptr++;
                continue;
            }
            if (*ptr) return false;
        }

        return *count > 0;
    }

    // Golden file has same name, but appends _golden.txt
    // ex: test_types.qvi -> test_types_golden.txt
    inline FILE* openGoldenFile(const char* fname, const char* mode = "r") {
        if (!fname || !mode) return NULL;

        const char* sep = strrchr(fname, '/');
        const char* backslash = strrchr(fname, '\\');

        if (backslash && (!sep || backslash > sep)) {
            sep = backslash;
        }

        const char* dot = strrchr(fname, '.');
        size_t nameLen = strlen(fname);

        if (dot && (!sep || dot > sep)) {
            nameLen = (size_t)(dot - fname);
        }

        static const char suffix[] = "_golden.txt";

        char* goldenName = (char*) malloc(nameLen + sizeof(suffix));
        if (!goldenName) return NULL;

        memcpy(goldenName, fname, nameLen);
        memcpy(goldenName + nameLen, suffix, sizeof(suffix));

        FILE* file = fopen(goldenName, mode);
        free(goldenName);

        return file;
    }

    inline bool parseGoldenFile(FILE* golden, File* file, const char* fname) {
        std::fseek(golden, 0, SEEK_END);
        long size = std::ftell(golden);
        std::fseek(golden, 0, SEEK_SET);

        if (size <= 0) {
            std::fclose(golden);
            return false;
        }

        char* buf = (char*) std::malloc(size + 1);
        size_t bytesRead = std::fread(buf, 1, size, golden);
        buf[bytesRead] = '\0';
        std::fclose(golden);

        file->bufferGolden = buf;

        char* cur = strstr(buf, "===");

        for (size_t i = 0; i < file->testCount; i++) {
            file->tests[i].result = NULL;
            if (!cur) continue;

            // Skip past the "=== [...] ==="
            char* lineEnd = strchr(cur, '\n');
            if (!lineEnd) break;
            char* start = lineEnd + 1;

            // Find the next section and advance cursor
            char* next = strstr(start, "===");
            char* end = next ? next : (start + strlen(start));
            cur = next;

            while (end > start && (*(end - 1) == '\n' || *(end - 1) == '\r')) {
                end--;
            }

            if (end == start) {
                static char empty[] = "";
                file->tests[i].result = empty;
            }
            else {
                *end = '\0';
                file->tests[i].result = start;
            }
        }

        return true;
    }

    inline bool parseGoldenFile(File* file, const char* fname) {
        FILE* golden = openGoldenFile(fname, "rb");
        if (!golden) return false;

        parseGoldenFile(golden, file, fname);
    }

    inline bool generateGoldenFile(FILE* golden, const Test::Suite* suite, const Test::File* file, Test::Result* result) {
        for (size_t i = 0; i < file->testCount; i++) {
            FILE* tmpStream = std::tmpfile();
            TestArgument arg;
            arg.outStream = tmpStream;

            _clearCurrentResult(result);

            if (suite->preCaseFcn) suite->preCaseFcn(false);
            if (setjmp(gJumpBuffer) == 0) {
                suite->file->fcn(file->tests[i].data, arg, result);
            }
            if (suite->postCaseFcn) suite->postCaseFcn(false);

            long size = std::ftell(tmpStream);
            if (size > 0) {
                fprintf(golden, "=== [%.*s] ===\n", (int) file->tests[i].name.len, file->tests[i].name.buff);
                std::rewind(tmpStream);
                char buf[1024];
                size_t bytes;
                while ((bytes = std::fread(buf, 1, sizeof(buf), tmpStream)) > 0) {
                    std::fwrite(buf, 1, bytes, golden);
                }
                fprintf(golden, "\n");
            }
            std::fclose(tmpStream);
        }

        if (std::fclose(golden) != 0) {
            _writeStatusLine(":FAIL", file->name, AC_BOLD_RED);
            return false;
        } else {
            _writeStatusLine(":GEN", file->name, AC_BOLD_YELLOW);
            return true;
        }
    }

    inline bool generateGoldenFile(const Test::Suite* suite, const Test::File* file, Test::Result* result) {
        FILE* golden = openGoldenFile(suite->file->fname, "wb");
        if (!golden) {
            _writeStatusLine(":FAIL", file->name, AC_BOLD_RED);
            return false;
        }

        return generateGoldenFile(golden, suite, file, result);
    }

    struct _LineSlice {
        const char* ptr;
        size_t      len;
    };

    inline size_t _extractLineSlices(const char* start, const char* end, _LineSlice* out, size_t maxCount) {
        size_t count = 0;
        const char* cur = start;
        while (cur < end && count < maxCount) {
            const char* next = (const char*) memchr(cur, '\n', end - cur);
            size_t len = next ? (size_t)(next - cur) : (size_t)(end - cur);
            if (len > 0 && cur[len - 1] == '\r') len--;
            out[count++] = { cur, len };
            if (!next) break;
            cur = next + 1;
        }
        return count;
    }

    // Highlights only the +/- marker and the differing characters within the lines
    inline void _printPairDiff(_LineSlice del, _LineSlice add) {
        size_t cp = 0;
        while (cp < del.len && cp < add.len && del.ptr[cp] == add.ptr[cp]) {
            cp++;
        }

        size_t cs = 0;
        while (cs < (del.len - cp) && cs < (add.len - cp) &&
                del.ptr[del.len - 1 - cs] == add.ptr[add.len - 1 - cs]) {
            cs++;
        }

        // DELETED line: [-] red, common prefix plain, diff bold red, common suffix plain
        printf(AC_BOLD_RED "-" AC_RESET " %.*s", (int)cp, del.ptr);
        if (del.len > cp + cs) {
            printf(AC_BOLD_RED "%.*s" AC_RESET, (int)(del.len - cp - cs), del.ptr + cp);
        }
        printf("%.*s\n", (int)cs, del.ptr + del.len - cs);

        // ADDED line: [+] green, common prefix plain, diff bold green, common suffix plain
        printf(AC_BOLD_GREEN "+" AC_RESET " %.*s", (int)cp, add.ptr);
        if (add.len > cp + cs) {
            printf(AC_BOLD_GREEN "%.*s" AC_RESET, (int)(add.len - cp - cs), add.ptr + cp);
        }
        printf("%.*s\n", (int)cs, add.ptr + add.len - cs);
    }

    // TODO: whatever vibed thing, too sleepy to think, maybe reread and stuff
    inline void printUnifiedDiff(const char* exp, const char* act,
                                 const char* expectedLabel = "Golden",
                                 const char* actualLabel = "Actual")
    {
        if (!exp || !act) return;

        size_t expLen = strlen(exp);
        size_t actLen = strlen(act);
        while (expLen > 0 && (exp[expLen - 1] == '\n' || exp[expLen - 1] == '\r')) expLen--;
        while (actLen > 0 && (act[actLen - 1] == '\n' || act[actLen - 1] == '\r')) actLen--;

        const char* eEnd = exp + expLen;
        const char* aEnd = act + actLen;

        // 1. Walk common prefix and track up to 3 context lines
        const char* ctxBefore[3] = { nullptr, nullptr, nullptr };
        const char* curLineStart = exp;
        const char* e = exp;
        const char* a = act;

        while (e < eEnd && a < aEnd && *e == *a) {
            if (*e == '\n') {
                ctxBefore[0] = ctxBefore[1];
                ctxBefore[1] = ctxBefore[2];
                ctxBefore[2] = curLineStart;
                curLineStart = e + 1;
            }
            e++;
            a++;
        }

        const char* pExp = curLineStart;
        const char* pAct = a - (e - curLineStart);

        // 2. Walk common suffix backwards
        const char* eBack = eEnd;
        const char* aBack = aEnd;
        const char* sExp = eEnd;
        const char* sAct = aEnd;

        while (eBack > pExp && aBack > pAct && *(eBack - 1) == *(aBack - 1)) {
            eBack--;
            aBack--;
            if (*eBack == '\n') {
                sExp = eBack + 1;
                sAct = aBack + 1;
            }
        }

        if (eBack == pExp && (pExp == exp || *(pExp - 1) == '\n')) {
            sExp = pExp;
            sAct = aBack;
        } else if (aBack == pAct && (pAct == act || *(pAct - 1) == '\n')) {
            sExp = eBack;
            sAct = pAct;
        }

        printf("\n" AC_BOLD_CYAN "--- %s" AC_RESET "\n", expectedLabel);
        printf(AC_BOLD_CYAN "+++ %s" AC_RESET "\n", actualLabel);

        // Leading Context (up to 3 lines)
        for (int k = 0; k < 3; k++) {
            if (ctxBefore[k]) {
                const char* next = (const char*) memchr(ctxBefore[k], '\n', pExp - ctxBefore[k]);
                size_t len = next ? (size_t)(next - ctxBefore[k]) : (size_t)(pExp - ctxBefore[k]);
                if (len > 0 && ctxBefore[k][len - 1] == '\r') len--;
                printf(AC_BRIGHT_BLACK "  %.*s" AC_RESET "\n", (int)len, ctxBefore[k]);
            }
        }

        // 3. Middle Slice
        const char* midEndE = sExp;
        if (sExp < eEnd && midEndE > pExp && *(midEndE - 1) == '\n') {
            midEndE--;
            if (midEndE > pExp && *(midEndE - 1) == '\r') midEndE--;
        }

        const char* midEndA = sAct;
        if (sAct < aEnd && midEndA > pAct && *(midEndA - 1) == '\n') {
            midEndA--;
            if (midEndA > pAct && *(midEndA - 1) == '\r') midEndA--;
        }

        constexpr size_t MAX_DIFF_LINES = 64;
        _LineSlice expLines[MAX_DIFF_LINES];
        _LineSlice actLines[MAX_DIFF_LINES];

        size_t expCount = _extractLineSlices(pExp, midEndE, expLines, MAX_DIFF_LINES);
        size_t actCount = _extractLineSlices(pAct, midEndA, actLines, MAX_DIFF_LINES);

        if (expCount >= MAX_DIFF_LINES || actCount >= MAX_DIFF_LINES) {
            // Fallback for large block
            size_t pairs = expCount < actCount ? expCount : actCount;
            for (size_t i = 0; i < pairs; i++) _printPairDiff(expLines[i], actLines[i]);
            for (size_t i = pairs; i < expCount; i++) printf(AC_BOLD_RED "- %.*s" AC_RESET "\n", (int)expLines[i].len, expLines[i].ptr);
            for (size_t j = pairs; j < actCount; j++) printf(AC_BOLD_GREEN "+ %.*s" AC_RESET "\n", (int)actLines[j].len, actLines[j].ptr);
        } else {
            // Stack-allocated DP Table
            uint16_t dp[MAX_DIFF_LINES + 1][MAX_DIFF_LINES + 1] = {};

            for (size_t i = 0; i < expCount; i++) {
                for (size_t j = 0; j < actCount; j++) {
                    bool match = (expLines[i].len == actLines[j].len &&
                                  memcmp(expLines[i].ptr, actLines[j].ptr, expLines[i].len) == 0);
                    if (match) {
                        dp[i + 1][j + 1] = dp[i][j] + 1;
                    } else {
                        dp[i + 1][j + 1] = dp[i + 1][j] > dp[i][j + 1] ? dp[i + 1][j] : dp[i][j + 1];
                    }
                }
            }

            // Backtrack
            enum DiffKind : uint8_t { DIFF_SAME, DIFF_DEL, DIFF_ADD };
            struct Action { DiffKind kind; _LineSlice line; };
            Action actions[MAX_DIFF_LINES * 2];
            size_t actionCount = 0;

            size_t i = expCount, j = actCount;
            while (i > 0 || j > 0) {
                bool match = (i > 0 && j > 0 &&
                              expLines[i - 1].len == actLines[j - 1].len &&
                              memcmp(expLines[i - 1].ptr, actLines[j - 1].ptr, expLines[i - 1].len) == 0);
                if (i > 0 && j > 0 && match) {
                    actions[actionCount++] = { DIFF_SAME, expLines[i - 1] };
                    i--; j--;
                } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
                    actions[actionCount++] = { DIFF_ADD, actLines[j - 1] };
                    j--;
                } else if (i > 0) {
                    actions[actionCount++] = { DIFF_DEL, expLines[i - 1] };
                    i--;
                }
            }

            // Print in forward order, pairing contiguous DELs and ADDs
            size_t k = actionCount;
            while (k > 0) {
                if (actions[k - 1].kind == DIFF_SAME) {
                    printf(AC_BRIGHT_BLACK "  %.*s" AC_RESET "\n", (int)actions[k - 1].line.len, actions[k - 1].line.ptr);
                    k--;
                } else {
                    // Collect contiguous DELs and ADDs from reversed actions
                    size_t delCount = 0, addCount = 0;
                    size_t scan = k;
                    while (scan > 0 && actions[scan - 1].kind == DIFF_DEL) { delCount++; scan--; }
                    while (scan > 0 && actions[scan - 1].kind == DIFF_ADD) { addCount++; scan--; }

                    size_t pairs = delCount < addCount ? delCount : addCount;
                    for (size_t p = 0; p < pairs; p++) {
                        _printPairDiff(actions[k - 1 - p].line, actions[k - 1 - delCount - p].line);
                    }
                    for (size_t p = pairs; p < delCount; p++) {
                        printf(AC_BOLD_RED "- %.*s" AC_RESET "\n", (int)actions[k - 1 - p].line.len, actions[k - 1 - p].line.ptr);
                    }
                    for (size_t p = pairs; p < addCount; p++) {
                        printf(AC_BOLD_GREEN "+ %.*s" AC_RESET "\n", (int)actions[k - 1 - delCount - p].line.len, actions[k - 1 - delCount - p].line.ptr);
                    }
                    k = scan;
                }
            }
        }

        // Trailing Context (up to 3 lines)
        const char* s = sExp;
        for (int k = 0; k < 3 && s < eEnd; k++) {
            const char* next = (const char*) memchr(s, '\n', eEnd - s);
            size_t len = next ? (size_t)(next - s) : (size_t)(eEnd - s);
            if (len > 0 && s[len - 1] == '\r') len--;
            printf(AC_BRIGHT_BLACK "  %.*s" AC_RESET "\n", (int)len, s);
            if (!next) break;
            s = next + 1;
        }
        printf("\n");
    }

    inline void freeTestFile(File* file) {
        if (!file) return;

        std::free(file->buffer);
        std::free(file->bufferGolden);
        std::free(file->tests);

        file->buffer       = NULL;
        file->bufferGolden = NULL;
        file->tests        = NULL;
        file->testCount    = 0;
    }



    inline void _verifyGoldenStream(FILE* tmpStream, const char* expectedGolden, bool visualize) {
        if (!tmpStream) return;
        long size = std::ftell(tmpStream);
        if (size == 0 && !expectedGolden) return;

        if (!expectedGolden) {
            printf(AC_BOLD_RED "  -> Missing golden data. Run with golden generation enabled!" AC_RESET "\n");
            assert(false);
            return;
        }

        std::rewind(tmpStream);
        char* actual = (char*) std::malloc(size + 1);
        size_t readCount = std::fread(actual, 1, size, tmpStream);
        actual[readCount] = '\0';

        char* actualEnd = actual + readCount;
        while (actualEnd > actual && (*(actualEnd - 1) == '\n' || *(actualEnd - 1) == '\r')) actualEnd--;
        *actualEnd = '\0';

        // TODO: Do we keep this as we have diffs?
        if (visualize) {
            printf(actual);
        }

        if (std::strcmp(actual, expectedGolden) != 0) {
            printUnifiedDiff(expectedGolden, actual, "Golden", "Actual");
            assert(false);
        } else {
            assert(true);
        }

        std::free(actual);
    }



    inline void runTestCase(Context* ctx, const Test::Case* testCase, Test::Result* result) {
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

    inline void runTestFileToPass(Context* ctx, const Test::File* testFile, FileFcn fcn, const int idx, Test::Result* result) {
        _clearCurrentResult(result);

        const String name = testFile->tests[idx].name;

        const char* input  = testFile->tests[idx].data;
        const char* output = testFile->tests[idx].result;

        if (setjmp(gJumpBuffer) == 0) {
            if (ctx->useGolden) {
                // TODO: do something if these fails
                FILE* stream = std::tmpfile();
                fcn(input, { .outStream = stream}, result);
                _verifyGoldenStream(stream, testFile->tests[idx].result, ctx->visualize);
                std::fclose(stream);
            } else {
                captureStdoutBegin();
                fcn(input, { .output = output }, result);
                captureStdoutEnd();
                assert((const char*) gStdoutBuffer, (const char*) output);

                if (ctx->visualize) {
                    printf("%s", gStdoutBuffer);
                }
            }
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

    // TODO: ? for now we dont do anything about golden files ? Do we even want to care ?
    inline void runTestFileToFail(Context* ctx, const Test::File* testFile, FileFcn fcn, const int idx, Test::Result* result) {
        _clearCurrentResult(result);
        const String name = testFile->tests[idx].name;

        const char* input = testFile->tests[idx].data;
        const char* spec  = testFile->tests[idx].result;

        int count;
        parseExpectedErrors(spec, gExpectedDiagnostics, &count);

        if (setjmp(gJumpBuffer) == 0) {
            fcn(input, { .diag = { .count = count, .data = gExpectedDiagnostics } }, result);
        }

        if (result->currentTestFailed) {
            _writeStatusLine(":FAIL", name, AC_BOLD_RED);
            _writeAssertLine(result);
            result->testsFailed++;
        } else {
            _writeStatusLine(":OKAY", name, AC_BOLD_GREEN);
            result->testsPassed++;
        }
        fflush(stdout);
    }

    inline void runTestSuite(Test::Context* ctx, const Test::Suite* suite, Test::Result* result) {
        if (!filterMatches(ctx->filter, FK_SUITE, suite->name)) return;

        int lineLen = _writeStatusLine("SUITE", suite->name, AC_BOLD_CYAN);

        if (suite->preSuiteFcn) suite->preSuiteFcn(ctx->visualize);

        if (suite->file) {
            ctx->useGolden = suite->useGolden;

            File file = parseTestFile(suite->file->fname);
            file.name.buff = (char*) suite->file->fname;
            file.name.len = strlen(suite->file->fname);



            // Golden generation
            // ---

            FILE* golden = NULL;
            if (suite->useGolden) {
                if (gGenerateGoldens) {
                    generateGoldenFile(suite, &file, result);
                    freeTestFile(&file);
                    return;
                }

                golden = openGoldenFile(suite->file->fname);
                if (!golden) {
                    _writeStatusLine(":WARN", file.name, AC_BOLD_YELLOW);

                    if (gAutoGenerateGoldens) {
                        printf(AC_BOLD_YELLOW "  -> Golden file missing. Auto-generating baseline...\n" AC_RESET);
                        generateGoldenFile(suite, &file, result);
                        freeTestFile(&file);
                    } else {
                        printf(AC_BOLD_RED "  -> Golden file does not exist! Run with -gg to generate.\n" AC_RESET);
                    }

                    return;
                }
            }



            // Standard run
            // ---

            if (suite->useGolden) {
                parseGoldenFile(golden, &file, suite->file->fname);
            }

            for (size_t i = 0; i < file.testCount; i++) {
                if (!filterMatches(ctx->filter, FK_CASE, file.tests[i].name)) continue;

                if (suite->fileKind == CK_RUN_PASS) {
                    // In pass case we still don want to visualize
                    if (suite->preCaseFcn) suite->preCaseFcn(false);
                    runTestFileToPass(ctx, &file, suite->file->fcn, i, result);
                    if (suite->postCaseFcn) suite->postCaseFcn(false);
                } else if (suite->fileKind == CK_RUN_FAIL) {
                    if (suite->preCaseFcn) suite->preCaseFcn(ctx->visualize);
                    runTestFileToFail(ctx, &file, suite->file->fcn, i, result);
                    if (suite->postCaseFcn) suite->postCaseFcn(ctx->visualize);
                } else {
                    if (suite->preCaseFcn) suite->preCaseFcn(ctx->visualize);
                    runTestFileToPass(ctx, &file, suite->file->fcn, i, result);
                    if (suite->postCaseFcn) suite->postCaseFcn(ctx->visualize);
                }
            }

            freeTestFile(&file);
        }

        for (size_t i = 0; i < suite->caseCount; i++) {
            if (suite->preCaseFcn) suite->preCaseFcn(false);
            runTestCase(ctx, suite->cases + i, result);
            if (suite->postCaseFcn) suite->postCaseFcn(false);
        }

        if (suite->postSuiteFcn) suite->postSuiteFcn(false);

        putchar('\n');
    }

    inline void runDirectorySuites(
        const char* dir,
        Test::FileCaseKind kind,
        bool useGolden,
        Test::FileFcn fcn,
        Test::PreFcn preCase,
        Test::PostFcn postCase,
        Test::Context* ctx,
        Test::Result* res
    ) {
        if (!std::filesystem::exists(dir)) return;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".qvi") {

                std::string pathStr = entry.path().string();
                std::string nameStr = entry.path().stem().string(); // e.g. "ast_expr"

                // Drop the redundant leading "test_" (e.g. test_casts -> casts)
                const std::string prefix = "test_";
                if (nameStr.compare(0, prefix.size(), prefix) == 0) {
                    nameStr.erase(0, prefix.size());
                }

                Test::FileCase fileCase = { pathStr.c_str(), fcn };
                Test::Suite suite = {
                    nameStr.c_str(),
                    &fileCase,
                    NULL, 0,
                    preCase,
                    postCase,
                    NULL,
                    NULL,
                    kind,
                    useGolden
                };

                Test::runTestSuite(ctx, &suite, res);
            }
        }
    }

}
