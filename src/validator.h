#pragma once

#include "array_list.h"
#include "data_types.h"
#include "dynamic_arena.h"
#include "registry.h"
#include "set.h"
#include "syntax.h"
#include "diagnostic.h"
#include <cstdint>



namespace Validator {

    // LOOK AT : Spacing defines max argument count
    enum ScoreWeight : uint64_t {
        FOS_IMPLICIT_CAST   = 1ULL << 0,
        FOS_SIZE_DECREASE   = 1ULL << 8,
        FOS_SIGN_CHANGE     = 1ULL << 16,
        FOS_TO_FLOAT        = 1ULL << 24,
        FOS_PROMOTION       = 1ULL << 32,
        FOS_EXACT_MATCH     = 1ULL << 40,
        FOS_NON_VAR_BONUS   = 1ULL << 48
    };

    struct FunctionScore {
        uint64_t value;
    };

    // TODO : better name?
    struct NamespaceIndex {
        DArray::Container symbols;
        Set::Container subIndex;
    };

    struct ValidationContext {
        Reg::Unit* unit;

        Set::Container    searchSet;
        DArray::Container fCandidates; // f as function

        // Used during validation to handle lookups
        // of unbound nodes
        NamespaceIndex* unboundIndex;
        DArray::Container gatheringStackA;
        DArray::Container gatheringStackB;

        Arena::Container stringArena;
        Arena::Container tmpArena;

        SyntaxNode* currentLoop;
        Function* currentFunction;

        FileSystem::Path fileDir;

        // TODO : deprecated, we shall use TaskSystem getter directly
        uint8_t workerId;
    };



    void init   (ValidationContext* ctx);
    void release(ValidationContext* ctx);

    Err::Err preValidate(ValidationContext* ctx);

    Err::Err validate       (ValidationContext* ctx);
    Err::Err ensureValidated(ValidationContext* ctx, SyntaxNode* node, SyntaxNode* triggerNode = NULL);

    Err::Err verifyFunctionsAreGlobal (ValidationContext* ctx);
    Err::Err verifyImportsAreGlobal   (ValidationContext* ctx);
    Err::Err verifyNamespacesAreGlobal(ValidationContext* ctx);

    Err::Err checkDuplicateNames(ValidationContext* ctx);

    Err::Err linkDataType(ValidationContext* ctx, VariableDefinition* def);
    Err::Err linkErrorSet(ValidationContext* ctx, Function* fcn);
    Err::Err linkVariable(ValidationContext* ctx, Variable* var);
    Err::Err linkGoto    (ValidationContext* ctx, GotoStatement* gt);
    Err::Err linkCall    (ValidationContext* ctx, Variable* callVar);

    Err::Err resolveResultType(ValidationContext* ctx, UnaryExpression* uex, Variable* var);
    Err::Err resolveResultType(ValidationContext* ctx, BinaryExpression* bex, Variable* var);
    Err::Err resolveResultType(ValidationContext* ctx, FunctionCall* fex, Variable* var);
    Err::Err applyImplicitCast(ValidationContext* ctx, Value* lval, Variable* rvar);
    Err::Err computeTypeInfo  (ValidationContext* ctx, Value* val, Type::TypeInfo** outInfo);
    Err::Err computeTypeInfo  (ValidationContext* ctx, TypeDefinition* td);
    void     castLiteral      (ValidationContext* ctx, Value* val, Type::Kind toDtype);

    Err::Err validate(ValidationContext* ctx, VariableDefinition* node);
    Err::Err validate(ValidationContext* ctx, Function* node);
    Err::Err validate(ValidationContext* ctx, TypeDefinition* node);

    Err::Err validate(ValidationContext* ctx, SyntaxNode* node);

    Err::Err validate(ValidationContext* ctx, Variable* node, Variable* target = NULL);
    Err::Err validate(ValidationContext* ctx, Scope* node);
    Err::Err validate(ValidationContext* ctx, VariableAssignment* node);
    Err::Err validate(ValidationContext* ctx, Branch* node);
    Err::Err validate(ValidationContext* ctx, SwitchCase* node);
    Err::Err validate(ValidationContext* ctx, WhileLoop* node);
    Err::Err validate(ValidationContext* ctx, Loop* node);
    Err::Err validate(ValidationContext* ctx, ReturnStatement* node);
    Err::Err validate(ValidationContext* ctx, Enumerator* node);
    Err::Err validate(ValidationContext* ctx, Statement* node);
    Err::Err validate(ValidationContext* ctx, ErrorSet* node);

    Err::Err validateCall(ValidationContext* ctx, Variable* callOp);

    Err::Err validateImplicitCast(const Type::Kind dtype, const Type::Kind dtypeRef);
    Err::Err validateImplicitCast(ValidationContext* ctx, void* dtype, void* dtypeRef, Type::Kind dtypeEnum, Type::Kind dtypeEnumRef);
    Err::Err validateAttributeCast(Variable* var, Variable* attribute);
    Err::Err validatePointerAssignment(AstContext* ast, const Value* const val);

    Err::Err applyVariableLinkage(ValidationContext* ctx, Variable* var, SyntaxNode* definition);

    SymbolIndexEntry* findSymbolAsIndexEntry(ValidationContext* ctx, Scope* scope, String name);
    SymbolIndexEntry* findSymbolAsIndexEntryRecursive(ValidationContext* ctx, Scope* startScope, String name);
    SyntaxNode*       findSymbol(ValidationContext* ctx, Scope* scope, String name);
    SyntaxNode*       findSymbolRecursive(ValidationContext* ctx, Scope* startScope, String name);

    Err::Err resolveQualifiedPath(ValidationContext* ctx, Scope* startScope, QualifiedName* qname, Scope** outScope);
    Err::Err resolveQualifiedNameAsIndexEntry(ValidationContext* ctx, Scope* startScope, QualifiedName* name, SymbolIndexEntry** outEntry, bool reportErrors = true);

    Function*   findExactFunction  (Scope* scope, INamed* const name, FunctionPrototype* const fptr);
    Function*   findClosestFunction(SymbolIndexEntry* entry, Variable* callOp);
    SyntaxNode* findInternalSymbol (const String* name);

    Value toValue(Type::Kind kind);

    bool areInOrder(Span* before, Span* after);
    bool isOrderingValid(SymbolIndexEntry* entry, QualifiedName* name);

    Type::Kind getFirstNonArrayDtype(Array* arr, int maxLevel = -1, int* level = NULL, void** outType = NULL);

}
