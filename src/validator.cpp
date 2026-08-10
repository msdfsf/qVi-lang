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

#include "validator.h"
#include "array_list.h"
#include "data_types.h"
#include "dynamic_arena.h"
#include "emitter_drivers/emitter_driver_debug.h"
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

            if (pA->value.typeKind != pB->value.typeKind) return false;

            if (pA->value.typeKind == Type::DT_CUSTOM) {
                if (pA->value.def && pB->value.def) {
                    if (pA->value.def != pB->value.def) return false;
                } else {
                    if (!cstrcmp((String*) &pA->name, (String*) &pB->name)) return false;
                }
            }
        }

        return true;
    }

    bool signaturesMatch(Function* fcnA, Function* fcnB) {
        return signaturesMatch(&fcnA->prototype, &fcnB->prototype);
    }

    Err::Err ensureUniqueOverloads(ValidationContext* ctx, SymbolIndexEntry* entry) {
        const uint32_t count     = entry->overloads.count;
        SyntaxNode** const nodes = entry->overloads.data;

        for (uint32_t i = 0; i < count; i++) {
            SyntaxNode* node = nodes[i];
            if (node->type != NT_FUNCTION) {
                Logger::logNoFlush(
                    { .level = Logger::Level::ERROR, .tag = ctx->unit->ast->tag },
                    "Symbol '%.*s' is redefined across incompatible types. Only functions support overloading.",
                    Ast::Node::getNameSpan(nodes[0]), entry->name.len, entry->name.buff
                );

                // TODO : add max count to config
                for (uint32_t i = 0; i < count; i++) {
                    Logger::logNoFlush(
                        { .level = Logger::Level::ERROR, .style = Logger::Style::NO_HEADER, .tag = ctx->unit->ast->tag },
                        "Defined here as a %s.",
                        nodes[i]->span,
                        Ast::Node::str(nodes[i]->type)
                    );
                }

                Diag::commit(ctx->unit->ast, nodes[0]->span, Err::SYMBOL_ALREADY_DEFINED);
                return Err::SYMBOL_ALREADY_DEFINED;
            }

            // Redefinition Check
            for (uint32_t j = 0; j < i; j++) {
                Function* fcnA = (Function*) node;
                Function* fcnB = (Function*) nodes[j];

                if (signaturesMatch(fcnA, fcnB)) {
                    Logger::logNoFlush(
                        { .level = Logger::Level::ERROR, .tag = ctx->unit->ast->tag },
                        "Function '%.*s' with this signature is already defined in this scope.",
                        fcnA->base.span, fcnA->name.len, fcnA->name.buff
                    );

                    Logger::logNoFlush(
                        { .level = Logger::Level::ERROR, .style = Logger::NO_HEADER, .tag = ctx->unit->ast->tag },
                        "Symbol '%.*s' is already defined here.",
                        fcnB->base.span, fcnB->name.len, fcnB->name.buff
                    );

                    Diag::commit(ctx->unit->ast, fcnB->base.span, Err::SYMBOL_ALREADY_DEFINED);

                    return Err::SYMBOL_ALREADY_DEFINED;
                }
            }

            nodes[i] = node;
        }

        return Err::OK;
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

            Err::Err err = ensureUniqueOverloads(ctx, entry);
            if (err != Err::OK) return err;
        }

        return Err::OK;
    }

    // Pass validated function and call
    void computeFunctionMatchScore(Function* fcn, FunctionCall* call, FunctionScore* outScore) {
        const uint32_t fcnInCnt  = fcn->prototype.inArgCount;
        const uint32_t callInCnt = call->inArgCount;

        const bool isVariadic = (fcnInCnt > 0 &&
            fcn->prototype.inArgs[fcnInCnt - 1]->var->value.typeKind == Type::DT_MULTIPLE_TYPES);

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

            const Type::Kind fDtype = fArg->value.typeKind;
            const Type::Kind cDtype = cArg->value.typeKind;

            // Exact Match
            if (fDtype == cDtype) {
                if (fDtype == Type::DT_CUSTOM) {
                    if (fArg->value.def != cArg->value.def) {
                        outScore->value = 0;
                        return;
                    }
                }

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

    Err::Err validateDataType(ValidationContext* ctx, Type::Kind kind, void* data, Span* span) {
        // As string is just internal type, we can skip it
        if (Type::isBasic(kind) || kind == Type::DT_STRING) {
            return Err::OK;
        }

        if (!data) {
            Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_ERROR,
                Diag::Format {
                    "Internal Compiler Error: Type kind '%s' requires metadata, but pointer was NULL."
                },
                Type::str(kind));
            return Err::UNEXPECTED_SYMBOL;
        }

        switch (kind) {
            case Type::DT_CUSTOM:
            case Type::DT_UNION:
            case Type::DT_ENUM: {
                return ensureValidated(ctx, (SyntaxNode*) data);
            }

            case Type::DT_POINTER: {
                Pointer* ptr = (Pointer*) data;
                return validateDataType(ctx, ptr->pointsToKind, ptr->pointsTo, span);
            }

            case Type::DT_ARRAY: {
                Array* arr = (Array*) data;

                Err::Err err = validateDataType(ctx, arr->base.pointsToKind, arr->base.pointsTo, span);
                if (err != Err::OK) return err;

                if (arr->length) {
                    err = validate(ctx, arr->length, NULL);
                    if (err != Err::OK) return err;

                    if (arr->flags & IS_CMP_TIME) {
                        Interpreter::eval(ctx, arr->length);
                    }
                }

                Value val = {
                    .typeKind = Type::DT_ARRAY,
                    .hasValue = false,
                    .arr = arr
                };

                Type::TypeInfo* info;
                computeTypeInfo(ctx, &val, &info);

                return Err::OK;
            }

            case Type::DT_FUNCTION: {
                return validate(ctx, (FunctionPrototype*) data);
            }

            default: {
                Diag::report(ctx->unit->ast, span, Err::UNEXPECTED_ERROR,
                    Diag::Format {
                        "Unhandled type kind (%s) in validateDataType."
                    },
                    Type::str(kind));
                return Err::UNEXPECTED_ERROR;
            }
        }

        return Err::OK;
    }

    // TODO : IS_CMP_TIME to IS_EMBEDED ?
    // TODO : use info to check?
    bool isStaticArray(Value* val) {
        return val->typeKind == Type::DT_ARRAY &&
               val->arr->flags & IS_CMP_TIME;
    }

    Err::Err validate(ValidationContext* ctx, VariableDefinition* def) {
        Err::Err err;

        err = linkDataType(ctx, def);
        if (err != Err::OK) return err;

        Value leftValue = def->var->value;

        //err = validateDataType(ctx, leftValue.typeKind, leftValue.any, def->var->base.span);
        //if (err != Err::OK) return err;

        // Emitter::driverDebug.emitNode(&DebugHelper::emitter, (SyntaxNode*) def, &DebugHelper::stream);
        err = validate(ctx, def->var, def->var);
        if (err != Err::OK) return err;

        // In case of static array, we need to compute/infer length
        // TODO: can we force parser always parse length as NULL if [] is empty
        //       to avoid ambiguity?
        if (isStaticArray(&leftValue)) {
            // TODO: This shall happen while validating dtype in var validation
            //       So we shall remove it...
            Array* arr = leftValue.arr;
            if (arr->length) {
                err = validate(ctx, arr->length, NULL);
                if (err != Err::OK) return err;

                err = Interpreter::eval(ctx, arr->length);
                if (err != Err::OK) return err;
            } else {
                leftValue.arr->length = def->var->value.arr->length;
            }
        }

        // TODO : as we cahche anyway, create a function without
        //        return value
        Type::TypeInfo* info;
        err = computeTypeInfo(ctx, &leftValue, &info);
        if (err != Err::OK) return err;

        if (def->var->expression) {
            err = applyImplicitCast(ctx, &leftValue, def->var);
            if (err != Err::OK) return err;

            def->var->value = leftValue;
        } else {
            int x = 0;
            int y = x + 2;
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, TypeDefinition* td) {
        Err::Err err;

        const int isUnion = td->base.type == NT_UNION;

        if (checkUniqueNames(td->vars, td->varCount) != Err::OK) {
            Diag::report(ctx->unit->ast, td->base.span, Err::INVALID_ATTRIBUTE_NAME);
            return Err::INVALID_ATTRIBUTE_NAME;
        }

        for (int i = 0; i < td->varCount; i++) {
            Variable* const var = td->vars[i];

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
    Err::Err validateExpression(ValidationContext* ctx, Variable* var, Variable* target) {
        Err::Err err;

        Expression* ex = var->expression;
        if (!ex) {
            // If no expression to resolve, we have to ensure that its
            // data type is valid, as we are the source of truth for
            // other expressions.
            err = validateDataType(ctx, var->value.typeKind, var->value.any, var->base.span);
            if (err != Err::OK) return err;

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

                err = validate(ctx, bex->left, target);
                if (err != Err::OK) return err;

                if (isMemberSelection(bex->base.opType)) {
                    // TODO : do we need to do something?
                } else {
                    err = validate(ctx, bex->right, target);
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
                    if (lastArg->var->value.typeKind == Type::DT_MULTIPLE_TYPES) {
                        fixedCount--;
                    }
                }

                int i = 0;
                for (; i < fixedCount && i < callArgCount; i++) {
                    Variable* rvar = call->inArgs[i];
                    Variable* lvar = fcn->prototype.inArgs[i]->var;

                    err = applyImplicitCast(ctx, &lvar->value, rvar);
                    if (err != Err::OK) return err;
                }

                if (fcn->prototype.outArg) {
                    // TODO : again, dont like this call, think about it more...
                    Ast::Node::copyRef(call->outArg, call->fcn->prototype.outArg->var);
                    resolveResultType(ctx, call, var);
                } else {
                    var->value.typeKind = Type::DT_VOID;
                }

                break;
            }

            case EXT_SLICE: {

                Slice* slice = (Slice*)ex;

                err = validate(ctx, slice->bidx, target);
                if (err != Err::OK) return err;
                if (!isInt(slice->bidx->value.typeKind)) {
                    Diag::report(ctx->unit->ast, slice->bidx->base.span, Err::INVALID_DATA_TYPE, "TODO");
                    return Err::INVALID_DATA_TYPE;
                }

                err = validate(ctx, slice->eidx, target);
                if (err != Err::OK) return err;
                if (!isInt(slice->eidx->value.typeKind)) {
                    Diag::report(ctx->unit->ast, slice->bidx->base.span, Err::INVALID_DATA_TYPE, "TODO");
                    return Err::INVALID_DATA_TYPE;
                }

                break;

            }

            case EXT_STRING_INITIALIZATION: {

                StringInitialization* init = (StringInitialization*) ex;

                // var->value.typeKind = Type::DT_STRING;
                // var->value.any = init;

                var->value.typeKind = Type::DT_ARRAY;
                var->value.arr = Ast::Node::makeArray();
                var->value.arr->base.pointsToKind = init->wideType;
                var->value.arr->base.pointsTo = NULL;
                var->value.arr->flags = IS_CMP_TIME;

                Variable* len = Ast::Node::makeVariable();
                len->value.hasValue = true;
                len->value.typeKind = Type::DT_U64;
                len->value.u64 = init->wideType == Type::DT_U8 ?
                    init->rawStr.len : init->wideStr.len;
                var->value.arr->length = len;

                Type::TypeInfo* info;
                err = computeTypeInfo(ctx, &var->value, &info);
                if (err != Err::OK) return err;

                return Err::OK;

            }

            case EXT_ARRAY_INITIALIZATION: {
                ArrayInitialization* init = (ArrayInitialization*) ex;

                Value* dominantVal = NULL;
                int dominantIdx = 0;
                bool isStatic = true;

                Variable** buffer = (Variable**)init->attributes;

                for (int i = 0; i < init->attributeCount; i++) {

                    Variable* arg = *(buffer + i);
                    err = validate(ctx, arg, target);
                    if (err != Err::OK) return err;

                    if (isStatic && !arg->value.hasValue) {
                        isStatic = false;
                    }

                    // we want to make array type of the most 'dominant' type
                    if (!dominantVal || Type::basicTypes[dominantVal->typeKind].rank < Type::basicTypes[arg->value.typeKind].rank) {
                        dominantVal = &arg->value;
                        dominantIdx = i;
                    }

                }

                if (isStatic) {
                    init->flags |= IS_CMP_TIME;
                }

                // now we check if we can cast all elments to the 'dominant' one
                for (int i = 0; i < init->attributeCount; i++) {
                    if (i == dominantIdx) continue;

                    Variable* arg = *(buffer + i);
                    err = applyImplicitCast(ctx, dominantVal, arg);
                    if (err != Err::OK) return err;

                }

                // TODO : for now we allocate new Array for each case
                var->value.typeKind = Type::DT_ARRAY;
                var->value.arr = Ast::Node::makeArray();
                var->value.arr->base.pointsToKind = dominantVal->typeKind;
                var->value.arr->base.pointsTo = dominantVal->any;
                var->value.arr->flags = IS_CMP_TIME;
                if (dominantVal->typeKind == Type::DT_POINTER || dominantVal->typeKind == Type::DT_ARRAY) {
                    var->value.arr->base.parentPointer = dominantVal->ptr->parentPointer;
                }

                Variable* len = Ast::Node::makeVariable();
                len->value.hasValue = true;
                len->value.typeKind = Type::DT_U64;
                len->value.u64 = init->attributeCount;
                var->value.arr->length = len;

                // TODO : not sure it shall happen here...
                Type::TypeInfo* info;
                err = computeTypeInfo(ctx, &var->value, &info);
                if (err != Err::OK) return err;

                return Err::OK;
            }

            case EXT_TYPE_INITIALIZATION: {
                TypeInitialization* init = (TypeInitialization*) ex;
                if (!target) {
                    Diag::report(ctx->unit->ast, var->base.span, Err::UNEXPECTED_SYMBOL, "TODO : Type cannot be deducted in this situation!");
                    return Err::UNEXPECTED_SYMBOL;
                }

                TypeDefinition* td = (TypeDefinition*) target->value.any;

                if (init->attributeCount > td->varCount) {
                    Diag::report(ctx->unit->ast, td->base.span, Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH);
                    return Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH;
                }

                if (td->base.type == NT_UNION && init->attributeCount > 1) {
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
                    Variable* dest;
                    Variable* src = init->attributes[i];

                    if (attributesAreNamed) {
                        int idx = Ast::Find::inArray(td->vars, td->varCount, (String*) &src->name, &dest);
                        init->idxs[i] = idx;
                    } else {
                        dest = td->vars[i];
                    }

                    applyVariableLinkage(ctx, src, (SyntaxNode*) dest->def);

                    err = validate(ctx, src, dest);
                    if (err != Err::OK) return err;

                    err = applyImplicitCast(ctx, &dest->value, src);
                    if (err != Err::OK) return err;
                }

                if (init->fillVar) {
                    for (; i < td->varCount; i++) {
                        Variable* dest = td->vars[i];

                        applyVariableLinkage(ctx, init->fillVar, (SyntaxNode*) dest->def);

                        err = validate(ctx, init->fillVar, NULL);
                        if (err != Err::OK) return err;

                        err = applyImplicitCast(ctx, &dest->value, init->fillVar);
                        if (err != Err::OK) return err;

                    }
                }

                var->value.typeKind = Type::DT_CUSTOM;
                var->value.any = (void*)td;

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
    Err::Err validate(ValidationContext* ctx, Variable* var, Variable* target) {
        Err::Err err;

        if (!var) return Err::OK;

        if (var->base.semStatus == TS_READY) return Err::OK;
        var->base.semStatus = TS_PENDING;

        if (var->value.hasValue) return Err::OK;

        if (!var->def) {
            err = linkVariable(ctx, var);
            if (err != Err::OK) return err;
        }

        if (!var->expression && var->def && var != target) {
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

        err = validate(ctx, ass->rvar, ass->lvar);
        if (err != Err::OK) return err;

        err = applyImplicitCast(ctx, &ass->lvar->value, ass->rvar);
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

            if (!Type::isTruthy(condition->value.typeKind)) {
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

            err = applyImplicitCast(ctx, &node->switchExp->value, caseVar);
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

        if (!Type::isTruthy(node->expression->value.typeKind)) {
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

    Err::Err validate(ValidationContext* ctx, Range* range) {
        Err::Err err;

        err = validate(ctx, range->bidx);
        if (err != Err::OK) return err;
        if (!Type::isInt(range->bidx->value.typeKind)) {
            Value lval = toValue(Type::DT_I64);
            applyImplicitCast(ctx, &lval, range->bidx);
        }

        err = validate(ctx, range->eidx);
        if (err != Err::OK) return err;
        if (!Type::isInt(range->eidx->value.typeKind)) {
            Value lval = toValue(Type::DT_I64);
            applyImplicitCast(ctx, &lval, range->eidx);
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
        if (node->arg.kind == Loop::Arg::ARRAY) {
            err = validate(ctx, node->arg.array);
            if (err != Err::OK) return err;

            Type::Kind kind = node->arg.array->value.typeKind;
            if (kind != Type::DT_ARRAY && kind != Type::DT_SLICE) {
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
        if (node->array) {
            if (node->arg.kind != Loop::Arg::ARRAY) {
                // TODO : error
            }
            aliasVariable(node->array, node->arg.array);
        }

        if (node->index.var) {
            if (node->index.var->base.type == NT_VARIABLE) {
                validate(ctx, node->index.var);
            } else {
                if (node->index.def->var->value.typeKind == Type::DT_VOID) {
                    node->index.def->var->value.hasValue = true;
                    node->index.def->var->value.typeKind = Type::DT_I64;
                    node->index.def->var->value.any = NULL;
                }
            }
        }

        // By
        if (node->stride) {
            err = validate(ctx, node->stride);
            if (!Type::isInt(node->stride->value.typeKind)) {
                Value lval = toValue(Type::DT_I64);
                applyImplicitCast(ctx, &lval, node->stride);
            }
        }

        // While
        if (node->condition) {
            err = validate(ctx, node->condition);
            if (err != Err::OK) return err;

            if (!Type::isTruthy(node->condition->value.typeKind)) {
                Diag::report(ctx->unit->ast, node->condition->base.span, Err::INVALID_DATA_TYPE);
                return Err::INVALID_DATA_TYPE;
            }
        }

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
            err = validate(ctx, ctx->currentFunction->prototype.outArg->var, NULL);
            if (err != Err::OK) return err;

            err = validate(ctx, node->var, ctx->currentFunction->prototype.outArg->var);
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
            expectedValue = Value { .typeKind = Type::DT_VOID };
        }

        if (!node->var) {
            if (expectedValue.typeKind != Type::DT_VOID) {
                Diag::report(ctx->unit->ast, node->base.span, Err::INVALID_DATA_TYPE,
                    Diag::Format{
                        "Function expects a return value of type '%s'."
                    }, Type::str(expectedValue.typeKind));
                return Err::INVALID_DATA_TYPE;
            }
            return Err::OK;
        }

        if (applyImplicitCast(ctx, &expectedValue, node->var) != Err::OK) {
            Diag::report(ctx->unit->ast, node->var->base.span, Err::INVALID_DATA_TYPE);
            return Err::INVALID_DATA_TYPE;
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, Enumerator* node) {
        if (!node) return Err::OK;

        if (!Type::isInt(node->dtype)) {
            Diag::report(ctx->unit->ast, node->base.span, Err::INVALID_DATA_TYPE);
            // Fallback to i32 to allow further validation
            node->dtype = Type::DT_I32;
        }

        uint64_t nextValue = 0;
        for (uint32_t i = 0; i < node->varCount; i++) {
            Variable* mVar = node->vars[i];
            if (!mVar) continue;

            mVar->value.typeKind = node->dtype;

            if (mVar->expression) {
                Err::Err err = Interpreter::eval(ctx, mVar);
                if (err != Err::OK) return err;

                nextValue = mVar->value.u64 + 1;
            } else {
                mVar->value.u64 = nextValue++;
                mVar->value.hasValue = true;
            }
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, Statement* node) {
        if (!node || !node->operand) return Err::OK;

        Err::Err err = validate(ctx, node->operand);
        if (err != Err::OK) return err;

        Variable* var = node->operand;
        if (var->value.typeKind != Type::DT_VOID) {
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

            mVar->value.typeKind = Type::DT_ERROR;
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

    Err::Err linkDataType(ValidationContext* ctx, VariableDefinition* def) {
        if (!def->dtype) return Err::OK;

        SyntaxNode* node;
        Err::Err err = resolveQualifiedName(ctx, def->base.scope, def->dtype, &node);
        if (err != Err::OK) return err;

        void** type;
        Type::Kind* typeKind;

        if (def->lastPtr) {
            type = (void**) &(def->lastPtr->pointsTo);
            typeKind = &(def->lastPtr->pointsToKind);
        } else {
            type = (void**) &(def->var->value.any);
            typeKind = &(def->var->value.typeKind);
        }

        // TODO : simplify by adding NT -> TK lookup
        switch (node->type) {
            case NT_TYPE_DEFINITION: {
                *type = (void*) node;
                *typeKind = Type::DT_CUSTOM;
                break;
            }

            case NT_ENUMERATOR: {
                Enumerator* en = (Enumerator*) node;
                *type = Type::basicTypes + en->dtype;
                *typeKind = en->dtype;
                break;
            }

            case NT_UNION: {
                *type = (void*) node;
                *typeKind = Type::DT_UNION;
                break;
            }

            case NT_ERROR: {
                *type = (void*) node;
                *typeKind = Type::DT_ERROR;
                break;
            }

            default: {
                Diag::report(ctx->unit->ast, def->base.span, Err::INVALID_DATA_TYPE,
                    Diag::Format {
                        "Symbol '%.*s' is not a data type."
                    },
                    def->dtype->len, def->dtype->buff);

                return Err::INVALID_DATA_TYPE;
            }
        }

        return Err::OK;
    }

    Err::Err linkErrorSet(ValidationContext* ctx, Function* fcn) {
        /*
        QualifiedName* const errName = fcn->errorSetName;

        if (!errName) {
            fcn->errorSet = NULL;
            return Err::OK;
        }

        if (errName->path->len > 0) {

            Namespace* nspace = NULL;
            ErrorSet* eset = NULL;

            const Err::Err err = validateQualifiedName(ctx, fcn->base.scope, errName, &nspace, &eset);
            if (err != Err::OK) return err;

            if (!eset) {
                Diag::report(ctx->unit->ast, fcn->base.span, Err::UNKNOWN_ERROR_SET);
                return Err::UNKNOWN_ERROR_SET;
            }

            if (nspace) {
                // TODO
                eset = NULL;//ctx->reg->Find.inArray(&nspace->scope.customErrors, (String*) errName);
                if (!eset) {
                    Diag::report(ctx->unit->ast, fcn->base.span, Err::UNKNOWN_ERROR_SET);
                    return Err::UNKNOWN_ERROR_SET;
                }
            } else if (eset) {
                eset = Ast::Find::inScopeErrorSet(eset->base.scope, (String*) errName);
                if (!eset) {
                    Diag::report(ctx->unit->ast, fcn->base.span, Err::UNKNOWN_ERROR_SET);
                    return Err::UNKNOWN_ERROR_SET;
                }
            }

            fcn->errorSet = eset;

        } else {

            ErrorSet* const eset = Ast::Find::inScopeErrorSet(fcn->base.scope, (String*) errName);
            if (!eset) {
                Diag::report(ctx->unit->ast, fcn->base.span, Err::UNKNOWN_ERROR_SET);
                return Err::UNKNOWN_ERROR_SET;
            }

            fcn->errorSet = eset;

        }
        */
        return Err::OK;
    }

    Err::Err applyVariableLinkage(ValidationContext* ctx, Variable* var, SyntaxNode* definition) {
        switch (definition->type) {
            case NT_VARIABLE_DEFINITION: {
                var->def = (VariableDefinition*) definition;
                break;
            }

            case NT_ENUMERATOR: {
                var->value.typeKind = Type::DT_ENUM;
                var->value.enm = (Enumerator*) definition;
                break;
            }

            case NT_FUNCTION: {
                var->value.typeKind = Type::DT_FUNCTION;
                var->value.fcn = NULL;
                break;
            }

            case NT_TYPE_DEFINITION: {
                // TODO : deprecated behaviour
                Err::Err err = validate(ctx, (TypeDefinition*) definition);
                if (err != Err::OK) return err;

                var->value.i64 = ((TypeDefinition*) definition)->typeInfo->base.size;
                var->value.hasValue = true;
                var->value.typeKind = Type::DT_I64;

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

        Type::Kind outTypeKind;
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
                outTypeKind = var->value.fcn->outArg->var->value.typeKind;
            } else {
                // TODO : proper error
                Diag::report(ctx->unit->ast, callOp->base.span, Err::SYMBOL_NOT_FOUND);
                return (Err::Err) Err::SYMBOL_NOT_FOUND;
            }

        } else {
            call->fptr = NULL;
            call->fcn = fcn;
            outTypeKind = fcn->prototype.outArg->var->value.typeKind;
        }

        call->outArg = new Variable();
        call->outArg->value.hasValue = false;
        call->outArg->value.typeKind = outTypeKind;

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

    Err::Err computeTypeInfo(ValidationContext* ctx, Value* val, Type::TypeInfo** outInfo) {
        const Type::Kind typeKind = val->typeKind;

        if (Type::isPrimitive(typeKind)) {
            *outInfo = Type::basicTypes + typeKind;
        } else if (typeKind == Type::DT_CUSTOM) {
            computeTypeInfo(ctx, val->def);
            *outInfo = (Type::TypeInfo*) val->def->typeInfo;
        } else if (typeKind == Type::DT_ARRAY) {
            Array* arr = val->arr;

            // array with runtime length is interpreted as slice
            if (arr->flags ^ IS_CMP_TIME || !arr->length) {
                *outInfo = (Type::TypeInfo*) Type::basicTypes + Type::DT_SLICE;
            } else {
                Type::TypeInfoEx* aInfo = (Type::TypeInfoEx*) alloc(alc, sizeof(Type::TypeInfoEx));
                Type::TypeInfo* eInfo;

                const uint64_t len = arr->length->value.u64;

                Value tmpVal = {
                    .typeKind = arr->base.pointsToKind,
                    .hasValue = 0,
                    .any = arr->base.pointsTo
                };

                Err::Err err = computeTypeInfo(ctx, &tmpVal, (Type::TypeInfo**) &eInfo);
                if (err != Err::OK) return err;

                aInfo->base.kind = Type::DT_ARRAY;
                aInfo->base.size = len * eInfo->size;
                aInfo->base.align = eInfo->align;
                aInfo->base.rank = Type::basicTypes[Type::DT_ARRAY].rank;

                aInfo->arr.element = eInfo;
                aInfo->arr.elementCount = len;

                *outInfo = (Type::TypeInfo*) aInfo;
            }

            arr->type = (Type::TypeInfoEx*) *outInfo;
        } else {
            return Err::NOT_YET_IMPLEMENTED;
        }

        return Err::OK;
    }

    Err::Err computeTypeInfo(ValidationContext* ctx, TypeDefinition* td) {
        if (td->state == TS_READY) return Err::OK;

        if (td->state == TS_RUNNING) {
            // TODO : add new error, add path logging
            Diag::report(ctx->unit->ast, td->base.span, Err::CIRCULAR_IMPORT);
            return Err::CIRCULAR_IMPORT;
        }

        td->state = TS_RUNNING;

        Type::StructInfo* sInfo;
        // TODO : shall we allocate this at definition creation?
        td->typeInfo = (Type::TypeInfoEx*) alloc(alc, sizeof(Type::StructInfo));
        sInfo = (Type::StructInfo*) td->typeInfo;

        sInfo->members = (Type::StructMemberInfo*) alloc(alc, sizeof(Type::StructMemberInfo) * td->varCount);
        sInfo->memberCount = td->varCount;

        uint64_t offset = 0;
        uint64_t align  = 0;

        for (int i = 0; i < td->varCount; i++) {
            Variable* var = td->vars[i];

            Type::TypeInfo* mInfo;
            Err::Err err = computeTypeInfo(ctx, &var->value, &mInfo);
            if (err != Err::OK) return err;

            sInfo->members[i].type = mInfo;
            sInfo->members[i].offset = offset;
            sInfo->members[i].name = { var->name.buff, var->name.len };

            offset += mInfo->size;
            offset += Utils::getPadding(offset, mInfo->align);
            align  = std::max(align, (uint64_t) mInfo->align);
        }

        td->typeInfo->base.size = offset;
        td->typeInfo->base.size += Utils::getPadding(offset, align);
        td->typeInfo->base.align = align;
        td->typeInfo->base.rank = 0;
        td->typeInfo->base.kind = Type::DT_CUSTOM;

        td->state = TS_READY;

        return Err::OK;
    }

    // Wraps rvar in a Cast expression using the type described by target.
    // Assumes the cast has already been validated.
    void wrapInCast(ValidationContext* ctx, Value* target, Variable* rvar) {
        // clone the current node to preserve also metadata
        // TODO: maybe no need to do a full copy
        Variable* innerOperand = Ast::Node::copy(rvar);

        Cast* castEx = Ast::Node::makeCast();
        castEx->operand = innerOperand;

        if (target->typeKind != Type::DT_ARRAY) {
            castEx->target = target->typeKind;
        } else {
            castEx->target = target->arr->base.pointsToKind;
        }

        rvar->expression = (Expression*) castEx;
        rvar->value.typeKind = target->typeKind;
        rvar->value.any = target->any;
    }

    Value toValue(Type::Kind kind) {
        return { .typeKind = kind, .hasValue = 0, .any = NULL };
    }

    Err::Err applyImplicitCast(ValidationContext* ctx, Value* lval, Variable* rvar) {
        if (lval->typeKind == Type::DT_MULTIPLE_TYPES) {
            return Err::OK;
        }

        if (Type::isPrimitive(lval->typeKind) &&
            lval->typeKind == rvar->value.typeKind
        ) {
            return Err::OK;
        }

        if (lval->typeKind == Type::DT_CUSTOM &&
            rvar->value.typeKind == lval->typeKind &&
            rvar->value.any == lval->any
        ) {
            return Err::OK;
        }

        // we can do inplace operations over primitive
        // literals only while result stays in primitve space
        if (rvar->value.hasValue && Type::isPrimitive(lval->typeKind)) {
            // TODO: suppose to cast literal in place.
            castLiteral(ctx, &rvar->value, lval->typeKind);
            return Err::OK;
        }

        // TODO : proper implicit cast validation requaried!!!
        const int64_t ans = (int64_t) validateImplicitCast(
            ctx,
            rvar->value.any, lval->any,
            rvar->value.typeKind, lval->typeKind
        );

        if (ans < 0) {
            // TODO : error
            Diag::report(ctx->unit->ast, rvar->base.span, Err::UNEXPECTED_SYMBOL, "It was a bad day for an implicit cast :(");
            return Err::UNEXPECTED_SYMBOL;
        } else if (ans == EXACT_MATCH) {
            return Err::OK;
        }

        wrapInCast(ctx, lval, rvar);

        return Err::OK;
    }

    // We cannot inherit full value of basic type in case
    // of litteral/cmp time value
    inline void inheritType(Variable* dest, Value* source) {
        if (Type::isBasic(source->typeKind)) {
            dest->value.typeKind = source->typeKind;
        } else {
            dest->value = *source;
        }
    }

    Err::Err resolveResultType(ValidationContext* ctx, UnaryExpression* uex, Variable* var) {
        const OperatorEnum op = uex->base.opType;
        if (op == OP_GET_ADDRESS) {
            // LOOK AT : do we need to create?
            Pointer* ptr = Ast::Node::makePointer();
            ptr->pointsToKind = uex->operand->value.typeKind;
            ptr->pointsTo = uex->operand->value.any;

            var->value.ptr = ptr;
            var->value.typeKind = Type::DT_POINTER;
        } else if (op == OP_GET_VALUE) {
            // TODO : view binary version
            if (!isIndexable(uex->operand->value.typeKind)) {
                // TODO : eerorr
            }

            var->value.any = uex->operand->value.ptr->pointsTo;
            var->value.typeKind = uex->operand->value.ptr->pointsToKind;
        } else if (op == OP_NEGATION) {
            const Type::Kind dtype = uex->operand->value.typeKind;

            if (Type::isPrimitive(dtype)) {
                var->value.typeKind = Type::DT_BOOL;
                return Err::OK;
            }

            Diag::report(ctx->unit->ast, var->base.span, Err::INVALID_TYPE_CONVERSION,
                Diag::Format{
                    "Operator '%s' cannot be applied to operand of type '%s'.\n"
                    "  Hint: Logical negation requires a primitive or boolean type."
                },
                OperatorToStr(op),
                Type::str(dtype)
            );

            return Err::INVALID_TYPE_CONVERSION;
        } else {
            inheritType(var, &uex->operand->value);
        }

        return Err::OK;
    }

    Err::Err resolveMember(ValidationContext* ctx, BinaryExpression* bex, Variable* var) {
        Variable* parent = bex->left;
        Variable* member = bex->right;

        const Type::Kind parentType = parent->value.typeKind;
        const Type::Kind memberType = member->value.typeKind;

        String* memberName = (String*) &member->name;
        // TODO : add validation of members path name, doesnt suppose to have one
        //  but not sure it should happen here...

        if (parentType == Type::DT_ARRAY) {

            // TODO : for now this way, but think to chenge the operator or
            //   expression type, a bit of fragmentation, but thats what happens
            //   with additional alloc/dealloc
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

            var->value.typeKind = Type::DT_U64;
            // TODO : I guess we can just alias GetLength and GetSize
            //  to a BinaryExpression and change the type, so we can
            //  just keep the data and clean them when the time comes
            //  as we may need dealloc to work in arena case for the last
            //  allocated pointer, therefore we cannot free here as we corrupt
            //  the arena.
            // dealloc(alc, member);

            return Err::OK;
        }

        if (parentType == Type::DT_ENUM) {
            Enumerator* en = parent->value.enm;
            Variable* attribute = Ast::Find::inArray(en->vars, en->varCount, memberName);
            if (!attribute) {
                Diag::report(ctx->unit->ast, en->base.span, Err::UNKNOWN_VARIABLE, "Unable to find member of enum!", member->base.span, 0);
                return Err::UNEXPECTED_SYMBOL;
            }

            member->def = attribute->def;
            var->value = attribute->value;

            return Err::OK;
        }

        TypeDefinition* td;
        if (parentType == Type::DT_POINTER) {
            Pointer* ptr = parent->value.ptr;

            if (ptr->pointsToKind != Type::DT_CUSTOM || !ptr->pointsTo) {
                Diag::report(ctx->unit->ast, td->base.span, Err::INVALID_TYPE_CONVERSION, "Invalid type of dereferenced pointer for member selection!", var->base.span, var->name.len);
                return Err::INVALID_TYPE_CONVERSION;
            }

            bex->base.opType = OP_DEREFERENCE_MEMBER_SELECTION;
            td = (TypeDefinition*) ptr->pointsTo;
        } else if (parentType == Type::DT_CUSTOM) {
            td = (TypeDefinition*) parent->value.any;
        } else {
            Diag::report(ctx->unit->ast, bex->right->base.span, Err::INVALID_TYPE_CONVERSION,
                Diag::Format{
                    "Invalid type '%*.s' for member selection!"
                }, 0, 0); // TODO
            return Err::INVALID_TYPE_CONVERSION;
        }

        Variable* attribute = NULL;
        uint32_t idx = Ast::Find::inArray(td->vars, td->varCount, (String*) &member->name, &attribute);
        if (!attribute) {
            Diag::report(ctx->unit->ast, attribute->base.span, Err::INVALID_ATTRIBUTE_NAME, "TODO: Type doesnt have requested attribute!");
            return Err::INVALID_ATTRIBUTE_NAME;
        }


        // Ast::Node::copyRef(bex->right, attribute);
        bex->right->name.id = idx;
        // TODO : create union member
        bex->right->value.typeKind = Type::DT_MEMBER;
        bex->right->value.any = (void*) (td->typeInfo->str.members + idx);
        // bex->right->def = attribute->def;

        var->value.typeKind = attribute->value.typeKind;
        var->value.any = attribute->value.any;

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
        Type::Kind lDtype = bex->left->value.typeKind;
        Type::Kind rDtype = bex->right->value.typeKind;

        const OperatorEnum op = bex->base.opType;
        // Usually the result type can be derived from
        // operands ranks but in few cases operator can
        // influence the output type (arr[i])
        if (op == OP_SUBSCRIPT) {
            // TODO : move from this to a direct check, as we
            //   may need different behavior for each type later
            if (!isIndexable(lDtype)) {
                // TODO : throw error
            }

            Pointer* ptr = bex->left->value.ptr;
            var->value.any = ptr->pointsTo;
            var->value.typeKind = ptr->pointsToKind;

            return Err::OK;
        } else if (op == OP_MEMBER_SELECTION) {
            // we also may want to change the operator to
            // OP_DEREFERENCE_MEMEBER_SELECTION here if needed,
            // so its simpler to compile
            return resolveMember(ctx, bex, var);
        } else if (isPredicate(op)) {
            if (Type::isPrimitive(lDtype) && Type::isPrimitive(rDtype)) {
                var->value.typeKind = Type::DT_BOOL;
                return Err::OK;
            }

            Diag::report(ctx->unit->ast, var->base.span, Err::INVALID_TYPE_CONVERSION,
                Diag::Format{
                    "Operator '%s' cannot be applied to operands of type '%s' and '%s'.\n"
                    "  Hint: Relational and logical operators are only supported for primitive types."
                },
                OperatorToStr(op),
                Type::str(lDtype),
                Type::str(rDtype)
            );

            return Err::INVALID_TYPE_CONVERSION;
        }

        Err::Err err = applyImplicitCast(ctx, &bex->left->value, bex->right);
        if (err != Err::OK) return err;

        if (Type::basicTypes[lDtype].rank > Type::basicTypes[rDtype].rank) {
            inheritType(var, &bex->left->value);
        } else {
            inheritType(var, &bex->left->value);
        }

        return Err::OK;
    }

    Err::Err resolveResultType(ValidationContext* ctx, FunctionCall* fex, Variable* var) {
        var->value = fex->outArg->value;
        var->value = fex->outArg->value;
        return Err::OK;
    }



    // ======================
    // VALIDATION FUNCTIONS

    // used within validateTypeInitialization
    // think about better name
    Err::Err validateAttributeCast(Variable* var, Variable* attribute) {

        // Variable* op, TypeDefinition** customDtype, Type::Kind lvalueType, TypeDefinition* lvalueTypeDef
        const int id = attribute->name.id;
        char* const name = attribute->name.buff;
        const int nameLen = attribute->name.len;

        VariableDefinition* def = var->def;
        Type::Kind dtypeA = attribute->value.typeKind;
        // Type::Kind dtypeA = (Type::Kind) evaluateDataTypes(
        //    attribute,
        //    NULL,
        //    def ? def->var->value.typeKind : DT_UNDEFINED,
        //    def ? def->var->value.def : NULL
        //);

        attribute->name.id = id;
        attribute->name.buff = name;
        attribute->name.len = nameLen;

        Type::Kind dtypeB = var->value.typeKind;
        // Type::Kind dtypeB = (Type::Kind) evaluateDataTypes(var);

        if (!validateImplicitCast(dtypeB, dtypeA)) {
            return Err::INVALID_DATA_TYPE;
        }

        return Err::OK;

    }

    // assuming dtypeInit has at least one attribute
    // both TypeDefinition  has to be valid
    Err::Err validateTypeInitialization(Reg::Unit* unit, TypeDefinition* dtype, TypeInitialization* dtypeInit) {
        Variable** attributes = (Variable**) dtypeInit->attributes;

        const int count = dtype->varCount;
        const int areNamed = dtypeInit->attributeCount == 0 || (attributes[0])->name.buff;

        if (count < dtypeInit->attributeCount) {
            Diag::report(unit->ast, dtype->base.span, Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH, dtypeInit->attributeCount, dtype->varCount);
            return Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH;
        }

        if (dtype->base.type == NT_UNION) {

            if (dtypeInit->attributeCount > 1) {
                Variable* var = attributes[1];
                Diag::report(unit->ast, var->base.span, Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH, "Only one attribute can be set while initializing union!");
                return Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH;
            }

            if (dtypeInit->fillVar) {
                Variable* var = dtypeInit->fillVar;
                Diag::report(unit->ast, var->base.span, Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH, "Fill th rest option is not allowed while initializing union!");
                return Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH;
            }

            if (!areNamed) {
                Variable* var = attributes[0];
                Diag::report(unit->ast, var->base.span, Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH, "Only initialization through specifying name of attribute is allowed while initializing union!");
                return Err::TYPE_INIT_ATTRIBUTES_COUNT_MISMATCH;
            }

        }

        if (areNamed) {
            // treating all as named

            dtypeInit->idxs = (int*) malloc(sizeof(int) * count);
            for (int i = 0; i < count; i++) {
                dtypeInit->idxs[i] = -1;
            }

            for (int i = 0; i < dtypeInit->attributeCount; i++) {

                Variable* attribute = attributes[i];

                const int idx = Ast::Find::inArray(dtype->vars, dtype->varCount, (String*) &attribute->name, NULL);
                if (idx < 0) {
                    Diag::report(unit->ast, attribute->base.span, Err::INVALID_ATTRIBUTE_NAME, attribute->name.len, attribute->name.buff);
                    return Err::INVALID_ATTRIBUTE_NAME;
                }

                Variable* var = dtype->vars[idx];

                dtypeInit->idxs[idx] = i;
                attribute->name.id = var->name.id;

                // typecheck
                // if (!attribute->expression) continue;

                const Err::Err err = validateAttributeCast(var, attribute);
                if (err < 0) return err;

            }

            if (dtypeInit->fillVar) {

                for (int i = 0; i < dtype->varCount; i++) {

                    Variable* attribute = dtype->vars[i];
                    if (dtypeInit->idxs[i] < 0) continue;

                    // typecheck
                    // if (!attribute->expression) continue;

                    const Err::Err err = validateAttributeCast(attribute, dtypeInit->fillVar);
                    if (err < 0) return err;

                }

            }

            return Err::OK;

        }

        for (int i = 0; i < dtypeInit->attributeCount; i++) {

            Variable* attribute = dtypeInit->attributes[i];
            Variable* var = dtype->vars[i];

            dtypeInit->idxs[i] = i;

            // typecheck
            if (!attribute->expression) continue;

            const Err::Err err = validateAttributeCast(var, attribute);
            if (err < 0) return err;

        }

        return Err::OK;
    }

    Err::Err validateCall(ValidationContext* ctx, Variable* callOp) {
        if (callOp->base.semStatus == TS_READY) return Err::OK;

        Err::Err err = linkCall(ctx, callOp);
        if (err != Err::OK) return err;

        // Variable* fcnCallOp = SyntaxNode::fcnCalls[i];
        FunctionCall* call = (FunctionCall*) (callOp->expression);
        // Function* fcn = fcnCall->fcn;
        FunctionPrototype* fcn = call->fcn ? &call->fcn->prototype : call->fptr->value.fcn;
        const int fcnInCount = fcn->inArgCount;
        // const int fcnCallInCount = fcnCall->inArgs.size();

        VariableDefinition** fcnInArgs = fcn->inArgs;
        const int variableNumberOfArguments = fcnInCount > 0 && (fcnInArgs[fcnInCount - 1])->var->value.typeKind == Type::DT_MULTIPLE_TYPES;

        // note:
        //  array argument is parsed as two arguments (pointer and length) in definition

        int j = 0;
        for (int i = 0; i < fcnInCount - variableNumberOfArguments; i++) {

            if (j >= call->inArgCount) {
                Diag::report(ctx->unit->ast, callOp->base.span, Err::NOT_ENOUGH_ARGUMENTS);
                return Err::NOT_ENOUGH_ARGUMENTS;
            }

            VariableDefinition* fcnVarDef = fcnInArgs[i];
            Variable* fcnVar = fcnVarDef->var;
            Variable* fcnCallVar = call->inArgs[j];

            int fcnCallVarDtype;
            if (fcnCallVar->expression) {
                // TODOD : TypeDefinition** customDtype, Type::Kind lvalueType, TypeDefinition* lvalueTypeDef
                // fcnCallVarDtype = evaluateDataTypes(fcnCallVar);
                fcnCallVarDtype = fcnCallVar->value.typeKind;
                if (fcnCallVarDtype < Err::OK) return (Err::Err) fcnCallVarDtype;
            } else {
                fcnCallVarDtype = fcnCallVar->value.typeKind;
            }

            if (fcnVar->value.typeKind == Type::DT_ARRAY) {

                if (!(fcnCallVar->value.typeKind == Type::DT_ARRAY)) {
                    Diag::report(ctx->unit->ast, fcnCallVar->base.span, Err::ARRAY_EXPECTED);
                    return Err::ARRAY_EXPECTED;
                }

                Variable* var = fcnCallVar;
                if (!(var->def) || !(var->def->var->value.arr)) {
                    Diag::report(ctx->unit->ast, fcnCallVar->base.span, Err::ARRAY_EXPECTED, "TODO error: array expected!");
                    return Err::ARRAY_EXPECTED;
                }

                //const Err::Err err = evaluateArrayLength(var);
                //if (err < 0) return err;

                call->inArgs[j] = var;

                /*
                Variable* lenVar = new Variable();
                const int err = evaluateArrayLength(var, lenVar);
                if (err < 0) return err;

                Value* val = new Value();
                val->arr

                var->value.arr->length = lenVar;
                var->value.arr->length->value.typeKind = DT_UINT_64;
                var->value.arr->length->flags |= IS_LENGTH;
                */
                /*
                Array* arr = var->def->var->value.arr;
                if (arr->flags & IS_ARRAY_LIST) {
                    fcnCall->inArgs.insert(fcnCall->inArgs.begin() + j + 1, arr->length);
                    i++;
                    j++;
                    continue;
                }

                // fcnVar->value.arr->length = arr->length;
                fcnCall->inArgs.insert(fcnCall->inArgs.begin() + j + 1, arr->length); // fcnCallVar->def->var->value.arr->length);
                // j++;
                */

            }

            if (!validateImplicitCast((Type::Kind) fcnCallVarDtype, fcnVar->value.typeKind)) {
                // error : cannot cast
                // TODO: Diag::report(unit->ast, Err::str(Err::INVALID_TYPE_CONVERSION), fcnCallVar->base.span, 1, (dataTypes + fcnVar->value.typeKind)->name, (dataTypes + fcnCallVarDtype)->name);
                return Err::INVALID_TYPE_CONVERSION;
            }

            j++;

        }

        const int fcnCallInCount = call->inArgCount;

        if (j < fcnCallInCount && !variableNumberOfArguments) {
            Diag::report(ctx->unit->ast, callOp->base.span, Err::TOO_MANY_ARGUMENTS);
            return Err::TOO_MANY_ARGUMENTS;
        }

        for (; j < fcnCallInCount; j++) {

            Variable* var = call->inArgs[j];

            int err;
            if (var->def) {
                // TODO : ALLOC case for now

                Variable* tmp = var->def->var;
                const Value tmpValue = tmp->value;
                const Type::Kind tmpDtype = tmp->value.typeKind;

                err = var->value.typeKind;// evaluateDataTypes(var, NULL, tmp->value.typeKind, tmp->value.def);

                if (tmp->value.typeKind == Type::DT_CUSTOM) {
                    // seems like evaluateDataTypes does the same thing
                    // validateTypeInitialization(tmp->value.def, (TypeInitialization*) ((WrapperExpression*) (var->expression))->operand->expression);
                } else {
                    const Err::Err err = validateImplicitCast(ctx, var->value.any, tmpValue.any, var->value.typeKind, tmpValue.typeKind);
                    if (err < 0) return err;
                    // tmp->value.typeKind = tmpDtype;
                }

                tmp->value = tmpValue;

            } else {
                err = var->value.typeKind; // evaluateDataTypes(var);
                if (err < 0) {
                    Diag::report(ctx->unit->ast, var->base.span, Err::UNEXPECTED_SYMBOL, "caused by %i argument", j);
                }
            }

            if (err < 0) {
                return (Err::Err) err;
            }

        }

        return Err::OK;

        // fcnCall->fcn = fcn;

    }

    inline Err::Err validatePointerAssignment(AstContext* ast, const Value* const val) {
        if (val->hasValue && val->u64 == 0) return Err::OK;
        Diag::report(ast, NULL, Err::INVALID_RVALUE, "Only 0 could be assigned to a pointer variable!");
        return Err::INVALID_RVALUE;
    }

    // TODO : naming could be better
    // TODO : use dest/src?
    Err::Err validateImplicitCast(const Type::Kind dtype, const Type::Kind dtypeRef) {
        const bool basicTypes = (dtype != Type::DT_VOID && dtypeRef != Type::DT_VOID) && (dtype <= Type::DT_F64 && dtypeRef <= Type::DT_F64);
        const bool arrayToPointer = (dtype == Type::DT_ARRAY && dtypeRef == Type::DT_POINTER);
        const bool pointerToArray = (dtypeRef == Type::DT_ARRAY && dtype == Type::DT_POINTER);
        const bool sliceToArray = dtype == Type::DT_SLICE && dtypeRef == Type::DT_ARRAY;
        // this case shouldnt ever happen except for 'printf', so we can just check it like this
        const bool stringToString = dtype == Type::DT_STRING && dtypeRef == Type::DT_STRING;

        if (basicTypes && dtype == dtypeRef ||
            dtype == Type::DT_POINTER && dtypeRef == Type::DT_POINTER
        ) {
            return (Err::Err) EXACT_MATCH;
        }

        const bool ans = ((basicTypes || arrayToPointer || pointerToArray || sliceToArray || stringToString));
        return (Err::Err) ((int64_t) ans - 1);
    }

    // uses ctx->tmpArena
    String resolveTypeName(Validator::ValidationContext* ctx, void* type, Type::Kind typeKind) {
        constexpr int bufferSize = 512;

        char* buffer = (char*) Arena::push(&ctx->tmpArena, bufferSize);
        String str = { buffer, bufferSize };

        IO::Stream stream = {
            .kind  = IO::Stream::SK_ARENA,
            .buffer = str
        };

        writeTypeName(&stream, type, typeKind);
        Arena::rollback(&ctx->tmpArena, str.buff - buffer);

        return str;
    }

    bool matchLengths(Array* arrL, Array* arrR) {
        return arrL && arrR && arrL->length && arrR->length &&
               arrL->length->value.hasValue && arrR->length->value.hasValue &&
               arrL->length->value.u64 == arrR->length->value.u64;
    }

    // TODO : unite all dtype->like structures and add spans, so
    //        precise errors can be reported
    // TODO : move from 'ref' convention
    // marks input lvalue array if value is cast non-array-like type -> array
    // TODO : clarify in better words what the thing above means
    Err::Err validateImplicitCast(ValidationContext* ctx, void* dtype, void* dtypeRef, Type::Kind typeKind, Type::Kind typeKindRef) {
        if (validateImplicitCast(typeKind, typeKindRef) >= 0) {
            return Err::OK;
        }

        if (dtype == dtypeRef) return Err::OK;

        if (typeKindRef == Type::DT_ARRAY && typeKind == Type::DT_STRING) {

            Array* arr = (Array*) dtypeRef;
            StringInitialization* str = (StringInitialization*) dtype;

            const int arrDtypeSize = Type::basicTypes[arr->base.pointsToKind].size;
            const int strDtypeSize = Type::basicTypes[str->wideType].size;

            if (arrDtypeSize < strDtypeSize) {
                Diag::report(ctx->unit->ast, NULL, Err::INVALID_TYPE_CONVERSION, "TODO error: ");
                return Err::INVALID_TYPE_CONVERSION;
            }

            if (validateImplicitCast(arr->base.pointsToKind, str->wideType) < 0) {
                Diag::report(ctx->unit->ast, NULL, Err::INVALID_TYPE_CONVERSION, "TODO error: ");
                return Err::INVALID_TYPE_CONVERSION;
            }

            if (strDtypeSize < arrDtypeSize) {
                Diag::report(ctx->unit->ast, NULL, Wrn::SMALLER_DTYPE_CAN_BE_USED);
            }

            return Err::OK;

        } else if (typeKindRef == Type::DT_ARRAY && typeKind == Type::DT_ARRAY) {
            Array* arrL = (Array*) dtypeRef;
            Array* arrR = (Array*) dtype;

            if (arrL->flags & IS_CMP_TIME && arrR->flags & IS_CMP_TIME) {
                // if both compile time, we have to verify lengths
                if (!matchLengths(arrL, arrR)) {
                    Diag::report(ctx->unit->ast, NULL, Err::INVALID_ARRAY_LENGTH,
                        Diag::Format { "Array length mismatch.\n" });
                    return Err::INVALID_ARRAY_LENGTH;
                }
            }

            return validateImplicitCast(ctx,
                arrL->base.pointsTo, arrR->base.pointsTo,
                arrL->base.pointsToKind, arrR->base.pointsToKind);
        } else if (typeKindRef == Type::DT_ARRAY) {
            Array* arr = (Array*) dtypeRef;
            const Type::Kind arrDtype = (Type::Kind) getFirstNonArrayDtype(arr);

            Err::Err err = validateImplicitCast(typeKind, arrDtype);
            if (err != Err::OK) return err;

            arr->flags |= IS_CASTED_FROM_LOWER_LEVEL;
            return Err::OK;
        }

        Arena::Marker marker = Arena::getMarker(&ctx->tmpArena);

        String dtypeName = resolveTypeName(ctx, dtype, typeKind);
        String dtypeRefName = resolveTypeName(ctx, dtypeRef, typeKindRef);
        Diag::report(ctx->unit->ast, NULL, Err::INVALID_TYPE_CONVERSION, dtypeName.len, dtypeName.buff, dtypeRefName.len, dtypeRefName.buff);

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

    void castLiteral(ValidationContext* ctx, Value* val, Type::Kind toDtype) {

        if (val->typeKind == toDtype) return;

        switch (val->typeKind) {

            case Type::DT_U8: {
                uint8_t src = val->u8;
                GENERATE_TARGET_SWITCH(toDtype, src);
                break;
            }

            case Type::DT_U16: {
                uint16_t src = val->u16;
                GENERATE_TARGET_SWITCH(toDtype, src);
                break;
            }

            case Type::DT_U32: {
                uint32_t src = val->u32;
                GENERATE_TARGET_SWITCH(toDtype, src);
                break;
            }

            case Type::DT_U64: {
                uint64_t src = val->u64;
                GENERATE_TARGET_SWITCH(toDtype, src);
                break;
            }

            case Type::DT_I8: {
                int8_t src = val->i8;
                GENERATE_TARGET_SWITCH(toDtype, src);
                break;
            }

            case Type::DT_I16: {
                int16_t src = val->i16;
                GENERATE_TARGET_SWITCH(toDtype, src);
                break;
            }

            case Type::DT_I32: {
                int32_t src = val->i32;
                GENERATE_TARGET_SWITCH(toDtype, src);
                break;
            }

            case Type::DT_I64: {
                int64_t src = val->i64;
                GENERATE_TARGET_SWITCH(toDtype, src);
                break;
            }

            case Type::DT_F32: {
                float src = val->f32;
                GENERATE_TARGET_SWITCH(toDtype, src)
                break;
            }

            case Type::DT_F64: {
                double src = val->f64;
                GENERATE_TARGET_SWITCH(toDtype, src)
                break;
            }

            default: {
                // TODO
                Diag::report(ctx->unit->ast, NULL, Err::UNEXPECTED_ERROR);
                break;
            }

        }

        val->typeKind = toDtype;

    }



    // ================
    //  MISCELLANEOUS

    // level has to be 0, if its output matters
    Type::Kind getFirstNonArrayDtype(Array* arr, const int maxLevel, int* level, void** outTypeData) {
        const Type::Kind typeKind = arr->base.pointsToKind;

        if (
            (maxLevel > 0 && *level >= maxLevel) ||
            (typeKind != Type::DT_ARRAY)
        ) {
            if (outTypeData) *outTypeData = arr->base.pointsTo;
            return typeKind;
        }

        if (level) *level = *level + 1;
        return getFirstNonArrayDtype((Array*) arr->base.pointsTo, maxLevel, level, outTypeData);
    }

}
