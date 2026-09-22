#include "../test_ete.h"



Test::FileCase gEtEGetAddrCase = {
    "test/end_to_end/to_pass/test_get_addr.qvi",
    gEtEFileToPass
};



extern const Test::Suite gSuiteEtEGetAddr = {
    "EtE:Pass - Get Address",
    &gEtEGetAddrCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_PASS
};
