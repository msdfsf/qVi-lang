// Each file (unit) is validated as a separate task in the TaskSystem.
//
// Every SyntaxNode carries two u8 statuses:
//   - semStatus: Tracks semantic validation (Linking, Types, Constraints).
//   - cmpStatus: Tracks compile-time evaluation (Bytecode, Constant Values).
//
// It is assumed that semantic validation must be completed before evaluation
// for all required nodes. Therefore, these statuses can be treated as linear:
// semStatus is processed first, followed by cmpStatus. As a result, each node
// has only one workerId shared across both procedures.
//
// Validation of each unit is handled locally. If a node from
// another unit is required, it can be claimed via an atomic
// reference and processed immediately locally, or awaited
// if another thread is already performing the work.
//
// In general, there is no limit to the dependency intricacies a program may
// form. For example, the whole program may be structured in such a way that
// compiling the first local statement consequently triggers resolution of all
// nodes, effectively resolving the entire program. Therefore, the system is
// designed around this behavior rather than around linear discrete passes.
//
// To lower multi-threading overhead, only top-level definitions
// (Functions, Types, Globals) that can serve as a 'communication' boundary
// between units are treated as multi-threaded and properly acquired through
// atomics. All basic nodes can be processed without such overhead, as they
// are either part of an already acquired resource or part of a resource that
// cannot be acquired independently at all.
//
// Discrete tasks that can be handled independently—such as
// compile-time evaluation after all type information is
// resolved—can be offloaded to the TaskSystem.

// TODO : unite calling TypeInfo instances as 'type' or 'aType' etc.
//        for now they usually go as 'info'

#include "validator.h"
#include "array_list.h"
#include "data_types.h"
#include "dynamic_arena.h"
#include "file_system.h"
#include "foreign_code.h"
#include "globals.h"
#include "logger.h"
#include "operators.h"
#include "registry.h"
#include "set.h"
#include "string.h"
#include "strlib.h"
#include "syntax.h"
#include "interpreter.h"
#include "lexer.h"
#include "supplement/runtime.h"
#include "task_status.h"
#include "utils.h"
#include "diagnostic.h"
#include "config.h"
#include "debug_helper.h"

#include <algorithm>
#include <cstdint>
#include <cstring>



namespace Validator {

    enum {
        EXACT_MATCH = 1
    };

    Err::Err evaluate(Variable* var);



    void init(ValidationContext* ctx) {
        DArray::init(&ctx->fCandidates, 32, sizeof(FunctionPrototype));
        Set::init(&ctx->searchSet, 64);
        Arena::init(&ctx->stringArena, 8 * 1024);
        Arena::init(&ctx->tmpArena, 8 * 1024);
    }

    void release(ValidationContext* ctx) {
        DArray::release(&ctx->fCandidates);
        Set::release(&ctx->searchSet);
        Arena::release(&ctx->stringArena);
        Arena::release(&ctx->tmpArena);
    }



    // TODO : move to appropriate place
    // TODO : later use set for counts >= 20 or something
    bool checkUniqueNames(Variable** arr, uint32_t len) {
        // TODO : later use set for counts >= 20 or something
        for (int i = 0; i < len; i++) {
            Variable* var = arr[i];
            INamed* src = (INamed*) &var->name;

            for (int j = i; j < len; j++) {
                Variable* var = arr[i];
                INamed* dest = (INamed*) &var->name;
                if (cstrcmp(*src, *dest)) return false;
            }
        }

        return true;
    }

    bool checkUniqueNames(SyntaxNode** arr, uint32_t len) {
        // TODO : later use set for counts >= 20 or something
        for (int i = 0; i < len; i++) {
            SyntaxNode* node = arr[i];
            String src = Ast::Node::getName(node);

            for (int j = i; j < len; j++) {
                SyntaxNode* node = arr[i];
                String dest = Ast::Node::getName(node);
                if (cstrcmp(src, dest)) return false;
            }
        }

        return true;
    }

    bool checkUniqueNames(DArray::Container* arr) {
        return checkUniqueNames((Variable**) arr->buffer, arr->size);
    }



    Err::Err bindFromExternalLibrary(ValidationContext* ctx, Function* fcn) {
        Err::Err err;

        if (fcn->lib) return Err::OK;

        ImportStatement* import = fcn->base.scope->base.import;
        if (!import) {
            Diag::report(ctx->unit->ast, fcn->base.span, Err::UNEXPECTED_ERROR,
                Diag::Format{
                    "No foreign library found for fcn '%.*s'.\n"
                },
                fcn->name.len, fcn->name.buff
            );

            return Err::UNEXPECTED_ERROR;
        }

        if (import->lib) {
            fcn->lib = import->lib;
            return Extern::resolveFunction(ctx->unit->ast, fcn);
        }

        err = Extern::loadLibrary(ctx->unit->ast, import->fname, Extern::LL_INSPECT, &fcn->lib);
        if (err != Err::OK) return err;

        import->lib = fcn->lib;

        return Extern::resolveFunction(ctx->unit->ast, fcn);
    }

    Err::Err validate(ValidationContext* ctx, Function* fcn) {
        Err::Err err;

        Function* prevFcn = ctx->currentFunction;
        ctx->currentFunction = fcn;

        linkErrorSet(ctx, fcn);

        for (uint32_t i = 0; i < fcn->prototype.inArgCount; i++) {
            err = validate(ctx, fcn->prototype.inArgs[i]);
            if (err != Err::OK) return err;
        }

        if (fcn->bodyScope) {
            err = validate(ctx, fcn->bodyScope);
            if (err != Err::OK) return err;
        } else {
            // signature only: function from extern library
            err = bindFromExternalLibrary(ctx, fcn);
            if (err != Err::OK) return err;
        }

        err = validate(ctx, fcn->prototype.outArg);
        if (err != Err::OK) return err;

        ctx->currentFunction = prevFcn;

        return Err::OK;
    }

    struct TmpOverloadEntry {
        SyntaxNode*       node;
        TmpOverloadEntry* next;
    };

    // Lock beforehand scope if needed
    Err::Err ensureIndexedIfNeeded(ValidationContext* ctx, Scope* scope) {
        if (scope->index || scope->definitionCount <= Config::LINEAR_SEARCH_THRESHOLD) {
            return Err::OK;
        }

        scope->index = (SymbolIndex*) alloc(alc, sizeof(SymbolIndex));

        Set::init(&scope->index->set, scope->definitionCount);
        scope->index->set.hashMethod = Set::HM_STRING_STRUCT_FNV1A;
        scope->index->set.keyOffset = offsetof(SymbolIndexEntry, name);
        scope->index->set.keyStorage = Set::KS_VALUE;

        Arena::Marker marker = Arena::getMarker(&ctx->tmpArena);

        for (uint32_t i = 0; i < scope->definitionCount; i++) {
            SyntaxNode* node = scope->definitions[i];
            String name = Ast::Node::getName(node);
            if (!name.buff) continue;

            SymbolIndexEntry* entry = (SymbolIndexEntry*) Set::find(&scope->index->set, name);
            if (entry) {
                TmpOverloadEntry* newEntry = (TmpOverloadEntry*) Arena::push(&ctx->tmpArena, sizeof(TmpOverloadEntry));
                newEntry->node = node;

                if (entry->kind == SymbolIndexEntry::SINGLE) {
                    TmpOverloadEntry* head = (TmpOverloadEntry*) Arena::push(&ctx->tmpArena, sizeof(TmpOverloadEntry));
                    head->node = entry->node;
                    head->next = newEntry;
                    newEntry->next = NULL;

                    entry->kind = SymbolIndexEntry::OVERLOAD;
                    entry->overloads.data = (SyntaxNode**) head;
                    entry->overloads.count = 2;
                } else {
                    newEntry->next = (TmpOverloadEntry*) entry->overloads.data;
                    entry->overloads.data = (SyntaxNode**) newEntry;
                    entry->overloads.count++;
                }
            } else {
                SymbolIndexEntry* entry = (SymbolIndexEntry*) alloc(alc, sizeof(SymbolIndexEntry));
                entry->name = name;
                entry->kind = SymbolIndexEntry::SINGLE;
                entry->node = node;

                Set::insert(&scope->index->set, (uint8_t*) entry);
            }
        }

        for (uint32_t i = 0; i < scope->index->set.tableSize; i++) {
            Set::Slot* slot = scope->index->set.table + i;
            if (slot->type != Set::ST_OCCUPIED) continue;

            SymbolIndexEntry* entry = (SymbolIndexEntry*) slot->data;
            if (entry->kind == SymbolIndexEntry::OVERLOAD) {
                const uint32_t count = entry->overloads.count;
                TmpOverloadEntry* curr = (TmpOverloadEntry*) entry->overloads.data;

                SyntaxNode** data = (SyntaxNode**) alloc(alc, sizeof(SyntaxNode*) * count);

                for (uint32_t j = 0; j < count; j++) {
                    data[j] = curr->node;
                    curr = curr->next;
                }

                entry->overloads.data = data;
            }
        }

        Arena::rollback(&ctx->tmpArena, marker);

        return Err::OK;
    }

    // Find Trivia:
    // All 'findSymbol' functions shall serve as 'dumb' search functions, that return
    // result (or not) as its without acunting for language semantics.
    // For 'smart' search 'resolveSymbol' functions shall be used. They, by the nature,
    // have to 'throw' errors if needed, which shall be possible to silent.
    //
    // 'Functions further differ by the provided result:
    // 'asIndexEntry' functions are the 'core' ones, that return general index entry,
    // which can support overloaded results. They need an allocator to supply, as
    // index may not be avaliable, or its not reasonable to build one, and result
    // in such cases has to be allocated.
    //
    // pure 'findSymbol' functions shall work direcly on SyntaxNode and ignore overloaded
    // cases (After pre-validation defintions that doesnt support overloading are guaranted
    // to be unique in scope).
    //
    // 'dumb' functions also can have 'Recursive' 'postfix'.
    // These functions shall search all scopes recursively, therefore each 'default'
    // function just search provided scope, no more. (Ex. one qualified path may be
    // resolved once and used in different consequent checks).
    //
    //
    //
    // TODO : create a function that automaticaly calls findClosestFunction etc.
    //
    // TODO : make these find function take directly arena instead of ctx
    // Allows to use the index abstraction even for cases when index is not builded
    // Uses ctx->tmpArena to allocate data, so make sure to unroll when needed.
    SymbolIndexEntry* findSymbolAsIndexEntry(ValidationContext* ctx, Scope* scope, String name) {
        if (scope->index) {
            SymbolIndexEntry* entry = (SymbolIndexEntry*) Set::find(&scope->index->set, name);
            return entry;
        }

        uint32_t matchCount = 0;
        SyntaxNode* firstMatch = NULL;

        for (uint32_t i = 0; i < scope->definitionCount; i++) {
            if (cstrcmp(name, Ast::Node::getName(scope->definitions[i]))) {
                if (matchCount == 0) firstMatch = scope->definitions[i];
                matchCount++;
            }
        }

        if (matchCount == 0) return NULL;

        SymbolIndexEntry* entry = (SymbolIndexEntry*) Arena::push(&ctx->tmpArena, sizeof(SymbolIndexEntry));
        entry->name = name;

        if (matchCount == 1) {
            entry->kind = SymbolIndexEntry::SINGLE;
            entry->node = firstMatch;
        } else {
            entry->kind = SymbolIndexEntry::OVERLOAD;
            entry->overloads.count = 0;

            entry->overloads.data = (SyntaxNode**) Arena::push(&ctx->tmpArena, sizeof(SyntaxNode*) * matchCount);

            for (uint32_t i = 0; i < scope->definitionCount; i++) {
                if (cstrcmp(name, Ast::Node::getName(scope->definitions[i]))) {
                    entry->overloads.data[entry->overloads.count++] = scope->definitions[i];
                }
            }
        }

        return entry;
    }

    SymbolIndexEntry* findSymbolAsIndexEntryRecursive(ValidationContext* ctx, Scope* startScope, String name) {
        Scope* current = startScope;

        while (current) {
            // translatorDebug.printNode(stdout, 0, (SyntaxNode*) startScope, NULL);
            SymbolIndexEntry* entry = findSymbolAsIndexEntry(ctx, current, name);
            if (entry) return entry;

            current = current->base.scope;
        }

        return NULL;
    }

    // Returns a node only if it's not overloaded
    // Supposed to be used in valid context (pre-validation step is completed).
    SyntaxNode* findSymbol(ValidationContext* ctx, Scope* scope, String name) {
        Arena::Marker marker = Arena::getMarker(&ctx->tmpArena);
        SymbolIndexEntry* entry = findSymbolAsIndexEntry(ctx, scope, name);
        Arena::rollback(&ctx->tmpArena, marker);

        if (entry && entry->kind == SymbolIndexEntry::SINGLE) {
            return entry->node;
        }

        return NULL;
    }

    // Returns a node only if it's not overloaded
    // Supposed to be used in valid context (pre-validation step is completed).
    SyntaxNode* findSymbolRecursive(ValidationContext* ctx, Scope* startScope, String name) {
        Scope* current = startScope;

        while (current) {
            SyntaxNode* node = findSymbol(ctx, current, name);
            if (node) return node;

            current = current->base.scope;
        }

        return NULL;
    }

    // Uses ctx->tmpBuffer
    Err::Err resolveQualifiedPath(ValidationContext* ctx, Scope* startScope, QualifiedName* qname, Scope** outScope) {
        if (!qname || qname->pathSize == 0) {
            *outScope = startScope;
            return Err::OK;
        }

        Scope* currentSearchScope = startScope;

        for (uint16_t i = 0; i < qname->pathSize; i++) {
            String segment = *(String*) (qname->path + i);

            Arena::Marker marker = Arena::getMarker(&ctx->tmpArena);

            SymbolIndexEntry* entry;
            if (i == 0) {
                entry = findSymbolAsIndexEntryRecursive(ctx, currentSearchScope, segment);
            } else {
                entry = findSymbolAsIndexEntry(ctx, currentSearchScope, segment);
            }

            Arena::rollback(&ctx->tmpArena, marker);

            if (!entry) {
                Diag::report(ctx->unit->ast, qname->span, Err::UNEXPECTED_SYMBOL,
                    Diag::Format {
                        "Segment '%.*s' not found."
                    }, segment.len, segment.buff);
                return Err::UNEXPECTED_SYMBOL;
            }

            if (entry->kind != SymbolIndexEntry::SINGLE || entry->node->type != NT_NAMESPACE) {
                Diag::report(ctx->unit->ast, qname->span, Err::UNEXPECTED_SYMBOL,
                    Diag::Format {
                        "Path segment '%.*s' is not a namespace."
                    }, segment.len, segment.buff);
                return Err::UNEXPECTED_SYMBOL;
            }

            Namespace* nspace = (Namespace*) entry->node;

            Err::Err err = ensureValidated(ctx, (SyntaxNode*) nspace);
            if (err != Err::OK) return err;

            currentSearchScope = &nspace->scope;
        }

        *outScope = currentSearchScope;
        return Err::OK;
    }

    // Uses ctx->tmpArena
    Err::Err resolveQualifiedNameAsIndexEntry(ValidationContext* ctx, Scope* startScope, QualifiedName* name, SymbolIndexEntry** outEntry, bool reportErrors) {
        Scope* scope;
        Err::Err err = resolveQualifiedPath(ctx, startScope, name, &scope);
        if (err != Err::OK) return err;

        Arena::Marker marker = Arena::getMarker(&ctx->tmpArena);

        bool isOrderingInvalid = false;

        SymbolIndexEntry tmpErrorEntry;
        SymbolIndexEntry* entry;
        if (name->pathSize == 0) {
            // We are not in a namespace and are free to search
            while (1) {
                entry = findSymbolAsIndexEntryRecursive(ctx, scope, *(String*) name);
                if (!entry) {
                    // If ordering invalid, we know there was previous successful lookup,
                    // therefore it cannot be internal symbol
                    if (isOrderingInvalid) {
                        entry = &tmpErrorEntry;
                        break;
                    }

                    SyntaxNode* node = findInternalSymbol((String*) name);
                    if (node) {
                        entry = (SymbolIndexEntry*)Arena::push(&ctx->tmpArena, sizeof(SymbolIndexEntry));
                        entry->kind = SymbolIndexEntry::SINGLE;
                        entry->node = node;
                    }
                    break;
                }

                if (isOrderingValid(entry, name)) {
                    isOrderingInvalid = false;
                    break;
                }

                isOrderingInvalid = true;
                tmpErrorEntry = *entry;

                scope = entry->node->scope->base.scope;
                Arena::rollback(&ctx->tmpArena, marker);
            }
        } else {
            // We search only in found namespace
            entry = findSymbolAsIndexEntry(ctx, scope, *(String*) name);
            if (entry) {
                isOrderingInvalid = !isOrderingValid(entry, name);
            }
        }

        if (entry && !isOrderingInvalid) {
            *outEntry = entry;
            return Err::OK;
        }

        if (reportErrors) {
            if (isOrderingInvalid) {
                SyntaxNode* node = entry->node;

                Logger::logNoFlush(
                    { .level = Logger::Level::ERROR, .tag = ctx->unit->ast->tag },
                    "Local symbol '%.*s' used before its declaration.",
                    name->span, name->len, name->buff
                );

                Logger::logNoFlush(
                    { .level = Logger::Level::ERROR, .tag = ctx->unit->ast->tag },
                    "Declaration of '%.*s' is here.",
                    node->span, name->len, name->buff
                );

                Diag::commit(ctx->unit->ast, node->span, Err::DECLARATION_AFTER_USE);
                err = Err::DECLARATION_AFTER_USE;
            } else if (!entry) {
                Diag::report(ctx->unit->ast, name->span, Err::SYMBOL_NOT_FOUND,
                    Diag::Format{
                        "Undefined symbol '%.*s'."
                    }, name->len, name->buff
                );
                err = Err::SYMBOL_NOT_FOUND;
            }
        } else {
            err = Err::OK;
        }

        Arena::rollback(&ctx->tmpArena, marker);

        *outEntry = NULL;
        return err;
    }

    Err::Err resolveQualifiedName(ValidationContext* ctx, Scope* startScope, QualifiedName* name, SyntaxNode** outNode) {
        Arena::Marker marker = Arena::getMarker(&ctx->tmpArena);

        SymbolIndexEntry* entry = NULL;
        Err::Err err = resolveQualifiedNameAsIndexEntry(ctx, startScope, name, &entry, true);
        if (err != Err::OK) return err;

        if (entry->kind == SymbolIndexEntry::SINGLE) {
            *outNode = entry->node;
        } else {
            Diag::report(ctx->unit->ast, name->span, Err::SYMBOL_NOT_FOUND,
                Diag::Format{
                    "Undefined symbol '%.*s'."
                }, name->len, name->buff
            );

            *outNode = NULL;
            err = Err::SYMBOL_NOT_FOUND;
        }

        Arena::rollback(&ctx->tmpArena, marker);
        return err;
    }

    bool signaturesMatch(FunctionPrototype* const fptrA, FunctionPrototype* const fptrB) {
        if (fptrA->inArgCount != fptrB->inArgCount) return false;

        for (int i = 0; i < fptrA->inArgCount; i++) {
            Variable* pA = fptrA->inArgs[i]->var;
            Variable* pB = fptrB->inArgs[i]->var;
            if (pA->value.type != pB->value.type) return false;
        }

        return true;
    }

    bool signaturesMatch(Function* fcnA, Function* fcnB) {
        return signaturesMatch(&fcnA->prototype, &fcnB->prototype);
    }

    Err::Err ensureUniqueDefinitions(ValidationContext* ctx, Scope* scope) {
        if (scope->base.flags & IS_UNIQUE) {
            return Err::OK;
        }

        SyntaxNode* firstToCollide;
        SyntaxNode* lastToCompare;

        const uint32_t count = scope->definitionCount;

        ensureIndexedIfNeeded(ctx, scope);
        for (int i = 0; i < count; i++) {
            String name = Ast::Node::getName(scope->definitions[i]);
            if (!name.buff) continue;

            Arena::Marker marker = Arena::getMarker(&ctx->tmpArena);
            SymbolIndexEntry* entry = findSymbolAsIndexEntry(ctx, scope, name);
            Arena::rollback(&ctx->tmpArena, marker);

            if (entry && entry->kind == SymbolIndexEntry::SINGLE) {
                continue;
            }

            const uint32_t overloadCount = entry ? entry->overloads.count : 0;
            SyntaxNode** const nodes = entry ? entry->overloads.data : NULL;
            if (
                overloadCount > 0 &&
                nodes[0]->type != NT_FUNCTION
            ) {
                Logger::logNoFlush(
                    { .level = Logger::Level::ERROR, .tag = ctx->unit->ast->tag },
                    "Symbol '%.*s' is redefined across incompatible types. Only functions support overloading.",
                    Ast::Node::getNameSpan(nodes[0]), entry->name.len, entry->name.buff
                );

                // TODO : add max count to config
                for (uint32_t k = 0; k < overloadCount; k++) {
                    Logger::logNoFlush(
                        { .level = Logger::Level::ERROR, .style = Logger::Style::NO_HEADER, .tag = ctx->unit->ast->tag },
                        "Defined here as a %s.",
                        nodes[k]->span,
                        Ast::Node::str(nodes[k]->type)
                    );
                }

                Diag::commit(ctx->unit->ast, nodes[0]->span, Err::SYMBOL_ALREADY_DEFINED);
                return Err::SYMBOL_ALREADY_DEFINED;
            }
        }

        return Err::OK;
    }

    // Pass validated function and call
    void computeFunctionMatchScore(Function* fcn, FunctionCall* call, FunctionScore* outScore) {
        const uint32_t fcnInCnt  = fcn->prototype.inArgCount;
        const uint32_t callInCnt = call->inArgCount;

        const bool isVariadic = (fcnInCnt > 0 &&
            fcn->prototype.inArgs[fcnInCnt - 1]->var->value.type->kind == Type::DT_MULTIPLE_TYPES);

        if (!isVariadic && fcnInCnt != callInCnt) {
            outScore->value = 0;
            return;
        }
        if (isVariadic && callInCnt < (fcnInCnt - 1)) {
            outScore->value = 0;
            return;
        }

        uint64_t score = 0;

        // Prefer fixed-signatures over variadic ones
        if (!isVariadic) score += FOS_NON_VAR_BONUS;

        const uint32_t fixedCount = isVariadic ? fcnInCnt - 1 : fcnInCnt;

        for (uint32_t i = 0; i < callInCnt; i++) {
            // Handle Variadic tail
            if (i >= fixedCount) {
                score += FOS_IMPLICIT_CAST;
                continue;
            }

            Variable* fArg = fcn->prototype.inArgs[i]->var;
            Variable* cArg = call->inArgs[i];

            const Type::Kind fDtype = fArg->value.type->kind;
            const Type::Kind cDtype = cArg->value.type->kind;

            // Exact Match
            if (fArg->value.type == cArg->value.type) {
                score += FOS_EXACT_MATCH;
                continue;
            }

            // Numeric Rules
            if (Type::isInt(cDtype) && Type::isInt(fDtype)) {
                if (Type::isSignedInt(cDtype) == Type::isSignedInt(fDtype)) {
                    // Promotion (Safe) vs Decrease (Dangerous)
                    score += Type::basicTypes[cDtype].size < Type::basicTypes[fDtype].size
                             ? FOS_PROMOTION : FOS_SIZE_DECREASE;
                } else {
                    score += FOS_SIGN_CHANGE;
                }
                continue;
            }

            if (Type::isFloat(cDtype) && Type::isFloat(fDtype)) {
                score += Type::basicTypes[cDtype].size < Type::basicTypes[fDtype].size
                         ? FOS_PROMOTION : FOS_SIZE_DECREASE;
                continue;
            }

            if (Type::isInt(cDtype) && Type::isFloat(fDtype)) {
                score += FOS_TO_FLOAT;
                continue;
            }

            // General Implicit Casts
            if (validateImplicitCast(cDtype, fDtype)) {
                score += FOS_IMPLICIT_CAST;
                continue;
            }

            // Missmatch
            outScore->value = 0;
            return;
        }

        outScore->value = score;
    }

    // TODO : on error fill ctx with top score functions for better message.
    Function* findClosestFunction(SymbolIndexEntry* entry, Variable* callOp) {
        Scope* scope        = callOp->base.scope;
        FunctionCall* call  = (FunctionCall*) callOp->expression;
        const int callInCnt = call->inArgCount;

        if (!entry) return NULL;
        if (entry->kind == SymbolIndexEntry::SINGLE) {
            if (entry->node->type == NT_FUNCTION) {
                return (Function*) entry->node;
            } else {
                return NULL;
            }
        }

        Function* bestFunction = NULL;
        FunctionScore bestScore = { .value = 0 };
        int sameScoreCnt = 0;

        for (int i = 0; i < entry->overloads.count; i++) {
            Function* fcn = (Function*) entry->overloads.data[i];

            FunctionScore score;
            computeFunctionMatchScore(fcn, call, &score);

            if (score.value > bestScore.value) {
                bestScore = score;
                bestFunction = fcn;
            } else if (score.value == bestScore.value) {
                sameScoreCnt++;
                continue;
            }

            sameScoreCnt = 0;
        }

        if (bestScore.value > 0 && sameScoreCnt == 0) {
            return bestFunction;
        }

        return NULL;
    }

    // TODO : incorporate to Ast::Internal
    SyntaxNode* findInternalSymbol(const String* name) {
        using namespace Ast::Internal;

        for (int i = 0; i < IV_COUNT; i++) {
            const char* internalStr = NULL;
            switch (i) {
                case IV_NULL:  internalStr = IVS_NULL;  break;
                case IV_TRUE:  internalStr = IVS_TRUE;  break;
                case IV_FALSE: internalStr = IVS_FALSE; break;
                default: continue;
            }

            if (internalStr && cstrcmp(*name, internalStr)) {
                return (SyntaxNode*) &variables[i];
            }
        }

        for (int i = 1; i < IF_COUNT; i++) {
            const char* internalStr = NULL;
            switch (i) {
                case IF_ALLOC:  internalStr = IFS_ALLOC;  break;
                case IF_FREE:   internalStr = IFS_FREE;   break;
                default: continue;
            }

            if (internalStr && cstrcmp(*name, internalStr)) {
                return (SyntaxNode*) &functions[i];
            }
        }

        return nullptr;
    }

    // Finds function where all arguments follow 'exact match'
    // TODO
    Function* findExactFunction(Scope* scope, INamed* const name, FunctionPrototype* const fptr) {
        return NULL;
    }

    // TODO
    Err::Err validate(ValidationContext* ctx, FunctionPrototype* fp) {
        return Err::OK;
    }

    Err::Err applyTypeSpecifier(ValidationContext* ctx, TypeSpecifier* spec, Type::TypeInfo** outType) {
        Type::TypeInfo* type = *outType;
        bool isAmbiguous = false;

        for (size_t i = 0; i < spec->decoratorCount; i++) {
            TypeDecorator* dec = spec->decorators[i];

            switch (dec->kind) {
                case TypeDecorator::DEC_POINTER: {
                    type = isAmbiguous ?
                        Type::tmpMakePointer(type) :
                        Type::makePointer(type);
                    break;
                }

                case TypeDecorator::DEC_ARRAY: {
                    if (dec->flags & IS_EMBEDED) {
                        type = isAmbiguous ?
                            Type::tmpMakeSlice(type, dec->flags) :
                            Type::makeSlice(type, dec->flags);
                        break;
                    }

                    // If len is null it has to be inherited from
                    // the right side. Any type from now on will
                    // be treated as ambiguous...
                    if (!dec->len) {
                        isAmbiguous = true;
                        type = Type::tmpMakeArray(type, Type::ARRAY_LEN_UNKNOWN);
                        break;
                    }

                    Err::Err err = validate(ctx, dec->len, NULL);
                    if (err != Err::OK) return err;

                    err = Interpreter::eval(ctx, dec->len);
                    if (err != Err::OK) return err;

                    if (!dec->len->value.hasValue || dec->len->value.i64 < 0) {
                        Diag::report(ctx->unit->ast, dec->span, Err::ARRAY_SIZE_MISMATCH,
                            Diag::Format {
                                "Invalid array size."
                            }
                        );
                        return Err::ARRAY_SIZE_MISMATCH;
                    }

                    type = isAmbiguous ?
                        Type::tmpMakeArray(type, dec->len->value.i64) :
                        Type::makeArray(type, dec->len->value.i64);
                }
            }

            *outType = type;
        }

        return (Err::Err) isAmbiguous;
    }

    Err::Err validate(ValidationContext* ctx, VariableDefinition* def) {
        Err::Err err;

        // TODO : think more if this needs to be abstrated together with 'applyTypeSpecifier'
        if (def->type.baseType == Type::DT_UNDEFINED) {
            SyntaxNode* node;
            Err::Err err = resolveQualifiedName(ctx, def->base.scope, def->type.baseName, &node);
            if (err != Err::OK) return err;

            switch (node->type) {
                case NT_UNION:
                case NT_TYPE_DEFINITION: {
                    err = ensureValidated(ctx, node);
                    def->var->value.type = &(((TypeDefinition*) node)->type->base);
                    break;
                }

                case NT_ERROR: {
                    // TODO
                    Diag::report(ctx->unit->ast, def->base.span, Err::NOT_YET_IMPLEMENTED);
                    return Err::NOT_YET_IMPLEMENTED;
                }

                case NT_ENUMERATOR: {
                    Enumerator* en = (Enumerator*) node;
                    def->var->value.type = en->memberType;
                    break;
                }

                default: {
                    Diag::report(ctx->unit->ast, def->base.span, Err::INVALID_DATA_TYPE,
                        Diag::Format {
                            "Symbol '%.*s' is not a data type."
                        },
                        def->type.baseName->len, def->type.baseName->buff
                    );

                    return Err::INVALID_DATA_TYPE;
                }
            }
        } else if (def->type.baseType == Type::DT_FUNCTION) {
            // TODO : resolve function pointer definition
            Diag::report(ctx->unit->ast, def->base.span, Err::NOT_YET_IMPLEMENTED);
            return Err::NOT_YET_IMPLEMENTED;
        } else {
            def->var->value.type = Type::basicTypes + def->type.baseType;
        }

        err = applyTypeSpecifier(ctx, &def->type, &def->var->value.type);
        if (err != Err::OK) return err;

        bool isAmbiguous = (int64_t) err == Type::RS_AMBIGUOUS;

        if (def->var->expression) {
            Type::TypeInfo* expectedType = def->var->value.type;

            err = validateExpression(ctx, def->var, expectedType);
            if (err != Err::OK) return err;

            if (isAmbiguous) {
                Type::tmpClear();
            } else {
                err = applyImplicitCast(ctx, def->var, expectedType);
                if (err != Err::OK) return err;
            }
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, TypeDefinition* td) {
        Err::Err err;

        const int isUnion = td->base.type == NT_UNION;

        if (checkUniqueNames((SyntaxNode**) td->vars, td->varCount) != Err::OK) {
            Diag::report(ctx->unit->ast, td->base.span, Err::INVALID_ATTRIBUTE_NAME);
            return Err::INVALID_ATTRIBUTE_NAME;
        }

        for (int i = 0; i < td->varCount; i++) {
            Variable* const var = td->vars[i]->var;

            if (isUnion && (var->expression || var->value.hasValue)) {
                Diag::report(ctx->unit->ast, var->base.span, Err::INVALID_RVALUE, "Default values are not allowed within union initialization!", var->base.span, var->name.len);
                return Err::INVALID_RVALUE;
            }
        }

        for (uint32_t i = 0; i < td->varCount; i++) {
            err = validate(ctx, td->vars[i]);
            if (err != Err::OK) return err;
        }

        computeTypeInfo(ctx, td);

        return err;
    }

    Err::Err validate(ValidationContext* ctx, Scope* scope) {
        for (uint32_t i = 0; i < scope->childrenCount; i++) {
            SyntaxNode* child = scope->children[i];
            Err::Err err = validate(ctx, child);
            if (err != Err::OK) return err;
        }

        return Err::OK;
    }

    // Resolves and type-checks the internal 'expression' of a variable.
    //
    // The resulting type and metadata of the expression are written directly into
    // variable. Basically a sub function of validate<Variable, Variable>, so look
    // there for more info.
    Err::Err validateExpression(ValidationContext* ctx, Variable* var, Type::TypeInfo* target) {
        Err::Err err;

        Expression* ex = var->expression;
        if (!ex) {
            // TODO : with 'new' type system I dont feel like we can endup
            //        here with unresolved type
            //        We may still want to run it anyway to just be sure,
            //        BUT for development its better to comment it out
            //        so it wont shadow bugs of outer type system ...
            //
            // If no expression to resolve, we have to ensure that its
            // data type is valid, as we are the source of truth for
            // other expressions.
            // err = validateDataType(ctx, var->value.type->kind, var->value.str, var->base.span);
            // if (err != Err::OK) return err;

            var->base.semStatus = TS_READY;
            return Err::OK;
        }

        switch (ex->type) {

            case EXT_UNARY: {

                UnaryExpression* uex = (UnaryExpression*) ex;

                err = validate(ctx, uex->operand, target);
                if (err != Err::OK) return err;

                err = resolveResultType(ctx, uex, var);
                if (err != Err::OK) return err;

                break;

            }

            case EXT_BINARY: {

                BinaryExpression* bex = (BinaryExpression*) ex;

                err = validate(ctx, bex->left, NULL);
                if (err != Err::OK) return err;

                if (isMemberSelection(bex->base.opType)) {
                    // TODO : do we need to do something?
                } else {
                    err = validate(ctx, bex->right, NULL);
                    if (err != Err::OK) return err;
                }

                err = resolveResultType(ctx, bex, var);
                if (err != Err::OK) return err;

                break;

            }

            case EXT_FUNCTION_CALL: {
                FunctionCall* call = (FunctionCall*) ex;

                // We need to beforehand validate arguments, so
                // later we can properly link function via overloads...
                for (int i = 0; i < call->inArgCount; i++) {
                    err = validate(ctx, call->inArgs[i], NULL);
                    if (err != Err::OK) return err;
                }

                err = linkCall(ctx, var);
                if (err != Err::OK) return err;

                Function* fcn = call->fcn;

                err = ensureValidated(ctx, &fcn->base);
                if (err != Err::OK) return err;

                int callArgCount = call->inArgCount;
                int fixedCount = fcn->prototype.inArgCount;

                if (fixedCount > 0) {
                    VariableDefinition* lastArg = fcn->prototype.inArgs[fixedCount - 1];
                    if (lastArg->var->value.type->kind == Type::DT_MULTIPLE_TYPES) {
                        fixedCount--;
                    }
                }

                int i = 0;
                for (; i < fixedCount && i < callArgCount; i++) {
                    Variable* rvar = call->inArgs[i];
                    Variable* lvar = fcn->prototype.inArgs[i]->var;

                    err = applyImplicitCast(ctx, rvar, lvar->value.type);
                    if (err != Err::OK) return err;
                }

                if (fcn->prototype.outArg) {
                    // TODO : again, dont like this call, think about it more...
                    Ast::Node::copyRef(call->outArg, call->fcn->prototype.outArg->var);
                    resolveResultType(ctx, call, var);
                } else {
                    var->value.type->kind = Type::DT_VOID;
                }

                break;
            }

            case EXT_RANGE: {
                RangeExpression* range = (RangeExpression*) ex;

                err = validate(ctx, range->bidx, target);
                if (err != Err::OK) return err;
                if (!isInt(range->bidx->value.type->kind)) {
                    Diag::report(ctx->unit->ast, range->bidx->base.span, Err::INVALID_DATA_TYPE, "TODO");
                    return Err::INVALID_DATA_TYPE;
                }

                err = validate(ctx, range->eidx, target);
                if (err != Err::OK) return err;
                if (!isInt(range->eidx->value.type->kind)) {
                    Diag::report(ctx->unit->ast, range->bidx->base.span, Err::INVALID_DATA_TYPE, "TODO");
                    return Err::INVALID_DATA_TYPE;
                }

                // TODO : try to evaluate idxs and compute len

                var->value.type->kind = Type::DT_RANGE;

                break;
            }

            case EXT_STRING_INITIALIZATION: {
                StringInitialization* init = (StringInitialization*) ex;

                const uint64_t len = init->rawData.len / init->charType->size;
                var->value.type = Type::makeArray(init->charType, len);

                const int arrDtypeSize = var->value.type->size;
                const int strDtypeSize = init->charType->size;

                if (arrDtypeSize < strDtypeSize) {
                    Diag::report(ctx->unit->ast, var->base.span, Err::INVALID_TYPE_CONVERSION,
                        Diag::Format {
                            "Cannot initialize array of size %d bytes with string literal requiring %d bytes; capacity exceeded."
                        },
                        arrDtypeSize, strDtypeSize
                    );
                    return Err::INVALID_TYPE_CONVERSION;
                }

                if (strDtypeSize < arrDtypeSize) {
                    if (strDtypeSize < arrDtypeSize) {
                    Diag::report(ctx->unit->ast, var->base.span, Wrn::SMALLER_DTYPE_CAN_BE_USED,
                        Diag::Format {
                            "String literal character width (%d bytes) is smaller than destination element width (%d bytes); implicit widening applied."
                        },
                        strDtypeSize, arrDtypeSize);
                    }
                }

                return Err::OK;
            }

            case EXT_ARRAY_INITIALIZATION: {
                ArrayInitialization* init = (ArrayInitialization*) ex;
                Type::ArrayInfo* aInfo = (Type::ArrayInfo*) target;

                if (aInfo && aInfo->elementCount != Type::ARRAY_LEN_UNKNOWN) {
                    if (aInfo->elementCount != (uint64_t) init->attributeCount) {
                        Diag::report(ctx->unit->ast, var->base.span, Err::INVALID_TYPE_CONVERSION,
                            Diag::Format {
                                "Array initializer has %d elements, but target type '%s' expects %llu elements.",
                            },
                            init->attributeCount, Type::str(&aInfo->base), (uint64_t) aInfo->elementCount
                        );
                        return Err::INVALID_TYPE_CONVERSION;
                    }

                    for (int i = 0; i < init->attributeCount; i++) {
                        Variable* eVar = init->attributes[i];

                        err = validate(ctx, eVar, aInfo->element);
                        if (err != Err::OK) return err;

                        // TODO: think about cast... I guess its better to not cast
                        //       and cast the array as whole, so then vec instructions
                        //       can be used, but it may result in additional ass pain
                        err = applyImplicitCast(ctx, eVar, aInfo->element);
                        if (err != Err::OK) return err;
                    }

                    if (Type::isTmp(&aInfo->base)) {
                        var->value.type = Type::makeArray(aInfo->element, init->attributeCount);
                    } else {
                        var->value.type = target;
                    }
                } else {
                    // we want to make array type of the most 'dominant' type

                    int dominantIdx = 0;
                    Type::TypeInfo* dominantType = NULL;

                    for (int i = 0; i < init->attributeCount; i++) {
                        Variable* var = init->attributes[i];

                        err = validate(ctx, var, NULL);
                        if (err != Err::OK) return err;

                        if (!dominantType || dominantType->rank < var->value.type->rank) {
                            dominantType = var->value.type;
                            dominantIdx = i;
                        }
                    }

                    // and cast all elements to the 'dominant' one
                    for (int i = 0; i < init->attributeCount; i++) {
                        if (i == dominantIdx) continue;

                        Variable* var = init->attributes[i];
                        err = applyImplicitCast(ctx, var, dominantType);
                        if (err != Err::OK) return err;
                    }

                    var->value.type = Type::makeArray(dominantType, init->attributeCount);
                }

                return Err::OK;
            }

            case EXT_TYPE_INITIALIZATION: {
                TypeInitialization* init = (TypeInitialization*) ex;
                if (!target) {
                    Diag::report(ctx->unit->ast, var->base.span, Err::UNEXPECTED_SYMBOL,
                        Diag::Format{
                            "TODO : Type cannot be deducted in this situation!"
                        });
                    return Err::UNEXPECTED_SYMBOL;
                }

                Type::StructInfo* sInfo = (Type::StructInfo*) target;
                TypeDefinition* td = (TypeDefinition*) ((Type::TypeInfoEx*) target)->astNode;

                if (init->attributeCount > sInfo->memberCount) {
                    Diag::report(ctx->unit->ast, td->base.span, Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH);
                    return Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH;
                }

                if (sInfo->base.kind == Type::DT_UNION && init->attributeCount > 1) {
                    Diag::report(ctx->unit->ast, td->base.span, Err::UNEXPECTED_SYMBOL, "TODO : union initialization more than one attribute defined!");
                    return Err::UNEXPECTED_SYMBOL;
                }

                bool attributesAreNamed = init->attributeCount > 0 &&
                    init->attributes[0]->name.len > 0;

                if (attributesAreNamed) {
                    init->idxs = (int*) alloc(alc, sizeof(int) * init->attributeCount);
                }

                int i = 0;
                for (; i < init->attributeCount; i++) {
                    Type::StructMemberInfo* mInfo;
                    Variable* var = init->attributes[i];

                    if (attributesAreNamed) {
                        int mIdx;
                        mInfo = Type::findMember(sInfo, (String*) &var->name, &mIdx);
                        init->idxs[i] = mIdx;
                    } else {
                        mInfo = sInfo->members + i;
                    }

                    applyVariableLinkage(ctx, var, (SyntaxNode*) td->vars[i]);

                    err = validate(ctx, var, mInfo->type);
                    if (err != Err::OK) return err;

                    err = applyImplicitCast(ctx, var, mInfo->type);
                    if (err != Err::OK) return err;
                }

                if (init->fillVar) {
                    for (; i < sInfo->memberCount; i++) {
                        Type::StructMemberInfo* mInfo = sInfo->members + i;

                        applyVariableLinkage(ctx, init->fillVar, (SyntaxNode*) td->vars[i]);

                        err = validate(ctx, init->fillVar, mInfo->type);
                        if (err != Err::OK) return err;

                        err = applyImplicitCast(ctx, init->fillVar, mInfo->type);
                        if (err != Err::OK) return err;
                    }
                }

                var->value.type = (Type::TypeInfo*) sInfo;

                return Err::OK;
            }

            default: {
                // TODO
            }
        }

        return Err::OK;
    }

    // Validates a Variable node. This is the primary entry point for all variables.
    //
    // Execution Flow:
    //  - Returns immediately if the node is already validated (NS_READY).
    //  - If the variable is unlinked, tries to do so.
    //  - If the node has a definition but contains no expression, it is treated as
    //    a reference -> forces the definition to be validated, copies needed metadata
    //    and returns.
    //  - If the node contains an expression (even if linked, such as a member in a
    //    struct initializer), it bypasses the reference treatment and proceed to
    //    its validation.
    //
    // Parameters:
    //   var:    any variable node to validate
    //   target: The 'context' variable (representing the left-hand side of an assignment
    //           or any binding-like relationship) used to guide type inference. Can be NULL.
    Err::Err validate(ValidationContext* ctx, Variable* var, Type::TypeInfo* target) {
        Err::Err err;

        if (!var) return Err::OK;

        if (var->base.semStatus == TS_READY) return Err::OK;
        var->base.semStatus = TS_PENDING;

        if (var->value.hasValue) return Err::OK;

        if (!var->def) {
            err = linkVariable(ctx, var);
            if (err != Err::OK) return err;
        }

        if (!var->expression && var->def && (!var->value.type || var->value.type != target)) {
            err = ensureValidated(ctx, &var->def->base, (SyntaxNode*) var);
            if (err != Err::OK) return err;

            // TODO : dont like this call, think about it more...
            Ast::Node::copyRef(var, var->def->var);

            var->base.semStatus = TS_READY;
            return Err::OK;
        }

        err = validateExpression(ctx, var, target);
        if (err != Err::OK) return err;

        var->base.semStatus = TS_READY;
        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, VariableAssignment* ass) {
        Err::Err err;

        err = validate(ctx, ass->lvar, NULL);
        if (err != Err::OK) return err;

        err = validate(ctx, ass->rvar, ass->lvar->value.type);
        if (err != Err::OK) return err;

        err = applyImplicitCast(ctx, ass->rvar, ass->lvar->value.type);
        if (err != Err::OK) return err;

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, Branch* node) {
        if (!node) return Err::OK;

        for (uint32_t i = 0; i < node->expressionCount; i++) {
            Variable* condition = node->expressions[i];
            if (!condition) continue;

            Err::Err err = validate(ctx, (SyntaxNode*) condition);
            if (err != Err::OK) return err;

            if (!Type::isTruthy(condition->value.type->kind)) {
                Diag::report(ctx->unit->ast, condition->base.span, Err::INVALID_DATA_TYPE);
                return Err::INVALID_DATA_TYPE;
            }
        }

        for (uint32_t i = 0; i < node->scopeCount; i++) {
            Scope* scope = node->scopes[i];
            if (!scope) continue;

            Err::Err err = validate(ctx, (SyntaxNode*) scope);
            if (err != Err::OK) return err;
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, SwitchCase* node) {
        Err::Err err;

        if (!node) return Err::OK;

        if (!node->switchExp) {
            Diag::report(ctx->unit->ast, node->base.span, Err::UNEXPECTED_ERROR, Diag::Format {
                "Empty switch expression."
            });
            return Err::UNEXPECTED_ERROR;
        }

        err = validate(ctx, (SyntaxNode*)node->switchExp);
        if (err != Err::OK) return Err::OK;

        for (uint32_t i = 0; i < node->caseExpCount; i++) {
            Variable* caseVar = node->casesExp[i];
            if (!caseVar) continue;

            err = validate(ctx, caseVar);
            if (err != Err::OK) return err;

            if (caseVar->def) {
                err = Interpreter::eval(ctx, caseVar);
                if (err != Err::OK) return err;
            }

            err = applyImplicitCast(ctx, caseVar, node->switchExp->value.type);
            if (err != Err::OK) return err;
        }

        for (uint32_t i = 0; i < node->caseCount; i++) {
            Scope* caseScope = node->cases[i];
            if (!caseScope) continue;

            err = validate(ctx, caseScope);
            if (err != Err::OK) return err;
        }

        if (node->elseCase) {
            err = validate(ctx, node->elseCase);
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, WhileLoop* node) {
        Err::Err err;

        if (!node) return Err::OK;

        if (!node->expression) {
            Diag::report(ctx->unit->ast, node->base.span, Err::UNEXPECTED_ERROR, Diag::Format {
                "While loop missing condition."
            });
            return Err::UNEXPECTED_ERROR;
        }

        err = validate(ctx, node->expression);
        if (err != Err::OK) return err;

        if (!Type::isTruthy(node->expression->value.type)) {
            Diag::report(ctx->unit->ast, node->expression->base.span, Err::INVALID_DATA_TYPE);
            return Err::INVALID_DATA_TYPE;
        }

        if (node->bodyScope) {
            // Record current loop, so we can validate break/continue etc.
            SyntaxNode* prevLoop = ctx->currentLoop;
            ctx->currentLoop = (SyntaxNode*) node;

            validate(ctx, node->bodyScope);

            ctx->currentLoop = prevLoop;
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, RangeExpression* range) {
        Err::Err err;

        err = validate(ctx, range->bidx);
        if (err != Err::OK) return err;
        if (!Type::isInt(range->bidx->value.type)) {
            applyImplicitCast(ctx, range->bidx, Type::basicTypes + Type::DT_I64);
        }

        err = validate(ctx, range->eidx);
        if (err != Err::OK) return err;
        if (!Type::isInt(range->eidx->value.type)) {
            applyImplicitCast(ctx, range->eidx, Type::basicTypes + Type::DT_I64);
        }

        return Err::OK;
    }

    void aliasVariable(VariableDefinition* aliasDef, Variable* source) {
        if (source->expression) {
            aliasDef->var->expression = source->expression;
        } else if (source->def) {
            UnaryExpression* uex = Ast::Node::makeUnaryExpression();
            uex->operand = source;
            uex->base.opType = OP_NONE;

            aliasDef->var->expression = (Expression*) uex;
        }

        aliasDef->var->value = source->value;
    }

    Err::Err validate(ValidationContext* ctx, Loop* node) {
        Err::Err err;

        if (!node) return Err::OK;

        // Arg
        if (node->arg.kind == Loop::Arg::EXPRESSION) {
            err = validate(ctx, node->arg.exp);
            if (err != Err::OK) return err;

            Type::Kind kind = node->arg.exp->value.type->kind;
            if (Type::isArrayLike(kind)) {
                if (kind != Type::DT_ERROR) {
                    Diag::report(ctx->unit->ast, node->base.span, Err::UNEXPECTED_ERROR, Diag::Format {
                        "Type '%s' is not iterable."
                    }, Type::str(kind));
                }
            }
        } else {
            err = validate(ctx, node->arg.range);
            if (err != Err::OK) return err;
        }

        // As
        if (node->item) {
            if (node->arg.kind != Loop::Arg::EXPRESSION) {
                // TODO : error
            }
            // aliasVariable(node->item, node->arg.exp);
        }

        if (node->index.var) {
            if (node->index.var->base.type == NT_VARIABLE) {
                validate(ctx, node->index.var);
            } else {
                if (node->index.def->var->value.type->kind == Type::DT_VOID) {
                    node->index.def->var->value.hasValue = true;
                    node->index.def->var->value.type->kind = Type::DT_I64;
                    node->index.def->var->value.i64 = 0;
                }
            }
        }

        // By
        /*
        if (node->stride) {
            err = validate(ctx, node->stride);
            if (!Type::isInt(node->stride->value.type)) {
                applyImplicitCast(ctx, node->stride, Type::basicTypes + Type::DT_I64);
            }

            // TODO : for now only compile time expression
            err = Interpreter::eval(ctx, node->stride);
            if (err != Err::OK) return err;
        }

        // While
        if (node->condition) {
            err = validate(ctx, node->condition);
            if (err != Err::OK) return err;

            if (!Type::isTruthy(node->condition->value.type)) {
                Diag::report(ctx->unit->ast, node->condition->base.span, Err::INVALID_DATA_TYPE);
                return Err::INVALID_DATA_TYPE;
            }
        }
        */

        // Body
        if (node->bodyScope) {
            SyntaxNode* prevLoop = ctx->currentLoop;
            ctx->currentLoop = (SyntaxNode*) node;

            err = validate(ctx, (SyntaxNode*) node->bodyScope);
            if (err != Err::OK) return err;

            ctx->currentLoop = prevLoop;
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, BreakStatement* node) {
        if (!node) return Err::OK;

        if (!ctx->currentLoop) {
            Diag::report(ctx->unit->ast, node->base.span, Err::INVALID_BREAK_TARGET);
            return Err::OK;
        }

        node->target = (SyntaxNode*) ctx->currentLoop;
        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, ContinueStatement* node) {
        if (!node) return Err::OK;

        if (!ctx->currentLoop) {
            Diag::report(ctx->unit->ast, node->base.span, Err::INVALID_CONTINUE_TARGET);
            return Err::OK;
        }

        node->target = (SyntaxNode*) ctx->currentLoop;
        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, ReturnStatement* node) {
        Err::Err err;

        if (!node) return Err::OK;

        if (!ctx->currentFunction) {
            Diag::report(ctx->unit->ast, node->base.span, Err::INVALID_RETURN_TARGET,
                "Return statement must be inside a function.");
            return Err::OK;
        }
        node->fcn = ctx->currentFunction;

        if (node->var) {
            VariableDefinition* outArg = ctx->currentFunction->prototype.outArg;
            err = validate(ctx, outArg->var, NULL);
            if (err != Err::OK) return err;

            err = validate(ctx, node->var, outArg->var->value.type);
            if (err != Err::OK) return err;
        }

        if (node->err) {
            err = validate(ctx, node->err);

            if (!ctx->currentFunction->errorSet) {
                Diag::report(ctx->unit->ast, node->err->base.span, Err::UNEXPECTED_ERROR);
            }
        }

        FunctionPrototype* proto = &ctx->currentFunction->prototype;

        Value expectedValue;
        if (proto->outArg && proto->outArg->var) {
            expectedValue = proto->outArg->var->value;
        } else {
            expectedValue = Value { .type = Type::basicTypes + Type::DT_VOID };
        }

        if (!node->var) {
            if (expectedValue.type->kind != Type::DT_VOID) {
                Diag::report(ctx->unit->ast, node->base.span, Err::INVALID_DATA_TYPE,
                    Diag::Format{
                        "Function expects a return value of type '%s'."
                    }, Type::str(expectedValue.type->kind));
                return Err::INVALID_DATA_TYPE;
            }
            return Err::OK;
        }

        if (applyImplicitCast(ctx, node->var, expectedValue.type) != Err::OK) {
            Diag::report(ctx->unit->ast, node->var->base.span, Err::INVALID_DATA_TYPE);
            return Err::INVALID_DATA_TYPE;
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, Enumerator* node) {
        if (!node) return Err::OK;

        if (!Type::isInt(node->memberType)) {
            Diag::report(ctx->unit->ast, node->base.span, Err::INVALID_DATA_TYPE);
            // Fallback to i32 to allow further validation
            // TODO : to a constant
            node->memberType = Type::basicTypes + Type::DT_I64;
        }

        uint64_t nextValue = 0;
        for (uint32_t i = 0; i < node->varCount; i++) {
            Variable* mVar = node->vars[i];
            if (!mVar) continue;

            mVar->value.type = node->type;

            if (mVar->expression) {
                Err::Err err = Interpreter::eval(ctx, mVar);
                if (err != Err::OK) return err;

                nextValue = mVar->value.u64 + 1;
            } else {
                mVar->value.u64 = nextValue++;
                mVar->value.hasValue = true;
            }
        }

        Err::Err err = computeTypeInfo(ctx, node);
        if (err != Err::OK) return err;

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, Statement* node) {
        if (!node || !node->operand) return Err::OK;

        Err::Err err = validate(ctx, node->operand);
        if (err != Err::OK) return err;

        Variable* var = node->operand;
        if (var->value.type->kind != Type::DT_VOID) {
            // TODO : discard result??
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, ErrorSet* node) {
        if (!node) return Err::OK;

        uint64_t nextValue = node->value;
        for (uint32_t i = 0; i < node->varCount; i++) {
            Variable* mVar = node->vars[i];
            if (!mVar) continue;

            mVar->value.type->kind = Type::DT_ERROR;
            if (mVar->expression) {
                Err::Err err = Interpreter::eval(ctx, mVar);
                if (err != Err::OK) return err;

                if (mVar->value.hasValue) {
                    nextValue = mVar->value.u64 + 1;
                }
            } else {
                mVar->value.u64 = nextValue++;
                mVar->value.hasValue = true;
            }
        }

        return Err::OK;
    }

    // TODO
    Err::Err validate(ValidationContext* ctx, ImportStatement* node) {
        ImportStatement* import = (ImportStatement*) node;
        if (import->tag.len > 0) {
            // Foreign import
            if (!cstrcmp(import->tag, String("C"))) {
                Diag::report(ctx->unit->ast, node->base.span, Err::UNEXPECTED_SYMBOL, "TODO : unknown tag");
                return Err::UNEXPECTED_SYMBOL;
            }
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, SyntaxNode* node) {
        Err::Err err;

        if (!node) return Err::OK;

        // TODO : ??
        if (node->ogNode) node = node->ogNode;

        switch (node->type) {
            case NT_SCOPE:
            case NT_NAMESPACE: {
                err = validate(ctx, (Scope*) node);
                break;
            }

            case NT_VARIABLE: {
                err = validate(ctx, (Variable*) node, NULL);
                break;
            }

            case NT_VARIABLE_DEFINITION: {
                err = validate(ctx, (VariableDefinition*) node);
                break;
            }

            case NT_FUNCTION: {
                err = validate(ctx, (Function*) node);
                break;
            }

            case NT_TYPE_DEFINITION:
            case NT_UNION: {
                err = validate(ctx, (TypeDefinition*) node);
                break;
            }

            case NT_VARIABLE_ASSIGNMENT: {
                err = validate(ctx, (VariableAssignment*) node);
                break;
            }

            case NT_BRANCH: {
                err = validate(ctx, (Branch*) node);
                break;
            }

            case NT_SWITCH_CASE: {
                err = validate(ctx, (SwitchCase*) node);
                break;
            }

            case NT_WHILE_LOOP: {
                err = validate(ctx, (WhileLoop*) node);
                break;
            }

            case NT_LOOP: {
                err = validate(ctx, (Loop*) node);
                break;
            }

            case NT_RETURN_STATEMENT: {
                err = validate(ctx, (ReturnStatement*) node);
                break;
            }

            case NT_ENUMERATOR: {
                err = validate(ctx, (Enumerator*) node);
                break;
            }

            case NT_IMPORT: {
                err = validate(ctx, (ImportStatement*) node);
                break;
            }

            case NT_STATEMENT: {
                err = validate(ctx, (Statement*) node);
                break;
            }

            case NT_ERROR: {
                err = validate(ctx, (ErrorSet*) node);
                break;
            }

            case NT_BREAK_STATEMENT: {
                err = validate(ctx, (BreakStatement*) node);
                break;
            }

            case NT_CONTINUE_STATEMENT: {
                err = validate(ctx, (ContinueStatement*) node);
                break;
            }

            default: {
                Diag::report(ctx->unit->ast, node->span, Err::NOT_YET_IMPLEMENTED);
                break;
            }
        }

        return Err::OK;
    }



    Err::Err ensureValidated(ValidationContext* ctx, SyntaxNode* node, SyntaxNode* triggerNode) {
        Err::Err err = Err::OK;

        if (node->semStatus == TS_READY) {
            return Err::OK;
        }

        AcquireNodeReturn ans =
            acquireNode(&node->semStatus, &node->workerId, ctx->workerId, true);

        if (ans == ANR_ACQUIRED_FOR_WORK) {
            switch(node->type) {
                case NT_FUNCTION: {
                    err = validate(ctx, (Function*) node);
                    break;
                }

                case NT_VARIABLE_DEFINITION: {
                    err = validate(ctx, (VariableDefinition*) node);
                    break;
                }

                case NT_TYPE_DEFINITION: {
                    err = validate(ctx, (TypeDefinition*) node);
                    break;
                }

                case NT_ENUMERATOR: {
                    err = validate(ctx, (Enumerator*) node);
                    break;
                }

                case NT_NAMESPACE: {
                    err = validate(ctx, (Scope*) node);
                    break;
                }

                case NT_IMPORT: {
                    err = validate(ctx, (ImportStatement*) node);
                    break;
                }

                default: {
                    Diag::report(ctx->unit->ast, node->span, Err::UNEXPECTED_ERROR, Diag::Format {
                        "Unexpected node in ensureReady function, should be only definition-like node!"
                    });
                    err = Err::UNEXPECTED_ERROR;
                }
            }

            releaseNode(&node->semStatus, true);
        } else if (ans == ANR_ALREADY_ACQUIRED_BY_CALLER) {
            // TODO : Proper Errors
            if (triggerNode) {
                Diag::report(ctx->unit->ast, triggerNode->span, Err::UNEXPECTED_ERROR, Diag::Format {
                    "TODO : Node being validated is already on stack! Was triggered from following node!"
                });
            } else {
                Diag::report(ctx->unit->ast, node->span, Err::UNEXPECTED_ERROR, Diag::Format {
                    "TODO : Node being validated is already on stack! Causing circular dependency!"
                });
            }
            return Err::UNEXPECTED_ERROR;
        }

        return err;
    }



    Err::Err preValidate(ValidationContext* ctx) {
        Err::Err err;

        AstRegistry* reg = ctx->unit->reg;
        for (int i = 0; i < reg->scopes.size; i++) {
            err = ensureUniqueDefinitions(ctx, *(Scope**) DArray::get(&reg->scopes, i));
            if (err != Err::OK) return err;
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx) {
        Err::Err err;

        FileSystem::FileInfo* finfo = ctx->unit->ast->root->base.span->fileInfo;
        FileSystem::getFileDir(finfo->absPath, &ctx->fileDir);



        err = verifyFunctionsAreGlobal(ctx);
        if (err != Err::OK) return err;

        err = verifyImportsAreGlobal(ctx);
        if (err != Err::OK) return err;

        err = verifyNamespacesAreGlobal(ctx);
        if (err != Err::OK) return err;



        AstRegistry* reg = ctx->unit->reg;



        // We basically go through top level nodes that can be exported
        // TODO : make sure reg has only top level stuff...
        for (int i = 0; i < reg->customDataTypes.size; i++) {
            ensureValidated(ctx, *(SyntaxNode**) DArray::get(&reg->customDataTypes, i));
        }

        for (int i = 0; i < reg->variableDefinitions.size; i++) {
            ensureValidated(ctx, *(SyntaxNode**) DArray::get(&reg->variableDefinitions, i));
        }

        for (int i = 0; i < reg->fcns.size; i++) {
            ensureValidated(ctx, *(SyntaxNode**) DArray::get(&reg->fcns, i));
        }

        // Validate remaining local nodes
        validate(ctx, ctx->unit->ast->root);

        // for (int i = 0; i < reg->imports.size; i++) {
        //    validate(ctx, *(SyntaxNode**) DArray::get(&reg->imports, i));
        //}


        /*
        for (int i = 0; i < (int) reg->fcnCalls.size; i++) {
            Variable* var = *(Variable**) DArray::get(&ctx->unit->reg->fcnCalls, i);

            err = validateCall(ctx, var);
            if (err != Err::OK) return err;
        }
        */

        // DEBUG:
        //for (int i = 0; i < reg->initializations.size; i++) {
        //    SyntaxNode* arr = *(SyntaxNode**) DArray::get(&reg->initializations, i);
        //    Emitter::driverDebug.emitNode(&DebugHelper::emitter, arr, &DebugHelper::stream);
        //}

        for (int i = 0; i < reg->cmpTimeVars.size; i++) {
            Variable* var = *(Variable**) DArray::get(&reg->cmpTimeVars, i);
            err = Interpreter::eval(ctx, var);
            if (err != Err::OK) return err;
        }

        return Err::OK;
    }



    // === Link functions
    //

    // TODO : move in appropriate place
    bool areInOrder(Span* before, Span* after) {
        return (before->start.idx < after->start.idx);
    }

    // TODO : move in appropriate place
    // Basically to 'abstract' errors in invalid ordering case, that are kinda ugly
    bool isOrderingValid(SymbolIndexEntry* entry, QualifiedName* name) {
        return (
            entry->kind == SymbolIndexEntry::OVERLOAD ||
            entry->node->flags & IS_UNORDERED ||
            areInOrder(entry->node->span, name->span)
        );
    }

    Err::Err linkErrorSet(ValidationContext* ctx, Function* fcn) {
        // TODO
        if (!fcn->errorSet) return Err::OK;

        Diag::report(ctx->unit->ast, fcn->base.span, Err::NOT_YET_IMPLEMENTED);
        return Err::NOT_YET_IMPLEMENTED;
    }

    Err::Err applyVariableLinkage(ValidationContext* ctx, Variable* var, SyntaxNode* definition) {
        Err::Err err = ensureValidated(ctx, definition);
        if (err != Err::OK) return err;

        switch (definition->type) {
            case NT_VARIABLE_DEFINITION: {
                var->def = (VariableDefinition*) definition;
                break;
            }

            case NT_ENUMERATOR: {
                Enumerator* en = (Enumerator*) definition;
                var->value.type = en->type;
                //Err::Err err = ensureValidated(en);
                //if (err != Err::OK) return err;

                //var->value.type = computeTypeInfo(ctx, en);
                break;
            }

            case NT_FUNCTION: {
                Diag::report(ctx->unit->ast, var->base.span, Err::NOT_YET_IMPLEMENTED);
                return Err::NOT_YET_IMPLEMENTED;
                //var->value.type->kind = Type::DT_FUNCTION;
                //var->value.fcn = NULL;
                break;
            }

            case NT_TYPE_DEFINITION: {
                // TODO : deprecated behaviour
                var->value.i64 = ((TypeDefinition*) definition)->type->base.size;
                var->value.hasValue = true;
                var->value.type = Type::basicTypes + Type::DT_I64;

                break;
            }

            default: {
                Diag::report(ctx->unit->ast, var->base.span, Err::UNEXPECTED_ERROR,
                    Diag::Format {
                        "Symbol '%.*s' is a %s and cannot be used as a variable."
                    },
                    var->name.len, var->name.buff, Ast::Node::str(definition->type)
                );

                return Err::UNEXPECTED_ERROR;
            }
        }

        return Err::OK;
    }

    Err::Err linkVariable(ValidationContext* ctx, Variable* var) {
        if (var->name.len == 0) return Err::OK;

        SyntaxNode* node;
        Err::Err err = resolveQualifiedName(ctx, var->base.scope, &var->name, &node);
        if (err != Err::OK) return err;

        return applyVariableLinkage(ctx, var, node);
    }

    // link goto statements
    Err::Err linkGoto(ValidationContext* ctx, GotoStatement* gt) {
        SyntaxNode* node = Ast::Find::inScope(gt->base.scope, (String*) &gt->name);
        if (!node) {
            Diag::report(ctx->unit->ast, gt->base.span, Err::UNKNOWN_VARIABLE, gt->name.len, gt->name.buff);
            return Err::UNKNOWN_VARIABLE;
        }

        gt->label = (Label*) node;
        return Err::OK;
    }

    Err::Err linkCall(ValidationContext* ctx, Variable* callOp) {
        Err::Err err;

        FunctionCall* call = (FunctionCall*) (callOp->expression);
        if (call->fcn) return Err::OK;

        Arena::Marker marker = Arena::getMarker(&ctx->tmpArena);

        SymbolIndexEntry* entry;
        err = resolveQualifiedNameAsIndexEntry(ctx, callOp->base.scope, &call->name, &entry);
        if (err != Err::OK) return err;

        Arena::rollback(&ctx->tmpArena, marker);

        Type::TypeInfo* outType;
        Function* fcn = findClosestFunction(entry, callOp);
        if (!fcn) {
            if (
                entry->kind == SymbolIndexEntry::SINGLE &&
                entry->node->type == NT_VARIABLE_DEFINITION
            ) {
                // assuming function pointer
                Variable* var = (Variable*) entry->node;

                call->fptr = var;
                call->fcn = NULL;
                outType = var->value.type;
            } else {
                // TODO : proper error
                Diag::report(ctx->unit->ast, callOp->base.span, Err::SYMBOL_NOT_FOUND);
                return (Err::Err) Err::SYMBOL_NOT_FOUND;
            }

        } else {
            call->fptr = NULL;
            call->fcn = fcn;
            outType = fcn->prototype.outArg->var->value.type;
        }

        // TODO: do we realy need to be a Variable
        call->outArg = new Variable();
        call->outArg->value.hasValue = false;
        call->outArg->value.type = outType;

        return Err::OK;
    }

    Err::Err verifyFunctionsAreGlobal(ValidationContext* ctx) {
        for (int i = 0; i < ctx->unit->reg->fcns.size; i++) {
            Function* const fcn = *(Function**) DArray::get(&ctx->unit->reg->fcns, i);

            Scope* sc = fcn->base.scope;
            while (sc) {
                if (sc->base.type == NT_SCOPE) break;
                sc = sc->base.scope;
            }

            if (sc != ctx->unit->ast->root) {
                Diag::report(ctx->unit->ast, fcn->name.span, Err::GLOBAL_SCOPE_REQUIRED);
                return Err::GLOBAL_SCOPE_REQUIRED;
            }
        }

        return Err::OK;
    }

    Err::Err verifyImportsAreGlobal(ValidationContext* ctx) {
        DArray::Container* imports = &ctx->unit->reg->imports;

        for (int i = 0; i < imports->size; i++) {
            ImportStatement* import = *(ImportStatement**) DArray::get(imports, i);

            if (import->base.scope != ctx->unit->ast->root) {
                Diag::report(ctx->unit->ast, import->base.span, Err::IMPORT_NOT_GLOBAL);
                return Err::IMPORT_NOT_GLOBAL;
            }
        }

        return Err::OK;
    }

    Err::Err verifyNamespacesAreGlobal(ValidationContext* ctx) {
        DArray::Container* namespaces = &ctx->unit->reg->namespaces;

        for (int i = 0; i < namespaces->size; i++) {
            Namespace* ns = *(Namespace**) DArray::get(namespaces, i);

            Scope* sc = ns->scope.base.scope;
            while (sc) {
                if (sc->base.type == NT_SCOPE) break;
                sc = sc->base.scope;
            }

            if (sc != ctx->unit->ast->root) {
                Diag::report(ctx->unit->ast, ns->scope.base.span, Err::NAMESPACE_NOT_GLOBAL);
                return Err::NAMESPACE_NOT_GLOBAL;
            }
        }

        return Err::OK;
    }







    // ======================
    // TYPE RESOLUTION STUFF

    // Pass already validated node
    Err::Err computeTypeInfo(ValidationContext* ctx, Enumerator* en) {
        Type::TypeInfo* mType = en->memberType;

        Type::EnumInfo* eType = (Type::EnumInfo*) alloc(alc, sizeof(Type::TypeInfoEx));
        eType->base.kind      = Type::DT_ENUM;
        eType->base.size      = mType->size;
        eType->base.align     = mType->align;
        eType->name           = { en->name.buff, en->name.len };
        eType->memberKind     = mType->kind;
        eType->memberCount    = en->varCount;

        if (eType->memberCount > 0) {
            eType->members = (Type::EnumMemberInfo*) alloc(
                alc, sizeof(Type::EnumMemberInfo) * eType->memberCount
            );

            for (uint64_t i = 0; i < en->varCount; i++) {
                QualifiedName* name = &en->vars[i]->name;
                eType->members[i].name  = { name->buff, name->len };
                eType->members[i].value = en->vars[i]->value.u64;
            }
        } else {
            eType->members = NULL;
        }

        ((Type::TypeInfoEx*) eType)->astNode = (SyntaxNode*) en;
        en->type = (Type::TypeInfo*) eType;

        return Err::OK;
    }

    // Pass already validated node
    Err::Err computeTypeInfo(ValidationContext* ctx, TypeDefinition* td) {
        // TODO: deprecate?
        if (td->state == TS_READY) return Err::OK;

        if (td->state == TS_RUNNING) {
            // TODO : add new error, add path logging
            Diag::report(ctx->unit->ast, td->base.span, Err::CIRCULAR_IMPORT);
            return Err::CIRCULAR_IMPORT;
        }

        td->state = TS_RUNNING;

        Type::StructInfo* sInfo;
        // TODO : shall we allocate this at definition creation?
        td->type = (Type::TypeInfoEx*) alloc(alc, sizeof(Type::TypeInfoEx));
        sInfo = (Type::StructInfo*) td->type;

        sInfo->members = (Type::StructMemberInfo*) alloc(alc, sizeof(Type::StructMemberInfo) * td->varCount);
        sInfo->memberCount = td->varCount;

        uint64_t offset = 0;
        uint64_t align  = 0;

        for (int i = 0; i < td->varCount; i++) {
            Variable* var = td->vars[i]->var;

            Type::TypeInfo* mInfo = var->value.type;
            sInfo->members[i].type = mInfo;
            sInfo->members[i].offset = offset;
            sInfo->members[i].name = { var->name.buff, var->name.len };

            offset += mInfo->size;
            offset += Utils::getPadding(offset, mInfo->align);
            align  = std::max(align, (uint64_t) mInfo->align);
        }

        td->type->base.size = offset;
        td->type->base.size += Utils::getPadding(offset, align);
        td->type->base.align = align;
        td->type->base.rank = 0;
        td->type->base.kind = Type::DT_STRUCT;

        td->state = TS_READY;

        return Err::OK;
    }

    // Wraps rvar in a Cast expression using the type described by target.
    // Assumes the cast has already been validated.
    void wrapInCast(ValidationContext* ctx, Variable* source, Type::TypeInfo* target, uint64_t flags) {
        // clone the current node to preserve also metadata
        // TODO: maybe no need to do a full copy
        Variable* innerOperand = Ast::Node::copy(source);

        Cast* castEx = Ast::Node::makeCast();
        castEx->operand = innerOperand;
        castEx->target = target;

        source->expression = (Expression*) castEx;
        source->value.type = target;
    }

    // TODO: lval/rvar are weird, as we not necessary cast during assignments
    Err::Err applyImplicitCast(ValidationContext* ctx, Variable* source, Type::TypeInfo* target) {
        if (target == source->value.type ||
            target->kind == Type::DT_MULTIPLE_TYPES) {
            return Err::OK;
        }

        // NOTE: we can do in place operations over scalar
        //       literals only while result stays in scalar space
        if (source->value.hasValue && Type::isScalar(target)) {
            // TODO: suppose to cast literal in place.
            castLiteral(ctx, &source->value, target);
            return Err::OK;
        }

        Err::Err err = validateImplicitCast(ctx, source->value.type, target);
        if (err < 0) {
            // TODO : error
            Diag::report(ctx->unit->ast, source->base.span, Err::UNEXPECTED_SYMBOL, "It was a bad day for an implicit cast :(");
            return Err::UNEXPECTED_SYMBOL;
        }

        wrapInCast(ctx, source, target, (uint64_t) err);

        return Err::OK;
    }

    Err::Err resolveResultType(ValidationContext* ctx, UnaryExpression* uex, Variable* var) {
        const OperatorEnum op = uex->base.opType;
        Type::TypeInfo* type = uex->operand->value.type;

        if (op == OP_GET_ADDRESS) {
            var->value.type = Type::makePointer(type);
        } else if (op == OP_GET_VALUE) {
            if (!isIndexable(type)) {
                // TODO : eerorr
            }

            var->value.type = ((Type::TypeInfoEx*) type)->ptr.element;
        } else if (op == OP_NEGATION) {
            if (Type::isPrimitive(type)) {
                var->value.type = Type::basicTypes + Type::DT_BOOL;
                return Err::OK;
            }

            Diag::report(ctx->unit->ast, var->base.span, Err::INVALID_TYPE_CONVERSION,
                Diag::Format{
                    "Operator '%s' cannot be applied to operand of type '%s'.\n"
                    "  Hint: Logical negation requires a primitive or boolean type."
                },
                OperatorToStr(op),
                Type::str(type)
            );

            return Err::INVALID_TYPE_CONVERSION;
        } else {
            var->value.type = type;
        }

        return Err::OK;
    }

    Err::Err resolveMember(ValidationContext* ctx, BinaryExpression* bex, Variable* var) {
        Variable* parent = bex->left;
        Variable* member = bex->right;

        const Type::TypeInfo* parentType = parent->value.type;
        const Type::TypeInfo* memberType = member->value.type;

        String* memberName = (String*) &member->name;
        // TODO : add validation of members path name, doesn't suppose to have one
        //        but not sure it should happen here...

        if (parentType->kind == Type::DT_ARRAY) {
            // TODO : think more
            if (Strings::compare(memberName, String(Lex::KWS_ARRAY_LENGTH))) {
                bex->base.base.type = EXT_GET_LENGTH;
                ((GetLength*) bex)->arr = parent;
            } else if (Strings::compare(memberName, String(Lex::KWS_ARRAY_SIZE))) {
                bex->base.base.type = EXT_GET_SIZE;
                ((GetSize*) bex)->arr = parent;
            } else {
                Diag::report(ctx->unit->ast, member->base.span, Err::UNEXPECTED_SYMBOL, "Unknown member of array!");
                return Err::UNEXPECTED_SYMBOL;
            }

            var->value.type = Type::basicTypes + Type::DT_U64;

            return Err::OK;
        }

        if (parentType->kind == Type::DT_ENUM) {
            Type::EnumInfo* eType = (Type::EnumInfo*) parentType;

            Type::EnumMemberInfo* mType = Type::findMember(eType, memberName);
            if (!mType) {
                Enumerator* en = (Enumerator*) ((Type::TypeInfoEx*) parentType)->astNode;
                Diag::report(ctx->unit->ast, en->base.span, Err::INVALID_ATTRIBUTE_NAME,
                    Diag::Format{
                        "Type Definition doesn't have requested attribute %.*s!"
                    }, memberName->len, memberName->len);
                return Err::INVALID_ATTRIBUTE_NAME;
            }

            bex->right->value.type = (Type::TypeInfo*) mType;
            var->value.type = (Type::TypeInfo*) eType;

            return Err::OK;
        }

        Type::TypeInfoEx* type = (Type::TypeInfoEx*) parent->value.type;
        Type::StructInfo* sType;
        if (parentType->kind == Type::DT_POINTER) {
            if (type->ptr.element->kind != Type::DT_STRUCT) {
                TypeDefinition* td = (TypeDefinition*) ((Type::TypeInfoEx*) parentType)->astNode;
                Diag::report(ctx->unit->ast, td->base.span, Err::INVALID_TYPE_CONVERSION, "Invalid type of dereferenced pointer for member selection!", var->base.span, var->name.len);
                return Err::INVALID_TYPE_CONVERSION;
            }

            bex->base.opType = OP_DEREFERENCE_MEMBER_SELECTION;
            sType = (Type::StructInfo*) type->ptr.element;
        } else if (parentType->kind == Type::DT_STRUCT) {
            sType = (Type::StructInfo*) parentType;
        } else {
            Diag::report(ctx->unit->ast, bex->right->base.span, Err::INVALID_TYPE_CONVERSION,
                Diag::Format{
                    "Invalid type '%*.s' for member selection!"
                }, 0, 0); // TODO
            return Err::INVALID_TYPE_CONVERSION;
        }

        Type::StructMemberInfo* mType = Type::findMember(sType, memberName);
        if (!mType) {
            TypeDefinition* td = (TypeDefinition*) ((Type::TypeInfoEx*) parentType)->astNode;
            Diag::report(ctx->unit->ast, td->base.span, Err::INVALID_ATTRIBUTE_NAME,
                Diag::Format {
                    "Type Definition doesn't have requested attribute %.*s!"
                }, memberName->len, memberName->len);
            return Err::INVALID_ATTRIBUTE_NAME;
        }

        bex->right->value.type = (Type::TypeInfo*) mType;
        var->value.type = mType->type;

        return Err::OK;
    }

    // Helper to identify operators that always return a Boolean
    // TODO : to a proper place, also unite such operators and make
    //        this only as < > comparasion
    inline bool isPredicate(OperatorEnum op) {
        switch (op) {
        case OP_EQUAL:
        case OP_NOT_EQUAL:
        case OP_LESS_THAN:
        case OP_GREATER_THAN:
        case OP_LESS_THAN_OR_EQUAL:
        case OP_GREATER_THAN_OR_EQUAL:

        case OP_BOOL_AND:
        case OP_BOOL_OR:
            return true;

        default:
            return false;
        }
    }

    Err::Err resolveResultType(ValidationContext* ctx, BinaryExpression* bex, Variable* var) {
        Type::TypeInfo* lType = bex->left->value.type;
        Type::TypeInfo* rType = bex->right->value.type;

        const OperatorEnum op = bex->base.opType;
        // Usually the result type can be derived from
        // operands ranks but in few cases operator can
        // influence the output type (arr[i])
        if (op == OP_SUBSCRIPT) {
            // TODO: move from this to a direct check, as we
            //       may need different behavior for each type later
            if (!isIndexable(lType)) {
                Diag::report(
                    ctx->unit->ast,
                    bex->left->base.span,
                    Err::INVALID_DATA_TYPE,
                    Diag::Format {
                        "Cannot index into a value of type '%s'; the subscript operator '[]' is only permitted on arrays or pointers.",
                    },
                    Type::str(lType)
                );
                return Err::INVALID_DATA_TYPE;
            }

            Type::TypeInfoEx* type = (Type::TypeInfoEx*) lType;

            if (rType->kind == Type::DT_RANGE) {
                bex->base.opType = OP_SLICE;

                RangeExpression* range = (RangeExpression*) bex->right->expression;
                Variable* bidxVar = unwrap(range->bidx);
                Variable* eidxVar = unwrap(range->eidx);

                if (bidxVar->value.hasValue && eidxVar->value.hasValue) {
                    const int64_t bidx = bidxVar->value.i64;
                    const int64_t eidx = eidxVar->value.i64;

                    if (eidx < bidx) {
                        Diag::report(ctx->unit->ast, bex->right->base.span, Err::UNEXPECTED_ERROR,
                            Diag::Format {
                                "Invalid slice range [%lld..%lld]: end index cannot be less than start index.",
                            },
                            bidx, eidx
                        );
                        return Err::UNEXPECTED_ERROR;
                    }

                    if (bidx < 0) {
                        Diag::report(ctx->unit->ast, bex->right->base.span, Err::UNEXPECTED_ERROR,
                            Diag::Format {
                                "Invalid slice start index (%lld); start index cannot be negative.",
                            },
                            bidx
                        );
                        return Err::UNEXPECTED_ERROR;
                    }

                    // TODO: Type::getMember ?
                    var->value.type = Type::makeArray(type->ptr.element, eidx - bidx);
                } else {
                    var->value.type = Type::makeSlice(type->ptr.element, IS_CONST);
                }
            } else {
                var->value.type = type->ptr.element;
            }

            return Err::OK;
        } else if (op == OP_MEMBER_SELECTION) {
            return resolveMember(ctx, bex, var);
        } else if (isPredicate(op)) {
            if (Type::isPrimitive(lType) && Type::isPrimitive(rType)) {
                var->value.type = Type::basicTypes + Type::DT_BOOL;
                return Err::OK;
            }

            Diag::report(ctx->unit->ast, var->base.span, Err::INVALID_TYPE_CONVERSION,
                Diag::Format{
                    "Operator '%s' cannot be applied to operands of type '%s' and '%s'.\n"
                    "  Hint: Relational and logical operators are only supported for primitive types."
                },
                OperatorToStr(op), Type::str(lType), Type::str(rType)
            );

            return Err::INVALID_TYPE_CONVERSION;
        }

        Err::Err err;
        if (lType->rank > rType->rank) {
            err = applyImplicitCast(ctx, bex->left, bex->right->value.type);
        } else {
            err = applyImplicitCast(ctx, bex->right, bex->left->value.type);
        }
        if (err != Err::OK) return err;

        var->value.type = bex->left->value.type;

        return Err::OK;
    }

    Err::Err resolveResultType(ValidationContext* ctx, FunctionCall* fex, Variable* var) {
        var->value = fex->outArg->value;
        var->value = fex->outArg->value;
        return Err::OK;
    }



    // ======================
    // VALIDATION FUNCTIONS

    inline Err::Err validatePointerAssignment(AstContext* ast, const Value* const val) {
        if (val->hasValue && val->u64 == 0) return Err::OK;
        Diag::report(ast, NULL, Err::INVALID_RVALUE, "Only 0 could be assigned to a pointer variable!");
        return Err::INVALID_RVALUE;
    }

    // TODO: can we get rid of it?
    bool validateImplicitCast(const Type::Kind source, const Type::Kind target) {
        const bool basicTypes     = Type::isBasic(source) || Type::isBasic(target);
        const bool arrayToPointer = (source == Type::DT_ARRAY && target == Type::DT_POINTER);
        const bool arrayToSlice   = (source == Type::DT_SLICE && target == Type::DT_ARRAY);
        const bool enumToInt      = Type::isIntegerOrEnum(source) && Type::isIntegerOrEnum(target);
        const bool ptrToPtr       = source == Type::DT_POINTER && target == Type::DT_POINTER;

        return basicTypes || arrayToPointer || arrayToSlice || enumToInt || ptrToPtr;
    }

    // uses ctx->tmpArena
    String resolveTypeName(Validator::ValidationContext* ctx, Type::TypeInfo* type) {
        constexpr int bufferSize = 512;

        char* buffer = (char*) Arena::push(&ctx->tmpArena, bufferSize);
        String str = { buffer, bufferSize };

        IO::Stream stream = {
            .kind  = IO::Stream::SK_BUFFER,
            .buffer = str
        };

        writeTypeName(&stream, type);
        Arena::rollback(&ctx->tmpArena, str.buff - buffer);

        return str;
    }

    Err::Err validateImplicitCast(ValidationContext* ctx, Type::TypeInfo* source, Type::TypeInfo* target) {
        if (source == target) return Err::OK;

        if (validateImplicitCast(source->kind, target->kind)) {
            return Err::OK;
        }

        if (target->kind == Type::DT_ARRAY) {
            Type::ArrayInfo* aInfo = (Type::ArrayInfo*) target;

            Err::Err err = validateImplicitCast(ctx, source, aInfo->element);
            if (err != Err::OK) return err;

            // TODO: make special ret type?
            return (Err::Err) IS_CASTED_FROM_LOWER_LEVEL;
        }

        Arena::Marker marker = Arena::getMarker(&ctx->tmpArena);

        String sourceName = resolveTypeName(ctx, source);
        String targetName = resolveTypeName(ctx, target);
        Diag::report(ctx->unit->ast, NULL, Err::INVALID_TYPE_CONVERSION,
            sourceName.len, sourceName.buff, targetName.len, targetName.buff);

        Arena::rollback(&ctx->tmpArena, marker);

        return Err::INVALID_TYPE_CONVERSION;
    }

    #define GENERATE_TARGET_SWITCH(dest, src) \
    switch (dest) { \
        case Type::DT_I8:  val->i8  = (int8_t)  (src); break; \
        case Type::DT_U8:  val->u8  = (uint8_t) (src); break; \
        case Type::DT_I16: val->i16 = (int16_t) (src); break; \
        case Type::DT_U16: val->u16 = (uint16_t)(src); break; \
        case Type::DT_I32: val->i32 = (int32_t) (src); break; \
        case Type::DT_U32: val->u32 = (uint32_t)(src); break; \
        case Type::DT_I64: val->i64 = (int64_t) (src); break; \
        case Type::DT_U64: val->u64 = (uint64_t)(src); break; \
        case Type::DT_F32: val->f32 = (float)   (src); break; \
        case Type::DT_F64: val->f64 = (double)  (src); break; \
        default: { \
            Diag::report(ctx->unit->ast, NULL, Err::UNEXPECTED_ERROR); \
        } \
    }

    // TODO : reuse eval
    void castLiteral(ValidationContext* ctx, Value* val, Type::Kind toType) {
        if (val->type->kind == toType) return;

        const Type::Kind kind = Type::getUnderlyingKind(val->type);
        switch (kind) {
            case Type::DT_U8: {
                uint8_t src = val->u8;
                GENERATE_TARGET_SWITCH(toType, src);
                break;
            }

            case Type::DT_U16: {
                uint16_t src = val->u16;
                GENERATE_TARGET_SWITCH(toType, src);
                break;
            }

            case Type::DT_U32: {
                uint32_t src = val->u32;
                GENERATE_TARGET_SWITCH(toType, src);
                break;
            }

            case Type::DT_U64: {
                uint64_t src = val->u64;
                GENERATE_TARGET_SWITCH(toType, src);
                break;
            }

            case Type::DT_I8: {
                int8_t src = val->i8;
                GENERATE_TARGET_SWITCH(toType, src);
                break;
            }

            case Type::DT_I16: {
                int16_t src = val->i16;
                GENERATE_TARGET_SWITCH(toType, src);
                break;
            }

            case Type::DT_I32: {
                int32_t src = val->i32;
                GENERATE_TARGET_SWITCH(toType, src);
                break;
            }

            case Type::DT_I64: {
                int64_t src = val->i64;
                GENERATE_TARGET_SWITCH(toType, src);
                break;
            }

            case Type::DT_F32: {
                float src = val->f32;
                GENERATE_TARGET_SWITCH(toType, src)
                break;
            }

            case Type::DT_F64: {
                double src = val->f64;
                GENERATE_TARGET_SWITCH(toType, src)
                break;
            }

            default: {
                // TODO
                Diag::report(ctx->unit->ast, NULL, Err::UNEXPECTED_ERROR);
                break;
            }
        }

        val->type = Type::basicTypes + toType;
    }

    void castLiteral(ValidationContext* ctx, Value* val, Type::TypeInfo* toType) {
        castLiteral(ctx, val, Type::getUnderlyingKind(toType));
    }

}
