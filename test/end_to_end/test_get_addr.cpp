#include "test_ete.h"



Test::FileCase gEtEGetAddrCase = {
    "test/end_to_end/test_get_addr.qvi",
    gEtEFile
};



extern const Test::Suite gSuiteEtEGetAddr = {
    "EtE - Get Address",
    &gEtEGetAddrCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
};
