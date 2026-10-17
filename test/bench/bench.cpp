#include "../../src/lexer.h"
#include "../../src/parser.h"
#include "../../src/config.h"
#include "../../src/file_driver.h"
#include "../test_core.h"
#include <cstdint>



// BEGIN [Lex]
// -----

struct BenchDataLex {
    char*    buffer;
    uint64_t tokenCount; // TODO: rename
};

const int gBenchLexIterCount = 32;

Test::BenchData gBenchPrepLexFile(const char* path) {
    char* buffer;
    int64_t len = FileDriver::readFile(path, &buffer);
    if (len < 0) return { .data = NULL };

    BenchDataLex* meta = (BenchDataLex*) malloc(sizeof(BenchDataLex));
    meta->buffer = buffer;
    meta->tokenCount = (uint64_t) len;

    return { .data = meta, .size = (uint32_t) len };
}

Test::BenchData gBenchPrepBigFile() {
    return gBenchPrepLexFile("test/bench/big_random_file.qvi");
}

Test::BenchData gBenchPrepNumbersAll() {
    return gBenchPrepLexFile("test/bench/numbers_all.qvi");
}

Test::BenchData gBenchPrepStringsAll() {
    return gBenchPrepLexFile("test/bench/strings_all.qvi");
}

void gBenchReleaseDataLex(Test::BenchData* data) {
    BenchDataLex* meta = (BenchDataLex*) data->data;
    if (meta) {
        free(meta->buffer);
        free(meta);
    }
}

inline void gBenchNextToken(Test::BenchData* data) {
    if (Test::assert(data != NULL)) return;

    BenchDataLex* meta = (BenchDataLex*) data->data;

    Pos pos = { 0, 0 };
    const char* const str = meta->buffer;

    Lex::TokenEx token = { 0 };
    //uint64_t tokenCount = 0;

    while (1) {
        token = Lex::nextToken(str, &pos);
        if (token.token.kind == Lex::TK_END || token.token.kind == Lex::TK_ERROR) {
            break;
        }
        pos.idx += token.token.size;
        //tokenCount++;
    }

    Test::assert(token.token.kind != Lex::TK_ERROR);
    Test::assert(meta->tokenCount == pos.idx);
}

inline void gBenchCharScan(Test::BenchData* data) {
    if (Test::assert(data != NULL)) return;

    BenchDataLex* meta = (BenchDataLex*) data->data;
    const char* ptr = (char*) meta->buffer;

    uint64_t whitespace = 0;

    while (*ptr != '\0') {
        char c = *ptr++;

        whitespace +=
            c == ' ' ||
            c == '\n' ||
            c == '\t';
    }

    volatile uint64_t sink = whitespace;
}

inline void gBenchParseNumber(Test::BenchData* data) {
    if (Test::assert(data != NULL)) return;

    BenchDataLex* meta = (BenchDataLex*) data->data;

    const char* const str = meta->buffer;

    uint64_t num;
    uint32_t pos = 0;
    Lex::TokenDetail detail = Lex::TD_NONE;

    while (str[pos] != '\0' && detail != Lex::TD_DT_VOID) {
        uint32_t len = 0;
        detail = Lex::parseNumber(str + pos, &len, &num);
        pos += len + 1;
    }

    Test::assert(pos == meta->tokenCount);
}

inline void gBenchParseDigitScan(Test::BenchData* data) {
    if (Test::assert(data != NULL)) return;

    BenchDataLex* meta = (BenchDataLex*) data->data;
    const char* ptr = (char*) meta->buffer;

    uint64_t digits = 0;

    while (*ptr != '\0') {
        char c = *ptr++;

        digits +=
            c == ' ' ||
            c == '\n' ||
            c == '\t';
    }

    volatile uint64_t sink = digits;
}

// -----
// END [Lex]



// BEGIN [Parser]
// -----

struct BenchDataParser {
    char*    buffer;
    uint64_t bufferSize;
    FileSystem::Handle fhnd;
};

const int gBenchParserIterCount = 32;
thread_local Parser::ParseContext gParser = { 0 };

// TODO: we need bench suite system, so we can init parser only once...
Test::BenchData gBenchPrepBigFileParse() {
    char* buffer;
    int64_t len = FileDriver::readFile("test/bench/big_random_file.qvi", &buffer);
    if (len < 0) return { .data = NULL };

    BenchDataParser* meta = (BenchDataParser*) malloc(sizeof(BenchDataParser));
    meta->buffer     = buffer;
    meta->bufferSize = (uint64_t) len;

    allocInit();
    Type::init();
    Ast::init();
    Diag::init();
    FileSystem::init();

    Reg::Unit* unit = alloc<Reg::Unit>();
    unit->ast = alloc<AstContext>();
    unit->reg = alloc<AstRegistry>();
    Ast::init(unit->ast);
    Ast::init(unit->reg);
    unit->ast = unit->ast;

    Config::opt.errorRecoveryEnabled = true;
    Config::opt.loggingEnabled = true;

    FileSystem::Handle hnd = FileSystem::loadBuffer("foo.qvi", meta->buffer, FileSystem::Origins::USER_START);

    Parser::init(&gParser);
    gParser.unit = unit;
    //gParser.rootDir = String(".");
    //gParser.fileSpan = alloc<Span>();

    meta->fhnd = hnd;
    // gParser.fileSpan->str = meta->buffer;
    // gParser.fileSpan->start = { 0, 1 };
    // gParser.fileSpan->end = { (uint32_t) 0, 1 };
    // gParser.fileSpan->fileInfo = hnd;

    return { .data = meta, .size = (uint32_t) len };
}

void gBenchReleaseDataParser(Test::BenchData* data) {
    BenchDataParser* meta = (BenchDataParser*) data->data;
    if (meta) {
        free(meta->buffer);
        free(meta);
    }

    Diag::release();
    Ast::release(gParser.unit->ast);
    Ast::release(gParser.unit->reg);
    Parser::release(&gParser);
    FileSystem::release();
}

inline void gBenchParse(Test::BenchData* data) {
    if (Test::assert(data != NULL)) return;

    BenchDataParser* meta = (BenchDataParser*) data->data;

    Err::Err err = Parser::parse(&gParser, meta->fhnd);
    Test::assert(err, Err::OK);

    const uint32_t errorCount = gParser.unit->ast->errorCount;
    Test::assert(errorCount == 0);


    for (int i = 0; i < (errorCount < 5 ? errorCount : 5); i++) {
        AstError* err = gParser.unit->ast->errors + i;
        printf("%.*s", (int) err->msg.len, err->msg.buff);
    }
}

// END [Parser]
// -----



extern const Test::BenchCase gBench[] = {
    {
        "[Lex] Reference Best Throughput (Simple Whitespace Counter)",
        gBenchLexIterCount,
        gBenchCharScan,
        gBenchPrepBigFile,
        gBenchReleaseDataLex,
    },

    {
        "[Lex] Non Batched nextToken",
        gBenchLexIterCount,
        gBenchNextToken,
        gBenchPrepBigFile,
        gBenchReleaseDataLex
    },

    {
        "[Lex] Reference Numbers Best Throughput (Simple Digit Counter)",
        gBenchLexIterCount,
        gBenchParseDigitScan,
        gBenchPrepNumbersAll,
        gBenchReleaseDataLex
    },

    {
        "[Lex] Numbers",
        gBenchLexIterCount,
        gBenchParseNumber,
        gBenchPrepNumbersAll,
        gBenchReleaseDataLex
    },

    {
        "[Lex] Strings",
        gBenchLexIterCount,
        gBenchNextToken,
        gBenchPrepStringsAll,
        gBenchReleaseDataLex
    },

    {
        "[Parser] Big Random File (28MB)",
        gBenchParserIterCount,
        gBenchParse,
        gBenchPrepBigFileParse,
        gBenchReleaseDataParser
    }
};

extern const int gBenchReps = sizeof(gBench) / sizeof(gBench[0]);
