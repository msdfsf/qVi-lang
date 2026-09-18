#include "test_ete.h"



Test::FileCase gEtEStructsCase = {
    "test/end_to_end/test_structs.qvi",
    gEtEFile
};



extern const Test::Suite gSuiteEtEStructs = {
    "EtE - Structs",
    &gEtEStructsCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
};
