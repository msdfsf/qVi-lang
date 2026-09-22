#include "../test_ete.h"



Test::FileCase gEtELoopsFileCase = {
    "test/end_to_end/to_pass/test_loops.qvi",
    gEtEFileToPass
};

extern const Test::Suite gSuiteEtELoops = {
    "EtE:Pass - Loops",
    &gEtELoopsFileCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_PASS
};
