#include "../test_ete.h"



Test::FileCase gSuiteSyncCase = {
    "test/end_to_end/to_fail/test_sync.qvi",
    gEtEFileToFail
};

extern const Test::Suite gSuiteEtESync = {
    "EtE:Fail Sync - Parser Recovery",
    &gSuiteSyncCase,
    NULL,
    0,
    gEtEFailPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_FAIL
};
