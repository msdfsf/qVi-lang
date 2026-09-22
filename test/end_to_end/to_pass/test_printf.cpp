#include "../test_ete.h"



Test::FileCase gEtEPrintfCase = {
    "test/end_to_end/to_pass/test_printf.qvi",
    gEtEFileToPass
};

extern const Test::Suite gSuiteEtEPrintf = {
    "EtE:Pass - Print Format",
    &gEtEPrintfCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_PASS
};
