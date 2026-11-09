#include "../../src/lexer.h"
#include "../../src/parser.h"
#include "../../src/config.h"
#include "../../src/file_driver.h"
#include "../test_core.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>



// BEGIN [Lex]
// -----

struct BenchDataLex {
    char*    buffer;
    uint64_t byteCount;
    uint64_t expectedCount;
};

const int gBenchLexIterCount = 32;

// Parses the '<prefix><count>.qvi' convention used by test/bench/gen.bat.
static bool benchParseFileChecksum(const char* const fileName, const char* const prefix, uint64_t* const outChecksum) {
    const size_t prefixLen = strlen(prefix);
    if (strncmp(fileName, prefix, prefixLen) != 0) return false;

    const char* digits = fileName + prefixLen;
    char* end = NULL;
    const unsigned long long checksum = strtoull(digits, &end, 10);
    if (end == digits || strcmp(end, ".qvi") != 0) return false;

    *outChecksum = (uint64_t) checksum;
    return true;
}

// Finds the newest file in test/bench matching '<prefix>*.qvi' and returns the
// count embedded in its name.
static bool findBenchFile(const char* const prefix, std::filesystem::path* const outPath, uint64_t* const outChecksum) {
    namespace fs = std::filesystem;

    const fs::path dir = "test/bench";
    std::error_code ec;

    bool found = false;
    fs::file_time_type bestTime = {};
    uint64_t bestChecksum = 0;
    fs::path bestPath;

    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;

        const std::string name = it->path().filename().string();
        uint64_t checksum = 0;
        if (!benchParseFileChecksum(name.c_str(), prefix, &checksum)) continue;

        const fs::file_time_type time = it->last_write_time(ec);
        if (!found || time > bestTime || (time == bestTime && checksum > bestChecksum)) {
            found = true;
            bestTime = time;
            bestChecksum = checksum;
            bestPath = it->path();
        }
    }

    if (!found) return false;

    *outPath = bestPath;
    *outChecksum = bestChecksum;
    return true;
}

Test::BenchData gBenchPrepLexFile(const char* const prefix) {
    std::filesystem::path path;
    uint64_t checksum = 0;
    if (!findBenchFile(prefix, &path, &checksum)) {
        fprintf(stderr, "bench: no input file matching test/bench/%s*.qvi\n", prefix);
        return { .data = NULL };
    }

    const std::string pathStr = path.string();

    char* buffer = NULL;
    int64_t len = FileDriver::readFile(pathStr.c_str(), &buffer);
    if (len < 0) return { .data = NULL };

    BenchDataLex* meta = (BenchDataLex*) malloc(sizeof(BenchDataLex));
    meta->buffer        = buffer;
    meta->byteCount     = (uint64_t) len;
    meta->expectedCount = checksum;

    return { .data = meta, .size = (uint32_t) len };
}

Test::BenchData gBenchPrepBigFile() {
    return gBenchPrepLexFile("big_random_file_");
}

Test::BenchData gBenchPrepNumbersAll() {
    return gBenchPrepLexFile("numbers_all_");
}

Test::BenchData gBenchPrepStringsAll() {
    return gBenchPrepLexFile("strings_all_");
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
    uint64_t tokenCount = 0;

    while (1) {
        token = Lex::nextToken(str, &pos);
        if (token.token.kind == Lex::TK_END || token.token.kind == Lex::TK_ERROR) {
            break;
        }
        pos.idx += token.token.size;
        tokenCount++;
    }

    Test::assert(token.token.kind != Lex::TK_ERROR);
    Test::assert(meta->byteCount == pos.idx);
    Test::assert(tokenCount == meta->expectedCount);
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
    uint64_t numberCount = 0;
    Lex::TokenDetail detail = Lex::TD_NONE;

    while (str[pos] != '\0' && detail != Lex::TD_DT_VOID) {
        uint32_t len = 0;
        detail = Lex::parseNumber(str + pos, &len, &num);
        pos += len + 1;
        numberCount++;
    }

    Test::assert(pos == meta->byteCount);
    Test::assert(numberCount == meta->expectedCount);
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
    AllocatorMarker mark;
    uint32_t expectedRootNodes;
};

const int gBenchParserIterCount = 32;
thread_local Parser::ParseContext gParser = { 0 };

// TODO: we need bench suite system, so we can init parser only once...
Test::BenchData gBenchPrepBigFileParse() {
    std::filesystem::path path;
    uint64_t checksum = 0;
    if (!findBenchFile("big_random_file_", &path, &checksum)) {
        fprintf(stderr, "bench: no input file matching test/bench/big_random_file_*.qvi\n");
        return { .data = NULL };
    }

    const std::string pathStr = path.string();

    char* buffer = NULL;
    int64_t len = FileDriver::readFile(pathStr.c_str(), &buffer);
    if (len < 0) return { .data = NULL };

    BenchDataParser* meta = (BenchDataParser*) malloc(sizeof(BenchDataParser));
    meta->buffer            = buffer;
    meta->bufferSize        = (uint64_t) len;
    meta->expectedRootNodes = (uint32_t) checksum;

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

    // Everything allocated before this point (unit, AST, file buffer, ...)
    // must survive per-iteration resets. Everything after it is parse data.
    meta->mark = allocMark();

    return { .data = meta, .size = (uint32_t) len };
}

void gBenchReleaseDataParser(Test::BenchData* data) {
    if (!data || !data->data) return;

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

    Scope* root = gParser.unit->ast->root;
    Test::assert(root != NULL);
    if (root) {
        Test::assert(root->childrenCount == meta->expectedRootNodes);
    }

    for (int i = 0; i < (errorCount < 5 ? errorCount : 5); i++) {
        AstError* err = gParser.unit->ast->errors + i;
        printf("%.*s", (int) err->msg.len, err->msg.buff);
    }
}

void gBenchParsePre(Test::BenchData* data) {
    BenchDataParser* meta = (BenchDataParser*) data->data;
    if (!meta) return;

    // Roll back to the state captured at the end of prep so iterations
    // do not accumulate memory.
    allocRollback(meta->mark);

    // The lexer keeps a thread-local token batch that continues from the last
    // lexed token we have to re-init.
    Lex::init();

    gParser.fileSpan        = NULL;
    gParser.currentScope    = NULL;
    gParser.currentFunction = NULL;
    gParser.currentLoop     = NULL;
    gParser.currentImport   = NULL;
    gParser.importRoot      = NULL;
    gParser.foreignContext  = false;
    gParser.idxInScope      = 0;
    gParser.arrId           = 0;
    gParser.varId           = 0;
    gParser.errId           = 0;
    gParser.defId           = 0;

    DArray::clear(&gParser.nodeStack);
    DArray::clear(&gParser.defStack);
    DArray::clear(&gParser.qnameStack);

    Ast::clear(gParser.unit->ast);
    Ast::clear(gParser.unit->reg);
    gParser.unit->ast->root = NULL;

    Diag::clear();
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
        "[Parser] Big Random File",
        gBenchParserIterCount,
        gBenchParse,
        gBenchPrepBigFileParse,
        gBenchReleaseDataParser,
        gBenchParsePre,
        NULL
    }
};

extern const int gBenchReps = sizeof(gBench) / sizeof(gBench[0]);
