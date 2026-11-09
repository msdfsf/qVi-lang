// === Rules, conventions and overview ===
// ====                               ====
//
// Each 'parse' function should return the last token it encountered.
// Each 'parse' function processes the input stream starting with the first token
// already consumed.
//   (We read one or more tokens first, then decide what to do. In most cases,
//    a single token is enough to decide, so we simply skip it and pass along
//    extra information about it, instead of rollback. This keeps the stack
//    smaller and prevents processing the same token twice. In case of multiple
//    tokens, we just roll back after the first one.)
//
// Each 'parse' function takes the ParseContext as the first argument and the
// parent Span as the second argument.
//   - The ParseContext tracks ambient state (current Scope, Function, stacks,
//     and IDs). Any function that modifies this state (e.g., entering a new
//     scope) must save the old state locally and restore it before returning.
//   - To ensure state restoration and span finalization, functions should
//     (if needed) use a 'defer:' label as a single exit point.
//   - The parent span's end position should be updated to match the local span
//     when the function exits.
//   - The start position of a span that the function does not own must not be modified.
//   - If previous token information is needed, it should be passed as the third
//     argument. (The type for this is not fixed and should be chosen based on needs.)
//   - If information about ending tokens is needed, it should be passed as the
//     fourth argument. If previous-token info is not required, the third argument
//     is used instead.
//   - All other arguments can be case-specific and do not need to follow any rules.
//   - To simplify function calls and signatures, it is recommended to use a
//    'flag argument' parameter to carry any additional info via flags if possible.
//
// AST nodes never use dynamic collections internally. Any node with a variable
// number of children (e.g., Scopes) must allocate memory for all of them at
// once in a single, contiguous block.
//
// During recursive parsing, buffers inside the ParseContext (nodeStack, defStack)
// are used to temporarily collect these children. Once the node is completely
// parsed, the exact required memory is allocated from the AstContext, and the
// collected data is permanently committed to the AST.
//
// This also applies to cached definition nodes used for fast symbol resolution,
// which may use its own buffer to not mess up contiguity.
//
// Errors are handled by the Diagnostics module. Because the Task System
// complicates program flow, simply returning an error is not trivial.
// Instead, the Diagnostics module manages errors by calling appropriate
// functions based on the error and configuration. Therefore
// parser doesn't need to handle error logic directly, simply report them
// via Diag::report.
//
// In 'CLI' mode, the program terminates immediately with an error message.
// In 'Error Recovery' mode, the parser performs synchronization to find
// a valid resume point. The sync function returns the token it lands on
// and is guaranteed to move at least one token forward to prevent
// getting stuck in place. Each 'parse' function syncs to the best valid
// token for its specific context and returns it. Hence, parsing avoid
// generating 'error states', which greatly simplifies the logic.
//
// In 'CLI' mode, the program terminates immediately with an error message.
// In 'Error Recovery' mode, the parser performs synchronization to find
// a valid resume point. The sync function either consumes the requested
// end token, if found, or rolls back one token when reaching a general
// synchronization point, assuming the requested end token is missing.
// In both cases, the caller simply reads the next token, making the two
// outcomes indistinguishable from its perspective. Each 'parse' function
// syncs to the best valid token for its specific context and returns it.
// Hence, parsing avoids generating 'error states', which greatly simplifies
// the logic.
//
// TODO: make spans for everything!

#include "parser.h"

#include <basetsd.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "allocator.h"
#include "array_list.h"
#include "data_types.h"
#include "config.h"
#include "tmp_string_allocator.h"
#include "globals.h"
#include "keywords.h"
#include "lexer.h"
#include "logger.h"
#include "operators.h"
#include "string.h"
#include "strlib.h"
#include "syntax.h"
#include "file_system.h"
#include "diagnostic.h"
#include "task_system.h"



namespace Parser {

    enum SyncType : uint8_t {
        // Recovery at the root of the file.
        // Looking for starting keywords (ex. fcn, def)
        ST_GLOBAL,

        // Recovery inside a scope
        // Looking for starting keywords or '}'
        ST_SCOPE,

        // Recovery to the start of execution block
        // Looking for ':', '{'
        ST_SEEK_BLOCK_BEGIN,

        // Recovery inside subscript
        // Looking for ']'
        ST_SUBSCRIPT,

        ST_FCN_NAME,
        ST_FCN_SCOPE_BEGIN,

        ST_ELSE_IF,

        ST_SCOPE_BEGIN,
    };



    // === Forward declaration ======
    // -----

    inline void offsetParentIdx(DArray::Container* vec, const int offset);
    int isArrayLHS(char* const str, Span* span);
    Variable* makeVariable(VariableDefinition* def);
    Lex::Token parseExpressionNode(ParseContext* ctx, Span* span, Lex::TokenEx tex, Variable* var, OperatorEnum prevOp, bool parsePrefix);
    Lex::TokenEx parseTypeExpression(ParseContext* ctx, Span* span, Lex::TokenEx tex, Variable* var);



    // === Scratch buffers ======
    // -----

    bool isNull(StackMark mark) {
        return mark == NULL_STACK_MARK;
    }

    // returns idx
    StackMark markStack(DArray::Container* stack) {
        return stack->size;
    }

    SyntaxNode** commitStack(DArray::Container* stack, StackMark mark, uint32_t* outCount) {
        const uint32_t count = stack->size - mark;
        *outCount = count;

        if (count == 0) return NULL;

        const uint32_t bytes = count * sizeof(SyntaxNode*);
        SyntaxNode** arr = (SyntaxNode**) alloc<char>(bytes);
        memcpy(arr, ((SyntaxNode**) stack->buffer) + mark, bytes);

        stack->size = mark;

        return arr;
    }



    // === Error related stuff ======
    // -----

    inline void logToken(Lex::TokenKind token) {
        const char* const str = Lex::str(token);
        Logger::write("'");
        Logger::write(str, cstrlen(str));
        Logger::write("'");
    }

    // returns 'true' if any token was added to the buffer
    inline bool logEndToken(ParseContext* ctx, End end) {
        if (end.a != Lex::TK_NONE) {
            logToken(end.a);
        }

        if (end.b != Lex::TK_NONE) {
            if (end.a != Lex::TK_NONE) {
                Logger::write(", ");
            }
            logToken(end.b);
        }

        return (end.a != Lex::TK_NONE || end.b != Lex::TK_NONE);
    }

    void reportLexError(ParseContext* ctx, Span* span, const Lex::Error err) {
        AstContext* ast = ctx->unit->ast;

        switch (err) {
            case Lex::ERR_UNTERMINATED_COMMENT: {
                Diag::report(ast, span, Err::UNTERMINATED_COMMENT);
                break;
            }

            case Lex::ERR_UNTERMINATED_STRING: {
                Diag::report(ast, span, Err::UNTERMINATED_STRING);
                break;
            }

            case Lex::ERR_INVALID_CHAR_ENCODING: {
                Diag::report(ast, span, Err::INVALID_CHAR_ENCODING);
                break;
            }

            case Lex::ERR_UNEXPECTED_SYMBOL: {
                Diag::report(ast, span, Err::UNEXPECTED_SYMBOL, Diag::Format { "Unexpected symbol!" });
                break;
            }

            default: {
                Diag::report(ast, span, Err::UNEXPECTED_ERROR);
                break;
            }
        }
    }



    // === Misc ======
    // -----

    inline String extractString(Lex::TokenEx tex) {
        return { tex.value.str , tex.token.size };
    }

    inline bool isEndToken(Lex::Token token, const End& end) {
        return token.kind == end.a || token.kind == end.b || token.kind == end.c;
    }

    inline void nullValue(Variable* var) {
        var->value = {};
        var->expression = NULL;
    }

    // TODO: Deprecate this id thing
    inline void assignId(Variable* var, VarId* id) {
        *id = *id & THREAD_MASK + (*id & INDEX_MASK + 1);
        var->name.id = *id;
    }

    inline void assignId(Id* dest, Id* src) {
        *src = *src & THREAD_MASK + (*src & INDEX_MASK + 1);
        *dest = *src;
    }

    inline ScopeEnd resolveScopeEnd(Lex::Token token) {
        return SE_DEFAULT; // For now we dont have 'inline' scopes
    }

    Variable* makeVariable(VariableDefinition* def) {
        Variable* var = Ast::Node::makeVariable();
        var->def = def;
        var->base.scope = def->base.scope;

        return var;
    }

    Variable* makeTypeValue(ParseContext* ctx, Type::TypeInfo* info) {
        Variable* var = Ast::Node::makeVariable();
        var->base.scope = ctx->currentScope;
        var->value.hasValue = true;
        var->value.type = Type::getInfo(Type::DT_TYPE);
        var->value.tval = info;

        return var;
    }

    void qualifiedNameToINamedEx(QualifiedName* qname, INamedEx* iname) {
        if (qname->pathSize > 0) {
            // TODO: error
        }

        iname->buff = qname->buff;
        iname->len  = qname->len;
        iname->span = qname->span;
        iname->id   = qname->id;
    }

    QualifiedName* toQualifiedName(Span* const span, Lex::TokenEx token) {
        QualifiedName* name = Ast::Node::makeQualifiedName();
        name->buff = token.value.str;
        name->len  = token.token.size;
        name->span = getSpanStamp(span);

        return name;
    }

    void annotateQualifiedName(QualifiedName* name, Lex::TokenEx token) {
        name->buff = token.value.str;
        name->len  = token.token.size;
    }

    void annotateQualifiedName(QualifiedName* name, Span* const span, Lex::TokenEx token) {
        annotateQualifiedName(name, token);
        name->span = getSpanStamp(span);
    }



    // === Import related stuff ======
    // -----

    ImportNode* initImportNode() {
        ImportNode* import = alloc<ImportNode>();
        import->file = FileSystem::null;
        import->import = NULL;
        import->parent = NULL;
        import->firstChild = NULL;
        import->nextSibling = NULL;

        return import;
    }

    bool doesImportExistInPath(ImportNode* pathNode, ImportNode* checkNode) {
        ImportNode* node = pathNode;
        while (node) {
            if (node->file == checkNode->file) {
                return true;
            }
            node = node->parent;
        }

        return false;
    }



    // === Access Point ======
    // -----

    void init(ParseContext* ctx) {
        //Lex::init();

        memset(ctx, 0, sizeof(ParseContext));

        DArray::init(&ctx->nodeStack, 1024, sizeof(void*));
        DArray::init(&ctx->defStack, 1024, sizeof(void*));

        DArray::init(&ctx->qnameStack, 32 * 2, sizeof(Lex::TokenEx));
    }

    void release(ParseContext* ctx) {
        //Lex::release();

        DArray::release(&ctx->nodeStack);
        DArray::release(&ctx->defStack);
        freeSpanStamp(ctx->fileSpan);

        DArray::release(&ctx->qnameStack);

        dealloc(ctx);
    }



    FileSystem::Path* obtainRelativePath(
        FileSystem::Path* abs,
        String root
    ) {
        using namespace FileSystem;

        Path* path = alloc<Path>();
        if (FileSystem::computeRelativePath(abs, root, path) < 0) {
            dealloc(path);
            return NULL;
        }

        return path;
    }

    // As each file is parsed as scope, result is in ctx->currentScope
    Err::Err parse(ParseContext* ctx, const FileSystem::Handle fhnd) {
        Span span;
        span.fileInfo = FileSystem::getFileInfo(fhnd);
        span.str = FileSystem::getBuffer(fhnd);
        span.start = { 0, 1 };
        span.end = { (uint32_t) Lex::INVALID_INDEX, 1 };

        span.fileInfo->relativePath = obtainRelativePath(
            span.fileInfo->absPath, ctx->rootDir);

        ctx->fileSpan = getSpanStamp(&span);

        ctx->currentScope = Ast::Node::makeScope();
        ctx->currentScope->base.scope = NULL;
        ctx->currentScope->base.span = ctx->fileSpan;
        ctx->idxInScope = 0;

        ctx->unit->ast->root = ctx->currentScope;

        Lex::Token token = parseScope(ctx, &span, SC_GLOBAL, SE_DEFAULT);
        return token.kind == Lex::TK_ERROR ? (Err::Err) -1 : Err::OK; // TODO: just remove, it can be just miss-leading, we have error in ctx
    }

    Err::Err parse(ParseContext* ctx, char* const flname) {
        return parse(ctx, FileSystem::getHandle({ flname }));
    }




    // === Sync Functions ======
    // -----

    inline Lex::TokenEx syncCore(Span* span, Lex::Token tokenA, Lex::Token tokenB = { Lex::TK_END }, bool stopOnNewline = false) {
        Lex::Token buffer[4];
        buffer[0] = { Lex::TK_STATEMENT_END };
        buffer[1] = { Lex::TK_END };
        buffer[2] = tokenA;
        buffer[3] = tokenB;

        Lex::TokenEx tex = Lex::syncToken(span, buffer, 4, stopOnNewline);

        if (Lex::match(tex.token, tokenA) || Lex::match(tex.token, tokenB)) {
            // We found our intended target.
            // We consume it leaving the parser ready for the next token.
        } else {
            // We miss the target, so we assume that the end is missing.
            // We need to rollback one token, so the parent code can
            // see it as a token after whatever the thing we synced for.
            Lex::rollback(span);
            tex = Lex::currentToken();
        }

        return tex;
    }

    Lex::TokenEx sync(Span* span, Lex::TokenKind tokenA, Lex::TokenKind tokenB = Lex::TK_END, bool stopOnNewline = false) {
        if (!Config::opt.errorRecoveryEnabled) {
            return { Lex::TK_END };
        }

        return syncCore(span, { tokenA }, { tokenB }, stopOnNewline);
    }

    Lex::TokenEx sync(Span* span, Lex::Token tokenA, Lex::Token tokenB = { Lex::TK_END }, bool stopOnNewline = false) {
        if (!Config::opt.errorRecoveryEnabled) {
            return { Lex::TK_END };
        }

        return syncCore(span, tokenA, tokenB, stopOnNewline);
    }

    Lex::TokenEx sync(Span* span, SyncType type) {
        if (!Config::opt.errorRecoveryEnabled) {
            return { Lex::TK_END };
        }

        switch (type) {

            case ST_GLOBAL:
            case ST_SCOPE: {
                return syncCore(span, { Lex::TK_STATEMENT_END }, { Lex::TK_SCOPE_END }, true);
            }

            // Seeking '{' after if/while/struct header.
            case ST_SEEK_BLOCK_BEGIN:
            case ST_SCOPE_BEGIN: {
                return syncCore(span, { Lex::TK_SCOPE_BEGIN }, { Lex::TK_END });
            }

            // Seeks ']'
            case ST_SUBSCRIPT: {
                return syncCore(span, { Lex::TK_ARRAY_END }, { Lex::TK_END });
            }

            // Seeks '(' to parse parameters
            case ST_FCN_NAME: {
                return syncCore(span, { Lex::TK_PARENTHESIS_BEGIN }, { Lex::TK_END });
            }

            // Seeking '{'
            case ST_FCN_SCOPE_BEGIN: {
                return syncCore(span, { Lex::TK_SCOPE_BEGIN }, { Lex::TK_END });
            }

            // Seeks 'else'
            case ST_ELSE_IF: {
                return syncCore(span, { Lex::TK_SCOPE_BEGIN }, { Lex::TK_END });
            }

            default: {
                break;
            }
        }

        return syncCore(span, { Lex::TK_STATEMENT_END }, { Lex::TK_SCOPE_END });
    }



    // === Main Parse Frame ======
    // -----

    Lex::Token parseStatement(ParseContext* ctx, SpanEx* span, Lex::TokenEx tex, const ScopeType type) {
        switch (tex.token.kind) {
            case Lex::TK_SCOPE_BEGIN: {
                Scope* scope = Ast::Node::makeScope();
                scope->base.scope = ctx->currentScope;

                ctx->currentScope = scope;
                tex.token = parseScope(ctx, span, SC_COMMON, SE_DEFAULT, NULL_STACK_MARK);
                ctx->currentScope = scope->base.scope;

                DArray::push(&ctx->nodeStack, (void**) &scope);
                break;
            }

            case Lex::TK_STRING: {
                tex.token = parsePrintLiteral(ctx, span, tex.token, tex.value);
                break;
            }

            case Lex::TK_KEYWORD: {
                switch ((Keyword) tex.token.detail) {
                    case KW_LOOP:     tex.token = parseLoop(ctx, span, NULL, false); break;
                    case KW_RETURN:   tex.token = parseReturnStatement(ctx, span); break;
                    case KW_CONTINUE: tex.token = parseContinueStatement(ctx, span); break;
                    case KW_BREAK:    tex.token = parseBreakStatement(ctx, span); break;
                    case KW_CASE:     tex.token = parseCaseStatement(ctx, span, NULL); break;
                    case KW_IMPORT:   tex.token = parseImport(ctx, span); break;
                    case KW_IF:       tex.token = parseIfStatement(ctx, span, NULL, false); break;
                    case KW_DEFER:    tex.token = parseDeferStatement(ctx, span); break;

                    default:
                        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_KEYWORD, "Unexpected keyword...");
                        tex = sync(span, SyncType::ST_SCOPE);
                }
                break;
            }

            case Lex::TK_DIRECTIVE: {
                tex.token = parseDirective(ctx, span);
                break;
            }

            case Lex::TK_SCOPE_RESOLUTION: {
                // Internal name
                tex = { .token = { .kind = Lex::TK_IDENTIFIER, .size = 0 }, .value = { NULL } };

                Lex::rollback(span);
                Lex::TokenEx next = Lex::peekTokenAfterQualifiedName(span);
                if (next.token.kind == Lex::TK_STATEMENT_BEGIN) {
                    tex.token = parseLabel(ctx, span, tex, End{ Lex::TK_STATEMENT_END });
                } else {
                    Lex::nextToken(span);
                    tex.token = parseBareStatement(ctx, span);
                }

                //tex.token = parseLabel(ctx, span, tex, End { Lex::TK_STATEMENT_END });
                break;
            }

            case Lex::TK_IDENTIFIER: {
                Lex::TokenEx next = Lex::peekTokenAfterQualifiedName(span);
                if (next.token.kind == Lex::TK_STATEMENT_BEGIN) {
                    tex.token = parseLabel(ctx, span, tex, End { Lex::TK_STATEMENT_END });
                } else {
                    tex.token = parseBareStatement(ctx, span);
                }
                break;
            }

            default: {
                tex.token = parseBareStatement(ctx, span);
                break;
            }
        }

        return tex.token;
    }

    // TODO: we can merge ScopeType and ScopeEnd
    Lex::Token parseScope(ParseContext* ctx, Span* span, const ScopeType type, const ScopeEnd end, StackMark dsmarkStart) {
        SpanEx    lspan  = markSpanStart(span);

        StackMark csmark = markStack(&ctx->nodeStack);
        StackMark dsmark = isNull(dsmarkStart) ? markStack(&ctx->defStack) : dsmarkStart;

        Scope* currentScope = ctx->currentScope;
        int    idxInscope   = ctx->idxInScope;

        Lex::TokenEx tex;
        Scope* node = currentScope;
        Pos prevPos = lspan.end;

        while (1) {
            tex = Lex::nextToken(&lspan);

            if (tex.token.kind == Lex::TK_END) {
                if (type == SC_GLOBAL) {
                    goto defer;
                }

                span->end = prevPos;
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_END_OF_FILE);
                tex = sync(&lspan, Lex::TK_SCOPE_END);

                goto defer;
            }

            if (tex.token.kind == Lex::TK_ERROR) {
                reportLexError(ctx, &lspan, Lex::toError(tex.token));
                tex = sync(&lspan, Lex::TK_SCOPE_END);

                goto defer;
            }

            if (tex.token.kind == Lex::TK_SCOPE_END) {
                if (type != SC_GLOBAL) goto defer;

                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "TODO: cannot close global scope");
                tex = sync(&lspan, Lex::TK_END);

                goto defer;
            }

            tex.token = parseStatement(ctx, &lspan, tex, type);
            if (end == SE_STATEMENT) goto defer;

            prevPos = lspan.end;
        }

        defer:
        ctx->currentScope = currentScope;
        ctx->idxInScope   = idxInscope;

        node->children = commitStack(&ctx->nodeStack, csmark, &node->childrenCount);
        node->definitions = commitStack(&ctx->defStack, dsmark, &node->definitionCount);
        node->base.span = finalizeSpan(&lspan, span);

        return tex.token;
    }

    // Encodes a string decoding error above the i32 range, so it can be
    // told apart from the regular Err codes
    inline int64_t stringError(uint8_t err) {
        return -0x100000000LL - err;
    }

    // target buffer have to be capable of holding result
    int64_t escapeStringLiteral(const char* const source, const uint32_t len, char* const target, uint32_t* targetLen) {
        uint32_t tmpLen = 0;

        for (uint32_t i = 0; i < len;) {
            if (source[i] == Lex::STRING_LITERAL) break;

            const int isCodePoint = Lex::isUnicodeEscape(source + i);

            const int value = Lex::parseChar(source, &i);
            if (value < 0) return value;

            if (isCodePoint) {
                tmpLen += Strings::putUtf8((uint32_t) value, (uint8_t*) target + tmpLen);
            } else {
                target[tmpLen++] = (char) value;
            }
        }

        *targetLen = tmpLen;
        return 0;
    }

    // requestedAlign 0 means auto align
    int64_t parseStringLiteral(const char* const str, const int len, const int requestedAlign, StringInitialization** initOut) {
        StringInitialization* const init = Ast::Node::makeStringInitialization();
        init->charType = Type::getInfo(Type::DT_U8);
        *initOut = init;

        tsClear();
        char* bytes = tsAlloc(len + 1, 1);
        uint32_t byteLen = 0;

        const int64_t err = escapeStringLiteral(str, len, bytes, &byteLen);
        if (err < 0) return err;

        const String raw = { (char*) bytes, byteLen };

        Strings::UTFInfo info = { 0 };
        info.elementBytesIn = (uint8_t) requestedAlign;
        info.dryRun         = true;

        info = Strings::decodeUtf8(info, raw, NULL);
        if (info.err != Strings::UTFInfo::E_NONE) return stringError(info.err);

        const int align = requestedAlign ? requestedAlign : info.elementBytesOut;

        init->charType     = Type::basicTypes + (Type::DT_U8 + (align >> 1));
        init->rawData.len  = info.length * align;
        init->rawData.buff = (char*) alloc<char>(init->rawData.len);

        info.elementBytesIn = (uint8_t) align;
        info.dryRun         = false;

        info = Strings::decodeUtf8(info, raw, init->rawData.buff);
        if (info.err != Strings::UTFInfo::E_NONE) return stringError(info.err);

        return Err::OK;
    }

    int64_t escapeStringLiteral(const char* const str, const int len, StringInitialization** initOut) {
        StringInitialization* const init = Ast::Node::makeStringInitialization();
        *initOut = init;

        tsClear();
        char* bytes = tsAlloc(len + 1, 1);
        uint32_t byteLen = 0;

        const int64_t err = escapeStringLiteral(str, len, bytes, &byteLen);
        if (err < 0) return err;

        init->charType     = Type::basicTypes + Type::DT_U8;
        init->rawData.len  = byteLen;
        init->rawData.buff = (char*) alloc<char>(init->rawData.len);

        memcpy(init->rawData.buff, bytes, byteLen);

        return Err::OK;
    }

    void reportStringLiteralError(ParseContext* ctx, Span* const span, const int64_t err) {
        // TODO
    }

    Lex::Token parseQualifiedName(ParseContext* ctx, Span* const span, Lex::TokenEx tex, QualifiedName* const qname) {
        annotateQualifiedName(qname, tex);

        tex = Lex::nextToken(span);
        if (tex.token.kind != Lex::TK_SCOPE_RESOLUTION) {
            return tex.token;
        }

        // Unlucky, we have to construct the path
        // TODO: bench if its faster to count first and then process
        //       instead of using dynamic queue
        DArray::clear(&ctx->qnameStack);

        while (1) {
            tex = Lex::nextToken(span);
            if (tex.token.kind != Lex::TK_IDENTIFIER) {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL,
                    Diag::Format {
                        "Expected an identifier after '::', but encountered %s."
                    }, Lex::str((Lex::TokenKind) tex.token.kind)
                );

                qname->pathSize = 0;
                qname->path     = nullptr;

                // TODO: do we want to sync?
                return tex.token;
            }

            DArray::push(&ctx->qnameStack, &tex);

            tex = Lex::nextToken(span);
            if (tex.token.kind != Lex::TK_SCOPE_RESOLUTION) {
                break;
            }
        }

        Lex::TokenEx* data = (Lex::TokenEx*) ctx->qnameStack.buffer;

        qname->pathSize = ctx->qnameStack.size;
        qname->path     = alloc<INamed>(qname->pathSize);

        for (int i = 0; i < qname->pathSize; i++) {
            qname->path[i].buff = data[i].value.str;
            qname->path[i].len  = data[i].token.size;
        }

        return tex.token;
    }

    Lex::Token parsePrintLiteral(ParseContext* ctx, Span* span, Lex::Token token, Lex::Value value) {
        SpanEx    lspan = markSpanStart(span);
        StackMark smark = markStack(&ctx->nodeStack);

        // TODO: move to semantics?
        ctx->unit->ast->usedFunctionMask |= (1 << Ast::Internal::IF_PRINTF);

        Function* const fcn = Ast::Internal::functions + Ast::Internal::IF_PRINTF;

        FunctionCall* call = Ast::Node::makeFunctionCall();
        call->fcn = fcn;
        call->name.buff = fcn->name.buff;
        call->name.len = fcn->name.len;

        Variable* format = Ast::Node::makeVariable();
        format->base.scope = ctx->currentScope;
        format->base.span = getSpanStamp(&lspan);
        const int64_t err = escapeStringLiteral(value.str, token.size, (StringInitialization**) &format->expression);
        if (err < 0) reportStringLiteralError(ctx, span, err);

        Variable* callWrapper = Ast::Node::makeVariable();
        callWrapper->base.scope = ctx->currentScope;
        callWrapper->expression = (Expression*) call;

        DArray::push(&ctx->nodeStack, &format);
        token = parseList(ctx, &lspan, Lex::TK_LIST_SEPARATOR, Lex::TK_STATEMENT_END);
        call->inArgs = (Variable**) commitStack(&ctx->nodeStack, smark, &call->inArgCount);

        DArray::push(&ctx->nodeStack, &callWrapper); // scope

        callWrapper->base.span = finalizeSpan(&lspan, span);

        return token;
    }

    // === Annotations ======
    // -----
    //
    // Annotations are single-quoted literals placed right before a body:
    //   struct '#packed' { ... }
    //   (x: i32) -> i32 '#inline' { ... }
    //   import '#abi(C)' ucrtbase as LibC: { ... }
    //
    // Every literal carries one '#name' with optional parenthesized arguments
    // (for example '#abi(C)'). Several literals may follow each other, with an
    // optional comma between them.

    struct Annotation {
        String name;
        String arg;
        Span*  span;
    };

    enum AnnotationTarget : uint8_t {
        AT_RECORD,
        AT_FUNCTION,
        AT_IMPORT,
    };

    inline bool isAnnotationSpace(const char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    }

    inline bool isAnnotationNameStart(const char ch) {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
    }

    inline bool isAnnotationNameChar(const char ch) {
        return isAnnotationNameStart(ch) || (ch >= '0' && ch <= '9');
    }

    // 'span' has to cover the annotation literal token itself.
    bool parseAnnotation(ParseContext* ctx, Span* span, Annotation* out) {
        const char* const text = span->str + span->start.idx + 1;
        int len = (int) span->end.idx - (int) span->start.idx - 2;
        if (len < 0) len = 0;

        int idx = 0;
        while (idx < len && isAnnotationSpace(text[idx])) idx++;

        if (idx >= len || text[idx] != '#') {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL,
                "Annotation has to start with '#' (for example '#packed').");
            return false;
        }

        idx++;
        const int nameStart = idx;

        if (idx >= len || !isAnnotationNameStart(text[idx])) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL,
                "Annotation name expected after '#'.");
            return false;
        }

        while (idx < len && isAnnotationNameChar(text[idx])) idx++;

        out->name = { (char*) text + nameStart, (uint64_t) (idx - nameStart) };
        out->arg  = {};
        out->span = getSpanStamp(span);

        while (idx < len && isAnnotationSpace(text[idx])) idx++;

        if (idx < len) {
            if (text[idx] != '(') {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL,
                    "Unexpected text after the annotation name.");
                return false;
            }

            idx++;
            int argStart = idx;
            int depth    = 1;

            while (idx < len && depth > 0) {
                if (text[idx] == '(') {
                    depth++;
                } else if (text[idx] == ')') {
                    depth--;
                    if (depth == 0) break;
                }

                idx++;
            }

            if (depth != 0) {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL,
                    "')' expected after the annotation arguments.");
                return false;
            }

            int argEnd = idx;
            idx++;

            while (argStart < argEnd && isAnnotationSpace(text[argStart])) argStart++;
            while (argEnd > argStart && isAnnotationSpace(text[argEnd - 1])) argEnd--;

            out->arg = { (char*) text + argStart, (uint64_t) (argEnd - argStart) };

            while (idx < len && isAnnotationSpace(text[idx])) idx++;
        }

        if (idx != len) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL,
                "Unexpected text after the annotation.");
            return false;
        }

        return true;
    }

    void applyAnnotation(ParseContext* ctx, const Annotation& annotation, AnnotationTarget target, void* node) {
        if (target == AT_RECORD) {
            RecordExpression* record = (RecordExpression*) node;

            if (Strings::compare(annotation.name, Lex::CDS_PACKED)) {
                record->flags |= SF_PACKED;
            } else if (Strings::compare(annotation.name, Lex::CDS_REORDER)) {
                record->flags |= SF_REORDER;
            }

            return;
        }

        if (Strings::compare(annotation.name, "abi")) {
            if (annotation.arg.len == 0) {
                Diag::report(ctx->unit->ast, annotation.span, Err::INVALID_LANGUAGE_TAG,
                    "ABI annotation requires a tag, for example '#abi(C)'.");
                return;
            }

            if (target == AT_FUNCTION) {
                Function* fcn = (Function*) node;
                fcn->code.tagStr = annotation.arg;
                fcn->code.tagLoc = annotation.span;
            } else {
                ImportStatement* import = (ImportStatement*) node;
                import->tag = annotation.arg;
            }
        }
    }

    // 'tex' is already consumed. Returns the last token after all annotations.
    Lex::TokenEx parseAnnotations(ParseContext* ctx, Span* span, Lex::TokenEx tex, AnnotationTarget target, void* node) {
        // The caller may have consumed the annotation through a nested parser
        // whose local span is not propagated back. Re-read it once so 'span'
        // describes the literal and its text can be extracted.
        if (tex.token.kind == Lex::TK_CHAR) {
            Lex::rollback(span);
            tex = Lex::nextToken(span);
        }

        while (tex.token.kind == Lex::TK_CHAR) {
            Annotation annotation;
            if (parseAnnotation(ctx, span, &annotation)) {
                applyAnnotation(ctx, annotation, target, node);
            }

            tex = Lex::nextToken(span);

            // Allow both '#packed' '#reorder' and '#packed', '#reorder'.
            if (tex.token.kind == Lex::TK_LIST_SEPARATOR && Lex::peekToken(span).token.kind == Lex::TK_CHAR) {
                tex = Lex::nextToken(span);
            }
        }

        return tex;
    }

    // DEPRECATE
    Lex::Token parseLanguageTag(ParseContext* ctx, Span* span, String* tag) {
        Lex::TokenEx token;

        token = Lex::nextToken(span);
        if (token.token.kind != Lex::TK_IDENTIFIER) {
            Diag::report(ctx->unit->ast, span, Err::INVALID_LANGUAGE_TAG,
                Diag::Format {
                    "Language tag must be a plain identifier!"
                }
            );
            return sync(span, Lex::TK_ARRAY_END).token;
        }

        tag->buff = token.value.str;
        tag->len  = token.token.size;

        token = Lex::nextToken(span);
        if (token.token.kind != Lex::TK_ARRAY_END) {
            Diag::report(ctx->unit->ast, span, Err::INVALID_LANGUAGE_TAG,
                Diag::Format {
                    "'%s' expected after language tag!"
                }, Lex::str(Lex::TK_ARRAY_END)
            );
            return sync(span, Lex::TK_ARRAY_END).token;
        }

        return token.token;
    }

    Lex::Token parseForeignScope(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        String tag;
        Lex::TokenEx token;

        token.token = parseLanguageTag(ctx, &lspan, &tag);
        if (token.token.kind == Lex::TK_ARRAY_END) {
            token = Lex::nextToken(&lspan);
        }

        if (token.token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, Lex::str(Lex::TK_SCOPE_BEGIN));
            token = sync(&lspan, SyncType::ST_GLOBAL);
            span->end = lspan.end;
            return token.token;
        }

        const char* const codeStr = span->str + span->end.idx;
        const int codeLen = Lex::findBlockEnd(&lspan, Lex::SCOPE_BEGIN, Lex::SCOPE_END);
        if (codeLen < 0) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_END_OF_FILE);
            token = { Lex::TK_END };
        }

        CodeBlock* codeBlock = Ast::Node::makeCodeBlock();
        codeBlock->code.tagStr = String(tag.buff, tag.len);
        codeBlock->code.codeStr = String((char*) codeStr, codeLen);

        codeBlock->base.span = finalizeSpan(&lspan, span);

        return token.token;
    }

    // Expects 'tex' to be identifier
    // TODO: we basically dont need qname...
    Lex::Token parseLabel(ParseContext* ctx, Span* span, Lex::TokenEx tex, End end) {
        QualifiedName* qname = NULL;

        if (tex.token.kind == Lex::TK_IDENTIFIER) {
            qname = alloc<QualifiedName>();
            tex.token = parseQualifiedName(ctx, span, tex, qname);
        }

        if (!qname || tex.token.kind != Lex::TK_STATEMENT_BEGIN) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL,
                Diag::Format { "Expected a label/declaration name followed by ':', but encountered %s." },
                Lex::str((Lex::TokenKind) tex.token.kind)
            );
            return sync(span, SyncType::ST_GLOBAL).token;
        }

        return parseDeclarationBody(ctx, span, qname, end);
    }

    // Parses everything that may follow label ('name:')
    Lex::Token parseDeclarationBody(ParseContext* ctx, Span* span, QualifiedName* name, End end) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex = Lex::nextToken(&lspan);

        if (tex.token.kind == Lex::TK_KEYWORD) {
            switch (tex.token.detail) {
                case KW_CASE:
                    return parseCaseStatement(ctx, span, name);
                case KW_LOOP:
                    return parseLoop(ctx, span, name, false);
                case KW_IF:
                    return parseIfStatement(ctx, span, name, false);
                default:
                    return parseVarDefinition(ctx, span, tex, name, Ast::Node::makeVariableDefinition(), end);
            }
        } else if (tex.token.kind == Lex::TK_PARENTHESIS_BEGIN) {
            return parseFunction(ctx, span, name);
        } else if (tex.token.kind == Lex::TK_SCOPE_BEGIN) {
            return parseNamespace(ctx, span, name);
        } else if (tex.token.kind == Lex::TK_DIRECTIVE) {
            tex = Lex::nextToken(span);

            if (Lex::isKeyword(tex.token, KW_LOOP)) {
                return parseLoop(ctx, span, name, true);
            }

            if (Lex::isKeyword(tex.token, KW_IF)) {
                return parseIfStatement(ctx, span, name, true);
            }
        } else {
            return parseVarDefinition(ctx, span, tex, name, Ast::Node::makeVariableDefinition(), end);
        }
    }

    // assignment or expression
    Lex::Token parseBareStatement(ParseContext* ctx, Span* span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex;

        Variable* lvar = NULL;
        End endToken = { Lex::TK_STATEMENT_END, Lex::TK_EQUAL };

        // Parse the left side (lvalue or just a standalone expression)
        Lex::rollback(&lspan);
        tex.token = parseExpression(ctx, &lspan, &lvar, endToken, ALLOW_UNEXPECTED_END);

        if (tex.token.kind == Lex::TK_STATEMENT_END) {
            // It's just a bare expression statement (do_something();)
            Statement* stmt = Ast::Node::makeStatement();
            stmt->base.scope = ctx->currentScope;
            stmt->operand = lvar;

            DArray::push(&ctx->nodeStack, (void**)&stmt);
            DArray::push(&ctx->unit->reg->statements, (void**)&stmt);

            stmt->base.span = finalizeSpan(&lspan, span);
            return tex.token;
        }

        // Ensure we hit an '=' or compound one if it's not a statement end
        OperatorEnum op = OP_NONE;
        if (tex.token.kind != Lex::TK_EQUAL) {
            if (Lex::isOperator(tex.token) && Lex::peekToken(&lspan).token.kind == Lex::TK_EQUAL) {
                op = Lex::toBinaryOperator(tex.token);
                tex = Lex::nextToken(&lspan);
            } else {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL,
                    Diag::Format { "Expected ';' to end statement or '=' for assignment, but encountered %s." },
                    Lex::str((Lex::TokenKind)tex.token.kind)
                );
                tex = sync(&lspan, SyncType::ST_GLOBAL);
                return tex.token;
            }
        }

        // It is an assignment (x = 5)
        Variable* rvar = Ast::Node::makeVariable();
        tex.token = parseRValue(ctx, &lspan, rvar, endToken);

        VariableAssignment* varAssignment = Ast::Node::makeVariableAssignment();
        varAssignment->base.scope = ctx->currentScope;
        varAssignment->lvar = lvar;
        varAssignment->rvar = rvar;
        varAssignment->op = op;

        DArray::push(&ctx->nodeStack, (void**)&varAssignment);

        varAssignment->base.span = finalizeSpan(&lspan, span);
        return tex.token;
    }

    Lex::Token parseAssignment(ParseContext* ctx, Span* span, const End endToken) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;

        VariableAssignment* const varAssignment = Ast::Node::makeVariableAssignment();
        varAssignment->base.span = span;
        varAssignment->base.scope = ctx->currentScope;
        varAssignment->rvar = Ast::Node::makeVariable();
        varAssignment->rvar->base.scope = ctx->currentScope;

        Lex::rollback(&lspan);
        token = parseExpression(ctx, &lspan, &(varAssignment->lvar), End { Lex::TK_EQUAL }, NULL_FLAG);

        if (token.kind != Lex::TK_EQUAL) {
            uint64_t val = Lex::toIntStr(Lex::EQUAL);
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, &val);
            token = sync(&lspan, Lex::TK_EQUAL).token;
        }

        token = parseRValue(ctx, &lspan, varAssignment->rvar, endToken);

        DArray::push(&ctx->nodeStack, (void*) &varAssignment);

        varAssignment->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    // Parses the body of a variable definition.
    // 'Type [= Init]'
    Lex::Token parseVarDefinition(ParseContext* ctx, Span* span, Lex::TokenEx tex, QualifiedName* name, VariableDefinition* def, const End end) {
        SpanEx lspan = markSpanStart(span);

        def->base.scope = ctx->currentScope;
        def->var = makeVariable(def);

        if (name) {
            def->var->name.buff = name->buff;
            def->var->name.len  = name->len;
            def->var->name.span = getSpanStamp(&lspan);
            def->var->name.pathSize = name->pathSize;
            def->var->name.path     = name->path;
        }

        if (tex.token.kind == Lex::TK_EQUAL) {
            // the type slot defaults to the metatype
            def->typeValue = makeTypeValue(ctx, Type::getInfo(Type::DT_TYPE));
            tex.token = parseExpression(ctx, &lspan, def->var, end);
        } else {
            def->typeValue = Ast::Node::makeVariable();
            def->typeValue->base.scope = ctx->currentScope;

            // Type slots may carry postfix type forms such as the
            // parameterized type 'Vec2(i32)', so run the type expression
            // through the node parser (type grammar + postfix operators).
            tex = Lex::TokenEx {
                .token = parseExpressionNode(ctx, &lspan, tex, def->typeValue, OP_NONE, false),
                .value = {},
            };

            if (tex.token.kind == Lex::TK_EQUAL) {
                tex.token = parseExpression(ctx, &lspan, def->var, end);
            }
        }

        if (isEndToken(tex.token, end)) {
            DArray::push(&ctx->defStack, &def);
            DArray::push(&ctx->nodeStack, &def);

            def->base.span = finalizeSpan(&lspan, span);
        } else {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL,
                "Expected end of declaration, but encountered %s.",
                Lex::str((Lex::TokenKind) tex.token.kind));
            const Lex::TokenKind endKind = end.a != Lex::TK_NONE ? end.a : Lex::TK_STATEMENT_END;
            tex.token = sync(&lspan, endKind).token;
            def->base.span = finalizeSpan(&lspan, span);
        }

        return tex.token;
    }

    // Parameter names are optional, defaults and the '...' tail are supported.
    // a : i32 = 5, b : i32, [const]u8, args : ...)
    Lex::Token parseParameterList(ParseContext* ctx, Span* const span, FunctionPrototype* out, Scope* paramScope) {
        SpanEx lspan = markSpanStart(span);

        Scope* currentScope = ctx->currentScope;
        ctx->currentScope = paramScope;

        StackMark csmark = markStack(&ctx->nodeStack);
        StackMark dsmark  = markStack(&ctx->defStack);

        Lex::TokenEx tex = Lex::nextToken(&lspan);

        while (tex.token.kind != Lex::TK_PARENTHESIS_END) {
            VariableDefinition* def = Ast::Node::makeVariableDefinition();
            def->base.scope = paramScope;
            def->base.flags |= IS_UNORDERED;

            if (tex.token.kind == Lex::TK_VARIADIC) {
                // '...'
                def->base.flags |= IS_VARIADIC;
                def->typeValue = alloc<Variable>();
                def->typeValue->value.type = Type::getInfo(Type::DT_MULTIPLE_TYPES);

                tex = Lex::nextToken(&lspan);
            } else if (tex.token.kind == Lex::TK_IDENTIFIER &&
                Lex::peekToken(&lspan).token.kind == Lex::TK_STATEMENT_BEGIN) {
                // 'name : Type [= default]'
                QualifiedName name = {};
                name.buff = tex.value.str;
                name.len = tex.token.size;
                name.span = getSpanStamp(&lspan);

                Lex::nextToken(&lspan);

                tex = Lex::nextToken(&lspan);
                tex.token = parseVarDefinition(ctx, &lspan, tex, &name, def,
                    End { Lex::TK_LIST_SEPARATOR, Lex::TK_PARENTHESIS_END });
            } else if (tex.token.kind == Lex::TK_SKIP &&
                Lex::peekToken(&lspan).token.kind == Lex::TK_STATEMENT_BEGIN) {
                // infered type

                Lex::nextToken(&lspan);

                tex = Lex::nextToken(&lspan);
                tex.token = parseVarDefinition(ctx, &lspan, tex, NULL, def,
                    End{ Lex::TK_LIST_SEPARATOR, Lex::TK_PARENTHESIS_END });
            } else {
                // unnamed parameter -> type only
                tex.token = parseVarDefinition(ctx, &lspan, tex, NULL, def,
                    End { Lex::TK_LIST_SEPARATOR, Lex::TK_PARENTHESIS_END });
            }

            if (tex.token.kind == Lex::TK_PARENTHESIS_END) break;
            if (tex.token.kind == Lex::TK_LIST_SEPARATOR) {
                tex = Lex::nextToken(&lspan);
                continue;
            }

            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL,
                "',' or ')' expected in the parameter list.");
            tex = sync(&lspan, { Lex::TK_LIST_SEPARATOR, Lex::TK_PARENTHESIS_END });
            if (tex.token.kind != Lex::TK_LIST_SEPARATOR) break;
        }

        out->inArgs = (VariableDefinition**) commitStack(&ctx->nodeStack, csmark, &out->inArgCount);
        paramScope->definitions = commitStack(&ctx->defStack, dsmark, &paramScope->definitionCount);

        ctx->currentScope = currentScope;

        return tex.token;
    }

    Lex::Token parseFunctionSignature(ParseContext* ctx, Span* const span, FunctionPrototype* out, Scope* paramScope, QualifiedName** errorSetOut) {
        SpanEx lspan = markSpanStart(span);

        // Returns the consumed ')'. Advance to the first token after the
        // parameter list before inspecting the optional clauses.
        Lex::Token token = parseParameterList(ctx, &lspan, out, paramScope);
        token = Lex::nextToken(&lspan).token;

        // Optional error set: 'using FileError'
        if (Lex::isKeyword(token, KW_USING)) {
            if (!errorSetOut) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_KEYWORD,
                    "Function types can not declare an error set.");
            }

            Lex::TokenEx tex = Lex::nextToken(&lspan);
            if (tex.token.encoded < 0) return tex.token;

            if (tex.token.kind != Lex::TK_IDENTIFIER) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNKNOWN_ERROR_SET,
                    "Error set name expected after 'using'.");
            } else {
                QualifiedName* errName = alloc<QualifiedName>();
                token = parseQualifiedName(ctx, &lspan, tex, errName);

                if (errorSetOut) *errorSetOut = errName;
            }
        }

        // Optional return: '-> Ret'
        out->outArg = NULL;

        if (token.kind == Lex::TK_OP_ARROW) {
            VariableDefinition* def = Ast::Node::makeVariableDefinition();
            def->base.scope = paramScope;
            def->typeValue = Ast::Node::makeVariable();
            def->typeValue->base.scope = ctx->currentScope;

            Lex::TokenEx tex = Lex::nextToken(&lspan);
            token = parseExpressionNode(ctx, span, tex, def->typeValue, OP_NONE, false);

            out->outArg = def;
        }

        return token;
    }

    Lex::Token parseArrayInitialization(ParseContext* ctx, Span* span, ArrayInitialization** initOut) {
        *initOut = Ast::Node::makeArrayInitialization();
        (*initOut)->literalBase = NULL; // filled during postfix operator parsing
        StackMark smark = markStack(&ctx->nodeStack);
        Lex::Token token = parseList(ctx, span, Lex::TK_LIST_SEPARATOR, Lex::TK_ARRAY_END);
        (*initOut)->attributes = (Variable**) commitStack(&ctx->nodeStack, smark, &(*initOut)->attributeCount);
        return token;
    }

    Lex::Token parseTypeInitialization(ParseContext* ctx, Span* const span, TypeInitialization** outTypeInit) {
        SpanEx    lspan = markSpanStart(span);
        StackMark smark = markStack(&ctx->nodeStack);

        Lex::TokenEx tex;

        TypeInitialization* typeInit = Ast::Node::makeTypeInitialization();
        typeInit->literalBase = NULL; // filled during postfix operator parsing
        typeInit->fillVar = NULL;

        tex = Lex::nextToken(&lspan);

        if (tex.token.kind == Lex::TK_SCOPE_END) {
            typeInit->attributes = (Variable**) commitStack(&ctx->nodeStack, smark, &typeInit->attributeCount);
            typeInit->idxs = (int*) alloc<char>(1);

            *outTypeInit = typeInit;

            span->start = lspan.super;
            span->end = lspan.end;
            return tex.token;
        }

        Lex::Token nextToken = Lex::peekToken(&lspan).token;

        const bool namedInit =
            tex.token.kind == Lex::TK_THE_REST ||
            (
                tex.token.kind == Lex::TK_IDENTIFIER &&
                nextToken.kind == Lex::TK_STATEMENT_BEGIN
            ) ||
            tex.token.kind == Lex::TK_OP_MEMBER_SELECTION;

        if (namedInit) {
            // names assumed
            bool hasFillVar = false;
            typeInit->fillVar = NULL;

            while (1) {
                Variable* var;

                if (tex.token.kind == Lex::TK_THE_REST) {
                    if (hasFillVar) {
                        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Fill the rest symbol can be used only once per initialization!");
                        tex = sync(&lspan, Lex::TK_SCOPE_END);
                        break;
                    }

                    var = Ast::Node::makeVariable();
                    var->base.scope = ctx->currentScope;
                    var->base.span = getSpanStamp(&lspan);

                    typeInit->fillVar = var;
                    hasFillVar = true;
                } else {
                    if (tex.token.kind != Lex::TK_IDENTIFIER) {
                        Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "Attribute name expected!");
                        tex = sync(&lspan, Lex::TK_SCOPE_END);
                        break;
                    }

                    var = Ast::Node::makeVariable();
                    var->base.scope = ctx->currentScope;
                    var->base.span = getSpanStamp(&lspan);
                    var->name.buff = tex.value.str;
                    var->name.len = tex.token.size;
                    var->name.span = var->base.span;
                    DArray::push(&ctx->nodeStack, &var);
                }

                tex = Lex::nextToken(&lspan);
                if (tex.token.kind != Lex::TK_STATEMENT_BEGIN && tex.token.kind != Lex::TK_EQUAL) {
                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "':' or '=' expected in field initializer!");
                    tex = sync(&lspan, { Lex::TK_STATEMENT_BEGIN, Lex::TK_EQUAL, Lex::TK_SCOPE_END });
                }

                tex.token = parseExpression(ctx, &lspan, var, End { Lex::TK_LIST_SEPARATOR, Lex::TK_SCOPE_END });

                if (tex.token.kind == Lex::TK_SCOPE_END) break;
                tex = Lex::nextToken(&lspan);
            }
        } else {
            // positional args
            Lex::rollback(&lspan);
            while (1) {
                Variable* var = NULL;
                tex.token = parseExpression(ctx, &lspan, &var, End { Lex::TK_LIST_SEPARATOR, Lex::TK_SCOPE_END, Lex::TK_THE_REST });

                if (var) DArray::push(&ctx->nodeStack, &var);
                if (tex.token.kind == Lex::TK_SCOPE_END) break;

                if (Lex::peekToken(&lspan).token.kind == Lex::TK_THE_REST) {
                    tex = Lex::nextToken(&lspan);

                    tex = Lex::peekToken(&lspan);
                    if (tex.token.kind != Lex::TK_STATEMENT_BEGIN) {
                        tex = sync(&lspan, Lex::TK_SCOPE_END);
                        break;
                    }
                    tex = Lex::nextToken(&lspan);

                    tex.token = parseExpression(ctx, &lspan, &var, End{ Lex::TK_SCOPE_END });
                    typeInit->fillVar = var;

                    if (tex.token.kind == Lex::TK_SCOPE_END) break;

                    // 'the rest' has to be last
                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "TODO");
                    tex = sync(&lspan, Lex::TK_SCOPE_END);
                    break;
                }
            }
        }
        typeInit->attributes = (Variable**) commitStack(&ctx->nodeStack, smark, &typeInit->attributeCount);
        typeInit->idxs = (int*) alloc<char>(typeInit->attributeCount * sizeof(int));

        *outTypeInit = typeInit;

        span->start = lspan.super;
        span->end = lspan.end;

        return tex.token;
    }

    Lex::Token parseRValue(ParseContext* ctx, Span* const span, Variable* outVar, const End endToken) {
        outVar->base.scope = ctx->currentScope;
        Lex::Token token = parseExpression(ctx, span, outVar, endToken);

        return token;
    }

    // NOTE: We isolate the signature in `paramScope` so prototypes can exist without a body.
    //       Only `defStack` is populated here, providing a dedicated lookup scope for clean,
    //       automatic semantic checks. The parameter nodes themselves live directly inside the
    //       `Function` node, serving as the single source of truth for AST visualization and
    //       interpreting.
    //
    //       Parameter definitions are also pushed to `bodyScope` to enable local symbol lookup
    //       and detect variable shadowing.
    //
    Lex::Token parseFunction(ParseContext* ctx, Span* const span, QualifiedName* name) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex;

        Function* fcn = Ast::Node::makeFunction();

        fcn->base.flags |= ctx->foreignContext ? IS_EXTERN : 0;
        fcn->base.scope = ctx->currentScope;
        fcn->errorSetName = NULL;
        fcn->bodyScope = NULL;

        if (!name) {
            fcn->name.buff = NULL;
            fcn->name.len  = 0;
            fcn->name.span = NULL;
        } else {
            fcn->name = *name;
            fcn->name.span = getSpanStamp(&lspan);
        }

        Scope* currentScope = ctx->currentScope;
        ctx->currentFunction = fcn;

        Scope* paramScope = Ast::Node::makeScope();
        paramScope->base.scope = currentScope;

        tex.token = parseFunctionSignature(ctx, &lspan, &fcn->prototype, paramScope, &fcn->errorSetName);
        tex = parseAnnotations(ctx, &lspan, tex, AT_FUNCTION, fcn);

        if (tex.token.kind == Lex::TK_STATEMENT_END) {
            fcn->base.span = finalizeSpan(&lspan, span);
            goto defer;
        }

        if (tex.token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "'{' expected for function body.");
            tex = sync(&lspan, SyncType::ST_FCN_SCOPE_BEGIN);
        }

        {
            StackMark dsmark = markStack(&ctx->defStack);

            // Populate args into the scope lookup
            for (uint32_t i = 0; i < fcn->prototype.inArgCount; i++) {
                DArray::push(&ctx->defStack, &fcn->prototype.inArgs[i]);
            }

            Scope* bodyScope = Ast::Node::makeScope();
            bodyScope->base.scope = paramScope;
            ctx->currentScope = bodyScope;

            tex.token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT, dsmark);
            fcn->bodyScope = bodyScope;
        }

        defer:
        ctx->currentScope = currentScope;
        ctx->currentFunction = NULL;

        DArray::push(&ctx->nodeStack, &fcn);
        DArray::push(&ctx->defStack, &fcn);

        fcn->base.span = finalizeSpan(&lspan, span);
        return tex.token;
    }

    Lex::Token parseIfStatement(ParseContext* ctx, Span* const span, QualifiedName* label, const bool compileTime) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;

        Scope* currentScope = ctx->currentScope;

        Scope* newScope = Ast::Node::makeScope();
        newScope->base.scope = ctx->currentScope;

        Variable* newOperand;
        token = parseExpression(ctx, &lspan, &newOperand, End { Lex::TK_SCOPE_BEGIN });

        ctx->currentScope = newScope;
        token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT);
        ctx->currentScope = currentScope;

        Branch* branch = Ast::Node::makeBranch();
        branch->base.scope = currentScope;
        if (compileTime) branch->base.flags |= IS_CMP_TIME;

        StackMark smark = markStack(&ctx->nodeStack);

        DArray::push(&ctx->nodeStack, &newScope);
        DArray::push(&ctx->nodeStack, &newOperand);

        bool hasElse = false;
        while (1) {
            token = Lex::tryKeyword(&lspan, KW_ELSE).token;
            if (token.kind != Lex::TK_KEYWORD) break;

            token = Lex::nextToken(&lspan).token;
            if (token.kind == Lex::TK_SCOPE_BEGIN) {
                // else case

                hasElse = true;

                Scope* newScope = Ast::Node::makeScope();
                newScope->base.scope = currentScope;

                ctx->currentScope = newScope;
                token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT);
                ctx->currentScope = currentScope;

                DArray::push(&ctx->nodeStack, &newScope);

                break;
            }

            // 'else #if' — a compile-time else-if continues the '#' chain.
            bool elseIfCompileTime = compileTime;
            if (token.kind == Lex::TK_DIRECTIVE) {
                elseIfCompileTime = true;

                token = Lex::nextToken(&lspan).token;
                if (!Lex::isKeyword(token, KW_IF)) {
                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "'if' expected after '#'.");
                    token = sync(&lspan, SyncType::ST_ELSE_IF).token;
                }
            }

            if (!Lex::isKeyword(token, KW_IF)) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "if, '#' or '{'");
                token = sync(&lspan, SyncType::ST_ELSE_IF).token;
            }

            Variable* newOperand;
            token = parseExpression(ctx, &lspan, &newOperand, End { Lex::TK_SCOPE_BEGIN });

            if (newOperand) {
                // TODO: why not to make this responsibility of parseExpression
                newOperand->base.span = getSpanStamp(&lspan);
            }

            Scope *newScope = Ast::Node::makeScope();
            newScope->base.scope = ctx->currentScope;

            ctx->currentScope = newScope;
            token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT);
            ctx->currentScope = currentScope;

            if (elseIfCompileTime) newOperand->base.flags |= IS_CMP_TIME;

            DArray::push(&ctx->nodeStack, &newScope);
            DArray::push(&ctx->nodeStack, &newOperand);
        }

        // we need to manually resolve 'nodeStack'
        const int count = (ctx->nodeStack.size - smark) / 2;
        SyntaxNode** buffer = ((SyntaxNode**) ctx->nodeStack.buffer) + smark;

        branch->scopes = alloc<Scope*>(count + hasElse);
        branch->expressions = alloc<Variable*>(count);
        branch->scopeCount = count + hasElse;
        branch->expressionCount = count;
        for (int i = 0; i < count; i++) {
            branch->scopes[i] = (Scope*) buffer[2 * i];
            branch->expressions[i] = (Variable*) buffer[2 * i + 1];
        }

        if (hasElse) {
            branch->scopes[count] = (Scope*) buffer[2 * count];
        }

        ctx->nodeStack.size = smark;

        DArray::push(&ctx->nodeStack, &branch);

        branch->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseCaseStatement(ParseContext* ctx, Span* const span, INamedEx* label) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;

        SwitchCase* switchCase = Ast::Node::makeSwitchCase();
        switchCase->base.scope = ctx->currentScope;
        switchCase->elseCase = NULL;

        switchCase->label = {};
        if (label) switchCase->label = String { label->buff, label->len };

        Variable* var;
        token = parseExpression(ctx, &lspan, &var, End { Lex::TK_SCOPE_BEGIN });

        switchCase->switchExp = var;

        if (token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL,
                "'{' expected after the case subject.");
            token = sync(&lspan, SyncType::ST_SCOPE_BEGIN).token;
        }

        int elseCase = 0;
        StackMark smark = markStack(&ctx->nodeStack);
        while (1) {
            token = Lex::tryKeyword(&lspan, KW_WHEN).token;
            if (token.kind != Lex::TK_KEYWORD) {
                token = Lex::tryKeyword(&lspan, KW_ELSE).token;
                if (token.kind != Lex::TK_KEYWORD) break;
            }

            elseCase = token.detail == KW_ELSE;

            Variable* cmpExp;
            if (!elseCase) {
                token = parseExpression(ctx, &lspan, &cmpExp, End { Lex::TK_SCOPE_BEGIN });
            } else {
                cmpExp = NULL;
                token = Lex::nextToken(&lspan).token;
            }

            Scope* sc = Ast::Node::makeScope();
            sc->base.scope = switchCase->base.scope;
            ctx->currentScope = sc;

            if (token.kind == Lex::TK_SCOPE_BEGIN) {
                token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT);
            } else {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "'{' expected");
                token = sync(&lspan, SyncType::ST_SCOPE_BEGIN).token;
            }
            ctx->currentScope = switchCase->base.scope;

            if (elseCase) {
                switchCase->elseCase = sc;
                break;
            }

            DArray::push(&ctx->nodeStack, &sc);
            DArray::push(&ctx->nodeStack, &cmpExp);
        }

        token = Lex::nextToken(&lspan).token;
        if (token.kind != Lex::TK_SCOPE_END) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "}");
            token = sync(&lspan, SyncType::ST_SCOPE).token;
        }

        // we need to manually resolve 'nodeStack'
        const int count = (ctx->nodeStack.size - smark) / 2;
        SyntaxNode** buffer = ((SyntaxNode**) ctx->nodeStack.buffer) + smark;

        switchCase->cases = alloc<Scope*>(count);
        switchCase->casesExp = alloc<Variable*>(count);
        switchCase->caseCount = count;
        switchCase->caseExpCount = count;
        for (int i = 0; i < count; i++) {
            switchCase->cases[i] = (Scope*) buffer[2 * i];
            switchCase->casesExp[i] = (Variable*) buffer[2 * i + 1];
        }

        ctx->nodeStack.size = smark;

        ctx->currentScope = switchCase->base.scope;
        DArray::push(&ctx->nodeStack, &switchCase);

        switchCase->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    // returns 'start' expression if its not a range via outLeftExp
    // we have a precedency over end token...
    // returns TK_ERROR if no range and also expression is corrupted...
    Lex::Token parseRangeExpression(ParseContext* ctx, Span* span, End end, RangeExpression** range, Variable** outLeftExp, const bool allowOpenBounds) {
        Lex::Token token;

        Variable* leftExp;

        token = Lex::peekToken(span).token;
        if (token.kind != Lex::TK_RANGE) {
            if (isEndToken(token, end)) {
                return Lex::nextToken(span).token;
            }

            token = parseExpression(ctx, span, &leftExp, { Lex::TK_RANGE },
                ALLOW_UNEXPECTED_END | EMPTY_EXPRESSION_ALLOWED);

            if (token.kind != Lex::TK_RANGE) {
                if (outLeftExp) *outLeftExp = leftExp;
                return leftExp ? token : Lex::Token { . kind = Lex::TK_ERROR };
            }
        } else {
            leftExp = NULL;
            token = Lex::nextToken(span).token;
        }

        // Slice operands are optional ('[:]', '[begin:]', '[begin:step:]'),
        // but parseExpression only tolerates the terminator after it has
        // parsed a primary expression. Detect the empty operand here instead
        // of relying on the flags being propagated into the expression parser.
        auto atOperandEnd = [&]() {
            return allowOpenBounds && isEndToken(Lex::peekToken(span).token, end);
        };

        // second expression could be 'eidx' OR 'step'
        Variable* secondExp = NULL;
        if (atOperandEnd()) {
            token = Lex::nextToken(span).token;
        } else {
            token = parseExpression(ctx, span, &secondExp, end,
                ALLOW_UNEXPECTED_END | EMPTY_EXPRESSION_ALLOWED | USE_KEYWORD_AS_END);
        }

        Variable* stepExp = NULL;
        Variable* rightExp = NULL;

        if (token.kind == Lex::TK_RANGE) {
            stepExp = secondExp;

            if (atOperandEnd()) {
                token = Lex::nextToken(span).token;
            } else {
                token = parseExpression(ctx, span, &rightExp, end, USE_KEYWORD_AS_END);
            }
        } else {
            rightExp = secondExp;
        }

        *range = Ast::Node::makeRangeExpression();
        (*range)->bidx = leftExp;
        (*range)->step = stepExp;
        (*range)->eidx = rightExp;

        return token;
    }

    Lex::Token parseLoop(ParseContext* ctx, Span* const span, INamedEx* label, const bool compileTime) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex;

        Loop* loop = Ast::Node::makeLoop();

        loop->label = {};
        if (label) loop->label = String { label->buff, label->len };

        if (compileTime) loop->base.flags |= IS_CMP_TIME;

        Scope* currentScope = ctx->currentScope;
        Scope* outerScope = Ast::Node::makeScope();
        outerScope->base.scope = ctx->currentScope;
        loop->base.scope = outerScope;

        ctx->currentScope = outerScope;
        StackMark smark = markStack(&ctx->nodeStack);
        StackMark dmark = markStack(&ctx->defStack);

        const End end = { Lex::TK_SCOPE_BEGIN };

        // Arg - Range, Condition or Array expression
        Variable* exp = NULL;
        tex.token = parseRangeExpression(ctx, &lspan, { Lex::TK_SCOPE_BEGIN }, &loop->arg.range, &exp);
        if (exp) {
            loop->arg.exp = exp;
            loop->arg.kind = Loop::Arg::EXPRESSION;
        } else if (loop->arg.range) {
            loop->arg.kind = Loop::Arg::RANGE;
        } else {
            loop->arg.kind = Loop::Arg::CONDITION;
            loop->arg.exp = Ast::Internal::variables + Ast::Internal::IV_TRUE;
        }

        if (isEndToken(tex.token, end))            goto parseBody;
        else if (Lex::isKeyword(tex.token, KW_AS)) goto parseAs;
        else if (Lex::isKeyword(tex.token, KW_AT)) goto parseAt;
        else goto errorExit;

        parseAs: {
            // As - plain item alias ('as' does not bind references anymore)
            tex = Lex::nextToken(&lspan);

            if (tex.token.kind == Lex::TK_IDENTIFIER) {
                VariableDefinition* def = Ast::Node::makeVariableDefinition();
                def->base.scope = outerScope;
                def->base.span = getSpanStamp(&lspan);
                def->typeValue = NULL; // inferred from the iterable by semantics

                def->var = makeVariable(def);
                def->var->name.buff = tex.value.str;
                def->var->name.len  = tex.token.size;

                DArray::push(&ctx->defStack, &def);
                DArray::push(&ctx->nodeStack, &def);

                loop->item = def;

                tex = Lex::nextToken(&lspan);
            }

            if (isEndToken(tex.token, end))               goto parseBody;
            else if (Lex::isKeyword(tex.token, KW_AT))    goto parseAt;
            else goto errorExit;
        }

        parseAt: {
            // At - [&]index[:type]
            tex = Lex::nextToken(&lspan);

            if (tex.token.kind == Lex::TK_OP_AND) {
                // '&' binds the counter to an existing outer variable.
                loop->index.var = Ast::Node::makeVariable();
                tex = Lex::nextToken(&lspan);
            }

            if (tex.token.kind == Lex::TK_IDENTIFIER) {
                QualifiedName* name = toQualifiedName(&lspan, tex);
                tex = Lex::nextToken(&lspan);

                if (loop->index.var) {
                    loop->index.var->name = *name;
                    loop->index.var->base.scope = outerScope;
                } else if (tex.token.kind == Lex::TK_STATEMENT_BEGIN) {
                    // Explicit index type: 'at i: i32 {'
                    tex = Lex::nextToken(&lspan);

                    VariableDefinition* idxDef = Ast::Node::makeVariableDefinition();
                    idxDef->base.scope = outerScope;

                    tex.token = parseVarDefinition(ctx, &lspan, tex, name, idxDef, End { Lex::TK_SCOPE_BEGIN });

                    loop->index.def = idxDef;
                } else {
                    // Plain counter, defaults to u64.
                    loop->index.def = Ast::Node::makeVariableDefinition();
                    loop->index.def->base.scope = outerScope;
                    loop->index.def->typeValue = makeTypeValue(ctx, Type::basicTypes + Type::DT_U64);
                    loop->index.def->var = makeVariable(loop->index.def);
                    loop->index.def->var->name = *name;
                    loop->index.def->base.span = getSpanStamp(&lspan);

                    loop->index.def->var->base.span = loop->index.def->base.span;

                    DArray::push(&ctx->defStack, &loop->index.def);
                    DArray::push(&ctx->nodeStack, &loop->index.def);
                }
            }

            if (isEndToken(tex.token, end)) goto parseBody;
            else                        goto errorExit;
        }

        parseBody: {
            outerScope->children = commitStack(&ctx->nodeStack, smark, &outerScope->childrenCount);
            outerScope->definitions = commitStack(&ctx->defStack, dmark, &outerScope->definitionCount);

            loop->bodyScope = Ast::Node::makeScope();
            loop->bodyScope->base.scope = loop->base.scope;

            SyntaxNode* tmpLoop = ctx->currentLoop;
            ctx->currentLoop = (SyntaxNode*) loop;

            ctx->currentScope = loop->bodyScope;
            tex.token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT);
            ctx->currentScope = currentScope;

            ctx->currentLoop = tmpLoop;

            DArray::push(&ctx->nodeStack, &loop);
            loop->base.span = finalizeSpan(&lspan, span);
        }

        exit: {
            return tex.token;
        }

        errorExit: {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL,
                Diag::Format {
                "Unexpected token '%s' in loop declaration."
                }, Lex::str((Lex::TokenKind) tex.token.kind));
            return sync(&lspan, ST_SEEK_BLOCK_BEGIN).token;
        }
    }

    Lex::Token parseGotoStatement(ParseContext* ctx, Span* const span) {
        auto [token, tokenVal] = Lex::nextToken(span);

        GotoStatement* gt = Ast::Node::makeGotoStatement();
        gt->span = getSpanStamp(span);
        gt->base.scope = ctx->currentScope;
        gt->label = NULL;

        DArray::push(&ctx->unit->reg->gotos, &gt);
        DArray::push(&ctx->nodeStack, &gt);

        if (token.kind != Lex::TK_IDENTIFIER) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Identifier");
            return sync(span, Lex::TK_STATEMENT_END).token;
        }

        gt->name.buff = tokenVal.str;
        gt->name.len = token.size;

        return token;
    }

    Lex::Token parseContinueStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex = Lex::nextToken(&lspan);

        SyntaxNode* targetNode = NULL;

        if (tex.token.kind == Lex::TK_IDENTIFIER) {
            Variable* labelVar = Ast::Node::makeVariable();
            labelVar->name.buff = tex.value.str;
            labelVar->name.len  = tex.token.size;
            labelVar->base.span = getSpanStamp(&lspan);
            targetNode = (SyntaxNode*) labelVar;

            tex = Lex::nextToken(&lspan);
        }

        if (tex.token.kind != Lex::TK_STATEMENT_END) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "';' expected after continue statement");
            tex.token = sync(&lspan, Lex::TK_STATEMENT_END).token;
        }

        ContinueStatement* stmt = Ast::Node::makeContinueStatement();
        stmt->base.scope = ctx->currentScope;
        stmt->target     = targetNode;

        DArray::push(&ctx->nodeStack, &stmt);

        stmt->base.span = finalizeSpan(&lspan, span);
        return tex.token;
    }

    Lex::Token parseDeferStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        DeferStatement* defer = Ast::Node::makeDeferStatement();
        defer->base.scope = ctx->currentScope;

        Lex::TokenEx tex = Lex::nextToken(&lspan);

        if (tex.token.kind == Lex::TK_SCOPE_BEGIN) {
            // Block form: 'defer { ... }'
            Scope* scope = Ast::Node::makeScope();
            scope->base.scope = ctx->currentScope;

            ctx->currentScope = scope;
            tex.token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT);
            ctx->currentScope = defer->base.scope;

            defer->body = (SyntaxNode*) scope;
        } else {
            // Single statement form: 'defer cleanup();'
            StackMark smark = markStack(&ctx->nodeStack);

            tex.token = parseStatement(ctx, &lspan, tex, SC_COMMON);

            if (ctx->nodeStack.size > smark) {
                defer->body = ((SyntaxNode**) ctx->nodeStack.buffer)[ctx->nodeStack.size - 1];
            }
            ctx->nodeStack.size = smark;
        }

        defer->base.span = finalizeSpan(&lspan, span);

        DArray::push(&ctx->nodeStack, &defer);
        return tex.token;
    }

    // 'using [name:] QualifiedName;'
    Lex::Token parseUsingStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex = Lex::nextToken(&lspan);

        Using* usingNode = Ast::Node::makeUsing();
        usingNode->base.scope = ctx->currentScope;
        usingNode->var = NULL;
        usingNode->label = {};
        usingNode->ref = alloc<QualifiedName>();

        // Optional named base: 'using transform: Vec3;'
        if (tex.token.kind == Lex::TK_IDENTIFIER) {
            const Lex::Token next = Lex::peekToken(&lspan).token;

            if (next.kind == Lex::TK_STATEMENT_BEGIN) {
                usingNode->label.buff = tex.value.str;
                usingNode->label.len  = tex.token.size;

                Lex::nextToken(&lspan);
                tex = Lex::nextToken(&lspan);
            }
        }

        // TODO: is there a way to unite these two cases?
        if (tex.token.kind == Lex::TK_IDENTIFIER) {
            tex.token = parseQualifiedName(ctx, &lspan, tex, usingNode->ref);
        } else if (tex.token.kind == Lex::TK_SCOPE_RESOLUTION) {
            // TODO to a const
            tex = { .token = { .kind = Lex::TK_IDENTIFIER, .size = 0 }, .value = { NULL } };
            Lex::rollback(&lspan);
            tex.token = parseQualifiedName(ctx, &lspan, tex, usingNode->ref);
        }else {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL,
                "Base type name expected after 'using'.");
            tex = sync(&lspan, Lex::TK_STATEMENT_END);
        }

        if (tex.token.kind != Lex::TK_STATEMENT_END) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "';' expected after 'using' clause.");
            tex.token = sync(&lspan, Lex::TK_STATEMENT_END).token;
        }

        DArray::push(&ctx->defStack, &usingNode);
        DArray::push(&ctx->nodeStack, &usingNode);

        usingNode->base.span = finalizeSpan(&lspan, span);
        return tex.token;
    }

    Lex::Token parseBreakStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex = Lex::nextToken(&lspan);

        SyntaxNode* targetNode = NULL;

        if (tex.token.kind == Lex::TK_IDENTIFIER) {
            Variable* labelVar = Ast::Node::makeVariable();
            labelVar->name.buff = tex.value.str;
            labelVar->name.len  = tex.token.size;
            labelVar->base.span = getSpanStamp(&lspan);
            targetNode = (SyntaxNode*) labelVar;

            tex = Lex::nextToken(&lspan);
        }

        if (tex.token.kind != Lex::TK_STATEMENT_END) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "';' expected after break statement");
            tex = sync(&lspan, Lex::TK_STATEMENT_END);
        }

        BreakStatement* stmt = Ast::Node::makeBreakStatement();
        stmt->base.scope = ctx->currentScope;
        stmt->target     = targetNode;

        DArray::push(&ctx->nodeStack, &stmt);

        stmt->base.span = finalizeSpan(&lspan, span);
        return tex.token;
    }

    Lex::Token parseNamespace(ParseContext* ctx, Span* const span, QualifiedName* name) {
        SpanEx lspan = markSpanStart(span);

        Scope* currentScope = ctx->currentScope;

        Lex::Token token = Lex::toToken(Lex::TK_NONE);

        Namespace* nsc = Ast::Node::makeNamespace();
        nsc->scope.base.scope = ctx->currentScope;
        nsc->name = *name;

        ctx->currentScope = (Scope*) nsc;
        token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT);
        ctx->currentScope = currentScope;

        DArray::push(&ctx->nodeStack, &nsc);
        DArray::push(&ctx->defStack, &nsc);

        nsc->scope.base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseFreeStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;

        Free* freeEx = Ast::Node::makeFree();
        freeEx->var->base.scope = ctx->currentScope;

        token = parseExpression(ctx, &lspan, freeEx->var, End { Lex::TK_STATEMENT_END });

        Variable* wrapper = Ast::Node::makeVariable();
        wrapper->base.scope = ctx->currentScope;
        wrapper->expression = (Expression*) freeEx;
        wrapper->base.span = getSpanStamp(&lspan);

        Statement* st = Ast::Node::makeStatement();
        st->base.scope = ctx->currentScope;
        st->operand = wrapper;

        DArray::push(&ctx->nodeStack, &st);

        token = Lex::nextToken(&lspan).token;
        return token;
    }

    Lex::Token parseReturnStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;

        ReturnStatement* ret = Ast::Node::makeReturnStatement();
        ret->fcn = ctx->currentFunction;
        ret->base.scope = ctx->currentScope;

        ret->var = NULL;
        ret->err = NULL;

        DArray::push(&ctx->unit->reg->returnStatements, &ret);
        DArray::push(&ctx->nodeStack, &ret);

        Pos startPos = lspan.end;

        token = Lex::peekToken(&lspan).token;
        if (token.kind == Lex::TK_STATEMENT_END) {
            token = Lex::nextToken(&lspan).token;
            ret->base.span = finalizeSpan(&lspan, span);
            return token;
        }

        {
            Variable* newVar;
            token = parseExpression(ctx, &lspan, &newVar, End { Lex::TK_LIST_SEPARATOR, Lex::TK_STATEMENT_END });

            // TODO
            newVar->base.span = getSpanStamp(&lspan);

            ret->var = newVar;
        }

        if (token.kind == Lex::TK_STATEMENT_END) {
            ret->base.span = finalizeSpan(&lspan, span);
            return token;
        }

        if (token.kind != Lex::TK_LIST_SEPARATOR) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, Lex::str(Lex::TK_LIST_SEPARATOR));
            token = sync(span, Lex::TK_LIST_SEPARATOR, Lex::TK_STATEMENT_END).token;
            ret->base.span = finalizeSpan(&lspan, span);
            return token;
        }

        // error case
        Variable* newVar;
        token = parseExpression(ctx, &lspan, &newVar, End { Lex::TK_STATEMENT_END });

        // TODO
        if (newVar) {
            newVar->base.span = getSpanStamp(&lspan);
        }

        ret->err = newVar;

        ret->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseEnumExpression(ParseContext* ctx, Span* const span, EnumExpression* enumerator) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex;

        enumerator->memberType = Type::basicTypes + Type::DT_I64;

        // Optional backing type directly after 'enum': 'enum u8 { ... }'
        tex = Lex::peekToken(&lspan);
        if (tex.token.kind == Lex::TK_KEYWORD && Lex::isInt((Keyword) tex.token.detail)) {
            Lex::nextToken(&lspan);
            enumerator->memberType = Type::basicTypes + Lex::toType((Keyword) tex.token.detail);
            enumerator->type = enumerator->memberType;
        }

        tex = Lex::nextToken(&lspan);
        if (tex.token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "'{' is expected!");
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }

        uint64_t lastValue = -1; //((uint64_t) 0) - ((uint64_t) 1);
        StackMark smark = markStack(&ctx->nodeStack);
        while (1) {
            tex = Lex::nextToken(&lspan);
            if (tex.token.kind == Lex::TK_SCOPE_END) break;

            VariableDefinition* newVarDef = Ast::Node::makeVariableDefinition();
            Variable* newVar = Ast::Node::makeVariable();
            newVar->base.scope = ctx->currentScope;
            newVar->base.span = getSpanStamp(&lspan);

            newVarDef->var = newVar;
            newVarDef->var->value.hasValue = 0;
            newVarDef->base.span = getSpanStamp(&lspan);
            newVarDef->base.flags = IS_CMP_TIME;
            newVarDef->typeValue = makeTypeValue(ctx, enumerator->memberType);

            newVar->def = newVarDef;
            newVar->name.buff = tex.value.str;
            newVar->name.len = tex.token.size;

            DArray::push(&ctx->nodeStack, &newVar);

            tex = Lex::nextToken(&lspan);
            if (tex.token.kind == Lex::TK_EQUAL) {
                tex.token = parseExpression(ctx, &lspan, newVar, End { Lex::TK_LIST_SEPARATOR, Lex::TK_SCOPE_END }, 0);
            }

            if (tex.token.kind == Lex::TK_LIST_SEPARATOR) continue;
            if (tex.token.kind == Lex::TK_SCOPE_END) break;

            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "',' or '}'");
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }
        enumerator->members = (Variable**) commitStack(&ctx->nodeStack, smark, &enumerator->memberCount);

        return tex.token;
    }

    // NOTE: called with the 'struct'/'union' keyword as the previous token.
    //       Structure:  struct [(params)] [annotations] { items }
    Lex::Token parseRecordExpression(ParseContext* ctx, Span* const span, RecordExpression* record) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex;

        record->params     = NULL;
        record->paramCount = 0;
        record->flags      = 0;

        {
            // Optional constructor sugar: 'struct (T : type) { ... }'
            const Lex::Token peeked = Lex::peekToken(&lspan).token;
            if (peeked.kind == Lex::TK_PARENTHESIS_BEGIN) {
                Lex::nextToken(&lspan); // '('

                Scope* paramScope = Ast::Node::makeScope();
                paramScope->base.scope = ctx->currentScope;

                FunctionPrototype params = {};
                parseParameterList(ctx, &lspan, &params, paramScope);

                record->params     = params.inArgs;
                record->paramCount = params.inArgCount;
            }
        }

        // Optional annotations
        tex = Lex::nextToken(&lspan);
        tex = parseAnnotations(ctx, &lspan, tex, AT_RECORD, record);

        if (tex.token.kind == Lex::TK_STATEMENT_END) {
            record->fields = NULL;
            record->fieldCount = 0;
            record->flags |= SF_BUILDER;
            return tex.token;
        }

        if (tex.token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL);
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }

        return parseRecordItems(ctx, &lspan, record);
    }

    Lex::Token parseRecordItems(ParseContext* ctx, Span* const span, RecordExpression* record) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex;

        StackMark mark = markStack(&ctx->nodeStack);
        while (1) {
            tex = Lex::nextToken(&lspan);
            if (tex.token.kind == Lex::TK_SCOPE_END) break;

            if (tex.token.kind == Lex::TK_KEYWORD) {
                const Keyword keyword = (Keyword) tex.token.detail;

                // Anonymous nested composite
                if (keyword == KW_STRUCT || keyword == KW_UNION) {
                    RecordExpression* nested = Ast::Node::makeRecordExpression();
                    nested->base.type = keyword == KW_STRUCT ? EXT_STRUCT : EXT_UNION;

                    tex.token = parseRecordExpression(ctx, &lspan, nested);

                    Variable* nestedVar = Ast::Node::makeVariable();
                    nestedVar->base.scope = ctx->currentScope;
                    nestedVar->expression = (Expression*) nested;

                    Statement* nestedStmt = Ast::Node::makeStatement();
                    nestedStmt->base.scope = ctx->currentScope;
                    nestedStmt->operand = nestedVar;

                    DArray::push(&ctx->nodeStack, &nestedStmt);

                    tex = Lex::nextToken(&lspan);
                    if (tex.token.kind == Lex::TK_STATEMENT_END) continue;
                    if (tex.token.kind == Lex::TK_SCOPE_END) break;

                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, Lex::str(Lex::TK_STATEMENT_END));
                    return Lex::toToken(Err::UNEXPECTED_SYMBOL);
                }

                // 'using Base;' / 'using name: Base;'
                if (keyword == KW_USING) {
                    parseUsingStatement(ctx, &lspan);
                    continue;
                }
            }

            tex.token = parseLabel(ctx, &lspan, tex, End { Lex::TK_STATEMENT_END });

            if (tex.token.kind == Lex::TK_STATEMENT_END) continue;
            if (tex.token.kind == Lex::TK_SCOPE_END) break;

            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, Lex::str(Lex::TK_STATEMENT_END));
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }
        record->fields = (SyntaxNode**) commitStack(&ctx->nodeStack, mark, &record->fieldCount);

        return tex.token;
    }

    // Parses the '{ Identifier [= Expression]; ... }' body of an error set. [= Expression]; ... }' body of an error set.
    // The set's name has to be already bound by the caller ('Set := error { ... }')
    Lex::Token parseErrorSetBody(ParseContext* ctx, Span* const span, ErrorSet* errorSet) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex = Lex::nextToken(&lspan);
        if (tex.token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, Lex::str(Lex::TK_SCOPE_BEGIN));
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }

        StackMark smark = markStack(&ctx->nodeStack);
        bool failed = false;
        while (1) {
            tex = Lex::nextToken(&lspan);
            if (tex.token.kind == Lex::TK_SCOPE_END) break;

            if (tex.token.kind != Lex::TK_IDENTIFIER) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "Error member name expected!");
                failed = true;
                break;
            }

            Variable* var = Ast::Node::makeVariable();
            var->base.scope = ctx->currentScope;
            var->base.span = getSpanStamp(&lspan);
            var->value.hasValue = 0;
            var->value.type = Type::basicTypes + Type::DT_ERROR;

            tex.token = parseQualifiedName(ctx, span, tex, &var->name);
            DArray::push(&ctx->nodeStack, &var);

            // Optional explicit value: 'NotFound = 404;'
            if (tex.token.kind == Lex::TK_EQUAL) {
                tex.token = parseExpression(ctx, &lspan, var, End { Lex::TK_STATEMENT_END, Lex::TK_NONE });
            }

            if (tex.token.kind == Lex::TK_STATEMENT_END) continue;
            if (tex.token.kind == Lex::TK_SCOPE_END) break;

            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "';' or '}' expected in error set.");
            failed = true;
            break;
        }

        errorSet->vars = (Variable**) commitStack(&ctx->nodeStack, smark, &errorSet->varCount);
        errorSet->base.span = finalizeSpan(&lspan, span);

        if (failed) return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        return tex.token;
    }

    void dispatchImport(ParseContext* ctx, Span* span, ImportStatement* import) {
        using namespace FileSystem;

        if (import->tag.len > 0) return;

        Handle fhnd = load(import->fname, Origins::COMPILER_SOURCE);
        if (!fhnd) {
            Diag::report(ctx->unit->ast, span, Err::FILE_LOAD_FAILED, import->fname.len, import->fname.buff);
            return;
        }

        TaskSystem::dispatchParse(fhnd);
    }

    Lex::Token parseImport(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenEx tex;
        bool pushedToStack = false;

        ImportStatement* import = Ast::Node::makeImportStatement();
        import->base.scope = ctx->currentScope;

        // import [annotations] QualifiedName as <target>
        tex = Lex::nextToken(&lspan);
        tex = parseAnnotations(ctx, &lspan, tex, AT_IMPORT, import);

        if (tex.token.kind == Lex::TK_SCOPE_RESOLUTION) {
            tex = Lex::nextToken(&lspan);
        }

        if (tex.token.kind != Lex::TK_IDENTIFIER) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "File or library name expected after 'import'.");
            tex = sync(&lspan, Lex::TK_KEYWORD);
            if (!Lex::isKeyword(tex.token, KW_AS)) return tex.token;

            tex = Lex::nextToken(&lspan);
        }

        QualifiedName* fname = Ast::Node::makeQualifiedName();
        tex.token = parseQualifiedName(ctx, &lspan, tex, fname);

        import->fname = { fname->buff, fname->len };
        import->keyword = KW_VOID;

        if (!Lex::isKeyword(tex.token, KW_AS)) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "Keyword 'as' expected!");
            tex = sync(&lspan, Lex::toToken(Lex::TK_STATEMENT_END));
            return tex.token;
        }

        tex = Lex::nextToken(&lspan);

        // '_' target, file's AST is flattened into the current scope
        if (tex.token.kind == Lex::TK_SKIP) {
            import->param = { (char*) "_", 1 };

            tex = Lex::nextToken(&lspan);
            if (tex.token.kind != Lex::TK_STATEMENT_END) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "';' expected after 'as _'.");
                tex.token = sync(&lspan, Lex::TK_STATEMENT_END).token;
            }
        } else if (tex.token.kind == Lex::TK_IDENTIFIER) {
            QualifiedName* target = Ast::Node::makeQualifiedName();
            target->buff = tex.value.str;
            target->len  = tex.token.size;

            import->param = { target->buff, target->len };

            tex = Lex::nextToken(&lspan);
            if (tex.token.kind == Lex::TK_STATEMENT_BEGIN) {
                // 'as name: <DeclarationBody>'

                if (import->tag.len > 0) {
                    ctx->foreignContext = true;
                }

                // Keep the import before its declaration so the emitted
                // order matches the source.
                DArray::push(&ctx->nodeStack, &import);
                pushedToStack = true;

                tex.token = parseDeclarationBody(ctx, &lspan, target, End { Lex::TK_STATEMENT_END });

                if (ctx->nodeStack.size > 0) {
                    void* last = DArray::getLast(&ctx->nodeStack);
                    SyntaxNode* node = last ? *(SyntaxNode**) last : NULL;
                    if (node) node->import = import;
                }
            } else if (tex.token.kind != Lex::TK_STATEMENT_END) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "';' expected after the import target.");
                tex.token = sync(&lspan, Lex::TK_STATEMENT_END).token;
            }
        } else {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "Import target expected after 'as'.");
            tex.token = sync(&lspan, Lex::TK_STATEMENT_END).token;
        }

        ctx->foreignContext = false;
        DArray::push(&ctx->unit->reg->imports, (void*) &import);

        if (!pushedToStack) {
            DArray::push(&ctx->nodeStack, &import);
        }

        // TODO : why dispatch before finalizeSpan?
        dispatchImport(ctx, span, import);

        import->base.span = finalizeSpan(&lspan, span);
        return tex.token;
    }

    Lex::Token parseDirective(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        // NOTE: lexer emits bare '#' as TK_DIRECTIVE. The directive
        //       name itself arrives as the next token
        Lex::TokenEx tex = Lex::nextToken(&lspan);

        if (tex.token.kind == Lex::TK_KEYWORD && (Keyword) tex.token.detail == KW_IF) {
            return parseIfStatement(ctx, span, NULL, true);
        }

        if (tex.token.kind == Lex::TK_KEYWORD && (Keyword) tex.token.detail == KW_LOOP) {
            return parseLoop(ctx, span, NULL, true);
        }

        // TODO: make this error message automatically print all directives
        Diag::report(ctx->unit->ast, &lspan, Err::UNKNOWN_DIRECTIVE,
            Diag::Format {
                "Unknown directive '#%.*s', only '#if' and '#loop' are statements."
            }, tex.token.size, tex.value.str
        );
        tex = sync(&lspan, SyncType::ST_SCOPE);
        return tex.token;
    }

    Lex::Token parseList(ParseContext* ctx, Span* const span, Lex::TokenKind separator, Lex::TokenKind end) {
        Lex::Token token;
        bool first = true;

        while (1) {
            if (first) {
                token = Lex::tryToken(span, Lex::toToken(end)).token;
                if (token.kind == end) return token;
            }

            Variable* newVar = NULL;
            token = parseExpression(ctx, span, &newVar, End{ separator, end }, first ? EMPTY_EXPRESSION_ALLOWED : 0);
            if (token.encoded < 0) return token;

            DArray::push(&ctx->nodeStack, &newVar);

            if (token.kind == end || token.kind == Lex::TK_END) {
                break;
            }

            if (token.kind != separator) {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, Lex::str(separator));
                token = sync(span, separator, end, false).token;

                if (token.kind != separator) {
                    break;
                }
            }

            first = false;
        }

        return token;
    }

    Lex::Token parseCatch(ParseContext* ctx, Span* const span, Variable* var) {
        Lex::TokenEx tex;



        Variable* errVar = Ast::Node::makeVariable();
        errVar->base.scope = var->base.scope;
        errVar->value.type = Type::basicTypes + Type::DT_ERROR;

        Scope* newScope = Ast::Node::makeScope();
        newScope->base.scope = var->base.scope;
        // setParentIdx(newScope);

        Catch* cex = Ast::Node::makeCatch();
        cex->call = (FunctionCall*) var->expression;

        // NOTE: Insert the catch node only after validating the clause,
        //       leaving the call expression intact on failure.

        tex = Lex::nextToken(span);

        if (Lex::isKeyword(tex.token, KW_RETURN)) {
            // basically just emulates following
            // .. .. catch err { return _, err; }

            ReturnStatement* ret = Ast::Node::makeReturnStatement();
            ret->err = errVar;
            ret->var = NULL;
            ret->base.scope = newScope;
            ret->fcn = ctx->currentFunction;

            DArray::push(&ctx->unit->reg->returnStatements, &ret);
            newScope->children = alloc<SyntaxNode*>();
            newScope->children[0] = (SyntaxNode*) ret;

            tex = Lex::nextToken(span);
            if (tex.token.kind != Lex::TK_STATEMENT_END) {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "'catch return' expression has to be terminated with ';'!");
                return Lex::toToken(Err::UNEXPECTED_SYMBOL);
            }

            var->expression = (Expression*) cex;
            return tex.token;
        }

        if (tex.token.kind != Lex::TK_IDENTIFIER) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "error identifier");
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }

        QualifiedName* errName = toQualifiedName(span, tex);

        tex = Lex::nextToken(span);
        if (
            tex.token.kind != Lex::TK_SCOPE_BEGIN &&
            !Lex::isKeyword(tex.token, KW_RETURN)
        ) {
            // 'catch err' case, capture into an existing variable
            errVar->name = *errName;

            DArray::push(&ctx->unit->reg->variables, &var);

            cex->err = errVar;
            cex->scope = NULL;

            var->expression = (Expression*) cex;
            return tex.token;
        }

        // 'catch err { ... }' case, capture into a block-local binding
        VariableDefinition* errDef = Ast::Node::makeVariableDefinition();
        errDef->var = errVar;
        errDef->base.scope = newScope;
        errDef->var->base.scope = newScope;
        errDef->var->value.type = Type::basicTypes + Type::DT_ERROR;
        errDef->var->base.span = getSpanStamp(span);

        errDef->var->name = *errName;

        DArray::push(&ctx->defStack, &errDef->var);

        cex->err = errDef->var;
        cex->scope = newScope;

        var->expression = (Expression*) cex;

        Scope* currentScope = ctx->currentScope;
        ctx->currentScope = newScope;
        tex.token = parseScope(ctx, span, SC_COMMON, resolveScopeEnd(tex.token));
        ctx->currentScope = currentScope;

        if (tex.token.kind == Lex::TK_SCOPE_END) {
            Lex::TokenEx next = Lex::peekToken(span);
            if (next.token.kind == Lex::TK_STATEMENT_END) {
                tex = Lex::nextToken(span);
            }
        }

        return tex.token;
    }



    // Returns 'true' if 'token' can start a type expression.
    // Types are values, so these tokens can also start ordinary expressions.
    // The expression parser should delegate every such start to parseTypeExpression.
    static inline bool isTypeStart(Lex::Token token) {
        switch (token.kind) {
            case Lex::TK_IDENTIFIER:
            case Lex::TK_SCOPE_RESOLUTION:
            case Lex::TK_OP_POINTER:
            case Lex::TK_SIGNATURE_BEGIN:
            case Lex::TK_ARRAY_BEGIN:
            case Lex::TK_VARIADIC:
                return true;

            case Lex::TK_KEYWORD: {
                const Keyword keyword = (Keyword) token.detail;

                return Lex::isType(token) ||
                       keyword == KW_TYPE ||
                       keyword == KW_CONST ||
                       keyword == KW_EMBED ||
                       keyword == KW_ERROR ||
                       keyword == KW_STRUCT ||
                       keyword == KW_UNION ||
                       keyword == KW_ENUM;
            }

            default: {
                return false;
            }
        }
    }

    // Consumes exactly one type expression and returns the first token after it.
    // Postfix operators are deliberately left to the caller, so '[alloc]i64.{...}'
    // is parsed as the type '[alloc]i64' followed by a struct initialization.
    Lex::TokenEx parseTypeExpression(ParseContext* ctx, Span* const span, Lex::TokenEx tex, Variable* var) {
        bool consumeToken = true;

        var->base.scope = ctx->currentScope;
        var->base.span = getSpanStamp(span);

        switch (tex.token.kind) {

            case Lex::TK_IDENTIFIER: {
                tex.token = parseQualifiedName(ctx, span, tex, &var->name);
                consumeToken = false;

                break;
            }

            case Lex::TK_SCOPE_RESOLUTION: {
                // Root-qualified name: '::name' — addresses the root namespace.
                Lex::rollback(span);
                Lex::TokenEx tmp = {};
                tmp.token.size = 0;
                tmp.value.str  = NULL;

                tex.token = parseQualifiedName(ctx, span, tmp, &var->name);
                consumeToken = false;

                break;
            }

            case Lex::TK_OP_POINTER: {
                // Prefix '^' at type start is a pointer type decorator.
                PointerTypeExpression* t = Ast::Node::makePointerTypeExpression();
                t->operand = Ast::Node::makeVariable();
                t->operand->base.scope = ctx->currentScope;

                tex = parseTypeExpression(ctx, span, Lex::nextToken(span), t->operand);

                var->expression = (Expression*) t;
                var->base.span = getSpanStamp(span);
                consumeToken = false;

                break;
            }

            case Lex::TK_SIGNATURE_BEGIN: {
                // '^(params) -> Ret' — pointer to a function type.
                FunctionPrototype* proto = Ast::Node::makeFunctionPrototype();

                Scope* sigScope = Ast::Node::makeScope();
                sigScope->base.scope = ctx->currentScope;

                // TODO: bind error set...
                QualifiedName* err;
                Lex::Token token = parseFunctionSignature(ctx, span, proto, sigScope, &err);

                FunctionTypeExpression* type = Ast::Node::makeFunctionTypeExpression();
                type->fcn = proto;

                var->expression = (Expression*) type;
                var->base.span = getSpanStamp(span);
                var->base.scope = ctx->currentScope;

                tex = Lex::TokenEx { .token = token, .value = {} };
                consumeToken = false;

                break;
            }

            case Lex::TK_ARRAY_BEGIN: {
                // [N]T / [_]T  -> ArrayTypeExpression
                // []T / [const]T / [alloc]T -> SliceTypeExpression
                const Lex::TokenEx peeked = Lex::peekToken(span);

                if (peeked.token.kind == Lex::TK_ARRAY_END) {
                    // []T
                    Lex::nextToken(span);

                    SliceTypeExpression* t = Ast::Node::makeSliceTypeExpression();
                    t->flags = 0;
                    t->element = Ast::Node::makeVariable();
                    t->element->base.scope = ctx->currentScope;

                    tex = parseTypeExpression(ctx, span, Lex::nextToken(span), t->element);

                    var->expression = (Expression*) t;
                } else if (peeked.token.kind == Lex::TK_KEYWORD &&
                    ((Keyword) peeked.token.detail == KW_CONST ||
                    (Keyword) peeked.token.detail == KW_ALLOC) &&
                    Lex::peekNthToken(span, 2).token.kind == Lex::TK_ARRAY_END) {
                    // [const/alloc]T
                    const Keyword keyword = (Keyword) peeked.token.detail;
                    Lex::nextToken(span);

                    if (Lex::nextToken(span).token.kind != Lex::TK_ARRAY_END) {
                        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "']' expected after the bracket qualifier.");
                        tex = sync(span, Lex::TK_ARRAY_END);
                    }

                    SliceTypeExpression* t = Ast::Node::makeSliceTypeExpression();
                    t->flags = keyword == KW_CONST ? IS_CONST : IS_DYNAMIC;
                    t->element = Ast::Node::makeVariable();
                    t->element->base.scope = ctx->currentScope;

                    tex = parseTypeExpression(ctx, span, Lex::nextToken(span), t->element);

                    var->expression = (Expression*) t;
                } else if (peeked.token.kind == Lex::TK_DIRECTIVE) {
                    // '[#format]T' — the lexer emits '#' and the name as
                    // two tokens, so both have to be consumed here.
                    Lex::nextToken(span); // '#'
                    Lex::TokenEx nameTok = Lex::nextToken(span);
                    bool formatOk = true;

                    if (
                        nameTok.token.kind != Lex::TK_IDENTIFIER ||
                        !Strings::compare(extractString(nameTok), Lex::CDS_FORMAT)
                    ) {
                        Diag::report(ctx->unit->ast, span, Err::UNKNOWN_DIRECTIVE,
                            "'format' expected after '#'.");
                        tex = sync(span, Lex::TK_ARRAY_END);
                        formatOk = false;
                    } else if (Lex::nextToken(span).token.kind != Lex::TK_ARRAY_END) {
                        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "']' expected after '#format'.");
                        tex = sync(span, Lex::TK_ARRAY_END);
                        formatOk = false;
                    }

                    SliceTypeExpression* t = Ast::Node::makeSliceTypeExpression();
                    // A malformed directive recovers as a plain slice, so the
                    // type slot never claims a qualifier it did not have.
                    t->flags = formatOk ? IS_FORMAT : 0;
                    t->element = Ast::Node::makeVariable();
                    t->element->base.scope = ctx->currentScope;

                    tex = parseTypeExpression(ctx, span, Lex::nextToken(span), t->element);

                    var->expression = (Expression*) t;
                } /*else if (peeked.token.kind == Lex::TK_SKIP) {
                    // [_]T
                    Lex::nextToken(span);

                    Variable* len = Ast::Node::makeVariable();
                    len->base.scope = ctx->currentScope;
                    len->value.type = Type::getInfo(Type::DT_INFER);

                    if (Lex::nextToken(span).token.kind != Lex::TK_ARRAY_END) {
                        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "']' expected after '_'.");
                        tex = sync(span, Lex::TK_ARRAY_END);
                    }

                    ArrayTypeExpression* t = Ast::Node::makeArrayTypeExpression();
                    t->length = len;
                    t->element = Ast::Node::makeVariable();
                    t->element->base.scope = ctx->currentScope;

                    tex = parseTypeExpression(ctx, span, Lex::nextToken(span), t->element);

                    var->expression = (Expression*) t;
                } */
                else {
                    // [N]T
                    Variable* len = Ast::Node::makeVariable();
                    len->base.scope = ctx->currentScope;

                    tex.token = parseExpression(ctx, span, len, End { Lex::TK_ARRAY_END });

                    ArrayTypeExpression* t = Ast::Node::makeArrayTypeExpression();
                    t->length = len;
                    t->element = Ast::Node::makeVariable();
                    t->element->base.scope = ctx->currentScope;

                    tex = parseTypeExpression(ctx, span, Lex::nextToken(span), t->element);

                    var->expression = (Expression*) t;
                }

                var->base.span = getSpanStamp(span);
                consumeToken = false;

                break;
            }

            case Lex::TK_KEYWORD: {
                const Keyword keyword = (Keyword) tex.token.detail;

                var->expression = NULL;
                var->value.hasValue = true;

                if (keyword == KW_TYPE) {
                    // metatype as a value: 't : type = type;'
                    var->value.type = Type::getInfo(Type::DT_TYPE);
                    var->value.tval = Type::getInfo(Type::DT_TYPE);
                } else if (keyword == KW_CONST || keyword == KW_EMBED) {
                    // Type operators: 'const T' / 'embed T' wrap the operand
                    // type value.
                    var->value.hasValue = false;

                    QualifierTypeExpression* type = Ast::Node::makeQualifierTypeExpression();
                    type->qualifier = keyword == KW_CONST ? Type::Q_CONST : Type::Q_EMBED;
                    type->operand = Ast::Node::makeVariable();
                    type->operand->base.scope = ctx->currentScope;

                    tex = parseTypeExpression(ctx, span, Lex::nextToken(span), type->operand);

                    var->expression = (Expression*) type;
                    var->base.span = getSpanStamp(span);
                    consumeToken = false;
                } else if (keyword == KW_ERROR) {
                    if (Lex::peekToken(span).token.kind == Lex::TK_SCOPE_BEGIN) {
                        // Anonymous error set as a type value:
                        // 'Set := error { ... };'
                        ErrorSet* errorSet = Ast::Node::makeErrorSet();
                        errorSet->base.scope = ctx->currentScope;
                        errorSet->name.buff = NULL;
                        errorSet->name.len  = 0;

                        tex.token = parseErrorSetBody(ctx, span, errorSet);

                        ErrorSetExpression* errorType = Ast::Node::makeErrorSetExpression();
                        errorType->set = errorSet;

                        var->value.hasValue = false;
                        var->expression = (Expression*) errorType;
                        var->base.span = getSpanStamp(span);
                    } else {
                        // Bare 'error' type in a declaration: 'err : error;'
                        var->value.type = Type::getInfo(Type::DT_TYPE);
                        var->value.tval = Type::getInfo(Type::DT_ERROR);
                    }
                } else if (Lex::isType(tex.token)) {
                    var->value.type = Type::getInfo(Type::DT_TYPE);
                    var->value.tval = Type::getInfo(Lex::toType(tex.token));
                } else if (keyword == KW_STRUCT || keyword == KW_UNION) {
                    RecordExpression* record = Ast::Node::makeRecordExpression();
                    record->base.type = keyword == KW_STRUCT ? EXT_STRUCT : EXT_UNION;
                    tex.token = parseRecordExpression(ctx, span, record);
                    // A record expression ends with '}' and the caller still
                    // has to see the trailing ';'. The import-builder form
                    // ('as Target: struct '#packed';') already ends on ';'.
                    consumeToken = tex.token.kind != Lex::TK_STATEMENT_END;

                    var->value.hasValue = false;
                    var->expression = (Expression*) record;
                    var->base.span = getSpanStamp(span);
                } else if (keyword == KW_ENUM) {
                    EnumExpression* enumerator = Ast::Node::makeEnumExpression();
                    tex.token = parseEnumExpression(ctx, span, enumerator);

                    var->expression = (Expression*) enumerator;
                    var->base.span = getSpanStamp(span);
                } else {
                    Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Type expression expected!");
                    return tex;
                }

                break;
            }

            case Lex::TK_VARIADIC: {
                var->base.flags |= IS_VARIADIC;
                var->value.type = Type::getInfo(Type::DT_TYPE);
                var->value.tval = Type::getInfo(Type::DT_MULTIPLE_TYPES);
                var->value.hasValue = true;

                break;
            }

            default: {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Type expression expected!");
                return tex;
            }
        }

        if (consumeToken) {
            tex = Lex::nextToken(span);
        }

        return tex;
    }

    // Takes the current token (already consumed) in 'tex' and fills 'var'.
    // Types are delegated to parseTypeExpression.
    Lex::Token parseExpressionNode(ParseContext* ctx, Span* const span, Lex::TokenEx tex, Variable* var, OperatorEnum prevOp, bool parsePrefix) {
        bool consumeToken = true;

        var->base.scope = ctx->currentScope;

        // Prefix part
        // ---

        if (parsePrefix) {
            while (1) {
                OperatorEnum op = Lex::toUnaryOperator(tex.token);
                if (op == OP_NONE) break;

                UnaryExpression* uex = Ast::Node::makeUnaryExpression();
                uex->base.opType = op;
                uex->operand = Ast::Node::makeVariable();
                uex->operand->base.scope = ctx->currentScope;

                var->expression = (Expression*) uex;
                var = uex->operand;

                tex = Lex::nextToken(span);
            }
        }

        var->base.span = getSpanStamp(span);



        // Primary part
        // ---

        if (isTypeStart(tex.token)) {
            tex = parseTypeExpression(ctx, span, tex, var);
            consumeToken = false;
        } else switch (tex.token.kind) {

            case Lex::TK_NUMBER: {
                //var->name.buff = NULL;
                //var->name.len = 0;
                var->expression = NULL;
                var->value.hasValue = true;
                var->value.type = Type::basicTypes + Lex::toType(tex.token);

                if (tex.token.detail == Lex::TD_DT_U64) {
                    var->value.u64 = tex.value.i64;
                } else if (tex.token.detail == Lex::TD_DT_I64) {
                    var->value.i64 = tex.value.i64;
                } else if (tex.token.detail == Lex::TD_DT_F32) {
                    var->value.f32 = tex.value.f32;
                } else if (tex.token.detail == Lex::TD_DT_F64) {
                    var->value.f64 = tex.value.f64;
                }

                break;
            }

            case Lex::TK_KEYWORD: {
                const Keyword keyword = (Keyword) tex.token.detail;

                var->expression = NULL;
                var->value.hasValue = true;

                // TODO: why we are not using predefined vars?
                if (keyword == KW_TRUE) {
                    var->value.type = Type::basicTypes + Type::DT_BOOL;
                    var->value.u64 = 1;
                } else if (keyword == KW_FALSE) {
                    var->value.type = Type::basicTypes + Type::DT_BOOL;
                    var->value.u64 = 0;
                } else if (keyword == KW_NULL) {
                    var->value.type = Type::makePointer(Type::basicTypes + Type::DT_VOID);
                    var->value.u64 = 0;
                }

                break;
            }

            case Lex::TK_PARENTHESIS_BEGIN: {
                tex.token = parseExpression(ctx, span, var, End { Lex::TK_PARENTHESIS_END, Lex::TK_NONE });
                break;
            }

            case Lex::TK_STRING:
            case Lex::TK_STRING_INFER: {
                StringInitialization* init = NULL;
                const int64_t err = parseStringLiteral(tex.value.str, tex.token.size, 1, &init);
                if (err < 0) reportStringLiteralError(ctx, span, err);

                var->expression = (Expression*) init;

                break;
            }

            case Lex::TK_CHAR: {
                var->value.type = Type::basicTypes + Type::DT_I64;
                var->value.i64 = tex.value.i64;
                var->value.hasValue = true;

                break;
            }

            case Lex::TK_SKIP: {
                var->value.type = Type::getInfo(Type::DT_INFER);
                var->value.hasValue = false;

                break;
            }

            case Lex::TK_ARRAY_LITERAL_BEGIN: {
                ArrayInitialization* init;
                tex.token = parseArrayInitialization(ctx, span, &init);
                var->expression = (Expression*) init;

                break;
            }

            case Lex::TK_SCOPE_LITERAL_BEGIN: {
                TypeInitialization* init;
                tex.token = parseTypeInitialization(ctx, span, &init);
                var->expression = (Expression*) init;
                var->base.span = getSpanStamp(span);

                break;
            }

            default: {
                if (tex.token.kind == Lex::TK_ERROR) {
                    reportLexError(ctx, span, Lex::toError(tex.token));
                    return tex.token;
                }

                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_END_OF_EXPRESSION, "TODO");
                return tex.token;
            }

        }

        if (consumeToken) {
            tex = Lex::nextToken(span);
        }



        // Postfix part
        // ---

        while (1) {

            OperatorEnum op = Lex::toPostfixOperator(tex.token);
            if (op == OP_NONE) {
                break;
            }

            // INFO :
            // We handle first operators, that are not in fact
            // unary form semantic standpoint, and therefore
            // have to be treated as special cases
            if (op == OP_MEMBER_SELECTION) {
                BinaryExpression* bex = Ast::Node::makeBinaryExpression();
                bex->left = Ast::Node::copy(var);
                bex->base.opType = op;

                tex = Lex::nextToken(span);
                if (tex.token.kind != Lex::TK_IDENTIFIER) {
                    Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Identifier");
                    var->expression = (Expression*) bex;
                    return tex.token;
                }

                bex->right = Ast::Node::makeVariable();
                bex->right->name.buff = tex.value.str;
                bex->right->name.len  = tex.token.size;

                var->expression = (Expression*) bex;
            } else if (op == OP_SUBSCRIPT) {
                BinaryExpression* bex = Ast::Node::makeBinaryExpression();
                bex->base.opType = op;
                bex->left = Ast::Node::copy(var); // The array/pointer being indexed

                // bex->right = Ast::Node::makeVariable();

                RangeExpression* range;
                tex.token = parseRangeExpression(ctx, span, { Lex::TK_ARRAY_END }, &range, &bex->right, true);
                if (tex.token.kind == Lex::TK_ERROR || tex.token.kind != Lex::TK_ARRAY_END) {
                    // TODO: error
                    tex = sync(span, Lex::TK_ARRAY_END);
                } else if (!bex->right) {
                    bex->right = Ast::Node::makeVariable();
                    bex->right->expression = (Expression*) range;
                    bex->right->value.type = Type::getInfo(Type::DT_RANGE);
                }

                // token = parseExpression(ctx, span, bex->right, End { Lex::TK_ARRAY_END, Lex::TK_SLICE });

                var->expression = (Expression*) bex;
            } else if (op == OP_CALL || op == OP_CALL_STRICT) {
                FunctionCall* call = Ast::Node::makeFunctionCall();
                call->fptr = NULL;
                call->fcn = NULL;

                StackMark smark = markStack(&ctx->nodeStack);
                tex.token = parseList(ctx, span, Lex::TK_LIST_SEPARATOR, Lex::TK_PARENTHESIS_END);
                call->inArgs = (Variable**) commitStack(&ctx->nodeStack, smark, &call->inArgCount);

                call->name = var->name;
                call->name.span = var->base.span;
                call->isStrict = op == OP_CALL_STRICT;

                var->value.hasValue = 0;
                var->expression = (Expression*) call;
                var->base.flags |= op == OP_CALL ? 0 : IS_STRICT_CALL;

                DArray::push(&ctx->unit->reg->fcnCalls, &var);
            } else if (op == OP_INIT_ARRAY) {
                ArrayInitialization* init;
                tex.token = parseArrayInitialization(ctx, span, &init);

                init->literalBase = Ast::Node::copy(var);

                var->expression = (Expression*) init;
            } else if (op == OP_INIT_STRUCT) {
                TypeInitialization* init;
                tex.token = parseTypeInitialization(ctx, span, &init);

                init->literalBase = Ast::Node::copy(var);

                var->expression = (Expression*) init;
                var->base.span = getSpanStamp(span);
            } else if (op == OP_INIT_STRING) {
                StringInitialization* init = NULL;

                if (var->value.type && var->value.type->kind == Type::DT_TYPE &&
                    !Type::isUnsignedInt(var->value.tval)) {
                    // TODO: what about u64
                    Diag::report(ctx->unit->ast, span, Err::INVALID_DATA_TYPE,
                        Diag::Format {
                            "String literals can only use builtin uint types such as 'u8', 'u16', 'u32'."
                        });
                }

                const int charSize = var->value.tval ? var->value.tval->size : 0;

                const int64_t err = parseStringLiteral(tex.value.str, tex.token.size, charSize, &init);
                if (err < 0) reportStringLiteralError(ctx, span, err);

                var->expression = (Expression*) init;
            } else {
                // generic case
                UnaryExpression* uex = Ast::Node::makeUnaryExpression();
                uex->base.opType = op;
                uex->operand = Ast::Node::makeVariable();
                uex->operand = Ast::Node::copy(var);

                var->expression = (Expression*) uex;
            }

            var->name = { 0 };
            tex = Lex::nextToken(span);
        }

        return tex.token;
    }


    // the lower the rank, the higher precedence
    Lex::Token parseExpressionRecursive(ParseContext* ctx, Span* const span, Variable** var, BinaryExpression* prevBex, OperatorEnum prevOp) {
        Lex::Token token;

        token = parseExpressionNode(ctx, span, Lex::nextToken(span), *var, prevOp);

        if (Lex::isOperator(token)) {
            OperatorEnum op = Lex::toBinaryOperator(token);

            while (op != OP_NONE) {
                // compound assignment
                if (Lex::peekToken(span).token.kind == Lex::TK_EQUAL) {
                    if (prevBex) prevBex->right = *var;
                    return token;
                }

                if (prevOp == OP_NONE || operators[prevOp].rank > operators[op].rank) {
                    // We are higher precedence, so we want to hug the next expression
                    BinaryExpression* bex = Ast::Node::makeBinaryExpression();
                    bex->left = *var;
                    bex->right = Ast::Node::makeVariable();
                    bex->right->base.scope = (*var)->base.scope;
                    bex->base.opType = op;

                    *var = Ast::Node::makeVariable();
                    (*var)->expression = (Expression*) bex;

                    if (prevBex) {
                        prevBex->right->expression = (Expression*) bex;
                        // prevOp = op;
                    }

                    token = parseExpressionRecursive(ctx, span, &bex->right, bex, op);

                    op = Lex::toBinaryOperator(token);
                    if (op < 0) return token;
                } else {
                    // We are lower precedence, so we just have to be hugged
                    prevBex->right = *var;
                    return Lex::toTokenAsBinaryOperator(op);
                }
            }
        } else {
            return token;
        }

        return token;
    }

    Lex::Token parseExpression(ParseContext* ctx, Span* const span, Variable** var, const End endToken, const Flags flags) {
        Lex::Token token;

        Variable* operand = Ast::Node::makeVariable();
        operand->base.scope = ctx->currentScope;
        token = parseExpressionRecursive(ctx, span, &operand, NULL, OP_NONE);

        // TODO: think of better way
        if (operand->expression || operand->value.hasValue || operand->name.len != 0 || operand->value.type) {
            *var = operand;
        } else {
            *var = NULL;
        }

        if (Lex::isKeyword(token, KW_CATCH)) {
            // TODO: move this to validator
            /*
            if (!operand->expression || operand->expression->type != EXT_FUNCTION_CALL) {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_END_OF_EXPRESSION, "Yet only 'pure' function call expressions are allowed to be caught 🐱.");

                token = sync(span, endToken.a, endToken.b).token;
                return token;
            }
            */

            // TODO : add flag something like ALLOW_CATCH_TO_DISRESPECT_ENDING
            token = parseCatch(ctx, span, operand);
            if (token.kind == Lex::TK_SCOPE_END) return token;
        }

        if (token.kind == Lex::TK_ERROR) {
            // The lexer error was already reported where the token surfaced,
            // just resynchronize.
            token = sync(span, endToken.a, endToken.b).token;
            return token;
        }

        if (
            isEndToken(token, endToken) ||
            (flags & USE_KEYWORD_AS_END && token.kind == Lex::TK_KEYWORD) ||
            flags & ALLOW_UNEXPECTED_END
        ) {
            return token;
        }

        // compound assignment... TODO: I guess we can pass flag
        if (Lex::isOperator(token) && Lex::peekToken(span).token.kind == Lex::TK_EQUAL) {
            return token;
        }

        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Unexpected symbol while parsing expression!");
        token = sync(span, endToken.a, endToken.b).token;
        return token;
    }

    // meh, but ok for now...
    Lex::Token parseExpression(ParseContext* ctx, Span* const span, Variable* var, const End endToken, const Flags flags) {
        Lex::Token token;

        Variable* tmpVar;
        token = parseExpression(ctx, span, &tmpVar, endToken, flags);

        if (!tmpVar && !(flags & EMPTY_EXPRESSION_ALLOWED)) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Expression expected!");
        }

        UnaryExpression* uex = Ast::Node::makeUnaryExpression();
        uex->operand = tmpVar;
        uex->base.opType = OP_NONE;

        var->expression = (Expression*) uex;

        return token;
    }

}
