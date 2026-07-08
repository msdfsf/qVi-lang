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
    }

    void release(ValidationContext* ctx) {
        DArray::release(&ctx->fCandidates);
        Set::release(&ctx->searchSet);
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

        err = Extern::loadLibrary(ctx, import->fname, Extern::LL_INSPECT, &fcn->lib);
        if (err != Err::OK) return err;

        return Extern::ensureFunctionExists(ctx, fcn->lib, fcn);
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

    struct ScopeIndexEntry {
        String name;
        SyntaxNode* node;
    };

    Err::Err ensureUniqueDefinitions(ValidationContext* ctx, Scope* scope) {
        if (scope->base.flags & IS_UNIQUE) {
            return Err::OK;
        }

        SyntaxNode* firstToCollide;
        SyntaxNode* lastToCompare;

        const uint32_t count = scope->definitionCount;

        if (count <= Config::LINEAR_SEARCH_THRESHOLD) {
            for (int i = 0; i < count; i++) {
                String src = Ast::Node::getName(scope->definitions[i]);

                for (int j = i + 1; j < count; j++) {
                    String dest = Ast::Node::getName(scope->definitions[j]);
                    if (cstrcmp(src, dest)) {
                        lastToCompare = scope->definitions[i];
                        firstToCollide = scope->definitions[j];
                        goto errorReturn;
                    }
                }
            }

            scope->base.flags = IS_UNIQUE;
        } else {
            if (!scope->index) {
                scope->index = (SymbolIndex*) alloc(alc, sizeof(SymbolIndex));
                scope->index->set.hashMethod = Set::HM_STRING_STRUCT_FNV1A;
                scope->index->set.keyOffset = 0;
                // TODO : move table size coef to config?
                Set::init(&scope->index->set, 2 * scope->definitionCount);
            }

            for (int i = 0; i < count; i++) {
                ScopeIndexEntry entry;
                entry.name = Ast::Node::getName(scope->definitions[i]);
                entry.node = scope->definitions[i];

                if (!Set::insert(&scope->index->set, (uint8_t*) &entry)) {
                    lastToCompare = scope->definitions[i];
                    firstToCollide = (SyntaxNode*) Set::find(&scope->index->set, entry.name);
                    goto errorReturn;
                }
            }

            scope->base.flags = IS_UNIQUE;
        }
            
        return Err::OK;
        
        errorReturn:
        // TODO : think of a function to find span of the name
        String name = Ast::Node::getName(firstToCollide);
        Logger::logNoFlush(
            { .level = Logger::Level::ERROR, .tag = ctx->unit->ast->tag },
            "Symbol '%.*s' is already defined in this scope.",
            Ast::Node::getNameSpan(firstToCollide),
            name.len, name.buff
        );

        name = Ast::Node::getName(lastToCompare);
        Logger::logNoFlush(
            { .level = Logger::Level::ERROR, .style = Logger::NO_HEADER, .tag = ctx->unit->ast->tag },
            "Previous definition of '%.*s' is here.",
            Ast::Node::getNameSpan(lastToCompare),
            name.len, name.buff
        );

        Diag::commit(ctx->unit->ast, lastToCompare->span, Err::SYMBOL_ALREADY_DEFINED);

        return Err::SYMBOL_ALREADY_DEFINED;
    }

    // TODO : make sure only one def of the same name exists in scope
    Err::Err validate(ValidationContext* ctx, VariableDefinition* def) {
        Err::Err err;

        err = linkDataType(ctx, def);
        if (err != Err::OK) return err;

        Value leftValue = def->var->value;

        err = validate(ctx, def->var, def->var);
        if (err != Err::OK) return err;

        err = applyImplicitCast(ctx, &leftValue, def->var);
        if (err != Err::OK) return err;

        // In case of static array, we may need to infer length
        // TODO : IS_CMP_TIME to IS_EMBEDED ?
        if (
            leftValue.typeKind == Type::DT_ARRAY &&
            leftValue.arr->flags & IS_CMP_TIME
        ) {
            Variable* len = (Variable*) unwrap(leftValue.arr->length);
            if (len && !len->value.hasValue) {
                // TODO : ??
                return Err::OK;
            }
        }

        def->var->value = leftValue;

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

    Err::Err validate(ValidationContext* ctx, Variable* var, Variable* target) {
        Err::Err err;

        if (!var) return Err::OK;

        if (var->base.semStatus == TS_READY) return Err::OK;
        var->base.semStatus = TS_PENDING;

        if (!var->def) {
            err = linkVariable(ctx, var);
            if (err != Err::OK) return err;

            if (target && var->def == target->def) {
                // usage of the variable in own definition...
                Diag::report(ctx->unit->ast, var->base.span, Err::INVALID_DECLARATION_ORDER);
                return Err::INVALID_DECLARATION_ORDER;
            }
        }

        if (var->def && var != target) {
            err = ensureValidated(ctx, &var->def->base, (SyntaxNode*) var);
            if (err != Err::OK) return err;

            // TODO : dont like this call, think about it more...
            Ast::Node::copyRef(var, var->def->var);
        }
        // TODO : we may want to move this to a validateExpression,
        //        so we can use it directly in validate<VariableDefinition>
        //        then we can drop var != target check... or maybe leave it
        //        for sanity anyway...

        Expression* ex = var->expression;
        if (!ex) {
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

                //
                Function* fcn = call->fcn;

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

                // TODO : ? We may want to compute specific
                //          runtim type info for here...

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

                StringInitialization* init = (StringInitialization*)ex;

                var->value.typeKind = Type::DT_STRING;

                return Err::OK;

            }

            case EXT_ARRAY_INITIALIZATION: {

                ArrayInitialization* init = (ArrayInitialization*)ex;

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
                len->value.hasValue = 1;
                len->value.typeKind = Type::DT_U64;
                len->value.u64 = init->attributeCount;
                var->value.arr->length = len;

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

                int i = 0;
                for (; i < init->attributeCount; i++) {
                    Variable* dest = td->vars[i];
                    Variable* src = init->attributes[i];

                    err = validate(ctx, src, dest);
                    if (err != Err::OK) return err;

                    err = applyImplicitCast(ctx, &dest->value, src);
                    if (err != Err::OK) return err;
                }

                if (init->fillVar) {
                    for (; i < td->varCount; i++) {
                        Variable* dest = td->vars[i];

                        err = validate(ctx, init->fillVar, dest);
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

        var->base.semStatus = TS_READY;
        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, VariableAssignment* ass) {
        Err::Err err;

        err = validate(ctx, ass->lvar, ass->lvar);
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

    Err::Err validate(ValidationContext* ctx, Loop* node) {
        Err::Err err;

        if (!node) return Err::OK;

        if (node->idxDef) {
            err = validate(ctx, node->idxDef);
            if (err != Err::OK) return err;
        }

        if (node->array) {
            err = validate(ctx, node->array);
            if (err != Err::OK) return err;
        }

        if (node->to) {
            err = validate(ctx, node->to);
            if (err != Err::OK) return err;
        }

        Type::Kind sourceKind = node->array ? node->array->value.typeKind : Type::DT_ERROR;

        if (node->to) {
            // CASE: Numeric Range Loop (for i = 0 to 10)
            Type::Kind limitKind = node->to->value.typeKind;

            if (!Type::isInt(sourceKind) || !Type::isInt(limitKind)) {
                Diag::report(ctx->unit->ast, node->base.span, Err::INVALID_DATA_TYPE);
                return Err::INVALID_DATA_TYPE;
            }
        } else {
            if (sourceKind != Type::DT_ARRAY && sourceKind != Type::DT_SLICE) {
                if (sourceKind != Type::DT_ERROR) {
                    Diag::report(ctx->unit->ast, node->array->base.span, Err::UNEXPECTED_ERROR, Diag::Format {
                        "Type '%s' is not iterable."
                    }, Type::str(sourceKind));
                }
            }
        }

        if (node->bodyScope) {
            SyntaxNode* prevLoop = ctx->currentLoop;
            ctx->currentLoop = (SyntaxNode*) node;

            err = validate(ctx, (SyntaxNode*) node->bodyScope);
            if (err != Err::OK) return err;

            ctx->currentLoop = prevLoop;
        }

        return Err::OK;
    }

    Err::Err validate(ValidationContext* ctx, ReturnStatement* node) {
        Err::Err err;

        if (!node) return Err::OK;

        if (!ctx->currentFunction) {
            Diag::report(ctx->unit->ast, node->base.span, Err::UNEXPECTED_ERROR);
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

    // TODO : move to Ast::Node ?
    void ensureIndexedIfNeeded(ValidationContext* ctx, Scope* scope) {
        if (scope->index || scope->definitionCount <= Config::LINEAR_SEARCH_THRESHOLD) {
            return;
        }

        // TODO : I dont think we need to lock the data, as
        //        they have to be already locked, but its better
        //        to think twice...

        scope->index = (SymbolIndex*) alloc(alc, sizeof(SymbolIndex));
        Set::init(&scope->index->set, scope->definitionCount * 2);

        for (uint32_t i = 0; i < scope->definitionCount; i++) {
            SyntaxNode* node = scope->definitions[i];
            String name = Ast::Node::getName(node);
            if (name.buff) {
                Set::insert(&scope->index->set, name, (uint8_t*) node);
            }
        }
    }

    // TODO : move to Ast::Node ?
    SyntaxNode* findSymbol(ValidationContext* ctx, Scope* startScope, String name) {
        Scope* current = startScope;

        while (current) {
            ensureIndexedIfNeeded(ctx, current);

            SyntaxNode* node;
            if (current->index) {
                node = (SyntaxNode*) Set::find(&current->index->set, name);
            } else {
                node = Ast::Find::inArray(current->definitions, current->definitionCount, &name);
            }
            if (node) return node;

            current = current->base.scope;
        }

        return NULL;
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

        err = checkDuplicateNames(ctx);
        if (err != Err::OK) return err;



        AstRegistry* reg = ctx->unit->reg;



        // We basically go through top level nodes that can be exported
        for (int i = 0; i < reg->customDataTypes.size; i++) {
            ensureValidated(ctx, *(SyntaxNode**) DArray::get(&reg->customDataTypes, i));
        }

        for (int i = 0; i < reg->variableDefinitions.size; i++) {
            ensureValidated(ctx, *(SyntaxNode**) DArray::get(&reg->variableDefinitions, i));
        }

        for (int i = 0; i < reg->fcns.size; i++) {
            ensureValidated(ctx, *(SyntaxNode**) DArray::get(&reg->fcns, i));
        }



        for (int i = 0; i < reg->imports.size; i++) {
            validate(ctx, *(SyntaxNode**) DArray::get(&reg->imports, i));
        }


        /*
        for (int i = 0; i < (int) reg->fcnCalls.size; i++) {
            Variable* var = *(Variable**) DArray::get(&ctx->unit->reg->fcnCalls, i);

            err = validateCall(ctx, var);
            if (err != Err::OK) return err;
        }
        */


        for (int i = 0; i < reg->cmpTimeVars.size; i++) {
            Variable* var = *(Variable**) DArray::get(&reg->cmpTimeVars, i);
            err = Interpreter::eval(ctx, var);
            if (err != Err::OK) return err;
        }

        return Err::OK;
    }



    // === Link functions
    //

    Err::Err linkDataType(ValidationContext* ctx, VariableDefinition* def) {
        if (!def->dtype) return Err::OK;

        void** type;
        Type::Kind* typeKind;

        if (def->lastPtr) {
            type = (void**) &(def->lastPtr->pointsTo);
            typeKind = &(def->lastPtr->pointsToKind);
        } else {
            type = (void**) &(def->var->value.any);
            typeKind = &(def->var->value.typeKind);
        }

        // TODO : why path is pointer
        Scope* scope = def->base.scope;
        if (def->dtype && def->dtype->path && def->dtype->path->len > 0) {
            Namespace* nspace = NULL;
            ErrorSet* eset = NULL;
            const Err::Err err = validateQualifiedName(ctx, scope, def->dtype, &nspace, &eset);
            if (err != Err::OK) return err;
            if (eset) return Err::OK;
            scope = (Scope*) nspace;
        }

        SyntaxNode* node = Ast::Find::inScope(scope, (const String*) def->dtype);

        if (!node) {
            Diag::report(ctx->unit->ast, def->base.span, Err::UNKNOWN_DATA_TYPE);
            return Err::UNKNOWN_DATA_TYPE;
        }

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
                Diag::report(ctx->unit->ast, def->base.span, Err::UNEXPECTED_ERROR);
                return Err::UNEXPECTED_ERROR;
            }
        }

        return Err::OK;
    }

    Err::Err linkErrorSet(ValidationContext* ctx, Function* fcn) {
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

        return Err::OK;
    }

    Err::Err linkVariable(ValidationContext* ctx, Variable* var) {
        // in case of scopeNames var is the last name (ex. point.x, var is then x)
        // scopeNames are sorted from left to right as written
        if (var->name.len == 0) return Err::OK;

        Variable* tmpVar;

        Scope* scope = var->base.scope;
        if (var->name.pathSize > 0) {
            Namespace* nspace = NULL;
            ErrorSet* eset = NULL;
            const Err::Err err = validateQualifiedName(ctx, scope, &var->name, &nspace, &eset);
            if (err != Err::OK) return err;
            if (eset) {
                tmpVar = Ast::Find::inArray(eset->vars, eset->varCount, (String*) &var->name);
                if (!tmpVar) {
                    Diag::report(ctx->unit->ast, var->base.span, Err::UNKNOWN_ERROR_SET);
                    return Err::UNKNOWN_ERROR_SET;
                }
                Ast::Node::copyRef(var, tmpVar);
                return Err::OK;
            }
            scope = (Scope*) nspace;

            // if namespace, we dont care about order

            SyntaxNode* node = Ast::Find::inArray(scope->children, scope->childrenCount, (String*) &var->name);
            if (node && node->type == NT_VARIABLE) {
                Ast::Node::copyRef(var, (Variable*) node);
                return Err::OK;
            }

        }

        tmpVar = findDefinition(ctx, scope, &var->name, var->base.definitionIdx);
        if (tmpVar) {
            Ast::Node::copyRef(var, tmpVar);
            return Err::OK;
        }

        tmpVar = Ast::Find::inArray(Ast::Internal::variables, Ast::Internal::IV_COUNT, (String*) &var->name);
        if (tmpVar) {
            Ast::Node::copyRef(var, tmpVar);
            return Err::OK;
        }

        SyntaxNode* node = Ast::Find::inScope(scope, (String*) &var->name);
        if (!node) {
            Diag::report(ctx->unit->ast, var->base.span, Err::UNKNOWN_VARIABLE, var->name.len, var->name.buff);
            return Err::UNKNOWN_VARIABLE;
        }

        switch (node->type) {
            case NT_ENUMERATOR: {
                var->value.typeKind = Type::DT_ENUM;
                var->value.enm = (Enumerator*) node;
                break;
            }

            case NT_FUNCTION: {
                var->value.typeKind = Type::DT_FUNCTION;
                var->value.fcn = NULL;
                break;
            }

            case NT_TYPE_DEFINITION: {
                Err::Err err = validate(ctx, (TypeDefinition*) node);
                if (err != Err::OK) return err;

                var->value.i64 = ((TypeDefinition*) node)->typeInfo->base.size;
                var->value.hasValue = true;
                var->value.typeKind = Type::DT_I64;

                break;
            }

            default: {
                Diag::report(ctx->unit->ast, var->base.span, Err::UNEXPECTED_ERROR);
                return Err::UNEXPECTED_ERROR;
            }
        }

        return Err::OK;
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
        FunctionCall* call = (FunctionCall*) (callOp->expression);
        if (call->fcn) return Err::OK;

        Function* fcn;
        const int err = findClosestFunction(ctx, callOp, &fcn);
        if (err < 0) {
            // check for function pointer
            Variable* var = findDefinition(ctx, callOp->base.scope, &call->name, callOp->base.definitionIdx);
            if (!var) {
                Diag::report(ctx->unit->ast, callOp->base.span, (Err::Err) err);
                return (Err::Err) err;
            }

            call->fptr = var;
            call->fcn = NULL;
            call->outArg = new Variable();
            call->outArg->value.hasValue = false;
            call->outArg->value.typeKind =
                var->value.fcn->outArg->var->value.typeKind;

            return Err::OK;
        }

        call->fptr = NULL;
        call->fcn = fcn;
        call->outArg = new Variable();
        call->outArg->value.hasValue = false;
        call->outArg->value.typeKind =
            fcn->prototype.outArg->var->value.typeKind;

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

    Err::Err checkDuplicateNames(ValidationContext* ctx) {
        DArray::Container* scopes = &ctx->unit->reg->scopes;

        for (int i = 0; i < scopes->size; i++) {
            Scope* const sc = *(Scope**) DArray::get(scopes, i);
            const uint32_t count = sc->definitionCount;

            if (count < 2) continue;
            if (count <= Config::LINEAR_SEARCH_THRESHOLD) {
                for (uint32_t i = 0; i < count; i++) {
                    SyntaxNode* node = sc->definitions[i];

                    String name = Ast::Node::getName(node);
                    if (name.len == 0) continue;

                    if (Ast::Find::inArray(sc->definitions, i, &name)) {
                        Diag::report(ctx->unit->ast, node->span, Err::SYMBOL_ALREADY_DEFINED);
                        return Err::SYMBOL_ALREADY_DEFINED;
                    }
                }

                continue;
            }

            for (uint32_t i = 0; i < sc->definitionCount; i++) {
                SyntaxNode* node = sc->definitions[i];

                String name = Ast::Node::getName(node);
                if (name.len == 0) continue;

                if (!Set::insert(&ctx->searchSet, (uint8_t*) name.buff)) {
                    Diag::report(ctx->unit->ast, node->span, Err::SYMBOL_ALREADY_DEFINED);
                    return Err::SYMBOL_ALREADY_DEFINED;
                }
            }

            Set::clear(&ctx->searchSet);
        }

        return Err::OK;
    }







    // ======================
    // TYPE RESOLUTION STUFF

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

            Type::TypeInfo* mInfo = NULL;
            Type::Kind typeKind = var->value.typeKind;

            if (Type::isPrimitive(typeKind)) {

                mInfo = Type::basicTypes + typeKind;

            } else if (typeKind == Type::DT_CUSTOM) {

                computeTypeInfo(ctx, td);
                mInfo = (Type::TypeInfo*) td->typeInfo;

            } else if (typeKind == Type::DT_ARRAY) {

                // if length is NULL, we are basicly dealing with
                // a runtime length, and we can interpret our dtype
                // as slice
                /*
                if (!arr->length) {
                    *size = 16;
                    *align = 8;
                    return Err::OK;
                }

                arr->length = unwrap(arr->length);
                const uint64_t len = arr->length->value.u64;

                uint64_t elementSize;
                uint64_t elementAlign;
                Value tmpVal = toValue(&arr->base);
                // TODO : cache
                err = getDtypeInfo(&tmpVal, &elementSize, &elementAlign);
                if (err != Err::OK) return err;

                *size = len * elementSize;
                *align = elementAlign;
                */
            } else {

                return Err::NOT_YET_IMPLEMENTED;

            }

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

        return Err::OK;
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

        if (rvar->value.hasValue) {
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

        // clone the current node to preserve also metadata
        // TODO: maybe no need to do a full copy
        Variable* innerOperand = Ast::Node::copy(rvar);

        Cast* castEx = Ast::Node::makeCast();
        castEx->operand = innerOperand;

        if (lval->typeKind != Type::DT_ARRAY) {
            castEx->target = lval->typeKind;
        } else {
            castEx->target = lval->arr->base.pointsToKind;
        }

        rvar->expression = (Expression*) castEx;
        rvar->value = *lval;

        return Err::OK;
    }

    inline void inheritType(Variable* dest, Value* source) {
        if (isPrimitive(source->typeKind)) {
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

            Diag::report(ctx->unit->ast, td->base.span, Err::INVALID_TYPE_CONVERSION, "Invalid type for member selection!", bex->right->base.span, bex->right->name.len);
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

        var->value.typeKind = attribute->value.typeKind;
        var->value.any = attribute->value.any;

        return Err::OK;

        /*
        if (dtypeA == DT_ENUM) {

            Enumerator* en = parent->value.enm;
            Variable* var = ctx->reg->Find.inArray(&en->vars, memberName);
            if (!var) {
                Diag::report(unit->ast, "Unable to find member of enum!", member->base.span, 0);
                return Err::UNEXPECTED_SYMBOL;
            }

            return Err::OK;

        }

        if (dtypeA == DT_POINTER) {

            Variable* var = (Variable*) bex->right;
            Pointer* ptr = (Pointer*) customDtypeA;

            if (ptr->pointsToEnum != DT_CUSTOM || !ptr->pointsTo) {
                Diag::report(unit->ast, "Invalid type of dereferenced pointer for member selection!", var->base.span, var->name.len);
                return Err::INVALID_TYPE_CONVERSION;
            }

            bex->base.opType = OP_DEREFERENCE_MEMBER_SELECTION;
            customDtypeA = (TypeDefinition*)(ptr->pointsTo);

        } else if (dtypeA != DT_CUSTOM) {
            Diag::report(unit->ast, "Invalid type for member selection!", bex->right->base.span, bex->right->name.len);
            return Err::INVALID_TYPE_CONVERSION;
        }

        Variable* var = (Variable*)bex->right;
        Variable* ans = ctx->reg->Find.inArray(&customDtypeA->vars, (String*)&var->name);
        if (!ans) {
            Diag::report(unit->ast, Err::str(Err::INVALID_ATTRIBUTE_NAME), var->base.span, var->name.len, var->name.len, var->name.buff);
            return Err::INVALID_ATTRIBUTE_NAME;
        }

        ctx->reg->Node.copy(var, ans);

        rdtype = ans->value.typeKind;
        break;
        */
    }

    Err::Err resolveResultType(ValidationContext* ctx, BinaryExpression* bex, Variable* var) {

        Type::Kind lDtype = bex->left->value.typeKind;
        Type::Kind rDtype = bex->right->value.typeKind;

        // Usually the result type can be derived from
        // operands ranks but in few cases operator can
        // influence the output type (arr[i])
        if (bex->base.opType == OP_SUBSCRIPT) {

            // TODO : move from this to a direct check, as we
            //   may need different behavior for each type later
            if (!isIndexable(lDtype)) {
                // TODO : throw error
            }

            Pointer* ptr = bex->left->value.ptr;
            var->value.any = ptr->pointsTo;
            var->value.typeKind = ptr->pointsToKind;

            return Err::OK;

        } else if (bex->base.opType == OP_MEMBER_SELECTION) {

            // we also may want to change the operator to
            // OP_DEREFERENCE_MEMEBER_SELECTION here if needed,
            // so its simpler to compile
            resolveMember(ctx, bex, var);
            return Err::OK;

        }

        // TODO:
        // validateImplicitCast(lDtype, rDtype);

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

    // assuming size > 0
    Err::Err validateQualifiedName(ValidationContext* ctx, Scope* sc, QualifiedName* name, Namespace** nspace, ErrorSet** eset) {
        Namespace* tmpNspace = NULL;
        INamed* names = name->path;
        // INamedLoc* names = (INamedLoc*) namesContainer->base.buffer;

        int i = 0;
        const int len = name->pathSize;

        for (; i < len; i++) {

            String name = *((String*) (names + i));
            tmpNspace = Ast::Find::inScopeNamespace(sc, &name);

            if (!tmpNspace) {

                ErrorSet* tmpEset = Ast::Find::inScopeErrorSet(sc, &name);
                if (!tmpEset) {

                    // check implicit errors
                    // TODO

                    /*
                    if (sc->fcn && sc->fcn->errorSet) {
                        tmpEset = sc->fcn->errorSet;
                    } else {
                        Diag::report(unit->ast, Err::str(Err::UNKNOWN_NAMESPACE), sc->base.span, name.len, name.buff);
                        return Err::UNKNOWN_NAMESPACE;
                    }
                    */

                }

                // *eset = tmpEset;

                // need to test that other fields align with error set
                i++;
                for (; i < len; i++) {

                    Variable* tmp = Ast::Find::inArray(tmpEset->vars, tmpEset->varCount, &name);
                    if (!tmp) {
                        Diag::report(ctx->unit->ast, tmpEset->base.span, Err::UNKNOWN_NAMESPACE);
                        return Err::UNKNOWN_NAMESPACE;
                    }

                    if (tmp->value.typeKind == Type::DT_ERROR && !(tmp->value.hasValue)) {
                        tmpEset = tmp->value.err;
                    } else if (i + 1 < len) {
                        Diag::report(ctx->unit->ast, tmpEset->base.span, Err::UNKNOWN_ERROR_SET, tmp->base.span);
                        return Err::UNKNOWN_ERROR_SET;
                    }

                }

                *eset = tmpEset;

                break;

            }

            sc = (Scope*) tmpNspace;

        }

        *nspace = (Namespace*) tmpNspace;
        return Err::OK;
    }

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

    Err::Err validateImplicitCast(ValidationContext* ctx, void* dtype, void* dtypeRef, Type::Kind typeKind, Type::Kind typeKindRef) {
        if (validateImplicitCast(typeKind, typeKindRef) >= 0) {
            return Err::OK;
        }

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

            Array* arrRef = (Array*) dtypeRef;
            Array* arr = (Array*) dtype;

            int levelRef = 0;
            const Type::Kind arrDtypeRef = (Type::Kind) getFirstNonArrayDtype(arrRef, -1, &levelRef);

            const int maxLevel = levelRef;
            levelRef = 0;
            const Type::Kind arrDtype = (Type::Kind) getFirstNonArrayDtype(arr, maxLevel, &levelRef);

            return validateImplicitCast(arrDtype, arrDtypeRef);


        } else if (typeKindRef == Type::DT_ARRAY) {

            Array* arr = (Array*) dtypeRef;
            const Type::Kind arrDtype = (Type::Kind) getFirstNonArrayDtype(arr);

            return validateImplicitCast(typeKind, arrDtype);

        }

        Diag::report(ctx->unit->ast, NULL, Err::INVALID_TYPE_CONVERSION, "tmp", "tmp");
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

    // TODO
    int computeSizeOfDataType(Reg::Unit* unit, TypeDefinition* const def) {

        if (def->typeInfo->base.size > 0) return def->typeInfo->base.size;

        const int isUnion = def->base.type == NT_UNION;

        int accSize = 0;
        for (int i = 0; i < def->varCount; i++) {

            Variable* const var = def->vars[i];

            int size = 0;
            switch (var->value.typeKind) {

                case Type::DT_CUSTOM: {
                    size = computeSizeOfDataType(unit, var->value.def);
                    break;
                }

                case Type::DT_ARRAY: {

                    Array* const arr = var->value.arr;
                    // evaluate(arr->length);

                    if (!(arr->length->value.hasValue)) {
                        Diag::report(unit->ast, var->base.span, Err::COMPILE_TIME_KNOWN_EXPRESSION_REQUIRED, "Was unable to compute array length while computing size of the %.*s!");
                        return Err::COMPILE_TIME_KNOWN_EXPRESSION_REQUIRED;
                    }

                    int chunkSize = 0;
                    if (arr->base.pointsToKind == Type::DT_CUSTOM) {
                        chunkSize = computeSizeOfDataType(unit, (TypeDefinition*) arr->base.pointsTo);
                    } else {
                        chunkSize = Type::basicTypes[arr->base.pointsToKind].size;
                    }

                    size = chunkSize * arr->length->value.i64;

                    break;
                }

                default: {
                    size = Type::basicTypes[var->def->var->value.typeKind].size;
                    break;
                }

            }

            if (isUnion) accSize = size > accSize ? size : accSize;
            else accSize += size;

        }

        def->typeInfo->base.size = accSize;
        return accSize;

    }

    // level has to be 0, if its output matters
    int getFirstNonArrayDtype(Array* arr, const int maxLevel, int* level) {

        const int dtype = arr->base.pointsToKind;

        if (maxLevel > 0 && *level >= maxLevel) return dtype;
        if (dtype != Type::DT_ARRAY) return dtype;

        if (level) *level = *level + 1;
        return getFirstNonArrayDtype((Array*) arr->base.pointsTo, maxLevel, level);

    }


    Variable* findErrorInErrorSet(ErrorSet* eset, QualifiedName* var) {

        Namespace* tmpNspace;

        int i = 0;
        const int len = var->pathSize;

        for (i = 0; i < len; i++) {

            INamed* nm = var->path + i;
            Variable* tmp = Ast::Find::inArray(eset->vars, eset->varCount, nm);
            if (!tmp) return NULL;

            eset = tmp->value.err;

        }

        return Ast::Find::inArray(eset->vars, eset->varCount, (String*) &var);

    }

    // scope cannot be NULL
    Variable* findDefinition(ValidationContext* ctx, Scope* scope, QualifiedName* const inVar, int idx) {

        const String name = { inVar->buff, (uint64_t) inVar->len };
        // int idx = inVar->parentIdx;

        while (scope) {

            SyntaxNode* node = Ast::Find::inArray(scope->definitions, scope->definitionCount, (String*) &name);
            if (node) {

                if (
                    node->definitionIdx < idx ||
                    scope->base.flags & IS_UNORDERED
                ) {
                    switch (node->type) {

                        case NT_VARIABLE_DEFINITION : {
                            return ((VariableDefinition*) node)->var;
                            //Variable* var = (Variable*) node;
                            //if (Utils::match(var, inVar)) return var;
                        }

                        default: {

                        }

                    }
                } else {
                    // Declaration after usage
                    Diag::report(ctx->unit->ast, node->span, Err::DECLARATION_AFTER_USE);
                    return NULL;
                }

            } else {

                // TODO
                /*
                for (int i = 0; i < scope->usings.size; i++) {

                    Using* usng = (Using*) DArray::get(&scope->usings.base, i);
                    if (usng->var->type != NT_FUNCTION) continue;

                    Variable* err = findErrorInErrorSet(((Function*) (usng->var))->errorSet, inVar);
                    if (err) return err;

                }
                */

            }

            idx = scope->base.definitionIdx;
            scope = scope->base.scope;

        }

        return NULL;

    }

    inline int isUniqueInCollection(Reg::Unit* unit, DArray::Container* collection, INamed* named, Span* span, MemberOffset mName) {

        for (int i = 0; i < collection->size; i++) {

            SyntaxNode* const item = (SyntaxNode*) DArray::get(collection, i);
            if (item->flags & IS_UNIQUE) continue;

            INamed* itemName = (INamed*) getMember(item, mName);
            if (Strings::compare(*named, *itemName)) {
                Diag::report(unit->ast, span, Err::SYMBOL_ALREADY_DEFINED, named->len);
                return Err::SYMBOL_ALREADY_DEFINED;
            }
            //item->flags |= IS_UNIQUE;

        }

        return Err::OK;

    }

    int match(FunctionPrototype* const fptrA, FunctionPrototype* const fptrB) {

        if (fptrA->inArgCount != fptrB->inArgCount) return 0;

        for (int i = 0; i < fptrA->inArgCount; i++) {
            const Type::Kind defA = fptrA->inArgs[i]->var->value.typeKind;
            const Type::Kind defB = fptrB->inArgs[i]->var->value.typeKind;
            if (defA != defB) return 0;
        }

        return 1;

    }

    Function* findExactFunction(Scope* scope, INamed* const name, FunctionPrototype* const fptr) {

        while (scope) {

            if (scope->base.type != NT_NAMESPACE) {
                continue;
            }

            Namespace* nspace = (Namespace*) scope;
            Function* fcns    = ((Function*) nspace->fcns);

            for (int i = 0; i < nspace->fcnCount; i++) {
                if (!Strings::compare((String*) &(fcns + i)->name, name) || !match(fptr, &(fcns + i)->prototype)) {
                    continue;
                }
                return fcns + i;
            }

            scope = scope->base.scope;

        }

        return NULL;

    }

    // working with global array fCandidates
    void findCandidateFunctions(ValidationContext* ctx, Scope* scope, FunctionCall* call) {

        DArray::clear(&ctx->fCandidates);

        const String callName = { call->name.buff, call->name.len };

        if (call->name.pathSize > 0) {
            Namespace* nspace = NULL;
            ErrorSet* eset = NULL;

            Logger::mute = 1;
            const Err::Err err = validateQualifiedName(ctx, scope, &call->name, &nspace, &eset);
            Logger::mute = 0;

            if (err < 0 || eset || !nspace) return;
            scope = (Scope*) nspace;
        }

        while (scope) {

            for (int i = 0; i < scope->childrenCount; i++) {
                if (!scope->children[i] || scope->children[i]->type != NT_FUNCTION) {
                    continue;
                }

                Function* fcn = (Function*) scope->children[i];
                const String fcnName = { fcn->name.buff, fcn->name.len };

                if (!cstrcmp(callName, fcnName)) {
                    continue;
                }

                const int fcnInCnt = fcn->prototype.inArgCount;
                const int callInCnt = call->inArgCount;
                const int lastArgDtype = fcnInCnt > 0 ? 0 : 0;// TODO (fcns + i)->prototype.inArgs[fcnInCnt - 1]->var->value.typeKind : DT_UNDEFINED;

                if (
                    fcnInCnt == callInCnt ||
                    (fcnInCnt < callInCnt && lastArgDtype == Type::DT_MULTIPLE_TYPES)
                ) {
                    FunctionScore tmp = { fcn, 0 };
                    DArray::push(&ctx->fCandidates, &tmp);
                }
            }

            scope = scope->base.scope;

        }

    }

    int findClosestFunction(ValidationContext* ctx, Variable* callOp, Function** outFcn) {

        Scope* scope = callOp->base.scope;
        FunctionCall* call = (FunctionCall*) callOp->expression;
        const int callInCnt = call->inArgCount;

        findCandidateFunctions(ctx, scope, call);

        for (int j = 0; j < ctx->fCandidates.size; j++) {

            Function* fcn = ((FunctionScore*) DArray::get(&ctx->fCandidates, j))->fcn;
            ensureValidated(ctx, (SyntaxNode*) fcn, (SyntaxNode*) callOp);

            int score = (fcn->prototype.inArgCount == 0) ? 100 : 0;
            const int fcnInCnt = fcn->prototype.inArgCount;

            int k = 0;
            for (int i = 0; i < fcn->prototype.inArgCount; i++) {

                if (k >= callInCnt) {
                    score = 0;
                    break;
                }

                Variable* fArg = fcn->prototype.inArgs[i]->var;
                Variable* cArg = call->inArgs[k];

                k++;

                const int fDtype = fArg->value.typeKind;
                const int cDtype = cArg->value.typeKind;// evaluateDataTypes(cArg);

                if (
                    (fDtype == Type::DT_CUSTOM && cDtype != Type::DT_CUSTOM) ||
                    (fDtype != Type::DT_CUSTOM && cDtype == Type::DT_CUSTOM)
                    ) {
                    score = 0;
                    break;
                }
                else if (fDtype == Type::DT_CUSTOM) {
                    if (fArg->value.def != cArg->value.def) {
                        score = 0;
                        break;
                    }
                }

                if (fDtype == Type::DT_MULTIPLE_TYPES) {
                    break;
                }

                if (fDtype == cDtype) {
                    score += FOS_EXACT_MATCH;
                    continue;
                }

                if (Type::isSignedInt(cDtype) && Type::isSignedInt(fDtype)) {
                    if (cDtype < fDtype) {
                        score += FOS_PROMOTION;
                    }
                    else {
                        score += FOS_SIZE_DECREASE;
                    }
                    continue;
                }

                if (Type::isUnsignedInt(cDtype) && Type::isUnsignedInt(fDtype)) {
                    if (cDtype < fDtype) {
                        score += FOS_PROMOTION;
                    }
                    else {
                        score += FOS_SIZE_DECREASE;
                    }
                    continue;
                }

                if (
                    Type::isSignedInt(cDtype) && Type::isUnsignedInt(fDtype) ||
                    Type::isSignedInt(cDtype) && Type::isUnsignedInt(fDtype)
                ) {

                    if ((cDtype - fDtype <= Type::DT_U64 - Type::DT_I64)) {
                        score += FOS_SIGN_CHANGE;
                    } else {
                        score += FOS_SIZE_DECREASE;
                    }

                    continue;

                }

                if (Type::isFloat(cDtype) && Type::isFloat(fDtype)) {
                    if ((cDtype - fDtype <= Type::DT_U64 - Type::DT_I64)) {
                        score += FOS_PROMOTION;
                    }
                    else {
                        score += FOS_SIZE_DECREASE;
                    }
                    continue;
                }

                if (Type::isInt(cDtype) && Type::isFloat(fDtype)) {
                    score += FOS_SIGN_CHANGE;
                    continue;
                }

                if (Type::isFloat(cDtype) && Type::isInt(fDtype)) {
                    score += FOS_SIZE_DECREASE;
                    continue;
                }

                if (validateImplicitCast((Type::Kind)cDtype, (Type::Kind)fDtype)) {
                    score += FOS_IMPLICIT_CAST;
                    continue;
                }

                if (fDtype == Type::DT_ARRAY && cDtype == Type::DT_STRING) {
                    // TODO: for now as quick patch
                    score += FOS_IMPLICIT_CAST;
                    continue;
                }

                break;

            }

            if (score != 0) {
                FunctionScore* fscore = (FunctionScore*) ctx->fCandidates.buffer;
                fscore[j].score = score;
            } else {
                FunctionScore* tmp = (FunctionScore*) DArray::get(&ctx->fCandidates, ctx->fCandidates.size - 1);
                DArray::set(&ctx->fCandidates, j, tmp);
                DArray::pop(&ctx->fCandidates);
                j--;
            }

        }

        int bestIdx = 0;
        int bestScore = 0;
        int sameScoreCnt = 0;
        for (int i = 0; i < ctx->fCandidates.size; i++) {
            const int score = ((FunctionScore*) DArray::get(&ctx->fCandidates, i))->score;
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            } else if (score == bestScore) {
                sameScoreCnt++;
                continue;
            }
            sameScoreCnt = 0;
        }

        if (bestScore > 0 && sameScoreCnt == 0) {
            *outFcn = ((FunctionScore*) DArray::get(&ctx->fCandidates, bestIdx))->fcn;
            return Err::OK;
        } else {

            if (bestScore <= 0 ) {
                //Diag::report(unit->ast, Err::str(Err::NO_MATCHING_FUNCTION_FOUND));
                return Err::NO_MATCHING_FUNCTION_FOUND;
            }

            //Diag::report(unit->ast, Err::str(Err::MORE_THAN_ONE_OVERLOAD_MATCH), callOp->span, 1);
            return Err::MORE_THAN_ONE_OVERLOAD_MATCH;

        }

        return Err::OK;

    }

}
