#include <stdio.h>
#include "test_core.h"
#include "../src/allocator.h"



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

int main() {
    printf(AC_BOLD_MAGENTA "Greetings" AC_RESET ", lets chill and wait for test results " AC_BOLD_MAGENTA ":3\n");
    printf(AC_BRIGHT_BLACK "==================================================\n\n" AC_RESET);

    allocInit();
    Test::gResult = { 0 };

    // Unit
    Test::runTestSuite(&gArenaSuite, &Test::gResult);
    Test::runTestSuite(&gDArraySuite, &Test::gResult);
    Test::runTestSuite(&gSetSuite, &Test::gResult);
    Test::runTestSuite(&gOrderedDictSuite, &Test::gResult);

    Test::runTestSuite(&gLexerSuite, &Test::gResult);

    Test::runTestSuite(&gParserSuiteVardef, &Test::gResult);
    Test::runTestSuite(&gParserSuiteTypedef, &Test::gResult);
    Test::runTestSuite(&gParserSuiteIf, &Test::gResult);
    Test::runTestSuite(&gParserSuiteCase, &Test::gResult);
    Test::runTestSuite(&gParserSuiteLoop, &Test::gResult);
    Test::runTestSuite(&gParserSuiteExpression, &Test::gResult);
    Test::runTestSuite(&gParserSuitePrecedence, &Test::gResult);
    Test::runTestSuite(&gParserSuiteFunction, &Test::gResult);
    Test::runTestSuite(&gParserSuiteMisc, &Test::gResult);

    Test::runTestSuite(&gTypeSystemSuite, &Test::gResult);

    Test::runTestSuite(&gVecKernel, &Test::gResult);
    Test::runTestSuite(&gPrintFormatSuite, &Test::gResult);

    allocRelease();


    // End-to-End
    Test::runTestSuite(&gSuiteEtEPrintf, &Test::gResult);
    Test::runTestSuite(&gSuiteEtEArraySlices, &Test::gResult);
    Test::runTestSuite(&gSuiteEtELoops, &Test::gResult);
    Test::runTestSuite(&gSuiteEtEGetAddr, &Test::gResult);
    Test::runTestSuite(&gSuiteEtECasts, &Test::gResult);
    Test::runTestSuite(&gSuiteEtEFunctions, &Test::gResult);
    Test::runTestSuite(&gSuiteEtEControlFlow, &Test::gResult);
    Test::runTestSuite(&gSuiteEtEEnums, &Test::gResult);
    Test::runTestSuite(&gSuiteEtEStructs, &Test::gResult);

    Test::_writeResult(&Test::gResult);

    return 0;
}
