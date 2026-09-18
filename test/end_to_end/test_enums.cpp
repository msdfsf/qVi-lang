#include "test_ete.h"



Test::FileCase gEtEEnumsCase = {
    "test/end_to_end/test_enums.qvi",
    gEtEFile
};



extern const Test::Suite gSuiteEtEEnums = {
    "EtE - Enums",
    &gEtEEnumsCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
};
