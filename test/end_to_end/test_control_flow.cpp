#include "test_ete.h"



Test::FileCase gEtEControlFlowFileCase = {
    "test/end_to_end/test_control_flow.qvi",
    gEtEFile
};



extern const Test::Suite gSuiteEtEControlFlow = {
    "EtE - Control Flow",
    &gEtEControlFlowFileCase,
    NULL,
    0,
    gEtEPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
};