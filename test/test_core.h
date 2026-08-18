#pragma once
#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include "../src/ansi_colors.h"



static int g_totalTests  = 0;
static int g_failedTests = 0;

#define min(a, b) ((a) < (b) ? (a) : (b))

#define TEST_CASE(name) \
    Test::Case{ #name, [](Test::Result* res)



namespace Test {
    constexpr uint32_t maxAssertToDisplay = 16;

    struct Result {
        uint32_t assertPassed = 0;
        uint32_t assertFailed = 0;
        uint32_t testsPassed  = 0;
        uint32_t testsFailed  = 0;

        uint32_t currentAssertFailed = 0;
        uint32_t currentAssertPassed = 0;
        uint32_t currentAssertMask   = 0;
        bool     currentTestFailed   = false;
    };

    using Fcn = void (*)(Test::Result* res);

    using PreFcn  = void (*) ();
    using PostFcn = void (*) ();

    struct Case {
        const char* name;
        Fcn         fcn;
    };

    struct Suite {
        const char*       name;

        const Test::Case* cases;
        size_t            caseCount;

        PreFcn        preCaseFcn   = NULL;
        PostFcn       postCaseFcn  = NULL;
        PreFcn        preSuiteFcn  = NULL;
        PreFcn        postSuiteFcn = NULL;
    };

    inline thread_local Result gResult;



    inline int _writeStatusLine(const char* status, const char* name, const char* color) {
        printf("%s[%-5s]%s %s\n", color, status, AC_RESET, name);
        return 1 + 5 + 2 + (int) strlen(name);
    }

    inline void _writeAssertLine(Result* result) {
        uint32_t mask = result->currentAssertMask;

        const int assertCount = result->currentAssertFailed + result->currentAssertPassed;
        const int count = min(maxAssertToDisplay, assertCount);

        printf(AC_BOLD_RED "[%2i|%-2i] " AC_RESET, result->currentAssertPassed, count);

        for (int i = 0; i < count; i++, mask >>= 1) {
            if (mask & 1) {
                printf(AC_BOLD_RED "%c " AC_RESET, '0' + i + 1);
            } else {
                printf(AC_BOLD_GREEN "%c " AC_RESET, '0' + i + 1);
            }
        }

        if (count < assertCount) {
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



    inline void assertTrue(Test::Result* result, bool condition) {
        if (condition) {
            result->assertPassed++;
            result->currentAssertPassed++;
        } else {
            const int idx = result->currentAssertPassed + result->currentAssertFailed;
            if (idx < maxAssertToDisplay) {
                result->currentAssertMask |= 1 << idx;
            }

            result->assertFailed++;
            result->currentAssertFailed++;
            result->currentTestFailed = true;
        }
    }

    inline void assert(bool condition) {
        return assertTrue(&gResult, condition);
    }

    template<typename T1, typename T2>
    inline void assertEqual(Test::Result* result, T1 actual, T2 expected) {
        assertTrue(result, actual == expected);
    }

    template<typename T1, typename T2>
    inline void assert(T1 actual, T2 expected) {
        return assertEqual(&gResult, actual, expected);
    }



    inline void _clearCurrentResult(Test::Result* result) {
        result->currentAssertMask   = 0;
        result->currentTestFailed   = 0;
        result->currentAssertFailed = 0;
        result->currentAssertPassed = 0;
    }

    inline void runTestCase(const Test::Case* testCase, Test::Result* result) {
        _clearCurrentResult(result);

        testCase->fcn(result);

        if (result->currentTestFailed) {
            _writeStatusLine(":FAIL", testCase->name, AC_BOLD_RED);
            _writeAssertLine(result);
            result->testsFailed++;
        } else {
            _writeStatusLine(":OKAY", testCase->name, AC_BOLD_GREEN);
            result->testsPassed++;
        }
    }

    inline void runTestSuite(const Test::Suite* suite, Test::Result* result) {
        int lineLen = _writeStatusLine("SUITE", suite->name, AC_BOLD_CYAN);

        if (suite->preSuiteFcn) suite->preSuiteFcn();

        for (size_t i = 0; i < suite->caseCount; i++) {
            if (suite->preCaseFcn) suite->preCaseFcn();
            runTestCase(suite->cases + i, result);
            if (suite->postCaseFcn) suite->postCaseFcn();
        }

        if (suite->postSuiteFcn) suite->postSuiteFcn();

        putchar('\n');
    }

}
