#pragma once

// #include "test_core.h"
#include "test_harness.h"
#include <cassert>
#include <cstdint>
#include <stdio.h>
#include "../src/allocator.h"
#include "../src/base/cli.h"



extern const Test::Suite gArenaSuite;
extern const Test::Suite gDArraySuite;
extern const Test::Suite gSetSuite;
extern const Test::Suite gOrderedDictSuite;

extern const Test::Suite gTypeSystemSuite;
extern const Test::Suite gLexerSuite;

extern const Test::Suite gStringLiteralSuite;

extern const Test::Suite gVecKernel;
extern const Test::Suite gPrintFormatSuite;

extern const Test::BenchCase gBench[];
extern const int gBenchReps;

enum OptionId {
    OPT_HELP      = 0,
    OPT_E2E,
    OPT_UNIT,
    OPT_SNAPSHOT,
    OPT_VISUALIZE,
    OPT_FILTER,
    OPT_BENCH,
    OPT_GOLDEN,
    OPT_AUTO_GOLDEN,

    OPT_COUNT,
};

CLI::Option options[OPT_COUNT] = {
    { OPT_HELP,        "h", "help",             "Print this help menu",         },
    { OPT_E2E,         "e", "e2e",              "Run end-to-end tests",         },
    { OPT_UNIT,        "u", "unit",             "Run unit tests",               },
    { OPT_SNAPSHOT,    "s", "snapshot",         "Run snapshot tests",           },
    { OPT_VISUALIZE,   "v", "visualize",        "Visualize failure diagnostics" },
    { OPT_FILTER,      "f", "filter",           "Filter tests by pattern",      },
    { OPT_BENCH,       "b", "bench",            "Run benchmarks",               },
    { OPT_GOLDEN,      "gg", "generate-golden", "Generate golden files",        },
    { OPT_AUTO_GOLDEN, "ag", "autogen-golden",  "Auto Generate golden files",   },
};
static_assert(OPT_COUNT == sizeof(options) / sizeof(CLI::Option));

enum TestKind : uint8_t {
    TK_NONE          = 0,

    TK_UNIT          = 1 << 0,
    TK_E2E_PASS      = 1 << 1,
    TK_E2E_FAIL      = 1 << 2,
    TK_SNAPSHOT      = 1 << 3,

    TK_COUNT = 5,
};

const char* str(TestKind kind) {
    switch (kind) {
        case TK_UNIT:          return "Unit";
        case TK_E2E_PASS:      return "E2E:Pass";
        case TK_E2E_FAIL:      return "E2E:Fail";
        case TK_SNAPSHOT:      return "Snapshot";
        default:               return "<unknown>";
    }
}

// Parsed options
TestKind testKind   = TK_NONE;
bool visualize      = false;
Test::Filter filter = { 0 };



bool parseFilterQuery(const char* query, Test::Filter* filter) {
    if (!query || !filter) return false;

    *filter = Test::Filter();

    // Find section colons ':'
    int colons[2]  = { -1, -1 };
    int colonCount = 0;

    int idx = 0;
    while (1) {
        const char ch = query[idx];
        if (ch == '\0') break;

        if (ch == ':') {
            if (colonCount < 2) {
                colons[colonCount++] = idx;
            } else {
                return false;
            }
        }
        idx++;
    }
    const uint64_t queryLen = idx;

    // No colons: Suite filter (e.g. --filter "array & pointers")
    if (colonCount == 0) {
        String str = { (char*) query, queryLen };
        return Test::parseFilterSection(str, filter->qSuite, &filter->qSuiteCount);
    }

    // One colon: Suite:Test (e.g. "Suite:Test")
    if (colonCount == 1) {
        String str = { (char*) query, (uint64_t) colons[0] };
        if (!parseFilterSection(str, filter->qSuite, &filter->qSuiteCount)) {
            return false;
        }

        str = { (char*) query + colons[0] + 1, queryLen - colons[0] - 1 };
        return parseFilterSection(str, filter->qTest, &filter->qTestCount);
    }

    // Two colons: File:Suite:Test (e.g. "File:Suite:Test", "File::Test", "File::")
    if (colonCount == 2) {
        String str = { (char*) query, (uint64_t) colons[0] };
        if (!parseFilterSection(str, filter->qFile, &filter->qFileCount)) {
            return false;
        }

        str = { (char*) query + colons[0] + 1, queryLen - colons[0] - 1 };
        if (!parseFilterSection(str, filter->qSuite, &filter->qSuiteCount)) {
            return false;
        }

        str = { (char*) query + colons[1] + 2, queryLen - colons[1] - 2 };
        return parseFilterSection(str, filter->qTest, &filter->qTestCount);
    }

    return false;
}



int main(int argc, const char** argv) {
    IO::Stream stream = { .kind = IO::Stream::SK_C_STREAM, .cstream = stdout };

    const int argRead = CLI::parseOptions(options, OPT_COUNT, argv + 1, argc - 1);
    if (argRead != argc - 1) {
        IO::write(&stream, "Error parsing command line options!\n");
        return -1;
    }

    if (options[OPT_HELP].encountered) {
        CLI::printOptionHelp(&stream, options, OPT_COUNT);
        return 0;
    }

    if (options[OPT_UNIT].encountered) {
        testKind = (TestKind) (testKind | TK_UNIT);
    }

    if (options[OPT_GOLDEN].encountered) {
        Test::gGenerateGoldens = true;
    }

    if (options[OPT_AUTO_GOLDEN].encountered) {
        Test::gAutoGenerateGoldens = true;
    }

    if (options[OPT_E2E].encountered) {
        if (options[OPT_E2E].hasValue) {
            CLI::Value* value = &options[OPT_E2E].value;
            while (value) {
                if (strcmp(value->data, "pass") == 0) {
                    testKind = (TestKind) (testKind | TK_E2E_PASS);
                } else if (strcmp(value->data, "fail") == 0) {
                    testKind = (TestKind) (testKind | TK_E2E_FAIL);
                }

                value = value->next;
            }
        }

        if (!(testKind & TK_E2E_PASS) && !(testKind & TK_E2E_FAIL)) {
            testKind = (TestKind) (testKind | TK_E2E_PASS | TK_E2E_FAIL);
        }
    }

    if (options[OPT_SNAPSHOT].encountered) {
        testKind = (TestKind) (testKind | TK_SNAPSHOT);
    }

    if (testKind == TestKind::TK_NONE) {
        testKind = (TestKind) (TK_UNIT | TK_E2E_PASS | TK_E2E_FAIL | TK_SNAPSHOT);
    }

    if (options[OPT_VISUALIZE].encountered) {
        visualize = true;
    }

    if (options[OPT_FILTER].encountered) {
        CLI::Value* value = &options[OPT_FILTER].value;
        while (options[OPT_FILTER].hasValue && value) {
            if (!parseFilterQuery(value->data, &filter)) {
                IO::write(&stream, "Failed to parse filter query!");
                return -1;
            }
            value = value->next;
        }
    }



    if (options[OPT_BENCH].encountered) {
        printf(AC_BOLD_MAGENTA "Welcome to the club, buddy" AC_RESET ", lets bench some iron to see whose the boss of this gym" AC_BOLD_MAGENTA "<3\n");
        printf(AC_BRIGHT_BLACK "====================================================================================\n\n" AC_RESET);

        for (int i = 0; i < gBenchReps; i++) {
            const Test::BenchCase* bench = gBench + i;

            // TODO: move directly to the Test::run?
            if (!Test::filterMatches(&filter, Test::FK_SUITE, bench->name)) {
                continue;
            }

            Test::BenchResult result = Test::run(bench);
            Test::_writeBenchResult(&result);
        }

        return 0;
    }



    printf(AC_BOLD_MAGENTA "Greetings" AC_RESET ", lets chill and wait for test results " AC_BOLD_MAGENTA ":3\n");
    printf(AC_BRIGHT_BLACK "==================================================\n\n" AC_RESET);

    Test::gResult = { 0 };
    Test::Context ctx = { .filter = &filter, .visualize = visualize };

    if (testKind & TK_UNIT) {
        allocInit();
        Test::runTestSuite(&ctx, &gArenaSuite, &Test::gResult);
        Test::runTestSuite(&ctx, &gDArraySuite, &Test::gResult);
        Test::runTestSuite(&ctx, &gSetSuite, &Test::gResult);
        Test::runTestSuite(&ctx, &gOrderedDictSuite, &Test::gResult);

        Test::runTestSuite(&ctx, &gLexerSuite, &Test::gResult);
        Test::runTestSuite(&ctx, &gStringLiteralSuite, &Test::gResult);
        Test::runTestSuite(&ctx, &gTypeSystemSuite, &Test::gResult);
        Test::runTestSuite(&ctx, &gVecKernel, &Test::gResult);
        Test::runTestSuite(&ctx, &gPrintFormatSuite, &Test::gResult);

        allocRelease();
    }

    if (testKind & TK_E2E_PASS) {
        Test::runDirectorySuites("test/end_to_end/to_pass", Test::CK_RUN_PASS, false,
            gTestRunPass, gPreCaseE2E, gTestPostCase, &ctx, &Test::gResult);
    }

    if (testKind & TK_E2E_FAIL) {
        Test::runDirectorySuites("test/end_to_end/to_fail", Test::CK_RUN_FAIL, false,
            gTestRunFail, gPreCaseE2E, gTestPostCase, &ctx, &Test::gResult);
    }

    if (testKind & TK_SNAPSHOT) {
        runDirectorySuites("test/snapshots", Test::CK_SNAPSHOT, true,  gTestSnapshotAst,
            gPreCaseFrontend, gTestPostCase, &ctx, &Test::gResult);
    }

    Test::_writeResult(&Test::gResult);

    return 0;
}
