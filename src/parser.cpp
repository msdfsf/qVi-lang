// === Rules, conventions and overview ===
// ====                               ====
//
// During recursive parsing, a global thread-local buffer is used to temporarily
// collect these children. Once the node is completely parsed, the exact required
// memory is allocated, and the collected data is permanently committed to the AST.
//
// This also applies to cached definition nodes used for fast symbol resolution.
// Which may use its own buffer to not mess up contiguity.

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
// Task System functions based on the error and configuration. Therefore
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


#include "parser.h"

#include <basetsd.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "allocator.h"
#include "array_list.h"
#include "data_types.h"
#include "config.h"
#include "globals.h"
#include "keywords.h"
#include "lexer.h"
#include "logger.h"
#include "operators.h"
#include "string.h"
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

        ST_FCN_NAME,
        ST_FCN_SCOPE_BEGIN,

        ST_ELSE_IF,

        ST_SCOPE_BEGIN,
    };



    // === Forward declaration
    //

    inline void offsetParentIdx(DArray::Container* vec, const int offset);
    int isArrayLHS(char* const str, Span* span);



    // === Scratch buffers
    //

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
        SyntaxNode** arr = (SyntaxNode**) alloc(alc, bytes);
        memcpy(arr, ((SyntaxNode**) stack->buffer) + mark, bytes);

        stack->size = mark;

        return arr;
    }



    // === Error related stuff
    //

    inline void logToken(Lex::TokenKind token) {
        const char* const str = Lex::toStr(token);
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



    // === Misc
    //

    inline bool isEndToken(Lex::Token token, const End& end) {
        return token.kind == end.a || token.kind == end.b;
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

    inline void setDefinitionIdx(ParseContext* ctx, SyntaxNode* node) {
        node->definitionIdx = ctx->idxInScope;
    }



    // === Import related stuff
    //

    ImportNode* initImportNode() {
        ImportNode* import = (ImportNode*) alloc(alc, sizeof(ImportNode));
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



    // === Main part
    //

    void init(ParseContext* ctx) {
        Lex::init();

        memset(ctx, 0, sizeof(ParseContext));

        DArray::init(&ctx->nodeStack, 1024, sizeof(void*));
        DArray::init(&ctx->defStack, 1024, sizeof(void*));
    }

    void release(ParseContext* ctx) {
        Lex::release();

        DArray::release(&ctx->nodeStack);
        DArray::release(&ctx->defStack);
        freeSpanStamp(ctx->fileSpan);

        dealloc(alc, ctx);
    }



    FileSystem::Path* obtainRelativePath(
        FileSystem::Path* abs,
        String root
    ) {
        using namespace FileSystem;

        Path* path = (Path*) alloc(alc, sizeof(Path));
        if (FileSystem::computeRelativePath(abs, root, path) < 0) {
            dealloc(alc, path);
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
        span.end = { -1, 1 };

        span.fileInfo->relativePath = obtainRelativePath(
            span.fileInfo->absPath, ctx->rootDir);

        ctx->fileSpan = getSpanStamp(&span);

        ctx->currentScope = Ast::Node::makeScope();
        ctx->currentScope->base.scope = NULL;
        ctx->currentScope->base.span = ctx->fileSpan;
        ctx->idxInScope = 0;

        ctx->unit->ast->root = ctx->currentScope;

        Lex::Token token = parseScope(ctx, &span, SC_GLOBAL, SE_DEFAULT);
        return (Err::Err) token.encoded;
    }

    Err::Err parse(ParseContext* ctx, char* const flname) {
        return parse(ctx, FileSystem::getHandle({ flname }));
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

    Lex::Token sync(Span* span, Lex::Token tokenA, Lex::Token tokenB = { Lex::TK_END }, Lex::TokenValue* val = NULL) {
        // Recovery mode
        if constexpr (Config::ERROR_RECOVERY_ENABLED) {
            return Lex::syncToken(span, tokenA, tokenB);
        }

        // Strict mode, we just act as a dumb function and pray
        // that we get optimized away.
        return { Lex::TK_END };
    }

    Lex::Token sync(Span* span, SyncType type, Lex::TokenValue* val = NULL) {
        // Strict mode, we just act as a dumb function and pray
        // that we get optimized away.
        if constexpr (!Config::ERROR_RECOVERY_ENABLED) {
            return { Lex::TK_END };
        }

        // Recovery mode
        switch (type) {
            case ST_GLOBAL: {
                return Lex::syncToken(span, Lex::TK_KEYWORD, Lex::TK_END);
            }

            case ST_SCOPE: {
                return Lex::syncToken(span, Lex::TK_KEYWORD, Lex::TK_SCOPE_END);
            }

            case ST_SEEK_BLOCK_BEGIN: {
                return Lex::syncToken(span, Lex::TK_STATEMENT_BEGIN, Lex::TK_SCOPE_BEGIN);
            }

            case ST_FCN_NAME: {
                // TODO
                return Lex::syncToken(span, Lex::TK_KEYWORD, Lex::TK_END);
                break;
            }

            case ST_FCN_SCOPE_BEGIN: {
                // TODO
                return Lex::syncToken(span, Lex::TK_KEYWORD, Lex::TK_END);
                break;
            }

            case ST_ELSE_IF: {
                // TODO
                return Lex::syncToken(span, Lex::TK_KEYWORD, Lex::TK_END);
                break;
            }

            case ST_SCOPE_BEGIN: {
                // TODO
                return Lex::syncToken(span, Lex::TK_KEYWORD, Lex::TK_END);
                break;
            }
        }
    }

    Lex::Token parseScope(ParseContext* ctx, Span* span, const ScopeType type, const ScopeEnd end, StackMark dsmarkStart) {
        SpanEx    lspan = markSpanStart(span);
        StackMark csmark = markStack(&ctx->nodeStack);
        StackMark dsmark = isNull(dsmarkStart) ? markStack(&ctx->defStack) : dsmarkStart;

        Scope* currentScope = ctx->currentScope;
        int    idxInscope   = ctx->idxInScope;

        Lex::TokenValue tokenVal;
        Lex::Token token;

        Scope* node = currentScope;

        Pos prevPos = lspan.start;
        Lex::Token prevToken = Lex::toToken(Lex::TK_NONE);

        while (1) {
            token = Lex::nextToken(&lspan, &tokenVal);
            if (token.encoded < 0) goto defer;

            switch (token.kind) {

                case Lex::TK_STATEMENT_END : continue;

                case Lex::TK_END : {
                    if (type == SC_GLOBAL) {
                        goto defer;
                    }

                    span->end = prevPos;

                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_END_OF_FILE);
                    token = sync(&lspan, { Lex::TK_SCOPE_END });
                    goto defer;
                }

                case Lex::TK_SCOPE_BEGIN : {
                    Scope* scope = Ast::Node::makeScope();
                    scope->base.scope = currentScope;

                    ctx->currentScope = scope;
                    token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT);
                    ctx->currentScope = currentScope;

                    if (type == SC_GLOBAL) {
                        DArray::push(&ctx->nodeStack, (void**) &scope);
                    }

                    break;
                }

                case Lex::TK_SCOPE_END : {
                    if (type != SC_GLOBAL) {
                        // node->children = commitStack(&ctx->nodeStack, csmark, &node->childrenCount);
                        goto defer;
                    }

                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL);
                    token = sync(&lspan, { Lex::TK_END });
                    goto defer;
                }

                case Lex::TK_STRING : {
                    token = parsePrintLiteral(ctx, &lspan, &tokenVal);
                    break;
                }

                case Lex::TK_KEYWORD : {
                    switch ((Keyword) token.detail) {
                        case KW_LOOP: {
                            token = parseLoop(ctx, &lspan);
                            break;
                        }

                        case KW_RETURN: {
                            token = parseReturnStatement(ctx, &lspan);
                            break;
                        }

                        case KW_CONTINUE: {
                            token = parseContinueStatement(ctx, &lspan);
                            break;
                        }

                        case KW_BREAK: {
                            token = parseBreakStatement(ctx, &lspan);
                            break;
                        }

                        case KW_CASE: {
                            token = parseCaseStatement(ctx, &lspan);
                            break;
                        }

                        case KW_IMPORT: {
                            token = parseImport(ctx, &lspan);
                            break;
                        }

                        default: {
                            const char* str = Lex::keywordStringTable[token.detail];
                            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_ERROR,
                                Diag::Format {
                                    "Unexpected keyword '%s'.\n"
                                    "  This keyword cannot be used to start a statement in this scope."
                                }, str
                            );

                            token = sync(&lspan, SyncType::ST_SCOPE);
                        }
                    }

                    break;
                }

                case Lex::TK_DIRECTIVE : {
                    token = parseDirective(ctx, &lspan, (Lex::Directive) token.detail, NULL_FLAG);
                    break;
                }

                case Lex::TK_ARRAY_BEGIN : {
                    token = parseForeignScope(ctx, &lspan);
                    break;
                }

                case Lex::TK_IDENTIFIER : {
                    // bare statement or label
                    token = Lex::tryToken(&lspan, { Lex::TK_IDENTIFIER });
                    if (token.kind == Lex::TK_IDENTIFIER) {
                        token = parseLabel(ctx, &lspan, (QualifiedName*) tokenVal.any, End { Lex::TK_STATEMENT_END });
                    } else {
                        token = parseBareStatement(ctx, &lspan, prevPos, End { Lex::TK_STATEMENT_END });
                    }

                    break;
                }

                default : {
                    // bare statement or error
                    token = parseBareStatement(ctx, &lspan, prevPos, End { Lex::TK_STATEMENT_END });
                    break;
                }
            }

            if (end == SE_STATEMENT) {
                if (token.kind != Lex::TK_STATEMENT_END) {
                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL);
                    token = sync(&lspan, { Lex::TK_STATEMENT_END });
                }
                goto defer;
            }

            prevPos = lspan.end;
            prevToken = token;

        }

        defer:
        ctx->currentScope = currentScope;
        ctx->idxInScope   = idxInscope;

        DArray::push(&ctx->unit->reg->scopes, (void**) &node);

        node->children = commitStack(&ctx->nodeStack, csmark, &node->childrenCount);
        node->definitions = commitStack(&ctx->defStack, dsmark, &node->definitionCount);
        node->base.span = finalizeSpan(&lspan, span);

        return token;
    }

    Lex::Token parsePrintLiteral(ParseContext* ctx, Span* span, Lex::TokenValue* startTokenVal) {
        SpanEx    lspan = markSpanStart(span);
        StackMark smark = markStack(&ctx->nodeStack);

        Lex::Token token;
        Lex::TokenValue tokenVal;

        ctx->unit->ast->usedFunctionMask |= (1 << Ast::Internal::IF_PRINTF);

        Function* const fcn = Ast::Internal::functions + Ast::Internal::IF_PRINTF;

        FunctionCall* call = Ast::Node::makeFunctionCall();
        call->fcn = fcn;
        call->name.buff = fcn->name.buff;
        call->name.len = fcn->name.len;

        Variable* format = Ast::Node::makeVariable();
        format->base.scope = ctx->currentScope;
        format->base.span = getSpanStamp(&lspan);
        format->expression = (Expression*) startTokenVal->any;

        Variable* callWrapper = Ast::Node::makeVariable();
        callWrapper->base.scope = ctx->currentScope;
        callWrapper->expression = (Expression*) call;

        DArray::push(&ctx->nodeStack, &format);
        token = parseList(ctx, &lspan, Lex::TK_LIST_SEPARATOR, Lex::TK_STATEMENT_END);
        call->inArgs = (Variable**) commitStack(&ctx->nodeStack, smark, &call->inArgCount);

        DArray::push(&ctx->unit->reg->fcnCalls, &callWrapper);
        DArray::push(&ctx->nodeStack, &callWrapper); // scope

        callWrapper->base.span = finalizeSpan(&lspan, span);

        return token;
    }

    // TODO : think about dealloc
    Lex::Token parseLanguageTag(ParseContext* ctx, Span* span, String* tag) {
        Lex::Token token;
        Lex::TokenValue tokenVal;

        token = Lex::nextToken(span, &tokenVal);
        QualifiedName* qname = (QualifiedName*) tokenVal.any;

        if (token.kind != Lex::TK_IDENTIFIER) {
            // ndealloc(nalc, qname);
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
            return sync(span, { Lex::TK_ARRAY_END });
        }

        if (qname->pathSize != 0) {
            Diag::report(ctx->unit->ast, span, Err::QUALIFIED_NAME_NOT_ALLOWED);
            return sync(span, { Lex::TK_ARRAY_END });
        }

        tag->buff = qname->buff;
        tag->len = qname->len;

        // ndealloc(nalc, qname);

        token = Lex::nextToken(span, &tokenVal);
        if (token.kind != Lex::TK_ARRAY_END) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
            return sync(span, { Lex::TK_ARRAY_END });
        }

        return token;
    }

    Lex::Token parseForeignScope(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        String tag;
        Lex::Token token;

        token = parseLanguageTag(ctx, &lspan, &tag);
        if (token.encoded < 0) return token;

        token = Lex::nextToken(&lspan, NULL);
        if (token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
            token = sync(&lspan, SyncType::ST_GLOBAL);
            span->end = lspan.end;
            return token;
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

        DArray::push(&ctx->unit->reg->codeBlocks, &codeBlock);

        codeBlock->base.span = finalizeSpan(&lspan, span);

        return token;
    }

    Lex::Token parseLabel(ParseContext* ctx, Span* span, FullToken prev, NodeType nodeType, End end) {
        Lex::TokenValue tokenVal = prev.value;
        Lex::Token token = prev.token;

        QualifiedName* name = NULL;

        if (token.kind == Lex::TK_IDENTIFIER) {
            name = (QualifiedName*) tokenVal.any;
            token = Lex::nextToken(span);
        }

        if (!name || token.kind != Lex::TK_STATEMENT_BEGIN) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL,
                Diag::Format {
                    "Expected a %s name for the %s, but encountered an unexpected token %s."
                },
                Lex::toStr((Lex::TokenKind) token.kind),
                Ast::Node::str(nodeType), Lex::toStr((Lex::TokenKind) token.kind)
            );
        }

        switch (nodeType) {
            case NT_VARIABLE_DEFINITION: {
                return parseVarDefinition(ctx, span, name, end);
            }

            case NT_SWITCH_CASE: {
                return parseCaseStatement(ctx, span);
            }

            case NT_ENUMERATOR: {
                return parseEnumDefinition(ctx, span, name);
            }

            case NT_TYPE_DEFINITION: {
                token = Lex::nextToken(span);
                if (token.kind == Lex::TK_KEYWORD && (token.kind == KW_STRUCT || token.kind == KW_UNION)) {
                    return parseTypeDefinition(ctx, span, name,
                        token.kind == KW_STRUCT ? Type::DT_STRUCT : Type::DT_UNION);
                }
                break;
            }

            case NT_FUNCTION: {
                return parseFunction(ctx, span, name, NULL_FLAG);
            }
        }

        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_ERROR,
            "parseLabel called with unsupported node type: %s", Ast::Node::str(nodeType)
        );

        return sync(span, SyncType::ST_GLOBAL);
    }

    // Expects '<name>:' consumed
    Lex::Token parseLabel(ParseContext* ctx, Span* span, QualifiedName* name, End end) {
        Lex::TokenValue tokenVal;
        Lex::Token token = Lex::nextToken(span, &tokenVal);

        if (token.kind == Lex::TK_IDENTIFIER || Lex::isDtype(token)) {
            return parseVarDefinition(ctx, span, name, end);
        } else if (token.kind == Lex::TK_KEYWORD) {
            switch (token.detail) {
                case KW_CASE: {
                    return parseCaseStatement(ctx, span);
                }

                case KW_ENUM: {
                    return parseEnumDefinition(ctx, span, name);
                }

                case KW_UNION: {
                    return parseTypeDefinition(ctx, span, name, Type::DT_UNION);
                }

                case KW_STRUCT: {
                    return parseTypeDefinition(ctx, span, name, Type::DT_STRUCT);
                }
            }
        } else if (token.kind == Lex::TK_PARENTHESIS_BEGIN) {
            return parseFunction(ctx, span, name, NULL_FLAG);
        }

        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_ERROR,
            Diag::Format {
                "Expected a valid declaration (variable, function, struct, etc.) after label '%.*s', "
                "but encountered an unexpected token."
            },
            name->len, name->buff
        );

        return sync(span, SyncType::ST_GLOBAL);
    }

    // assignment or expression
    Lex::Token parseBareStatement(ParseContext* ctx, Span* span, const Pos startPos, const End endToken) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;

        Variable* lvar;
        token = parseExpression(ctx, &lspan, &lvar, startPos, endToken, ALLOW_UNEXPECTED_END);

        if (isEndToken(token, endToken)) {
            Statement* stmt = Ast::Node::makeStatement();
            stmt->base.scope = ctx->currentScope;
            stmt->operand = lvar;

            DArray::push(&ctx->nodeStack, &stmt);
            DArray::push(&ctx->unit->reg->statements, &stmt);

            stmt->base.span = finalizeSpan(&lspan, span);
            return token;
        }

        if (token.kind != Lex::TK_EQUAL) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "'='");
            token = sync(&lspan, { Lex::TK_EQUAL });
        };

        Variable* rvar = Ast::Node::makeVariable();
        token = parseRValue(ctx, &lspan, rvar, endToken);

        VariableAssignment* varAssignment = Ast::Node::makeVariableAssignment();
        varAssignment->base.span = span;
        varAssignment->base.scope = ctx->currentScope;
        varAssignment->lvar = lvar;
        varAssignment->rvar = rvar;

        DArray::push(&ctx->unit->reg->variableAssignments, &varAssignment);
        DArray::push(&ctx->nodeStack, &varAssignment);

        varAssignment->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseAssignment(ParseContext* ctx, Span* span, const Pos startPos, const End endToken) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;

        VariableAssignment* const varAssignment = Ast::Node::makeVariableAssignment();
        varAssignment->base.span = span;
        varAssignment->base.scope = ctx->currentScope;
        varAssignment->rvar = Ast::Node::makeVariable();
        varAssignment->rvar->base.scope = ctx->currentScope;

        token = parseExpression(ctx, &lspan, &(varAssignment->lvar), startPos, End { Lex::TK_EQUAL }, NULL_FLAG);

        if (token.kind != Lex::TK_EQUAL) {
            uint64_t val = Lex::toIntStr(Lex::EQUAL);
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, &val);
            token = sync(&lspan, { Lex::TK_EQUAL });
        }

        token = parseRValue(ctx, &lspan, varAssignment->rvar, endToken);

        DArray::push(&ctx->unit->reg->variableAssignments, (void*) &varAssignment);
        DArray::push(&ctx->nodeStack, (void*) &varAssignment);

        varAssignment->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Variable* makeVariable(VariableDefinition* def) {
        Variable* var = Ast::Node::makeVariable();
        var->def = def;
        var->base.scope = def->base.scope;

        return var;
    }

    // def->flags will be rewritten
    Lex::Token parseDataType(ParseContext* ctx, Span* span, FullToken prevToken, Flags flags, TypeSpecifier* type) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token = prevToken.token;
        Lex::TokenValue tokenVal = prevToken.value;

        if (Lex::isQualifier(token)) {
            if (!(flags & ALLOW_QUALIFIER)) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "Qualifier not expected here");
            }

            type->qualifier = Lex::toQualifier((Keyword) token.detail);
            token = Lex::nextToken(&lspan, &tokenVal);
        }

        if (Lex::isDtype(token)) {
            type->baseType = Lex::toDtype(token);
        } else if (token.kind == Lex::TK_IDENTIFIER) {
            type->baseType = Type::DT_UNDEFINED;
            type->baseName = (QualifiedName*) tokenVal.any;
        } else if (token.kind == Lex::TK_PARENTHESIS_BEGIN) {
            type->baseType = Type::DT_FUNCTION;
            parseFunctionPointer(ctx, &lspan, &type->baseFcn);
        } else {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL,
                "Expected a valid data type (ex 'i32', custom struct name or function pointer)."
            );
        }

        token = parseTypeDecorators(ctx, &lspan, type);

        type->span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseFunctionPointer(ParseContext* ctx, Span* const span, FunctionPrototype** fcnOut) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;
        Lex::TokenValue tokenVal;

        VariableDefinition* def;
        FunctionPrototype* fcn = Ast::Node::makeFunctionPrototype();

        token = Lex::nextToken(&lspan, NULL);
        if (token.kind != Lex::TK_PARENTHESIS_BEGIN) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
            token = sync(&lspan, SyncType::ST_GLOBAL);
            span->end = lspan.end;
            return token;
        }

        token = Lex::nextToken(&lspan, NULL);
        if (token.kind != Lex::TK_OP_ARROW) {
            StackMark smark = markStack(&ctx->nodeStack);

            while (1) {
                def = Ast::Node::makeVariableDefinition();
                def->base.scope = ctx->currentScope;
                def->var = makeVariable(def);
                DArray::push(&ctx->nodeStack, &def);

                token = parseDataType(ctx, &lspan, { token, tokenVal }, ALLOW_QUALIFIER, &def->type);

                if (token.kind == Lex::TK_LIST_SEPARATOR) {
                    token = Lex::nextToken(&lspan, NULL);
                    continue;
                } else if (token.kind == Lex::TK_OP_ARROW) {
                    break;
                } else {
                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL);
                    token = sync(&lspan, { Lex::TK_LIST_SEPARATOR, Lex::TK_OP_ARROW });
                    if (token.kind == Lex::TK_OP_ARROW) break;
                }
            }

            fcn->inArgs = (VariableDefinition**) commitStack(&ctx->nodeStack, smark, &fcn->inArgCount);
        }

        token = Lex::nextToken(&lspan, NULL);
        if (token.kind != Lex::TK_PARENTHESIS_END) {
            def = Ast::Node::makeVariableDefinition();
            def->base.scope = ctx->currentScope;
            def->var = makeVariable(def);

            token = parseDataType(ctx, &lspan, { token, tokenVal }, NULL_FLAG, &def->type);

            if (token.kind != Lex::TK_PARENTHESIS_END) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL);
                token = sync(&lspan, { Lex::TK_PARENTHESIS_END });
            }
        } else {
            def = Ast::Node::makeVariableDefinition();
        }

        fcn->outArg = def;
        *fcnOut = fcn;

        def->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseVarDefinition(ParseContext* ctx, Span* const span, QualifiedName* name, End end) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;
        Lex::TokenValue tokenVal;

        VariableDefinition* def = Ast::Node::makeVariableDefinition();
        def->base.scope = ctx->currentScope;
        def->var = makeVariable(def);
        def->var->name = *name;

        token = Lex::nextToken(&lspan, &tokenVal);
        token = parseDataType(ctx, &lspan, { token, tokenVal }, ALLOW_QUALIFIER, &def->type);

        if (token.kind == Lex::TK_EQUAL) {
            token = parseRValue(ctx, &lspan, def->var, end);
        }

        if (isEndToken(token, end)) {
            DArray::push(&ctx->defStack, &def);
            DArray::push(&ctx->nodeStack, &def);
            DArray::push(&ctx->unit->reg->variableDefinitions, &def);

            def->base.scope = ctx->currentScope;
            def->var->base.scope = ctx->currentScope;
            def->base.span = finalizeSpan(&lspan, span);
        }

        return token;
    }

    Lex::Token parseArrayInitialization(ParseContext* ctx, Span* span, ArrayInitialization** initOut) {
        *initOut = Ast::Node::makeArrayInitialization();
        StackMark smark = markStack(&ctx->nodeStack);
        Lex::Token token = parseList(ctx, span, Lex::TK_LIST_SEPARATOR, Lex::TK_ARRAY_END);
        (*initOut)->attributes = (Variable**) commitStack(&ctx->nodeStack, smark, &(*initOut)->attributeCount);
        return token;
    }

    Lex::Token parseTypeInitialization(ParseContext* ctx, Span* const span, TypeInitialization** outTypeInit) {
        SpanEx    lspan = markSpanStart(span);
        StackMark smark = markStack(&ctx->nodeStack);

        Lex::Token token;
        Lex::TokenValue tokenVal;

        TypeInitialization* typeInit = Ast::Node::makeTypeInitialization();

        token = Lex::nextToken(&lspan, &tokenVal);
        Lex::Token nextToken = Lex::peekToken(&lspan);

        if (nextToken.kind == Lex::TK_STATEMENT_BEGIN || token.kind == Lex::TK_THE_REST) {
            // names assumed

            int hasFillVar = 0;
            typeInit->fillVar = NULL;

            while (1) {
                Variable* var;

                if (token.kind == Lex::TK_THE_REST) {

                    if (hasFillVar) {
                        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Fill the rest symbol can be used only once per initialization!");
                        token = sync(&lspan, { Lex::TK_SCOPE_END });
                        break;
                    }

                    var = Ast::Node::makeVariable();
                    var->base.scope = ctx->currentScope;
                    var->base.span = getSpanStamp(&lspan);

                    typeInit->fillVar = var;
                    hasFillVar = 1;

                } else if (token.kind == Lex::TK_IDENTIFIER) {

                    var = Ast::Node::makeVariable();
                    var->base.scope = ctx->currentScope;
                    var->base.span = getSpanStamp(&lspan);
                    var->name.buff = tokenVal.str->buff;
                    var->name.len = tokenVal.str->len;
                    var->name.span = var->base.span;
                    DArray::push(&ctx->nodeStack, &var);

                } else {

                    // ERROR
                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "Attribute name expected!");
                    token = sync(&lspan, { Lex::TK_SCOPE_END });
                    break;

                }

                token = Lex::nextToken(&lspan, NULL);
                if (token.kind != Lex::TK_STATEMENT_BEGIN) {
                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, Lex::toStr(Lex::TK_STATEMENT_BEGIN));
                    token = sync(&lspan, { Lex::TK_STATEMENT_BEGIN, Lex::TK_SCOPE_END });
                }

                token = parseExpression(ctx, &lspan, var, INVALID_POS, End { Lex::TK_LIST_SEPARATOR, Lex::TK_SCOPE_END });

                if (token.kind == Lex::TK_SCOPE_END) break;
                token = Lex::nextToken(&lspan, &tokenVal);
            }

        } else {

            lspan.end = span->start;
            while (1) {
                Variable* var;
                token = parseExpression(ctx, &lspan, &var, INVALID_POS, End { Lex::TK_LIST_SEPARATOR, Lex::TK_SCOPE_END });

                DArray::push(&ctx->nodeStack, &var);

                if (token.kind == Lex::TK_SCOPE_END) break;
            }

        }
        typeInit->attributes = (Variable**) commitStack(&ctx->nodeStack, smark, &typeInit->attributeCount);

        typeInit->idxs = (int*) nalloc(nalc, AT_BYTE, typeInit->attributeCount * sizeof(int));
        if (!typeInit->idxs) {
            Diag::report(ctx->unit->ast, NULL, Err::MALLOC);
        }

        *outTypeInit = typeInit;

        span->start = lspan.super;
        span->end = lspan.end;

        return token;
    }

    // either alloc or expression
    // alloc [DtypeName, omitted if mainDtype >= 0] ['[' Expression defining length ']'] [:] [DtypeInit]
    Lex::Token parseRValue(ParseContext* ctx, Span* const span, Variable* outVar, const End endToken) {
        Lex::Token token;
        Lex::TokenValue tokenVal;

        Pos startPos = span->end;

        token = Lex::nextToken(span, NULL);
        if (Lex::isKeyword(token, KW_ALLOC)) {
            Pos startPos = span->end;

            Alloc* alloc = Ast::Node::makeAlloc();

            VariableDefinition* varDef = alloc->def;
            varDef->base.scope = ctx->currentScope;
            varDef->base.span = getSpanStamp(span);

            Variable* var = alloc->def->var;

            token = Lex::nextToken(span, &tokenVal);
            if (token.kind == Lex::TK_IDENTIFIER) {
                varDef->type.baseName = Ast::Node::makeQualifiedName();
                varDef->type.baseName->buff = tokenVal.str->buff;
                varDef->type.baseName->len  = tokenVal.str->len;
                varDef->type.baseName->span = getSpanStamp(span);
            } else if (token.kind == Lex::TK_KEYWORD) {
                if (!Lex::isDtype(token)) {
                    Diag::report(ctx->unit->ast, span, Err::INVALID_DATA_TYPE, "TODO : data type expected, no place for generic keyword!");
                    return sync(span, { Lex::TK_STATEMENT_END });
                }

                var->value.type = Type::basicTypes + Lex::toDtype((Keyword) token.detail);
            } else {
                if (outVar->def) {
                    varDef->type.baseName = Ast::Node::makeQualifiedName();
                    varDef->type.baseName = outVar->def->type.baseName;
                    varDef->type.baseName->span = getSpanStamp(span);
                }

                span->end = startPos;
            }

            varDef->var = makeVariable(varDef);
            token = parseTypeDecorators(ctx, span, &varDef->type);

            if (token.kind == Lex::TK_STATEMENT_BEGIN) {
                token = parseExpression(ctx, span, var, INVALID_POS, endToken);
            } else if (token.kind != Lex::TK_STATEMENT_END) {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "TODO error: parseRValue unexpected symbol!");
                return sync(span, { Lex::TK_STATEMENT_END });
            }

            outVar->expression = (Expression*) alloc;
            DArray::push(&ctx->unit->reg->allocations, &outVar);

            outVar->base.flags |= IS_ALLOCATION;
        } else {
            outVar->base.scope = ctx->currentScope;
            token = parseExpression(ctx, span, outVar, startPos, endToken);
        }

        return token;
    }

    Lex::Token parseTypeDecorators(ParseContext* ctx, Span* const span, TypeSpecifier* spec) {
        Lex::Token token;
        Lex::TokenValue tokenVal;

        StackMark smark = markStack(&ctx->nodeStack);

        while (1) {
            token = Lex::nextToken(span, &tokenVal);

            if (token.kind == Lex::TK_POINTER) {
                TypeDecorator* dec = (TypeDecorator*) alloc(alc, sizeof(TypeDecorator));
                dec->kind = TypeDecorator::DEC_POINTER;
                dec->span = getSpanStamp(span);

                DArray::push(&ctx->nodeStack, &dec);
            } else if (token.kind == Lex::TK_ARRAY_BEGIN) {
                Pos arrStart = span->end;

                TypeDecorator* dec = (TypeDecorator*) alloc(alc, sizeof(TypeDecorator));
                dec->span = getSpanStamp(span);

                token = Lex::nextToken(span, &tokenVal);

                if (token.kind == Lex::TK_KEYWORD) {
                    // Slice with qualifier ex. [const]
                    const int keyword = token.detail;

                    token = Lex::nextToken(span, NULL);
                    if (token.kind != Lex::TK_ARRAY_END) {
                        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
                        return sync(span, { Lex::TK_ARRAY_END });
                    }

                    dec->kind = TypeDecorator::Kind::DEC_ARRAY;
                    if (keyword == KW_CONST) {
                        dec->flags = IS_CONST;
                    } else if (keyword == KW_FLUID) {
                        dec->flags = IS_ARRAY_LIST;
                    } else if (keyword == KW_ALLOC) {
                        dec->flags = IS_DYNAMIC;
                    } else {
                        dec->flags = 0;
                    }
                } else if (token.kind == Lex::TK_ARRAY_END) {
                    // Empty is fixed-size but has to be compile time resolved
                    dec->kind  = TypeDecorator::DEC_ARRAY;
                    dec->flags = 0;
                } else {
                    // Fixed-size array
                    Variable* lenVar = Ast::Node::makeVariable();
                    lenVar->base.span  = getSpanStamp(span);
                    lenVar->base.scope = ctx->currentScope;

                    token = parseExpression(ctx, span, lenVar, arrStart, End { Lex::TK_ARRAY_END }, EMPTY_EXPRESSION_ALLOWED);

                    dec->kind = TypeDecorator::DEC_ARRAY;
                    dec->len  = lenVar;
                }

                dec->span->end = span->end;
                DArray::push(&ctx->nodeStack, &dec);

            } else {
                break;
            }
        }

        spec->decorators = (TypeDecorator**) commitStack(&ctx->nodeStack, smark, &spec->decoratorCount);
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
    Lex::Token parseFunction(ParseContext* ctx, Span* const span, QualifiedName* name, Flags flags) {
        SpanEx lspan = markSpanStart(span);
        StackMark csmark;
        StackMark dsmark;

        Lex::TokenValue tokenVal;
        Lex::Token token;

        Function* fcn;

        if (token.kind == Lex::TK_ARRAY_BEGIN) {
            fcn = (Function*) Ast::Node::makeForeignFunction();

            Span* tagLoc = getSpanStamp(&lspan);

            String tag;
            token = parseLanguageTag(ctx, &lspan, &tag);

            ((ForeignFunction*) fcn)->code.tagStr = { tag.buff, tag.len };
            ((ForeignFunction*) fcn)->code.tagLoc = tagLoc;
        } else {
            fcn = Ast::Node::makeFunction();
        }

        fcn->base.flags |= ctx->foreignContext ? IS_EXTERN : 0;
        fcn->base.scope = ctx->currentScope;
        fcn->errorSetName = NULL;
        fcn->bodyScope = NULL;



        // Identifier Binding
        // ---

        if (name != NULL && name->buff != NULL) {
            fcn->name = *name;
        } else if (token.kind == Lex::TK_IDENTIFIER) {
            fcn->name = *((QualifiedName*) tokenVal.any);
            token = Lex::nextToken(&lspan, &tokenVal);
        } else {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "Function name expected!");
            token = sync(&lspan, SyncType::ST_FCN_NAME, &tokenVal);

            if (token.kind == Lex::TK_IDENTIFIER) {
                fcn->name = *((QualifiedName*)tokenVal.any);
                token = Lex::nextToken(&lspan, &tokenVal);
            } else {
                fcn->name.buff = NULL;
                fcn->name.len  = 0;
            }
        }

        fcn->name.span = getSpanStamp(&lspan);

        Scope* currentScope = ctx->currentScope;
        ctx->currentFunction = fcn;



        // Input Args
        // ---

        Scope* paramScope = Ast::Node::makeScope();
        paramScope->base.scope = currentScope;
        ctx->currentScope = paramScope;

        token = Lex::nextToken(&lspan);
        if (token.kind != Lex::TK_PARENTHESIS_BEGIN) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "'(' expected for parameter list");
            // TODO: add also '->' to sync
            token = sync(&lspan, { Lex::TK_PARENTHESIS_BEGIN }, { Lex::TK_SCOPE_BEGIN });
        }

        if (token.kind == Lex::TK_PARENTHESIS_BEGIN) {
            StackMark csmark = markStack(&ctx->nodeStack);
            StackMark dsmark = markStack(&ctx->defStack);

            while (true) {
                token = Lex::nextToken(&lspan, &tokenVal);
                if (token.kind == Lex::TK_PARENTHESIS_END) break;

                token = parseLabel(ctx, &lspan, { token, tokenVal }, NT_VARIABLE_DEFINITION,
                    End{ Lex::TK_PARENTHESIS_END, Lex::TK_LIST_SEPARATOR }
                );

                VariableDefinition* def = *(VariableDefinition**) DArray::getLast(&ctx->nodeStack);
                def->base.flags |= IS_UNORDERED;

                if (token.kind == Lex::TK_PARENTHESIS_END) break;
            }

            fcn->prototype.inArgs = (VariableDefinition**) commitStack(&ctx->nodeStack, csmark, &fcn->prototype.inArgCount);
            paramScope->definitions = commitStack(&ctx->defStack, dsmark, &paramScope->definitionCount);
        }



        // Optional Error Set 'using ErrorSet'
        // ---

        token = Lex::nextToken(&lspan, &tokenVal);
        if (Lex::isKeyword(token, KW_USING)) {
            fcn->errorSetName = Ast::Node::makeQualifiedName();
            token = Lex::nextToken(&lspan, &tokenVal);
            if (token.encoded < 0) return token;

            Using* tmp = Ast::Node::makeUsing();
            tmp->var = (SyntaxNode*) fcn;

            token = Lex::nextToken(&lspan, &tokenVal);
        }



        // Optional return '-> Type'
        //

        fcn->prototype.outArg = NULL;

        if (token.kind == Lex::TK_OP_ARROW) {
            fcn->prototype.outArg = Ast::Node::makeVariableDefinition();
            token = Lex::nextToken(&lspan, &tokenVal);

            token = parseDataType(ctx, &lspan, { token, tokenVal }, NULL_FLAG, &fcn->prototype.outArg->type);
        }



        // Body Scope (if not a function prototype)
        // NOTE: function prototype allowed exclusively for 'foreign' functions
        // ---

        const bool isPrototypeOnly = token.kind == Lex::TK_STATEMENT_END && ctx->foreignContext;

        if (!isPrototypeOnly) {
            // TODO: to a function isScopeBegin()
            if (token.kind != Lex::TK_SCOPE_BEGIN && token.kind != Lex::TK_STATEMENT_BEGIN) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "'{' or ':' expected for function body");
                token = sync(&lspan, SyncType::ST_FCN_SCOPE_BEGIN);
            }

            StackMark dsmark = markStack(&ctx->defStack);

            // Populate args into the scope lookup
            for (uint32_t i = 0; i < fcn->prototype.inArgCount; i++) {
                DArray::push(&ctx->defStack, &fcn->prototype.inArgs[i]);
            }

            const ScopeEnd scopeEnd = (ScopeEnd)(token.kind == Lex::TK_STATEMENT_BEGIN);

            Scope* bodyScope = Ast::Node::makeScope();
            bodyScope->base.scope = paramScope;
            ctx->currentScope = bodyScope;

            token = parseScope(ctx, &lspan, SC_COMMON, scopeEnd, dsmark);
            fcn->bodyScope = bodyScope;
        }



        ctx->currentScope = currentScope;
        ctx->currentFunction = NULL;

        DArray::push(&ctx->nodeStack, &fcn);
        DArray::push(&ctx->defStack, &fcn);
        DArray::push(&ctx->unit->reg->fcns, &fcn);

        fcn->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseIfStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenValue tokenVal;
        Lex::Token token;

        Scope* currentScope = ctx->currentScope;

        Scope* newScope = Ast::Node::makeScope();
        newScope->base.scope = ctx->currentScope;
        setDefinitionIdx(ctx, &newScope->base);

        Variable* newOperand;
        token = parseExpression(ctx, &lspan, &newOperand, INVALID_POS, End { Lex::TK_STATEMENT_BEGIN, Lex::TK_SCOPE_BEGIN });

        const ScopeEnd scopeEnd = (ScopeEnd) (token.kind == Lex::TK_STATEMENT_BEGIN);

        ctx->currentScope = newScope;
        token = parseScope(ctx, &lspan, SC_COMMON, scopeEnd);
        ctx->currentScope = currentScope;

        Branch* branch = Ast::Node::makeBranch();
        branch->base.scope = currentScope;

        StackMark smark = markStack(&ctx->nodeStack);

        DArray::push(&ctx->nodeStack, &newScope);
        DArray::push(&ctx->nodeStack, &newOperand);
        DArray::push(&ctx->unit->reg->branchExpressions, &newOperand);

        bool hasElse = false;
        while (1) {
            token = Lex::tryKeyword(&lspan, KW_ELSE);
            if (token.kind != Lex::TK_KEYWORD) break;

            token = Lex::nextToken(&lspan, &tokenVal);
            if (token.kind == Lex::TK_SCOPE_BEGIN || token.kind == Lex::TK_STATEMENT_BEGIN) {
                // else case

                hasElse = true;

                Scope* newScope = Ast::Node::makeScope();
                newScope->base.scope = currentScope;
                setDefinitionIdx(ctx, &newScope->base);

                ScopeEnd scopeEnd = (ScopeEnd) (token.kind == Lex::TK_STATEMENT_BEGIN);

                ctx->currentScope = newScope;
                token = parseScope(ctx, &lspan, SC_COMMON, scopeEnd);
                ctx->currentScope = currentScope;

                DArray::push(&ctx->nodeStack, &newScope);

                break;
            }

            // else if case

            if (!Lex::isKeyword(token, KW_IF)) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "if, ':' or '{'");
                token = sync(&lspan, SyncType::ST_ELSE_IF);
            }

            Variable* newOperand;
            token = parseExpression(ctx, &lspan, &newOperand, INVALID_POS, End { Lex::TK_STATEMENT_BEGIN, Lex::TK_SCOPE_BEGIN });

            // TODO
            newOperand->base.span = getSpanStamp(&lspan);

            Scope *newScope = Ast::Node::makeScope();
            newScope->base.scope = ctx->currentScope;
            setDefinitionIdx(ctx, &newScope->base);

            ScopeEnd scopeEnd = (ScopeEnd) (token.kind == Lex::TK_STATEMENT_BEGIN);
            ctx->currentScope = newScope;
            token = parseScope(ctx, &lspan, SC_COMMON, scopeEnd);
            ctx->currentScope = currentScope;

            DArray::push(&ctx->nodeStack, &newScope);
            DArray::push(&ctx->nodeStack, &newOperand);
            DArray::push(&ctx->unit->reg->branchExpressions, &newOperand);
        }

        // we need to manually resolve 'nodeStack'
        const int count = (ctx->nodeStack.size - smark) / 2;
        SyntaxNode** buffer = ((SyntaxNode**) ctx->nodeStack.buffer) + smark;

        branch->scopes = (Scope**) alloc(alc, sizeof(SyntaxNode**) * (count + hasElse));
        branch->expressions = (Variable**) alloc(alc, sizeof(SyntaxNode**) * count);
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

    Lex::Token parseCaseStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenValue tokenVal;
        Lex::Token token;

        SwitchCase* switchCase = Ast::Node::makeSwitchCase();
        switchCase->base.scope = ctx->currentScope;
        switchCase->elseCase = NULL;

        Variable* var;
        token = parseExpression(ctx, &lspan, &var, INVALID_POS, End { Lex::TK_STATEMENT_BEGIN, Lex::TK_SCOPE_BEGIN });

        switchCase->switchExp = var;

        int asStatement = token.kind == Lex::TK_STATEMENT_BEGIN;
        int elseCase = 0;
        int atLeastOneCase = 0;
        StackMark smark = markStack(&ctx->nodeStack);
        while (1) {

            if (!asStatement) {
                token = Lex::nextToken(&lspan);
                if (token.kind == Lex::TK_SCOPE_END) break;
            } else {
                token = Lex::tryKeyword(&lspan, KW_CASE);
                if (token.kind != Lex::TK_KEYWORD) {
                    token = Lex::tryKeyword(&lspan, KW_ELSE);
                    if (token.kind != Lex::TK_KEYWORD) break;
                }
            }

            elseCase = token.detail == KW_ELSE;
            atLeastOneCase = 1;

            Variable* cmpExp;
            if (!elseCase) {
                token = parseExpression(ctx, &lspan, &cmpExp, INVALID_POS, End { Lex::TK_STATEMENT_BEGIN, Lex::TK_SCOPE_BEGIN });
            } else {
                if (!atLeastOneCase) {
                    Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "Else case can't be the first case!");
                }
                token = Lex::nextToken(&lspan);
            }

            Scope* sc = Ast::Node::makeScope();
            sc->base.scope = ctx->currentScope;
            // setParentIdx(sc);

            if (token.kind == Lex::TK_STATEMENT_BEGIN) {
                token = parseScope(ctx, &lspan, SC_COMMON, SE_STATEMENT);
            } else if (token.kind == Lex::TK_SCOPE_BEGIN) {
                token = parseScope(ctx, &lspan, SC_COMMON, SE_DEFAULT);
            } else {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL);
                token = sync(&lspan, SyncType::ST_SCOPE_BEGIN);
            }

            if (elseCase) {
                switchCase->elseCase = sc;
                break;
            }

            DArray::push(&ctx->nodeStack, &sc);
            DArray::push(&ctx->nodeStack, &cmpExp);

        }

        // we need to manually resolve 'nodeStack'
        const int count = (ctx->nodeStack.size - smark) / 2;
        SyntaxNode** buffer = ((SyntaxNode**) ctx->nodeStack.buffer) + smark;

        switchCase->cases = (Scope**) alloc(alc, sizeof(SyntaxNode**) * count);
        switchCase->casesExp = (Variable**) alloc(alc, sizeof(SyntaxNode**) * count);
        switchCase->caseCount = count;
        switchCase->caseExpCount = count;
        for (int i = 0; i < count; i++) {
            switchCase->cases[i] = (Scope*) buffer[2 * i];
            switchCase->casesExp[i] = (Variable*) buffer[2 * i + 1];
        }

        ctx->nodeStack.size = smark;

        DArray::push(&ctx->unit->reg->switchCases, &switchCase);
        DArray::push(&ctx->nodeStack, &switchCase);

        switchCase->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseWhileLoop(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;

        WhileLoop* loop = Ast::Node::makeWhileLoop();

        Scope* currentScope = ctx->currentScope;
        Scope* newScope = Ast::Node::makeScope();
        newScope->base.scope = ctx->currentScope;
        // setParentIdx(newScope);

        Variable* newOperand;
        ctx->currentScope = newScope;
        token = parseExpression(ctx, &lspan, &newOperand, INVALID_POS, End { Lex::TK_SCOPE_BEGIN, Lex::TK_STATEMENT_BEGIN });
        ctx->currentScope = currentScope;

        SyntaxNode* tmpLoop = ctx->currentLoop;
        ctx->currentLoop = (SyntaxNode*) loop;

        ScopeEnd scopeEnd = (ScopeEnd) (token.kind == Lex::TK_STATEMENT_BEGIN);

        ctx->currentScope = newScope;
        token = parseScope(ctx, &lspan, SC_COMMON, scopeEnd);
        ctx->currentScope = currentScope;

        ctx->currentLoop = tmpLoop;

        loop->base.scope = ctx->currentScope;
        loop->bodyScope = newScope;
        loop->expression = newOperand;

        DArray::push(&ctx->nodeStack, &loop);

        loop->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    // returns 'start' expression if its not a range via outLeftExp
    // we have a precedency over end token...
    Lex::Token parseRangeExpression(ParseContext* ctx, Span* span, End end, RangeExpression** range, Variable** outLeftExp) {
        Lex::Token token;

        Variable* leftExp;

        token = Lex::peekToken(span);
        if (token.kind != Lex::TK_RANGE) {
            if (isEndToken(token, end)) {
                return Lex::nextToken(span);
            }

            token = parseExpression(ctx, span, &leftExp, INVALID_POS, { Lex::TK_RANGE },
                ALLOW_UNEXPECTED_END | EMPTY_EXPRESSION_ALLOWED);

            if (token.kind != Lex::TK_RANGE) {
                if (outLeftExp) *outLeftExp = leftExp;
                return token;
            }
        } else {
            leftExp = NULL;
            token = Lex::nextToken(span);
        }

        // second expression could be 'eidx' OR 'step'
         Variable* secondExp = NULL;
         token = parseExpression(ctx, span, &secondExp, INVALID_POS,
             { Lex::TK_RANGE, Lex::TK_STATEMENT_BEGIN, Lex::TK_SCOPE_BEGIN },
             ALLOW_UNEXPECTED_END | EMPTY_EXPRESSION_ALLOWED | USE_KEYWORD_AS_END);

         Variable* stepExp  = NULL;
         Variable* rightExp = NULL;

         if (token.kind == Lex::TK_RANGE) {
             stepExp = secondExp;

             token = parseExpression(ctx, span, &rightExp, INVALID_POS,
                 { Lex::TK_STATEMENT_BEGIN, Lex::TK_SCOPE_BEGIN }, USE_KEYWORD_AS_END);
         } else {
             rightExp = secondExp;
         }

         *range = (RangeExpression*) alloc(alc, sizeof(RangeExpression));
         (*range)->bidx = leftExp;
         (*range)->step = stepExp;
         (*range)->eidx = rightExp;

        return token;
    }

    Lex::Token parseLoop(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenValue tokenVal;
        Lex::Token token;

        Loop* loop = Ast::Node::makeLoop();

        Scope* currentScope = ctx->currentScope;
        Scope* outerScope = Ast::Node::makeScope();
        outerScope->base.scope = ctx->currentScope;
        loop->base.scope = outerScope;

        ctx->currentScope = outerScope;
        StackMark smark = markStack(&ctx->nodeStack);
        StackMark dmark = markStack(&ctx->defStack);

        // TODO : check to a function
        End end = { Lex::TK_STATEMENT_BEGIN, Lex::TK_SCOPE_BEGIN };

        // Arg - Range, Condition or Array expression
        Variable* exp = NULL;
        token = parseRangeExpression(ctx, &lspan, { Lex::TK_SCOPE_BEGIN, Lex::TK_STATEMENT_BEGIN }, &loop->arg.range, &exp);
        if (exp) {
            loop->arg.exp = exp;
            loop->arg.kind = Loop::Arg::EXPRESSION;
        } else {
            loop->arg.kind = Loop::Arg::RANGE;
        }

        if (isEndToken(token, end))            goto parseBody;
        else if (Lex::isKeyword(token, KW_AS)) goto parseAs;
        else if (Lex::isKeyword(token, KW_AT)) goto parseAt;
        else goto errorExit;

        parseAs: {
            // As - [&]name
            token = Lex::nextToken(&lspan, &tokenVal);

            if (token.kind == Lex::TK_OP_AND) {
                loop->item = Ast::Node::makeVariable();
                loop->item->expression = (Expression*) Ast::Node::makeUnaryExpression();
                ((OperationExpression*) loop->item->expression)->opType = OP_BITWISE_AND;

                token = Lex::nextToken(&lspan, &tokenVal);
            }

            if (token.kind == Lex::TK_IDENTIFIER) {
                if (loop->item) {
                    UnaryExpression* uex = (UnaryExpression*) loop->item->expression;
                    uex->operand->name = *(QualifiedName*) tokenVal.any;
                } else {
                    loop->item = Ast::Node::makeVariable();
                    loop->item->name = *(QualifiedName*) tokenVal.any;
                }

                token = Lex::nextToken(&lspan, &tokenVal);
            }

            if (isEndToken(token, end))               goto parseBody;
            else if (Lex::isKeyword(token, KW_AT))    goto parseAt;
            else goto errorExit;
        }

        parseAt: {
            // At - [&]index[:type]
            token = Lex::nextToken(&lspan, &tokenVal);

            if (token.kind == Lex::TK_OP_AND) {
                loop->index.var = Ast::Node::makeVariable();
                token = Lex::nextToken(&lspan, &tokenVal);

            }

            if (token.kind == Lex::TK_IDENTIFIER) {
                QualifiedName* name = (QualifiedName*) tokenVal.any;
                token = Lex::nextToken(&lspan, &tokenVal);

                if (loop->index.var) {
                    loop->index.var->name = *name;
                } else {
                    // Check if an explicit type is declared: `at i : Type :` or `at i : Type {`
                    bool hasExplicitType = false;

                    if (token.kind == Lex::TK_STATEMENT_BEGIN) {
                        Lex::Token nextTok  = Lex::peekToken(&lspan);
                        Lex::Token afterTok = Lex::peekNthToken(&lspan, NULL, 2);

                        if (afterTok.kind == Lex::TK_STATEMENT_BEGIN || afterTok.kind == Lex::TK_SCOPE_BEGIN) {
                            hasExplicitType = true;
                        }
                    }

                    if (hasExplicitType) {
                        parseVarDefinition(ctx, &lspan, name, end);
                        VariableDefinition* def = *(VariableDefinition**) DArray::getLast(&ctx->nodeStack);
                        loop->index.def = def;
                    } else {
                        loop->index.def = Ast::Node::makeVariableDefinition();
                        loop->index.def->var = makeVariable(loop->index.def);
                        loop->index.def->var->name = *name;
                        loop->index.def->type.baseType = Type::DT_U64;
                        loop->index.def->type.qualifier = Type::Q_NONE;
                        loop->index.def->type.decoratorCount = 0;
                    }
                }
            }

            if (isEndToken(token, end)) goto parseBody;
            else                        goto errorExit;
        }

        parseBody: {
            outerScope->children = commitStack(&ctx->nodeStack, smark, &outerScope->childrenCount);
            outerScope->definitions = commitStack(&ctx->defStack, dmark, &outerScope->definitionCount);

            loop->bodyScope = Ast::Node::makeScope();
            loop->bodyScope->base.scope = loop->base.scope;

            SyntaxNode* tmpLoop = ctx->currentLoop;
            ctx->currentLoop = (SyntaxNode*) loop;

            ScopeEnd scopeEnd = (ScopeEnd) (token.kind == Lex::TK_STATEMENT_BEGIN);
            ctx->currentScope = loop->bodyScope;
            token = parseScope(ctx, &lspan, SC_COMMON, scopeEnd);
            ctx->currentScope = currentScope;

            ctx->currentLoop = tmpLoop;

            DArray::push(&ctx->unit->reg->loops, &loop);
            DArray::push(&ctx->nodeStack, &loop);

            loop->base.span = finalizeSpan(&lspan, span);
        }

        exit: {
            return token;
        }

        errorExit: {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL,
                "Unexpected token '%.*s' in loop declaration.", tokenVal.str->len, tokenVal.str->buff);
            return sync(&lspan, ST_SEEK_BLOCK_BEGIN);
        }
    }

    Lex::Token parseGotoStatement(ParseContext* ctx, Span* const span) {
        Lex::TokenValue tokenVal;
        Lex::Token token = Lex::nextToken(span, &tokenVal);

        GotoStatement* gt = Ast::Node::makeGotoStatement();
        gt->span = getSpanStamp(span);
        gt->base.scope = ctx->currentScope;
        gt->label = NULL;

        DArray::push(&ctx->unit->reg->gotos, &gt);
        DArray::push(&ctx->nodeStack, &gt);

        if (token.kind != Lex::TK_IDENTIFIER) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Identifier");
            return sync(span, { Lex::TK_STATEMENT_END });
        }

        gt->name.buff = tokenVal.str->buff;
        gt->name.len = tokenVal.str->len;

        return token;
    }
    Lex::Token parseContinueStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenValue tokenVal;
        Lex::Token token = Lex::nextToken(&lspan, &tokenVal);

        SyntaxNode* targetNode = NULL;

        if (token.kind == Lex::TK_IDENTIFIER) {
            Variable* labelVar = Ast::Node::makeVariable();
            labelVar->name = *((QualifiedName*) tokenVal.any);
            labelVar->base.span = getSpanStamp(&lspan);
            targetNode = (SyntaxNode*) labelVar;

            token = Lex::nextToken(&lspan, &tokenVal);
        }

        if (token.kind != Lex::TK_STATEMENT_END) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "';' expected after continue statement");
            token = sync(&lspan, { Lex::TK_STATEMENT_END });
        }

        ContinueStatement* stmt = Ast::Node::makeContinueStatement();
        stmt->base.scope = ctx->currentScope;
        stmt->target     = targetNode;

        DArray::push(&ctx->nodeStack, &stmt);

        stmt->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseBreakStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenValue tokenVal;
        Lex::Token token = Lex::nextToken(&lspan, &tokenVal);

        SyntaxNode* targetNode = NULL;

        if (token.kind == Lex::TK_IDENTIFIER) {
            Variable* labelVar = Ast::Node::makeVariable();
            labelVar->name = *((QualifiedName*) tokenVal.any);
            labelVar->base.span = getSpanStamp(&lspan);
            targetNode = (SyntaxNode*) labelVar;

            token = Lex::nextToken(&lspan, &tokenVal);
        }

        if (token.kind != Lex::TK_STATEMENT_END) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "';' expected after break statement");
            token = sync(&lspan, { Lex::TK_STATEMENT_END });
        }

        BreakStatement* stmt = Ast::Node::makeBreakStatement();
        stmt->base.scope = ctx->currentScope;
        stmt->target     = targetNode;

        DArray::push(&ctx->nodeStack, &stmt);

        stmt->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseNamespace(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Scope* currentScope = ctx->currentScope;

        Lex::TokenValue tokenVal;
        Lex::Token token = Lex::nextToken(&lspan, &tokenVal);

        Namespace* nsc = Ast::Node::makeNamespace();
        nsc->scope.base.scope = ctx->currentScope;
        nsc->name.buff = tokenVal.str->buff;
        nsc->name.len = tokenVal.str->len;
        // setParentIdx(nsc);

        token = Lex::nextToken(&lspan);
        if (token.kind != Lex::TK_SCOPE_BEGIN && token.kind != Lex::TK_STATEMENT_BEGIN) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
            return sync(&lspan, SyncType::ST_SCOPE_BEGIN);
        }

        ScopeEnd scopeEnd = (ScopeEnd) (token.kind == Lex::TK_STATEMENT_BEGIN);

        ctx->currentScope = (Scope*) nsc;
        token = parseScope(ctx, &lspan, SC_COMMON, scopeEnd);
        ctx->currentScope = currentScope;

        DArray::push(&ctx->nodeStack, &nsc);
        DArray::push(&ctx->defStack, &nsc);

        nsc->scope.base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseAllocStatement(ParseContext* ctx, Span* const span) {

        return Lex::Token { Lex::TK_END };

    }

    Lex::Token parseFreeStatement(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;

        Free* freeEx = Ast::Node::makeFree();
        freeEx->var->base.scope = ctx->currentScope;

        token = parseExpression(ctx, &lspan, freeEx->var, INVALID_POS, End { Lex::TK_STATEMENT_END });

        Variable* wrapper = Ast::Node::makeVariable();
        wrapper->base.scope = ctx->currentScope;
        wrapper->expression = (Expression*) freeEx;
        wrapper->base.span = getSpanStamp(&lspan);

        Statement* st = Ast::Node::makeStatement();
        st->base.scope = ctx->currentScope;
        st->operand = wrapper;

        DArray::push(&ctx->nodeStack, &st);

        token = Lex::nextToken(&lspan);
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

        // ret->idx = ctx->currentFunction->returnCount;

        DArray::push(&ctx->unit->reg->returnStatements, &ret);
        DArray::push(&ctx->nodeStack, &ret);

        Pos startPos = lspan.end;

        token = Lex::nextToken(&lspan);
        if (token.kind == Lex::TK_STATEMENT_END) {
            ret->base.span = finalizeSpan(&lspan, span);
            return token;
        }

        if (token.kind != Lex::TK_SKIP) {
            Variable* newVar;
            token = parseExpression(ctx, &lspan, &newVar, startPos, End { Lex::TK_LIST_SEPARATOR, Lex::TK_STATEMENT_END });

            // TODO
            newVar->base.span = getSpanStamp(&lspan);

            ret->var = newVar;
        } else {
            token = Lex::nextToken(&lspan);
        }

        if (token.kind == Lex::TK_STATEMENT_END) {
            ret->base.span = finalizeSpan(&lspan, span);
            return token;
        }

        if (token.kind != Lex::TK_LIST_SEPARATOR) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, Lex::toStr(Lex::TK_LIST_SEPARATOR));
            token = sync(span, { Lex::TK_LIST_SEPARATOR }, { Lex::TK_STATEMENT_END });
            ret->base.span = finalizeSpan(&lspan, span);
            return token;
        }

        // error case
        Variable* newVar;
        token = parseExpression(ctx, &lspan, &newVar, INVALID_POS, End { Lex::TK_STATEMENT_END });

        // TODO
        newVar->base.span = getSpanStamp(&lspan);

        ret->err = newVar;

        ret->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseEnumDefinition(ParseContext* ctx, Span* const span, QualifiedName* name) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenValue tokenVal;
        Lex::Token token;

        Enumerator* enumerator = Ast::Node::makeEnumerator();
        enumerator->memberType = Type::basicTypes + Type::DT_I64;
        enumerator->base.scope = ctx->currentScope;
        qualifiedNameToINamedEx(name, &enumerator->name);

        token = Lex::nextToken(&lspan);
        if (token.kind == Lex::TK_STATEMENT_BEGIN) {

            token = Lex::nextToken(&lspan, &tokenVal);
            if (token.kind != Lex::TK_KEYWORD) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "Data type expected!");
                return Lex::toToken(Err::UNEXPECTED_SYMBOL);
            }

            if (!Lex::isInt((Keyword) token.detail)) {
                Diag::report(ctx->unit->ast, &lspan, Err::UNKNOWN_DATA_TYPE);
                return Lex::toToken(Err::UNKNOWN_DATA_TYPE);
            }

            enumerator->memberType = Type::basicTypes + Lex::toDtype((Keyword) token.detail);

            token = Lex::nextToken(&lspan);

        }

        if (token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "'{' is expected!");
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }

        enumerator->base.span = getSpanStamp(&lspan);

        uint64_t lastValue = -1; //((uint64_t) 0) - ((uint64_t) 1);
        StackMark smark = markStack(&ctx->nodeStack);
        while (1) {

            token = Lex::nextToken(&lspan, &tokenVal);
            if (token.kind == Lex::TK_SCOPE_END) break;

            VariableDefinition* newVarDef = Ast::Node::makeVariableDefinition();
            Variable* newVar = Ast::Node::makeVariable();
            newVar->base.scope = ctx->currentScope;
            newVar->base.span = getSpanStamp(&lspan);

            newVarDef->var = newVar;
            newVarDef->var->value.hasValue = 0;
            newVarDef->base.span = getSpanStamp(&lspan);
            newVarDef->base.flags = IS_CMP_TIME;
            newVarDef->type.baseType = enumerator->memberType->kind;

            newVar->def = newVarDef;
            newVar->name.buff = tokenVal.str->buff;
            newVar->name.len = tokenVal.str->len;

            assignId(newVar, &ctx->varId);

            DArray::push(&ctx->nodeStack, &newVar);

            token = Lex::nextToken(&lspan);
            if (token.kind == Lex::TK_EQUAL) {

                token = parseExpression(ctx, &lspan, newVar, INVALID_POS, End { Lex::TK_LIST_SEPARATOR, Lex::TK_SCOPE_END }, 0);
                if (token.encoded < 0) return token;

                newVarDef->var->value.hasValue = true;

            }

            if (token.kind == Lex::TK_LIST_SEPARATOR) continue;
            if (token.kind == Lex::TK_SCOPE_END) break;

            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, "',' or '}'");
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);

        }
        enumerator->vars = (Variable**) commitStack(&ctx->nodeStack, smark, &enumerator->varCount);

        DArray::push(&ctx->unit->reg->enumerators, &enumerator);
        DArray::push(&ctx->nodeStack, &enumerator);
        DArray::push(&ctx->defStack, &enumerator);

        enumerator->base.span = finalizeSpan(&lspan, span);

        return token;
    }

    // NOTE: has to be called from context where previous token is struct or union
    Lex::Token parseTypeDefinition(ParseContext* ctx, Span* const span, QualifiedName* name, Type::Kind kind) {
        SpanEx lspan = markSpanStart(span);

        Lex::TokenValue tokenVal;
        Lex::Token token;

        TypeDefinition* typeDef;
        if (kind == Type::DT_STRUCT) {
            typeDef = Ast::Node::makeTypeDefinition();
        } else if (kind == Type::DT_UNION){
            typeDef = (TypeDefinition*) Ast::Node::makeUnion();
        } else {
            // TODO: error
        }

        typeDef->base.scope = ctx->currentScope;
        typeDef->base.span = getSpanStamp(&lspan);
        qualifiedNameToINamedEx(name, &typeDef->name);

        token = Lex::nextToken(&lspan);
        if (token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL);
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }

        StackMark mark = markStack(&ctx->nodeStack);
        while (1) {
            token = Lex::nextToken(&lspan, &tokenVal);
            if (token.kind == Lex::TK_SCOPE_END) break;

            // TODO: create scope for a struct
            token = parseLabel(ctx, &lspan, { token, tokenVal }, NT_VARIABLE_DEFINITION, End { Lex::TK_STATEMENT_END });
            if (token.encoded < 0) return token;

            if (token.kind == Lex::TK_STATEMENT_END) continue;
            if (token.kind == Lex::TK_SCOPE_END) break;

            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL);
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }
        typeDef->vars = (VariableDefinition**) commitStack(&ctx->nodeStack, mark, &typeDef->varCount);

        DArray::push(&ctx->unit->reg->customDataTypes, &typeDef);
        DArray::push(&ctx->nodeStack, &typeDef);
        DArray::push(&ctx->defStack, &typeDef);

        typeDef->base.span = finalizeSpan(&lspan, span);

        return token;
    }

    Lex::Token parseErrorDeclaration(ParseContext* ctx, Span* const span) {

        Lex::Token token;
        Lex::TokenValue tokenVal;

        token = Lex::nextToken(span, &tokenVal);
        if (token.kind != Lex::TK_IDENTIFIER) {
            // ERROR
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }

        VariableDefinition* def = Ast::Node::makeVariableDefinition();
        def->var->name.buff = tokenVal.str->buff;
        def->var->name.len = tokenVal.str->len;
        def->var->base.span = getSpanStamp(span);
        def->base.scope = ctx->currentScope;
        def->type.baseType = Type::DT_ERROR;
        setDefinitionIdx(ctx, (SyntaxNode*) def->var);

        DArray::push(&ctx->defStack, &def->var);
        assignId(def->var, &ctx->varId);

        token = Lex::nextToken(span, NULL);
        if (token.kind == Lex::TK_EQUAL) {

            token = parseExpression(ctx, span, def->var, INVALID_POS, End { Lex::TK_STATEMENT_END, Lex::TK_NONE });
            if (token.kind < 0) return token;

        } else if (token.kind != Lex::TK_STATEMENT_END) {

            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);

        }

        DArray::push(&ctx->unit->reg->variableDefinitions, &def);
        DArray::push(&ctx->nodeStack, &def);

        return token;

    }

    Lex::Token parseErrorDefinition(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;
        Lex::TokenValue tokenVal;

        token = Lex::nextToken(&lspan, &tokenVal);

        ErrorSet* errorSet = Ast::Node::makeErrorSet();
        errorSet->base.scope = ctx->currentScope;
        errorSet->name.buff = tokenVal.str->buff;
        errorSet->name.len = tokenVal.str->len;

        assignId(&errorSet->value, &ctx->errId);
        assignId(&errorSet->name.id, &ctx->varId);

        token = Lex::nextToken(&lspan, NULL);
        if (token.kind != Lex::TK_SCOPE_BEGIN) {
            Diag::report(ctx->unit->ast, &lspan, Err::UNEXPECTED_SYMBOL, Lex::toStr(Lex::TK_SCOPE_BEGIN));
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }

        StackMark smark = markStack(&ctx->nodeStack);
        while (1) {
            token = Lex::nextToken(&lspan, &tokenVal);
            if (token.kind == Lex::TK_SCOPE_END) break;

            Variable* var = Ast::Node::makeVariable();
            var->base.scope = ctx->currentScope;
            var->name.buff = tokenVal.str->buff;
            var->name.len = tokenVal.str->len;
            var->base.span = getSpanStamp(&lspan);
            var->value.hasValue = 0;
            var->value.type = Type::basicTypes + Type::DT_ERROR;

            assignId(&var->value.u64, &ctx->errId);

            DArray::push(&ctx->nodeStack, &var);

            token = Lex::nextToken(&lspan, &tokenVal);
            if (token.kind == Lex::TK_STATEMENT_END) continue;
            if (token.kind == Lex::TK_SCOPE_END) break;

            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }
        errorSet->vars = (Variable**) commitStack(&ctx->nodeStack, smark, &errorSet->varCount);

        DArray::push(&ctx->unit->reg->customErrors, &errorSet);
        DArray::push(&ctx->nodeStack, &errorSet);

        errorSet->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseError(ParseContext* ctx, Span* const span) {

        Lex::TokenValue tokenVal;
        Lex::Token token;

        token = Lex::peekNthToken(span, &tokenVal, 2);
        if (token.kind != Lex::TK_SCOPE_BEGIN) {

            token = parseErrorDeclaration(ctx, span);
            if (token.encoded < 0) return token;

        }

        token = parseErrorDefinition(ctx, span);
        if (token.encoded < 0) return token;

        return token;

    }

    void dispatchImport(ParseContext* ctx, Span* span, ImportStatement* import) {
        using namespace FileSystem;

        if (import->tag.len > 0) return;

        Handle fhnd = load(import->fname, Origins::COMPILER_SOURCE);
        if (!fhnd) {
            Diag::report(ctx->unit->ast, span, Err::FILE_LOAD_FAILED, import->fname.len, import->fname.buff);
        }

        TaskSystem::dispatchParse(fhnd);
    }

    Lex::Token parseImport(ParseContext* ctx, Span* const span) {
        SpanEx lspan = markSpanStart(span);

        Lex::Token token;
        Lex::TokenValue tokenVal;

        ImportStatement* import = Ast::Node::makeImportStatement();
        // import->root = fileRootScope;
        import->base.scope = ctx->currentScope;

        token = Lex::nextToken(&lspan);
        if (token.kind == Lex::TK_ARRAY_BEGIN) {
            token = parseLanguageTag(ctx, &lspan, &import->tag);
            token = Lex::nextFileName(&lspan, &tokenVal);
        } else if (token.kind != Lex::TK_KEYWORD) {
            token = Lex::nextFileName(&lspan, &tokenVal);
        }

        import->fname = *tokenVal.str;
        import->keyword = KW_VOID;

        token = Lex::nextToken(&lspan, &tokenVal);
        if (!Lex::isKeyword(token, KW_AS)) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Keyword 'as' expected!");
        }

        token = Lex::nextToken(&lspan, &tokenVal);
        if (token.kind != Lex::TK_IDENTIFIER) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Identifier expected!");
        }

        token = Lex::nextToken(&lspan, NULL);
        if (token.kind == Lex::TK_STATEMENT_BEGIN) {
            if (import->tag.len > 0) {
                ctx->foreignContext = true;
            }

            token = parseLabel(ctx, &lspan, (QualifiedName*) tokenVal.any, End { Lex::TK_STATEMENT_END });

            SyntaxNode* node = *(SyntaxNode**) DArray::getLast(&ctx->nodeStack);
            node->import = import;
        } else {
           import->param = *tokenVal.str;
        }

        ctx->foreignContext = false;
        DArray::push(&ctx->unit->reg->imports, (void*) &import);

        // TODO : why dispatch before finalizeSpan?
        dispatchImport(ctx, span, import);

        import->base.span = finalizeSpan(&lspan, span);
        return token;
    }

    Lex::Token parseDirective(ParseContext* ctx, Span* const span, Lex::Directive directive, Flags param) {

        switch (directive) {

            case Lex::CD_TEST: {
                return Lex::toToken(Lex::TK_STATEMENT_END);
            }

            default: {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Unsupported language directive processing code encountered!");
                return Lex::toToken(Err::UNEXPECTED_SYMBOL);
            }

        }

        return Lex::toToken(Err::UNEXPECTED_SYMBOL);

    }

    Lex::Token parseList(ParseContext* ctx, Span* const span, Lex::TokenKind separator, Lex::TokenKind end) {
        Lex::Token token;
        int first = 1;

        while (1) {

            if (first) {
                token = Lex::tryToken(span, Lex::toToken(end));
                if (token.kind == end) return token;
            }

            Variable* newVar = NULL;
            token = parseExpression(ctx, span, &newVar, INVALID_POS, End{ separator, end }, first ? EMPTY_EXPRESSION_ALLOWED : 0);
            if (token.encoded < 0) return token;

            DArray::push(&ctx->nodeStack, &newVar);

            if (token.kind == end) {
                break;
            }

            if (token.kind != separator) {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL);
            }

            first = 0;

        }

        return token;
    }

    Lex::Token parseCatch(ParseContext* ctx, Span* const span, Variable* var) {
        Lex::Token token;
        Lex::TokenValue tokenVal;



        Variable* errVar = Ast::Node::makeVariable();
        errVar->base.scope = var->base.scope;
        errVar->value.type = Type::basicTypes + Type::DT_ERROR;

        Scope* newScope = Ast::Node::makeScope();
        newScope->base.scope = var->base.scope;
        // setParentIdx(newScope);

        Catch* cex = Ast::Node::makeCatch();
        cex->call = (FunctionCall*) var->expression;

        var->expression = (Expression*) cex;



        token = Lex::nextToken(span, &tokenVal);

        if (Lex::isKeyword(token, KW_RETURN)) {
            // basically just emulates following
            // .. .. catch err { return _, err; }

            ReturnStatement* ret = Ast::Node::makeReturnStatement();
            ret->err = errVar;
            ret->var = NULL;
            ret->base.scope = newScope;
            ret->fcn = ctx->currentFunction;

            DArray::push(&ctx->unit->reg->returnStatements, &ret);
            newScope->children = (SyntaxNode**) alloc(alc, sizeof(SyntaxNode*));
            newScope->children[0] = (SyntaxNode*) ret;

            token = Lex::nextToken(span, &tokenVal);
            if (token.kind != Lex::TK_STATEMENT_END) {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "'catch return' expression has to be terminated with ';'!");
                return Lex::toToken(Err::UNEXPECTED_SYMBOL);
            }

            return token;
        }

        if (token.kind != Lex::TK_IDENTIFIER) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "error identifier");
            return Lex::toToken(Err::UNEXPECTED_SYMBOL);
        }

        QualifiedName* errName = (QualifiedName*) (tokenVal.any);

        token = Lex::nextToken(span, &tokenVal);
        if (
            token.kind != Lex::TK_SCOPE_BEGIN &&
            token.kind != Lex::TK_STATEMENT_BEGIN &&
            !Lex::isKeyword(token, KW_RETURN)
        ) {
            // 'catch err' case
            errVar->name = *errName;

            DArray::push(&ctx->unit->reg->variables, &var);

            cex->err = errVar;
            cex->scope = NULL;

            return token;
        }



        // 'catch err { ... }' case
        VariableDefinition* errDef = Ast::Node::makeVariableDefinition();
        errDef->var = errVar;
        errDef->base.scope = newScope;
        errDef->var->base.scope = newScope;
        errDef->var->value.type = Type::basicTypes + Type::DT_ERROR;
        errDef->var->base.span = getSpanStamp(span);
        errDef->var->base.definitionIdx = -1;

        errDef->var->name = *errName;

        assignId(errDef->var, &ctx->varId);

        DArray::push(&ctx->unit->reg->variableDefinitions, &errDef);
        DArray::push(&ctx->nodeStack, &errDef->var);
        ctx->nodeStack.size -= sizeof(SyntaxNode*);

        DArray::push(&ctx->defStack, &errDef->var);

        cex->err = errDef->var;
        cex->scope = newScope;

        ScopeEnd scopeEnd = (ScopeEnd) (token.kind == Lex::TK_STATEMENT_BEGIN);

        Scope* currentScope = ctx->currentScope;
        ctx->currentScope = newScope;
        token = parseScope(ctx, span, SC_COMMON, scopeEnd);
        ctx->currentScope = currentScope;

        return token;
    }



    Lex::Token parseExpressionNode(ParseContext* ctx, Span* const span, Variable* var, OperatorEnum prevOp = OP_NONE) {
        Lex::Token token;
        Lex::TokenValue tokenVal;

        int consumeToken = 1;

        var->base.scope = ctx->currentScope;

        //Prefix part
        //

        while (1) {
            token = Lex::nextToken(span, &tokenVal);

            OperatorEnum op = Lex::toUnaryOperator(token);
            if (op == OP_NONE) {
                break;
            }

            UnaryExpression* uex = Ast::Node::makeUnaryExpression();
            uex->base.opType = op;
            uex->operand = Ast::Node::makeVariable();
            uex->operand->base.scope = ctx->currentScope;

            var->expression = (Expression*) uex;
            var = uex->operand;
        }

        var->base.span = getSpanStamp(span);



        // Primary part
        //

        switch (token.kind) {

            case Lex::TK_IDENTIFIER: {
                token = Lex::nextToken(span, NULL);

                var->name = *((QualifiedName*) tokenVal.any);
                consumeToken = 0;

                if (prevOp != OP_MEMBER_SELECTION) {
                    DArray::push(&ctx->unit->reg->variables, &var);
                }

                var->name.span = var->base.span;
                var->base.definitionIdx = ctx->defStack.size;

                break;
            }

            case Lex::TK_NUMBER: {
                //var->name.buff = NULL;
                //var->name.len = 0;
                var->expression = NULL;
                var->value.hasValue = true;
                var->value.type = Type::basicTypes + Lex::toDtype(token);

                if (token.detail == Lex::TD_DT_U64) {
                    var->value.u64 = tokenVal.ival;
                } else if (token.detail == Lex::TD_DT_I64) {
                    var->value.i64 = tokenVal.ival;
                } else if (token.detail == Lex::TD_DT_F32) {
                    var->value.f32 = tokenVal.f32;
                } else if (token.detail == Lex::TD_DT_F64) {
                    var->value.f64 = tokenVal.f64;
                }

                break;
            }

            case Lex::TK_KEYWORD: {
                var->expression = NULL;
                var->value.hasValue = true;

                // TODO: why we are not using predefined vars?
                if (token.detail == KW_TRUE) {
                    var->value.type = Type::basicTypes + Type::DT_BOOL;
                    var->value.u64 = 1;
                } else if (token.detail == KW_FALSE) {
                    var->value.type = Type::basicTypes + Type::DT_BOOL;
                    var->value.u64 = 0;
                } else if (token.detail == KW_NULL) {
                    var->value.type = Type::makePointer(Type::basicTypes + Type::DT_VOID);
                    var->value.u64 = 0;
                }

                break;
            }

            case Lex::TK_PARENTHESIS_BEGIN: {
                token = parseExpression(ctx, span, var, INVALID_POS, End { Lex::TK_PARENTHESIS_END, Lex::TK_NONE });
                break;
            }

            case Lex::TK_STRING: {
                StringInitialization* init = (StringInitialization*) tokenVal.any;

                // Reg.Node.initStringInitialization();
                // init->rawPtr = tokenVal.str->buff;
                // init->rawPtrLen = tokenVal.str->len;

                var->expression = (Expression*) init;

                break;
            }

            case Lex::TK_CHAR: {
                var->value.type = Type::basicTypes + Type::DT_I64;
                var->value.i64 = tokenVal.ival;

                break;
            }

            case Lex::TK_ARRAY_BEGIN: {
                ArrayInitialization* init;
                token = parseArrayInitialization(ctx, span, &init);
                var->expression = (Expression*) init;

                break;
            }

            case Lex::TK_SCOPE_BEGIN: {
                TypeInitialization* init;
                token = parseTypeInitialization(ctx, span, &init);
                var->expression = (Expression*) init;
                var->base.span = getSpanStamp(span);

                break;
            }

            default: {
                return token;
            }

        }

        if (consumeToken) token = Lex::nextToken(span, &tokenVal);



        // Postfix part
        //

        while (1) {

            OperatorEnum op = Lex::toPostfixOperator(token);
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

                token = Lex::nextToken(span, &tokenVal);

                bex->right = Ast::Node::makeVariable();
                bex->right->name = *((QualifiedName*) tokenVal.any);

                var->expression = (Expression*) bex;
            } else if (op == OP_SUBSCRIPT) {
                BinaryExpression* bex = Ast::Node::makeBinaryExpression();
                bex->base.opType = op;
                bex->left = Ast::Node::copy(var); // The array/pointer being indexed

                bex->right = Ast::Node::makeVariable();
                token = parseExpression(ctx, span, bex->right, INVALID_POS, End { Lex::TK_ARRAY_END, Lex::TK_SLICE });

                if (token.kind == Lex::TK_SLICE) {
                    RangeExpression* range;
                    parseRangeExpression(ctx, span, { Lex::TK_ARRAY_END }, &range, &bex->right);
                }

                var->expression = (Expression*) bex;
            } else if (op == OP_CALL) {
                FunctionCall* call = Ast::Node::makeFunctionCall();
                call->fptr = NULL;
                call->fcn = NULL;

                StackMark smark = markStack(&ctx->nodeStack);
                token = parseList(ctx, span, Lex::TK_LIST_SEPARATOR, Lex::TK_PARENTHESIS_END);
                if (token.encoded < 0) return token;
                call->inArgs = (Variable**) commitStack(&ctx->nodeStack, smark, &call->inArgCount);

                call->name = *((QualifiedName*) tokenVal.any);
                call->name.span = var->base.span;

                var->value.hasValue = 0;
                var->expression = (Expression*) call;

                DArray::push(&ctx->unit->reg->fcnCalls, &var);
            } else {
                // generic case
                UnaryExpression* uex = Ast::Node::makeUnaryExpression();
                uex->base.opType = op;
                uex->operand = Ast::Node::makeVariable();
                uex->operand = Ast::Node::copy(var);

                var->expression = (Expression*) uex;
            }

            var->name = { 0 };
            token = Lex::nextToken(span, &tokenVal);
        }

        return token;
    }


    // the lower the rank, the higher precedence
    Lex::Token parseExpressionRecursive(ParseContext* ctx, Span* const span, Variable** var, BinaryExpression* prevBex, OperatorEnum prevOp) {
        Lex::Token token;
        Lex::TokenValue tokenVal;

        token = parseExpressionNode(ctx, span, *var, prevOp);

        if (Lex::isOperator(token)) {
            OperatorEnum op = Lex::toBinaryOperator(token);

            while (op != OP_NONE) {
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

    Lex::Token parseExpression(ParseContext* ctx, Span* const span, Variable** var, const Pos startPos, const End endToken, const Flags flags) {
        Lex::Token token;
        Lex::TokenValue tokenVal;

        if (isValidPos(startPos)) span->end = startPos;

        Variable* operand = Ast::Node::makeVariable();
        operand->base.scope = ctx->currentScope;
        token = parseExpressionRecursive(ctx, span, &operand, NULL, OP_NONE);

        *var = operand;

        if (Lex::isKeyword(token, KW_CATCH)) {
            if (operand->expression->type != EXT_FUNCTION_CALL) {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_END_OF_EXPRESSION, "Yet only 'pure' function call expressions are allowed to be caught 🐱.");
            }

            // TODO : add flag somethign like ALLOW_CATCH_TO_DISRESPECT_ENDING
            token = parseCatch(ctx, span, operand);
            if (token.kind == Lex::TK_SCOPE_END) return token;
        }

        if (
            isEndToken(token, endToken) ||
            (flags & USE_KEYWORD_AS_END && token.kind == Lex::TK_KEYWORD) ||
            flags & ALLOW_UNEXPECTED_END
        ) {
            return token;
        }

        Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_SYMBOL, "Blablbalba");
        return token;
    }

    // meh, but ok for now...
    Lex::Token parseExpression(ParseContext* ctx, Span* const span, Variable* var, const Pos startPos, const End endToken, const Flags flags) {
        Lex::Token token;

        Variable* tmpVar;
        token = parseExpression(ctx, span, &tmpVar, startPos, endToken, flags);

        UnaryExpression* uex = Ast::Node::makeUnaryExpression();
        uex->operand = tmpVar;
        uex->base.opType = OP_NONE;

        var->expression = (Expression*) uex;

        return token;
    }

}
