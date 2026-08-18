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



int main() {
    printf(AC_BOLD_MAGENTA "Greetings" AC_RESET ", lets chill and wait for test results " AC_BOLD_MAGENTA ":3\n");
    printf(AC_BRIGHT_BLACK "==================================================\n\n" AC_RESET);

    Arena::Container arena;
    alc = &arena;
    initAlloc(&arena);

    Test::gResult = { 0 };

    Test::runTestSuite(&gArenaSuite, &Test::gResult);
    Test::runTestSuite(&gDArraySuite, &Test::gResult);
    Test::runTestSuite(&gSetSuite, &Test::gResult);
    Test::runTestSuite(&gOrderedDictSuite, &Test::gResult);

    Test::runTestSuite(&gTypeSystemSuite, &Test::gResult);
    Test::runTestSuite(&gLexerSuite, &Test::gResult);

    Test::_writeResult(&Test::gResult);

    return 0;
}
