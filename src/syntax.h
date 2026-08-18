// This header contains all syntax related stuff that directly
// reprsents it or helps in some way.
// Most of stuff here are core parts of the system, so they are
// in one file as mainly used together anyway.
// May be splited or wrapped into more files/namespaces, but
// for now I feel like this is the most comfortable layaut at least
// for my self.

#pragma once

#include <string>
#include <cstdint>
#include <array>

#include "allocator.h"
#include "globals.h"
#include "operators.h"
#include "data_types.h"
#include "keywords.h"
#include "array_list.h"
#include "dynamic_arena.h"
#include "ordered_dict.h"
#include "set.h"
#include "task_status.h"



// =========================================
// FORWARD DECLARATIONS
// ===

struct SyntaxNode;
struct Scope;
struct FunctionPrototype;
struct Namespace;
struct VariableDefinition;
struct VariableAssignment;
struct TypeDefinition;
struct TypeInitialization;
struct StringInitialization;
struct ArrayInitialization;
struct Enumerator;
struct Variable;
struct Function;
struct Branch;
struct SwitchCase;
struct WhileLoop;
struct Loop;
struct ErrorSet;
struct ReturnStatement;
struct ContinueStatement;
struct BreakStatement;
struct GotoStatement;
struct Label;
struct Expression;
struct ConstExpression;
struct OperatorExpression;
struct UnaryExpression;
struct BinaryExpression;
struct TernaryExpression;
struct RangeExpression;
struct Statement;
struct FunctionCall;
struct ImportStatement;
struct Union;
struct GetLength;
struct GetSize;
struct Catch;
struct Alloc;
struct Free;
struct Using;

struct ScopeName;

struct IForeignCode;
struct CodeBlock;
struct ForeignFunction;
struct LangDef;

struct TypeDefinition;

namespace Interpreter {
    struct ExeBlock;
}

namespace Extern {
    struct Library;
    typedef Library* LibraryHandle;

    namespace Abi {
        struct CallContext;
        struct TypeInfo;
    };
}



// ======================================
// ENUMS & BASIC TYPES
// ===

typedef int AllocType;

enum NodeType : AllocType {
    NT_SCOPE,
    NT_VARIABLE_DEFINITION,
    NT_VARIABLE_ASSIGNMENT,
    NT_TYPE_DEFINITION,
    NT_TYPE_INITIALIZATION,
    NT_ENUMERATOR,
    NT_VARIABLE,
    NT_FUNCTION,
    NT_BRANCH,
    NT_SWITCH_CASE,
    NT_WHILE_LOOP,
    NT_LOOP,
    NT_RETURN_STATEMENT,
    NT_CONTINUE_STATEMENT,
    NT_BREAK_STATEMENT,
    NT_GOTO_STATEMENT,
    NT_LABEL,
    NT_NAMESPACE,
    NT_STATEMENT,
    NT_CODE_BLOCK,
    NT_ERROR,
    NT_UNION,
    NT_USING,
    NT_IMPORT,
    NT_COUNT
};

enum AllocExType : AllocType {
    AT_BYTE = NT_COUNT,
    AT_EXT_FUNCTION_CALL,
    AT_EXT_OPERATION,
    AT_EXT_UNARY,
    AT_EXT_BINARY,
    AT_EXT_TERNARY,
    AT_EXT_RANGE,
    AT_EXT_TYPE_INITIALIZATION,
    AT_EXT_STRING_INITIALIZATION,
    AT_EXT_ARRAY_INITIALIZATION,
    AT_EXT_CATCH,
    AT_EXT_CAST,
    AT_EXT_ALLOC,
    AT_EXT_FREE,
    AT_EXT_GET_LENGTH,
    AT_EXT_GET_SIZE,
    AT_INAMED,
    AT_QUALIFIED_NAME,
    AT_FUNCTION_PROTOTYPE,
    AT_FOREIGN_FUNCTION,
    AT_TYPE_SPECIFIER,
    AT_ERROR_STRING,
    AT_COUNT
};

enum ScopeType {
    SC_GLOBAL = 1,
    SC_COMMON = 0
};

enum ExpressionType {
    EXT_FUNCTION_CALL = AT_EXT_FUNCTION_CALL,
    EXT_OPERATION,
    EXT_UNARY,
    EXT_BINARY,
    EXT_TERNARY,
    EXT_RANGE,
    EXT_TYPE_INITIALIZATION,
    EXT_STRING_INITIALIZATION,
    EXT_ARRAY_INITIALIZATION,
    EXT_CATCH,
    EXT_CAST,
    EXT_ALLOC,
    EXT_FREE,
    EXT_GET_LENGTH,
    EXT_GET_SIZE,
};

enum ExpressionQualifier {
    EXQ_VARIABLE,
    EXQ_CONSTANT,
    EXQ_COMPILE_TIME
};

enum AcquireNodeReturn {
    ANR_ALREADY_READY,
    ANR_ACQUIRED_FOR_WORK,
    ANR_ACQUIRED_BY_ANOTHER_THREAD,
    ANR_ALREADY_ACQUIRED_BY_CALLER
};

struct QualifiedName : INamedEx {
    uint16_t pathSize;
    INamed* path;
};

struct SymbolIndexEntry {
    String name;

    enum {
        SINGLE,
        OVERLOAD
    } kind;

    union {
        SyntaxNode* node;
        struct {
            SyntaxNode** data;
            uint32_t     count;
        } overloads;
    };
};

struct SymbolIndex {
    Set::Container set;
};

// ======================================
//  SYNTAX NODES
// ===

struct SyntaxNode {
    static Scope* root;
    static INamed dir;

    // as files are parsed only once and linked via pointers
    // need option to get access to the original node
    SyntaxNode* ogNode = NULL;

    NodeType type;
    Scope*   scope = NULL;
    Span*    span;
    uint64_t flags;

    int definitionIdx;

    // What import we came from...
    ImportStatement* import;

    // Status of Linking, Type Resolution etc...
    uint8_t semStatus;
    // Status of Bytecode Generation, Evaluation etc...
    uint8_t cmpStatus;
    // TODO
    uint8_t workerId;
};

struct Scope {
    SyntaxNode  base;

    SyntaxNode** children;
    SyntaxNode** definitions;

    uint32_t childrenCount;
    uint32_t definitionCount;

    SymbolIndex* index;
};

struct Namespace {
    Scope    scope;
    INamedEx name;

    Function** fcns;
    uint32_t   fcnCount;
};

// only as defSearch
struct Using {
    SyntaxNode  base;
    SyntaxNode* var; // node that is being included
};

// used for interoperability to keep code of the foreign language
struct IForeignCode {
    Span*  tagLoc;
    String tagStr;
    String codeStr;
};

struct CodeBlock {
    SyntaxNode   base;
    IForeignCode code;
};

struct Enumerator {
    SyntaxNode base;
    INamedEx   name;

    Variable** vars;
    uint32_t   varCount;

    Type::TypeInfo* memberType;
    Type::TypeInfo* type;
};

struct Statement {
    SyntaxNode base;
    Variable*  operand;
};

struct Value {
    union {
        int8_t      i8;
        int16_t     i16;
        int32_t     i32;
        int64_t     i64;

        uint8_t     u8;
        uint16_t    u16;
        uint32_t    u32;
        uint64_t    u64;

        float_t     f32;
        double_t    f64;

        char*       str;

        Expression* exp;
        ArrayInitialization*  aex;
        TypeInitialization*   tex;
        StringInitialization* sex;
    };

    // TODO : later we can try to use the last bit of
    //        type to store hasValue, so this struct
    //        can fit into one reg
    Type::TypeInfo* type;
    bool hasValue = false;
};

struct Expression {
    ExpressionType type;
};

struct OperationExpression {
    Expression base;
    OperatorEnum opType;
};

struct UnaryExpression {
    OperationExpression base;
    Variable* operand;
};

struct BinaryExpression {
    OperationExpression base;
    Variable* left;
    Variable* right;
};

struct TernaryExpression {
    OperationExpression base;
    Variable* condition;
    Variable* trueExp;
    Variable* falseExp;
};

struct RangeExpression {
    Expression base;

    Variable* bidx;
    Variable* eidx;
    Variable* step;
};

struct TypeInitialization {
    Expression base;

    Variable** attributes;
    uint32_t   attributeCount;

    int* idxs; // maps attributes to og indicies in TypeDefinition

    // to fill the rest of the attributes with the same value
    // if NULL then no filling
    Variable* fillVar;
};

struct StringInitialization {
    Expression base;

    String rawData;
    Type::TypeInfo* charType;
};

struct ArrayInitialization {
    Expression base;
    Flags      flags;
    Variable** attributes;
    uint32_t   attributeCount;
};

struct Catch {
    Expression base;

    FunctionCall* call;

    // its either one of these, other is NULL
    Variable* err;
    Scope* scope;
};

struct Cast {
    Expression base;

    Type::TypeInfo* target;
    Variable*       operand;

    enum Kind {
        DEFAULT = 0,
        FROM_LOWER_LEVEL
    } kind;
};

struct Alloc {
    Expression base;
    VariableDefinition* def;
};

struct Free {
    Expression base;
    Variable*  var;
};

struct GetLength {
    Expression base;
    Variable*  arr;
};
static_assert(sizeof(GetLength) <= sizeof(BinaryExpression),
              "GetLength exceeded size of BinaryExpression");

struct GetSize {
    Expression base;
    Variable*  arr;
};
static_assert(sizeof(GetSize) <= sizeof(BinaryExpression),
              "GetLength exceeded size of BinaryExpression");

// Represents a single decorator applied to a type (*, [10], [const], ...)
struct TypeDecorator {
    enum Kind {
        DEC_POINTER,
        DEC_ARRAY,
    } kind;

    Span* span;
    uint64_t flags;

    // only for DEC_ARRAY
    Variable* len;
};

struct TypeSpecifier {
    Type::Kind baseType;

    // DT_UNDEFINED if baseName
    // DT_FUNCTION if baseFcn
    union {
        QualifiedName* baseName;    // The base complex type name (struct name, enum etc.)
        FunctionPrototype* baseFcn; // The base function pointer type
    };

    Span* span;

    // In the order they were written
    TypeDecorator** decorators;
    uint32_t        decoratorCount;
};

struct VariableDefinition {
    SyntaxNode base;

    Variable*     var; // it may be enough
    TypeSpecifier type;

    // local offset in vm
    uint64_t vmOffset;
    Interpreter::ExeBlock* vmOwnerExe;
};

struct VariableAssignment {
    SyntaxNode base;
    Variable*  lvar;
    Variable*  rvar;
};

struct Variable {
    SyntaxNode base;

    VariableDefinition* def;

    // Resolved semantic type and optional compile-time value
    Value value;

    // AST representation of an expression.
    // If NULL, this node represents a direct literal or primitive value.
    // Should be preserved even when resolved at compile-time to maintain
    // source-level AST metadata.
    Expression* expression;

    // In case of member .id is members index
    QualifiedName name;
};

struct FunctionPrototype {
    VariableDefinition** inArgs;
    VariableDefinition*  outArg;
    uint32_t inArgCount;
};

struct Function {
    SyntaxNode base;
    FunctionPrototype prototype;
    INamedEx name;

    // TODO : not sure we need this
    ReturnStatement** returns;
    uint32_t returnCount;

    Scope* bodyScope;

    QualifiedName* errorSetName;
    ErrorSet* errorSet;

    TaskStatus compilationStatus;
    Interpreter::ExeBlock* exe;

    Extern::Abi::CallContext* abiCtx;
    Extern::LibraryHandle lib;
    void* externAddress;

    int internalIdx; // if it is > 0, then its internal function, and value represents unique id, otherwise should be ignored ***** TODO : for now value: -1 is used as identifer to not render function, fix it later *****
};

struct ForeignFunction {
    Function     fcn;
    IForeignCode code;
};

struct FunctionCall {
    Expression    base;
    QualifiedName name;

    Function*  fcn;
    Variable*  fptr; // for function pointers, meh...
    Variable** inArgs;
    Variable*  outArg;
    uint32_t   inArgCount;
};

struct Branch {
    SyntaxNode base;

    Scope**    scopes;
    Variable** expressions;

    uint32_t   scopeCount;
    uint32_t   expressionCount;
};

struct SwitchCase {
    SyntaxNode base;

    Variable*  switchExp;
    Variable** casesExp;

    Scope** cases;
    Scope*  elseCase;

    uint32_t caseExpCount;
    uint32_t caseCount;
};

struct WhileLoop {
    SyntaxNode base;
    Scope*     bodyScope;
    Variable*  expression;
};

struct Loop {
    SyntaxNode base;

    Scope* bodyScope;

    // Argument
    struct Arg {
        union {
            Variable*        array;
            RangeExpression* range;
        };

        enum Kind {
            ARRAY,
            RANGE,
        } kind;
    } arg;

    // As Clause
    VariableDefinition* array;
    // TODO : change this, as its awkward to access
    union {
        Variable*           var;
        VariableDefinition* def;
    } index;

    // By Clause
    Variable* stride;

    // While Clause
    Variable* condition;
};

struct ReturnStatement {
    SyntaxNode base;

    Function* fcn;
    Variable* var;
    Variable* err;

    int idx; // indexes itself in Function.returns
};

struct ContinueStatement {
    SyntaxNode base;
    SyntaxNode* target;
};

struct BreakStatement {
    SyntaxNode base;
    SyntaxNode* target;
};

struct GotoStatement {
    SyntaxNode base;
    INamed     name;
    Span*      span;
    Label*     label;
};

struct Label {
    SyntaxNode base;
    INamedEx   name;
    Span*      span;
    uint64_t   vmAddress;
};

struct TypeDefinition {
    SyntaxNode base;
    INamedEx   name;

    TaskStatus state;

    Type::TypeInfoEx* type;

    Variable** vars;
    uint32_t   varCount;
};

// TODO : unite under TypeDefinition?
struct Union {
    TypeDefinition base;
};

struct ErrorSet {
    SyntaxNode base;
    INamedEx   name;

    uint64_t   value;
    Variable** vars;
    uint32_t   varCount;
};

struct LangDef {
    String tag;

    // compile command used to compile foreign language
    String cmpCommand;

    //
    String fcnFormat;

    //
    String fcnFormatInArgs;

    //
    String fcnFormatOutArgs;

    // for now just simple solution, type is used as length
    Keyword* dtypeMap;
    int dtypeMapLen;
};

struct ImportStatement {
    SyntaxNode base;

    // tag of the import, like C for C-ABI
    String tag;

    // name of the importing file
    String fname;

    // defines how the file file content is wrapped
    // for now only KW_NAMESPACE, -1 is used for no wrapp
    Keyword keyword;

    // additional parrameter for ketWord
    // for KW_NAMESPACE its the name of the namespace
    String param;

    // root scope of the imports file
    // Scope* root;

    Extern::LibraryHandle lib;
};



// ======================================
// AST 'Interface' and stuff
// ===

// contain references to nodes in linear way
// so validator dont have to traverse tree that much
struct AstRegistry {
    static constexpr int dataSize = 29;
    union {
        DArray::Container     data[dataSize];
        struct {
            DArray::Container scopes;
            DArray::Container namespaces;
            DArray::Container langDefs;
            DArray::Container codeBlocks;
            DArray::Container foreignFunctions;
            DArray::Container variables;
            DArray::Container fcnCalls;
            DArray::Container fcns;
            DArray::Container customDataTypesReferences;
            DArray::Container variableAssignments;
            DArray::Container cmpTimeVars;
            DArray::Container arrays;
            DArray::Container loops;
            DArray::Container labels;
            DArray::Container branchExpressions;
            DArray::Container statements;
            DArray::Container initializations;
            DArray::Container returnStatements;
            DArray::Container switchCases;
            DArray::Container variableDefinitions;
            DArray::Container customErrors;
            DArray::Container unions;
            DArray::Container arraysAllocations;
            DArray::Container imports;
            DArray::Container customDataTypes;
            DArray::Container enumerators;
            DArray::Container gotos;
            DArray::Container allocations;
        };
    };
};
static_assert(sizeof(AstRegistry) == sizeof(AstRegistry::data),
              "AstRegistry: sizeof(data) != sizeof(AstRegistry)");

enum Severity : uint8_t {
    SEV_NOTE,
    SEV_WARNING,
    SEV_ERROR,
    SEV_FATAL
};

struct AstError {
    String   msg;
    Span*    span;
    uint32_t err;
    Severity severity;
};

struct AstContext {
    Scope* root;

    AstRegistry* reg; // TODO : remove?

    // each bit corresponds with the InternaFunction enum
    // indicates if function was used at least once in code
    uint64_t usedFunctionMask;

    // Null terminated string representing current working
    // scope. Ex. specify tag while logging.
    char* tag;

    // If we are used as library we may want to
    // track all errors
    AstError* errors;
    uint32_t  errorCount;
    uint32_t  totalErrorCount;
    // to handle allocations, primally
    Arena::Container errorArena;
};

namespace Ast {

    void init();
    void release();

    void init   (AstContext* ast);
    void release(AstContext* ast);

    void init   (AstRegistry* reg);
    void release(AstRegistry* reg);

    namespace Node {
        void init(Scope* node);
        void init(Namespace* node);
        void init(Using* node);
        void init(CodeBlock* node);
        void init(Enumerator* node);
        void init(Statement* node);
        void init(VariableDefinition* node);
        void init(VariableAssignment* node);
        void init(Variable* node);
        void init(Function* node);
        void init(ForeignFunction* node);
        void init(Branch* node);
        void init(SwitchCase* node);
        void init(WhileLoop* node);
        void init(Loop* node);
        void init(ReturnStatement* node);
        void init(ContinueStatement* node);
        void init(BreakStatement* node);
        void init(GotoStatement* node);
        void init(Label* node);
        void init(TypeDefinition* node);
        void init(Union* node);
        void init(ErrorSet* node);
        void init(ImportStatement* node);

        void init(FunctionCall* node);
        void init(OperationExpression* node);
        void init(UnaryExpression* node);
        void init(BinaryExpression* node);
        void init(TernaryExpression* node);
        void init(RangeExpression* node);
        void init(Catch* node);
        void init(Cast* node);
        void init(Alloc* node);
        void init(Free* node);

        void init(FunctionPrototype* node);
        void init(ArrayInitialization* node);
        void init(StringInitialization* node);
        void init(TypeInitialization* node);
        void init(QualifiedName* node);
        void init(TypeSpecifier* node);

        Scope*              makeScope();
        Namespace*          makeNamespace();
        Using*              makeUsing();
        CodeBlock*          makeCodeBlock();
        Enumerator*         makeEnumerator();
        Statement*          makeStatement();
        VariableDefinition* makeVariableDefinition();
        VariableAssignment* makeVariableAssignment();
        Variable*           makeVariable();
        Function*           makeFunction();
        ForeignFunction*    makeForeignFunction();
        Branch*             makeBranch();
        SwitchCase*         makeSwitchCase();
        WhileLoop*          makeWhileLoop();
        Loop*               makeLoop();
        ReturnStatement*    makeReturnStatement();
        ContinueStatement*  makeContinueStatement();
        BreakStatement*     makeBreakStatement();
        GotoStatement*      makeGotoStatement();
        Label*              makeLabel();
        TypeDefinition*     makeTypeDefinition();
        Union*              makeUnion();
        ErrorSet*           makeErrorSet();
        ImportStatement*    makeImportStatement();

        FunctionCall*        makeFunctionCall();
        OperationExpression* makeOperationExpression();
        UnaryExpression*     makeUnaryExpression();
        BinaryExpression*    makeBinaryExpression();
        TernaryExpression*   makeTernaryExpression();
        RangeExpression*     makeRangeExpression();
        Catch*               makeCatch();
        Cast*                makeCast();
        Alloc*               makeAlloc();
        Free*                makeFree();

        FunctionPrototype*    makeFunctionPrototype();
        ArrayInitialization*  makeArrayInitialization();
        StringInitialization* makeStringInitialization();
        TypeInitialization*   makeTypeInitialization();
        QualifiedName*        makeQualifiedName();
        TypeSpecifier*        makeTypeSpecifier();

        Scope*              copy(Scope* node);
        Namespace*          copy(Namespace* node);
        Using*              copy(Using* node);
        CodeBlock*          copy(CodeBlock* node);
        Enumerator*         copy(Enumerator* node);
        Statement*          copy(Statement* node);
        VariableDefinition* copy(VariableDefinition* node);
        VariableAssignment* copy(VariableAssignment* node);
        Variable*           copy(Variable* node);
        Function*           copy(Function* node);
        ForeignFunction*    copy(ForeignFunction* node);
        Branch*             copy(Branch* node);
        SwitchCase*         copy(SwitchCase* node);
        WhileLoop*          copy(WhileLoop* node);
        Loop*               copy(Loop* node);
        ReturnStatement*    copy(ReturnStatement* node);
        ContinueStatement*  copy(ContinueStatement* node);
        BreakStatement*     copy(BreakStatement* node);
        GotoStatement*      copy(GotoStatement* node);
        Label*              copy(Label* node);
        TypeDefinition*     copy(TypeDefinition* node);
        Union*              copy(Union* node);
        ErrorSet*           copy(ErrorSet* node);
        ImportStatement*    copy(ImportStatement* node);

        FunctionCall*           copy(FunctionCall* node);
        OperationExpression*    copy(OperationExpression* node);
        UnaryExpression*        copy(UnaryExpression* node);
        BinaryExpression*       copy(BinaryExpression* node);
        TernaryExpression*      copy(TernaryExpression* node);

        Variable* copy(Variable* dest, Variable* src);
        Variable* copyRef(Variable* dest, Variable* src);

        const char* str(NodeType type);
        const char* str(ExpressionType type);

        String getName(SyntaxNode* node);
        void   getName(SyntaxNode* node, String** str);

        Span* getNameSpan(SyntaxNode* node);
    };

    namespace Find {
        SyntaxNode* inArray(SyntaxNode** arr, uint32_t len, const String* const name);
        SyntaxNode* inArray(DArray::Container* arr, const String* const name);
        SyntaxNode* inScope(Scope* scope, const String* const name);

        Variable* inArray(Variable* arr, uint32_t len, String* name);

        Variable*           inArray(Variable**           arr, uint32_t len, String* name);
        Function*           inArray(Function**           arr, uint32_t len, String* name);
        Enumerator*         inArray(Enumerator**         arr, uint32_t len, String* name);
        Namespace*          inArray(Namespace**          arr, uint32_t len, String* name);
        Union*              inArray(Union**              arr, uint32_t len, String* name);
        ErrorSet*           inArray(ErrorSet**           arr, uint32_t len, String* name);
        TypeDefinition*     inArray(TypeDefinition**     arr, uint32_t len, String* name);
        Label*              inArray(Label**              arr, uint32_t len, String* name);
        GotoStatement*      inArray(GotoStatement**      arr, uint32_t len, String* name);
        Using*              inArray(Using**              arr, uint32_t len, String* name);
        ImportStatement*    inArray(ImportStatement**    arr, uint32_t len, String* name);
        CodeBlock*          inArray(CodeBlock**          arr, uint32_t len, String* name);
        ForeignFunction*    inArray(ForeignFunction**    arr, uint32_t len, String* name);
        VariableDefinition* inArray(VariableDefinition** arr, uint32_t len, String* name);

        // And 'full' functions that can be used to get the index. In fact, these alone
        // are sufficient, but I feel having both versions is more convenient.
        // There's something about going down the rabbit hole...
        int inArray(Variable**           arr, uint32_t len, String* name, Variable** out);
        int inArray(Function**           arr, uint32_t len, String* name, Function** out);
        int inArray(Enumerator**         arr, uint32_t len, String* name, Enumerator** out);
        int inArray(Namespace**          arr, uint32_t len, String* name, Namespace** out);
        int inArray(Union**              arr, uint32_t len, String* name, Union** out);
        int inArray(ErrorSet**           arr, uint32_t len, String* name, ErrorSet** out);
        int inArray(TypeDefinition**     arr, uint32_t len, String* name, TypeDefinition** out);
        int inArray(Label**              arr, uint32_t len, String* name, Label** out);
        int inArray(GotoStatement**      arr, uint32_t len, String* name, GotoStatement** out);
        int inArray(Using**              arr, uint32_t len, String* name, Using** out);
        int inArray(ImportStatement**    arr, uint32_t len, String* name, ImportStatement** out);
        int inArray(CodeBlock**          arr, uint32_t len, String* name, CodeBlock** out);
        int inArray(ForeignFunction**    arr, uint32_t len, String* name, ForeignFunction** out);
        int inArray(VariableDefinition** arr, uint32_t len, String* name, VariableDefinition** out);

        Variable*       inScopeVariable       (Scope* scope, String* name);
        Function*       inScopeFunction       (Scope* scope, String* name);
        Union*          inScopeUnion          (Scope* scope, String* name);
        Label*          inScopeLabel          (Scope* scope, String* name);
        ErrorSet*       inScopeErrorSet       (Scope* scope, String* name);
        TypeDefinition* inScopeTypeDefinition (Scope* scope, String* name);
        Enumerator*     inScopeEnumerator     (Scope* scope, String* name);
        Namespace*      inScopeNamespace      (Scope* scope, String* name);
        GotoStatement*  inScopeGotoStatement  (Scope* scope, String* name);
    };

    namespace Internal {
        const char IFS_PRINTF[] = "printf";
        const char IFS_ALLOC[]  = "malloc";
        const char IFS_FREE[]   = "free";

        const char IVS_NULL[]    = "null";
        const char IVS_TRUE[]    = "true";
        const char IVS_FALSE[]   = "false";

        enum VariableType {
            IV_NULL,
            IV_TRUE,
            IV_FALSE,
            IV_COUNT
        };

        enum FunctionType : uint32_t {
            IF_PRINTF = 1,
            IF_ALLOC  = 2,
            IF_FREE   = 3,
            IF_COUNT  = 4,
        };

        extern Variable variables[IV_COUNT];
        extern Function functions[IF_COUNT];

        extern Variable* zero;
    };

};



// ======================================
// NODE ALLOCATOR
// ===

constexpr int nodeTypeSize[AT_COUNT] = {
    sizeof(Scope),
    sizeof(VariableDefinition),
    sizeof(VariableAssignment),
    sizeof(TypeDefinition),
    sizeof(TypeInitialization),
    sizeof(Enumerator),
    sizeof(Variable),
    sizeof(Function),
    sizeof(Branch),
    sizeof(SwitchCase),
    sizeof(WhileLoop),
    sizeof(Loop),
    sizeof(ReturnStatement),
    sizeof(ContinueStatement),
    sizeof(BreakStatement),
    sizeof(GotoStatement),
    sizeof(Label),
    sizeof(Namespace),
    sizeof(Statement),
    sizeof(CodeBlock),
    sizeof(ErrorSet),
    sizeof(Union),
    sizeof(Using),
    sizeof(ImportStatement),

    sizeof(uint8_t),

    sizeof(FunctionCall),
    sizeof(OperationExpression),
    sizeof(UnaryExpression),
    sizeof(BinaryExpression),
    sizeof(TernaryExpression),
    sizeof(RangeExpression),
    sizeof(TypeInitialization),
    sizeof(StringInitialization),
    sizeof(ArrayInitialization),
    sizeof(Catch),
    sizeof(Cast),
    sizeof(Alloc),
    sizeof(Free),
    sizeof(GetLength),
    sizeof(GetSize),

    sizeof(INamed),
    sizeof(QualifiedName),
    sizeof(FunctionPrototype),
    sizeof(ForeignFunction),
    sizeof(TypeSpecifier),

    sizeof(char)
};

// This is more specific version of the allocator to handle allocations
// of nodes defined here, so there is an abstraction how each node type
// is handled.
// Look at allocator.h to for the 'actual' allocator that this is based on
#if !defined(_CUSTOM_ALLOCATOR_)

    inline thread_local AllocatorHandle nalc = NULL;

    inline void initNAlloc(AllocatorHandle allocator) {
        nalc = allocator;
    }

    inline void releaseNAlloc(AllocatorHandle allocator) {
    }

    inline void* nalloc(AllocatorHandle allocator, AllocType type) {
        return alloc(allocator, nodeTypeSize[type]);
    }

    inline void* nalloc(AllocatorHandle allocator, AllocType type, size_t count) {
        return alloc(allocator, nodeTypeSize[type] * count);
    }

    inline void ndealloc(AllocatorHandle allocator, void* ptr) {

    }

#else

    extern thread_local AllocatorHandle nalc;

    extern void* nalloc   (AllocatorHandle allocator, AllocType type);
    extern void* nalloc   (AllocatorHandle allocator, AllocType type, size_t count);
    extern void  ndealloc (AllocatorHandle allocator, void* ptr);

#endif



// ======================================
// MISC
// ===

Variable* unwrap(Variable* var);



AcquireNodeReturn acquireNode(uint8_t* statusField, uint8_t* nodeWorkerId, uint8_t workerId, bool wait);
void releaseNode(uint8_t* statusField, bool success);
