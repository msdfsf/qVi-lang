#include <stdio.h>
#include "test_core.h"
#include "../src/dynamic_arena.h"
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

int main() {
    printf(AC_BOLD_MAGENTA "Greetings" AC_RESET ", lets chill and wait for test results " AC_BOLD_MAGENTA ":3\n");
    printf(AC_BRIGHT_BLACK "==================================================\n\n" AC_RESET);

    allocInit();
    Test::gResult = { 0 };

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

    Test::_writeResult(&Test::gResult);

    return 0;
}
