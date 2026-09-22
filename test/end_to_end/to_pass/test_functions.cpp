#include "../test_ete.h"



Test::FileCase gEtEFunctionsFileCase = {
    "test/end_to_end/to_pass/test_functions.qvi",
    gEtEFileToPass
};



extern const Test::Suite gSuiteEtEFunctions = {
    "EtE:Pass - Functions",
    &gEtEFunctionsFileCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_PASS
};
