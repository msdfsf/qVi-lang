#include "../test_ete.h"



Test::FileCase gEtEStructsCase = {
    "test/end_to_end/to_pass/test_structs.qvi",
    gEtEFileToPass
};



extern const Test::Suite gSuiteEtEStructs = {
    "EtE:Pass - Structs",
    &gEtEStructsCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_PASS
};
