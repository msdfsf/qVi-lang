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

    struct FullToken {
        Lex::Token token;
        Lex::TokenValue value;
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
        DArray::Container nodeStack;
        DArray::Container defStack;

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
    Lex::Token parseBareStatement(ParseContext* ctx, Span* span, const Pos startPos, const End end);
    Lex::Token parseFreeStatement(ParseContext* ctx, Span* span);
    Lex::Token parseAllocStatement(ParseContext* ctx, Span* span);
    Lex::Token parsePrintLiteral(ParseContext* ctx, Span* span, Lex::TokenValue* startTokenVal);

    Lex::Token parseLabel(ParseContext* ctx, Span* span, FullToken prev, NodeType nodeType, End end);
    Lex::Token parseLabel(ParseContext* ctx, Span* span, QualifiedName* name, End end);
    Lex::Token parseAssignment(ParseContext* ctx, Span* span, const Pos startPos, const End end);
    Lex::Token parseVarDefinition(ParseContext* ctx, Span* span, FullToken prev, QualifiedName* name, const End end);
    Lex::Token parseDefinitionAssignment(ParseContext* ctx, Span* span, FullToken prev, VariableDefinition* def, const End end, Flags flags);

    // Types
    Lex::Token parseDataType(ParseContext* ctx, Span* span, FullToken prev, Flags flags, TypeSpecifier* type);
    Lex::Token parseTypeDecorators(ParseContext* ctx, Span* const span, TypeSpecifier* spec);
    Lex::Token parseEnumDefinition(ParseContext* ctx, Span* span, QualifiedName* name);
    Lex::Token parseTypeDefinition(ParseContext* ctx, Span* span, QualifiedName* name, Type::Kind kind);
    Lex::Token parseTypeInitialization(ParseContext* ctx, Span* span, TypeInitialization** out);
    Lex::Token parseArrayInitialization(ParseContext* ctx, Span* span, ArrayInitialization** out);

    // Control Flow
    Lex::Token parseIfStatement(ParseContext* ctx, Span* span);
    Lex::Token parseCaseStatement(ParseContext* ctx, Span* span);
    Lex::Token parseLoop(ParseContext* ctx, Span* span);
    Lex::Token parseBreakStatement(ParseContext* ctx, Span* span);
    Lex::Token parseContinueStatement(ParseContext* ctx, Span* span);
    Lex::Token parseReturnStatement(ParseContext* ctx, Span* span);

    // Expression / R-Value
    Lex::Token parseExpression(ParseContext* ctx, Span* span, Variable** outVar, const Pos startPos, const End end, const Flags flags = NULL_FLAG);
    Lex::Token parseExpression(ParseContext* ctx, Span* span, Variable* var, const Pos startPos, const End end, const Flags flags = NULL_FLAG);
    Lex::Token parseRValue(ParseContext* ctx, Span* span, Variable* outVar, const End end);
    Lex::Token parseCatch(ParseContext* ctx, Span* span, Variable* var);
    Lex::Token parseRangeExpression(ParseContext* ctx, Span* span, End end, RangeExpression** range, Variable** outLeftExp);

    // Functions
    Lex::Token parseFunction(ParseContext* ctx, Span* span, QualifiedName* name, Flags flags);
    Lex::Token parseFunctionPointer(ParseContext* ctx, Span* span, FunctionPrototype** out);

    // Misc
    Lex::Token parseList(ParseContext* ctx, Span* span, Lex::TokenKind separator, Lex::TokenKind end);
    Lex::Token parseError(ParseContext* ctx, Span* span);
    Lex::Token parseImport(ParseContext* ctx, Span* span);
    Lex::Token parseDirective(ParseContext* ctx, Span* span, const Lex::Directive dir, Flags flags);
    Lex::Token parseLanguageTag(ParseContext* ctx, Span* span, String* tag);

}
