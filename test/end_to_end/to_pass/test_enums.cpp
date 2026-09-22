#include "../test_ete.h"



Test::FileCase gEtEEnumsCase = {
    "test/end_to_end/to_pass/test_enums.qvi",
    gEtEFileToPass
};

extern const Test::Suite gSuiteEtEEnums = {
    "EtE:Pass - Enums",
    &gEtEEnumsCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_PASS
};
