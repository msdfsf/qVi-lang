#include "test_ete.h"



Test::FileCase gEtELoopsFileCase = {
    "test/end_to_end/test_loops.qvi",
    gEtEFile
};



extern const Test::Suite gSuiteEtELoops = {
    "EtE - Loops",
    &gEtELoopsFileCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
};
