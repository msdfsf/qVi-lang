#pragma once

#include "array_list.h"
#include "data_types.h"
#include "file_system.h"
#include "globals.h"
#include "lexer.h"
#include "syntax.h"
#include "diagnostic.h"
#include "registry.h"
#include <cstdint>

namespace Parser {

    enum {
        USE_KEYWORD_AS_END = 0x1,
        EMPTY_EXPRESSION_ALLOWED = 0x2,
        ALLOW_QUALIFIER = 0x4,
        INCLUDE_TO_TREE = 0x8,
        INCLUDE_TO_SCOPE = 0x10,
        ALLOW_UNEXPECTED_END = 0x12,
    };

    enum ScopeEnd {
        SE_DEFAULT = 0,
        SE_STATEMENT = 1,
    };

    struct End {
        Lex::TokenKind a;
        Lex::TokenKind b;
        Lex::TokenKind c;
    };

    typedef uint32_t StackMark;
    constexpr StackMark NULL_STACK_MARK = UINT32_MAX;

    // To represent tree of imports so we can check
    // for circular imports and log to user full path
    struct ImportNode {
        FileSystem::Handle file;
        ImportStatement*   import;
        ImportNode*        parent;
        ImportNode*        firstChild;
        ImportNode*        nextSibling;
    };

    struct ParseContext {
        Reg::Unit* unit;

        // The 'global' span of the file being parsed
        Span*       fileSpan;

        // Root directory of the project
        String rootDir;

        // State - change as we move through the tree
        Scope*      currentScope;    // The scope currently being filled
        Function*   currentFunction; // NULL if at global level
        SyntaxNode* currentLoop;

        bool foreignContext;

        // Import that 'triggered' compilation of the current file
        ImportNode* currentImport;
        ImportNode* importRoot;

        // Stacks and Buffers
        // TODO: we need either custom data structure to handle both stack
        //       or just use one dynamic array for both, so we pay some
        //       fragmentation for less jumping around.
        //       BUT first we need refactor parser and prepare benchmarks...
        DArray::Container nodeStack;
        DArray::Container defStack;

        // We will push the offsets of each qualified name part here while parsing,
        // so we can pack the offsets more tightly in the result object,
        // as we will know their total size in advance.
        DArray::Container qnameStack;

        // Counters and Indices
        int   idxInScope; // Track definition index in current scope
        // DEPRECATE
        ArrId arrId;
        VarId varId;
        ErrId errId;
        DefId defId;
    };



    // Main

    // Call per thread, calls Lex::init/release
    void init   (ParseContext* ctx);
    void release(ParseContext* ctx);

    Err::Err parse(ParseContext* ctx, char* const flname);
    Err::Err parse(ParseContext* ctx, const FileSystem::Handle file);



    // Scopes and Blocks
    Lex::Token parseScope(ParseContext* ctx, Span* span, const ScopeType type, const ScopeEnd end, StackMark dsmarkStart = NULL_STACK_MARK);
    Lex::Token parseNamespace(ParseContext* ctx, Span* span, QualifiedName* name);
    Lex::Token parseForeignScope(ParseContext* ctx, Span* span);

    // Statements
    Lex::Token parseBareStatement(ParseContext* ctx, Span* span);
    Lex::Token parseFreeStatement(ParseContext* ctx, Span* span);
    Lex::Token parseAllocStatement(ParseContext* ctx, Span* span);
    Lex::Token parseDeferStatement(ParseContext* ctx, Span* span);
    Lex::Token parsePrintLiteral(ParseContext* ctx, Span* span, Lex::Token token, Lex::Value value);

    Lex::Token parseLabel(ParseContext* ctx, Span* span, Lex::TokenEx tex, End end);
    Lex::Token parseDeclarationBody(ParseContext* ctx, Span* span, QualifiedName* name, End end);
    Lex::Token parseAssignment(ParseContext* ctx, Span* span, const End end);
    Lex::Token parseVarDefinition(ParseContext* ctx, Span* span, Lex::TokenEx prev, QualifiedName* name, VariableDefinition* def, const End end);
    Lex::Token parseQualifiedName(ParseContext* ctx, Span* span, Lex::TokenEx tex, QualifiedName* qname);

    // Types
    Lex::Token parseTypeInitialization(ParseContext* ctx, Span* span, TypeInitialization** out);
    Lex::Token parseArrayInitialization(ParseContext* ctx, Span* span, ArrayInitialization** out);

    // Control Flow
    Lex::Token parseIfStatement(ParseContext* ctx, Span* const span, QualifiedName* label, const bool compileTime);
    Lex::Token parseCaseStatement(ParseContext* ctx, Span* span, INamedEx* label);
    Lex::Token parseLoop(ParseContext* ctx, Span* span, INamedEx* label, const bool compileTime);
    Lex::Token parseBreakStatement(ParseContext* ctx, Span* span);
    Lex::Token parseContinueStatement(ParseContext* ctx, Span* span);
    Lex::Token parseReturnStatement(ParseContext* ctx, Span* span);

    // Expression / R-Value
    Lex::Token parseExpression(ParseContext* ctx, Span* span, Variable** outVar, const End end, const Flags flags = NULL_FLAG);
    Lex::Token parseExpression(ParseContext* ctx, Span* span, Variable* var, const End end, const Flags flags = NULL_FLAG);
    Lex::Token parseRValue(ParseContext* ctx, Span* span, Variable* outVar, const End end);
    Lex::Token parseCatch(ParseContext* ctx, Span* span, Variable* var);
    Lex::Token parseRangeExpression(ParseContext* ctx, Span* span, End end, RangeExpression** range, Variable** outLeftExp, const bool allowOpenBounds = false);
    Lex::Token parseExpressionNode(ParseContext* ctx, Span* const span, Lex::TokenEx tex, Variable* var, OperatorEnum prevOp = OP_NONE, bool parsePrefix = true);

    // Functions
    Lex::Token parseParameterList(ParseContext* ctx, Span* const span, FunctionPrototype* out, Scope* paramScope);
    Lex::Token parseFunctionSignature(ParseContext* ctx, Span* const span, FunctionPrototype* out, Scope* paramScope, QualifiedName** errorSetOut);
    Lex::Token parseFunction(ParseContext* ctx, Span* span, QualifiedName* name);

    // Composites
    Lex::Token parseRecordExpression(ParseContext* ctx, Span* span, RecordExpression* record);
    Lex::Token parseRecordItems(ParseContext* ctx, Span* span, RecordExpression* record);
    Lex::Token parseEnumExpression(ParseContext* ctx, Span* span, EnumExpression* enumerator);
    Lex::Token parseErrorSetBody(ParseContext* ctx, Span* span, ErrorSet* errorSet);
    Lex::Token parseUsingStatement(ParseContext* ctx, Span* span);

    // Misc
    Lex::Token parseList(ParseContext* ctx, Span* span, Lex::TokenKind separator, Lex::TokenKind end);
    Lex::Token parseImport(ParseContext* ctx, Span* span);
    Lex::Token parseDirective(ParseContext* ctx, Span* span);
    Lex::Token parseLanguageTag(ParseContext* ctx, Span* span, String* tag);

    int64_t parseStringLiteral(const char* const str, const int len, const int requestedAlign, StringInitialization** initOut);
}
