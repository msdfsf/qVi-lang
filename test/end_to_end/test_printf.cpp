#include "test_ete.h"



Test::FileCase gEtEPrintfCase = {
    "test/end_to_end/test_printf.qvi",
    gEtEFile
};



extern const Test::Suite gSuiteEtEPrintf = {
    "EtE - Print Format",
    &gEtEPrintfCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
};
