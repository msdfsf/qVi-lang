#include "../test_ete.h"



Test::FileCase gEtEControlFlowFileCase = {
    "test/end_to_end/to_pass/test_control_flow.qvi",
    gEtEFileToPass
};

extern const Test::Suite gSuiteEtEControlFlow = {
    "EtE:Pass - Control Flow",
    &gEtEControlFlowFileCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_PASS
};
