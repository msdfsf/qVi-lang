#pragma once
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



namespace Test {
    constexpr uint32_t maxAssertToDisplay = 16;

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
