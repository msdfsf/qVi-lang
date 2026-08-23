#pragma once
#include "../../src/syntax.h"
#include "../../src/parser.h"
#include "../../src/globals.h"
#include "../../src/registry.h"
#include "../../src/allocator.h"
#include "../../src/config.h"
#include "../test_core.h"
#include <cstring>



#define STR(lit) (String { (char*) lit, sizeof(lit) - 1 })

inline thread_local Parser::ParseContext ctx;
inline thread_local Span span;
inline thread_local QualifiedName name;

inline void gParserPreSuite() {
    allocInit();
    Type::init();
    Ast::init();

    Reg::Unit* unit = alloc<Reg::Unit>();
    unit->ast = alloc<AstContext>();
    unit->reg = alloc<AstRegistry>();
    Ast::init(unit->ast);
    Ast::init(unit->reg);
    unit->ast = unit->ast;
    unit->ast->errorCount = Config::maxErrorCount;

    Parser::init(&ctx);
    ctx.unit = unit;
    ctx.rootDir = STR(".");
    ctx.fileSpan = alloc<Span>();
}

inline void gParserPostSuite() {
    Ast::release(ctx.unit->ast);
    Ast::release(ctx.unit->reg);
    Parser::release(&ctx);
}

inline void gParserPreCase() {
}

inline void gParserPostCase() {
    DArray::clear(&ctx.nodeStack);
    DArray::clear(&ctx.defStack);
}

inline bool cmpPos(Pos a, Pos b) {
    return a.idx == b.idx && a.ln == b.ln;
}

inline void assertSpan(const Span& actualSpan, Pos start, Pos end) {
    Test::assert(cmpPos(actualSpan.start, start));
    Test::assert(cmpPos(actualSpan.end, end));
}

inline void initSpanTest(const char* input) {
    span.str = input;
    span.start = { 0, 1 };
    span.end = { -1, 1 };
    span.fileInfo = NULL;
}

inline void initQName(const char* input) {
    name.buff = (char*) input;
    name.len = strlen(input);
}
