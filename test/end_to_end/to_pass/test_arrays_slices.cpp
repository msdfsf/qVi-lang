#include "../test_ete.h"



Test::FileCase gEtEArraySlicesFileCase = {
    "test/end_to_end/to_pass/test_arrays_slices.qvi",
    gEtEFileToPass
};

extern const Test::Suite gSuiteEtEArraySlices = {
    "EtE:Pass - Array/Slices",
    &gEtEArraySlicesFileCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_PASS
};
