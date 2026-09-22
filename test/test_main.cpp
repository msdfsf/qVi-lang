#pragma once
#include "test_core.h"
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

extern const Test::Suite gParserSuiteVardef;
extern const Test::Suite gParserSuiteLoop;
extern const Test::Suite gParserSuiteCase;
extern const Test::Suite gParserSuiteTypedef;
extern const Test::Suite gParserSuiteIf;
extern const Test::Suite gParserSuiteFunction;
extern const Test::Suite gParserSuitePrecedence;
extern const Test::Suite gParserSuiteExpression;
extern const Test::Suite gParserSuiteEnum;
extern const Test::Suite gParserSuiteMisc;

extern const Test::Suite gVecKernel;
extern const Test::Suite gPrintFormatSuite;

extern const Test::Suite gSuiteEtEArraySlices;
extern const Test::Suite gSuiteEtELoops;
extern const Test::Suite gSuiteEtEGetAddr;
extern const Test::Suite gSuiteEtECasts;
extern const Test::Suite gSuiteEtEFunctions;
extern const Test::Suite gSuiteEtEControlFlow;
extern const Test::Suite gSuiteEtEPrintf;
extern const Test::Suite gSuiteEtEEnums;
extern const Test::Suite gSuiteEtEStructs;

extern const Test::Suite gSuiteEtESync;



enum OptionId {
    OPT_HELP      = 0,
    OPT_E2E,
    OPT_UNIT,
    OPT_VISUALIZE,
    OPT_FILTER,

    OPT_COUNT,
};

CLI::Option options[OPT_COUNT] = {
    { OPT_HELP,      "h", "help",      "Print this help menu",         },
    { OPT_E2E,       "e", "e2e",       "Run end-to-end tests",         },
    { OPT_UNIT,      "u", "unit",      "Run unit tests",               },
    { OPT_VISUALIZE, "v", "visualize", "Visualize failure diagnostics" },
    { OPT_FILTER,    "f", "filter",    "Filter tests by pattern",      },
};
static_assert(OPT_COUNT == sizeof(options) / sizeof(CLI::Option));

enum TestKind : uint8_t {
    TK_NONE     = 0,

    TK_UNIT     = 1 << 0,
    TK_E2E_PASS = 1 << 1,
    TK_E2E_FAIL = 1 << 2,

    TK_COUNT = 3,
};

const char* str(TestKind kind) {
    switch (kind) {
        case TK_UNIT:     return "Unit";
        case TK_E2E_PASS: return "E2E:Pass";
        case TK_E2E_FAIL: return "E2E:Fail";
        default:          return "<unknown>";
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

    if (testKind == TestKind::TK_NONE) {
        testKind = (TestKind) (TK_UNIT | TK_E2E_PASS | TK_E2E_FAIL);
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

    printf(AC_BOLD_MAGENTA "Greetings" AC_RESET ", lets chill and wait for test results " AC_BOLD_MAGENTA ":3\n");
    printf(AC_BRIGHT_BLACK "==================================================\n\n" AC_RESET);

    printf(AC_BRIGHT_BLACK "Options: ");
    // TODO:

    Test::gResult = { 0 };
    Test::Context ctx = { .filter = &filter, .visualize = visualize };

    if (testKind & TK_UNIT) {
        allocInit();
        Test::runTestSuite(&ctx, &gArenaSuite, &Test::gResult);
        Test::runTestSuite(&ctx, &gDArraySuite, &Test::gResult);
        Test::runTestSuite(&ctx, &gSetSuite, &Test::gResult);
        Test::runTestSuite(&ctx, &gOrderedDictSuite, &Test::gResult);

        Test::runTestSuite(&ctx, &gLexerSuite, &Test::gResult);

        //Test::runTestSuite(&gParserSuiteVardef, &filter, &Test::gResult);
        //Test::runTestSuite(&gParserSuiteTypedef, &filter, &Test::gResult);
        //Test::runTestSuite(&gParserSuiteIf, &filter, &Test::gResult);
        //Test::runTestSuite(&gParserSuiteCase, &filter, &Test::gResult);
        //Test::runTestSuite(&gParserSuiteLoop, &filter, &Test::gResult);
        //Test::runTestSuite(&gParserSuiteExpression, &filter, &Test::gResult);
        //Test::runTestSuite(&gParserSuitePrecedence, &filter, &Test::gResult);
        //Test::runTestSuite(&gParserSuiteFunction, &filter, &Test::gResult);
        //Test::runTestSuite(&gParserSuiteMisc, &filter, &Test::gResult);

        Test::runTestSuite(&ctx, &gTypeSystemSuite, &Test::gResult);

        Test::runTestSuite(&ctx, &gVecKernel, &Test::gResult);
        Test::runTestSuite(&ctx, &gPrintFormatSuite, &Test::gResult);

        allocRelease();
    }

    if (testKind & TK_E2E_PASS) {
        Test::runTestSuite(&ctx, &gSuiteEtEPrintf, &Test::gResult);
        Test::runTestSuite(&ctx, &gSuiteEtEEnums, &Test::gResult);
        Test::runTestSuite(&ctx, &gSuiteEtEStructs, &Test::gResult);
        Test::runTestSuite(&ctx, &gSuiteEtELoops, &Test::gResult);
        Test::runTestSuite(&ctx, &gSuiteEtEArraySlices, &Test::gResult);
        Test::runTestSuite(&ctx, &gSuiteEtEGetAddr, &Test::gResult);
        Test::runTestSuite(&ctx, &gSuiteEtECasts, &Test::gResult);
        Test::runTestSuite(&ctx, &gSuiteEtEFunctions, &Test::gResult);
        Test::runTestSuite(&ctx, &gSuiteEtEControlFlow, &Test::gResult);
    }

    if (testKind & TK_E2E_FAIL) {
        Test::runTestSuite(&ctx, &gSuiteEtESync, &Test::gResult);
    }

    Test::_writeResult(&Test::gResult);

    return 0;
}
