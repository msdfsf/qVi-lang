#include "test_ete.h"



Test::FileCase gEtEArraySlicesFileCase = {
    "test/end_to_end/test_arrays_slices.qvi",
    gEtEFile
};



extern const Test::Suite gSuiteEtEArraySlices = {
    "EtE - Array/Slices",
    &gEtEArraySlicesFileCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
};
