#include "../test_ete.h"




Test::FileCase gParserErrorsCase = {
    "test/end_to_end/to_fail/test_parser_errors.qvi",
    gEtEFileToFail
};




extern const Test::Suite gSuiteEtEParserErrors = {
    "EtE:Fail Parser - All Errors",
    &gParserErrorsCase,
    NULL,
    0,
    gEtEFailPreCase,
    gEtEPostCase,
    gEtEPreSuite,
    gEtEPostSuite,
    Test::CK_RUN_FAIL
};
