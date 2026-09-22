#include "../test_ete.h"



Test::FileCase gEtECastsCase = {
    "test/end_to_end/to_pass/test_casts.qvi",
    gEtEFileToPass
};

extern const Test::Suite gSuiteEtECasts = {
    "EtE:Pass - Casts",
    &gEtECastsCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_PASS
};
