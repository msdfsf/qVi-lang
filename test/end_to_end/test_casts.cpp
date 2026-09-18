#include "test_ete.h"



Test::FileCase gEtECastsCase = {
    "test/end_to_end/test_casts.qvi",
    gEtEFile
};



extern const Test::Suite gSuiteEtECasts = {
    "EtE - Casts",
    &gEtECastsCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
};
