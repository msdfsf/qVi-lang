#include "test_ete.h"



Test::FileCase gEtEFunctionsFileCase = {
    "test/end_to_end/test_functions.qvi",
    gEtEFile
};



extern const Test::Suite gSuiteEtEFunctions = {
    "EtE - Functions",
    &gEtEFunctionsFileCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
};