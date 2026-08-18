// interpreter related code that focuses
// on 'ast' walking evaluation

#include "data_types.h"
#include "diagnostic.h"
#include "foreign_code.h"
#include "interpreter.h"
#include "operators.h"
#include "syntax.h"
#include "task_system.h"
#include "validator.h"



#define TO_PVOID(var) ((void*) *((uint64_t*) &var))

namespace Interpreter {

    template<Type::Kind>
    struct DataTypeToCppType;

    template<>
    struct DataTypeToCppType<Type::DT_I8> {
        using type = int8_t;
        static constexpr auto member = &Value::i8;
    };

    template<>
    struct DataTypeToCppType<Type::DT_I16> {
        using type = int16_t;
        static constexpr auto member = &Value::i16;
    };

    template<>
    struct DataTypeToCppType<Type::DT_I32> {
        using type = int32_t;
        static constexpr auto member = &Value::i32;
    };

    template<>
    struct DataTypeToCppType<Type::DT_I64> {
        using type = int64_t;
        static constexpr auto member = &Value::i64;
    };

    template<>
    struct DataTypeToCppType<Type::DT_U8> {
        using type = uint8_t;
        static constexpr auto member = &Value::u8;
    };

    template<>
    struct DataTypeToCppType<Type::DT_U16> {
        using type = uint16_t;
        static constexpr auto member = &Value::u16;
    };

    template<>
    struct DataTypeToCppType<Type::DT_U32> {
        using type = uint32_t;
        static constexpr auto member = &Value::u32;
    };

    template<>
    struct DataTypeToCppType<Type::DT_U64> {
        using type = uint64_t;
        static constexpr auto member = &Value::u64;
    };

    template<>
    struct DataTypeToCppType<Type::DT_F32> {
        using type = float;
        static constexpr auto member = &Value::f32;
    };

    template<>
    struct DataTypeToCppType<Type::DT_F64> {
        using type = double;
        static constexpr auto member = &Value::f64;
    };

    template<Type::Kind TargetType>
    inline void cast(Value* value) {
        using T = typename DataTypeToCppType<TargetType>::type;
        auto M = DataTypeToCppType<TargetType>::member;

        switch (value->type->kind) {
            case Type::DT_I8:     value->*M = static_cast<T>(value->i8);  break;
            case Type::DT_I16:    value->*M = static_cast<T>(value->i16); break;
            case Type::DT_I32:    value->*M = static_cast<T>(value->i32); break;
            case Type::DT_I64:    value->*M = static_cast<T>(value->i64); break;
            case Type::DT_U8:     value->*M = static_cast<T>(value->u8);  break;
            case Type::DT_U16:    value->*M = static_cast<T>(value->u16); break;
            case Type::DT_U32:    value->*M = static_cast<T>(value->u32); break;
            case Type::DT_U64:    value->*M = static_cast<T>(value->u64); break;
            case Type::DT_F32:    value->*M = static_cast<T>(value->f32); break;
            case Type::DT_F64:    value->*M = static_cast<T>(value->f64); break;
            default:
                return; // TODO

        }

        value->type->kind = TargetType;
    }

    // There doesn't seem to be a clever trick to handle all casts
    // using some generic operation or bit-level hack.
    // So we're going with a straightforward double look-up approach.
    // A single look-up would be faster, but it's less readable and modular.
    // This way, we can still use specific cast functions in 'known-context'.
    void cast(Value* value, Type::Kind dtype) {

        switch (dtype) {

            case Type::DT_I8 :    cast<Type::DT_I8>(value); return;
            case Type::DT_I16 :   cast<Type::DT_I16>(value); return;
            case Type::DT_I32 :   cast<Type::DT_I32>(value); return;
            case Type::DT_I64 :   cast<Type::DT_I64>(value); return;
            case Type::DT_U8 :    cast<Type::DT_U8>(value); return;
            case Type::DT_U16 :   cast<Type::DT_U16>(value); return;
            case Type::DT_U32 :   cast<Type::DT_U32>(value); return;
            case Type::DT_U64 :   cast<Type::DT_U64>(value); return;
            case Type::DT_F32 :   cast<Type::DT_F32>(value); return;
            case Type::DT_F64 :   cast<Type::DT_F64>(value); return;
            case Type::DT_POINTER :   /* TODO */ return;
            case Type::DT_ARRAY :     /* TODO */ return;
            case Type::DT_SLICE :     /* TODO */ return;
            case Type::DT_STRUCT :    /* TODO */ return;
            case Type::DT_ENUM :      /* TODO */ return;
            case Type::DT_UNION :     /* TODO */ return;
            default: return; // TODO

        }

    }



    inline void applyUnaryOperatorPlusI32(Value* value) {}
    inline void applyUnaryOperatorPlusI64(Value* value) {}
    inline void applyUnaryOperatorPlusF32(Value* value) {}
    inline void applyUnaryOperatorPlusF64(Value* value) {}



    inline void applyUnaryOperatorMinusI32(Value* value) {
        value->i32 = -value->i32;
    }

    inline void applyUnaryOperatorMinusI64(Value* value) {
        value->i64 = -value->i64;
    }

    inline void applyUnaryOperatorMinusF32(Value* value) {
        value->f32 = -value->f32;
    }

    inline void applyUnaryOperatorMinusF64(Value* value) {
        value->f64 = -value->f64;
    }



    inline void applyBinaryOperatorAdditionI32(Value* a, Value* b) {
        cast<Type::DT_I32>(b);
        a->i32 = a->i32 + b->i32;
    }

    inline void applyBinaryOperatorAdditionI64(Value* a, Value* b) {
        cast<Type::DT_I64>(b);
        a->i64 = a->i64 + b->i64;
    }

    inline void applyBinaryOperatorAdditionF32(Value* a, Value* b) {
        cast<Type::DT_F32>(b);
        a->f32 = a->f32 + b->f32;
    }

    inline void applyBinaryOperatorAdditionF64(Value* a, Value* b) {
        cast<Type::DT_F64>(b);
        a->f64 = a->f64 + b->f64;
    }

    inline void applyBinaryOperatorAdditionArray(Value* a, Value* b) {
    }

    inline void applyBinaryOperatorSubtractionArray(Value* a, Value* b) {
    }



    inline void applyBinaryOperatorSubtractionI32(Value* a, Value* b) {
        cast<Type::DT_I32>(b);
        a->i32 = a->i32 - b->i32;
    }

    inline void applyBinaryOperatorSubtractionI64(Value* a, Value* b) {
        cast<Type::DT_I64>(b);
        a->i64 = a->i64 - b->i64;
    }

    inline void applyBinaryOperatorSubtractionF32(Value* a, Value* b) {
        cast<Type::DT_F32>(b);
        a->f32 = a->f32 - b->f32;
    }

    inline void applyBinaryOperatorSubtractionF64(Value* a, Value* b) {
        cast<Type::DT_F64>(b);
        a->f64 = a->f64 - b->f64;
    }


    inline void applyBinaryOperatorMultiplicationI32(Value* a, Value* b) {
        cast<Type::DT_I32>(b);
        a->i32 = a->i32 * b->i32;
    }

    inline void applyBinaryOperatorMultiplicationI64(Value* a, Value* b) {
        cast<Type::DT_I64>(b);
        a->i64 = a->i64 * b->i64;
    }

    inline void applyBinaryOperatorMultiplicationF32(Value* a, Value* b) {
        cast<Type::DT_F32>(b);
        a->f32 = a->f32 * b->f32;
    }

    inline void applyBinaryOperatorMultiplicationF64(Value* a, Value* b) {
        cast<Type::DT_F64>(b);
        a->f64 = a->f64 * b->f64;
    }



    inline void applyBinaryOperatorDivisionI32(Value* a, Value* b) {
        cast<Type::DT_I32>(b);
        a->i32 = a->i32 / b->i32;
    }

    inline void applyBinaryOperatorDivisionI64(Value* a, Value* b) {
        cast<Type::DT_I64>(b);
        a->i64 = a->i64 / b->i64;
    }

    inline void applyBinaryOperatorDivisionF32(Value* a, Value* b) {
        cast<Type::DT_F32>(b);
        a->f32 = a->f32 / b->f32;
    }

    inline void applyBinaryOperatorDivisionF64(Value* a, Value* b) {
        cast<Type::DT_F64>(b);
        a->f64 = a->f64 / b->f64;
    }



    inline void applyBinaryOperatorModuloI32(Value* a, Value* b) {
        cast<Type::DT_I32>(b);
        a->i32 = a->i32 % b->i32;
    }

    inline void applyBinaryOperatorModuloI64(Value* a, Value* b) {
        cast<Type::DT_I64>(b);
        a->i64 = a->i64 % b->i64;
    }



    inline void applyBinaryOperatorEqual(Value* a, Value* b) {
        a->i64 = a->i64 == b->i64;
    }

    inline void applyBinaryOperatorNotEqual(Value* a, Value* b) {
        a->i64 = a->i64 != b->i64;
    }

    inline void applyBinaryOperatorLessThan(Value* a, Value* b) {
        a->i64 = a->i64 < b->i64;
    }

    inline void applyBinaryOperatorGreaterThan(Value* a, Value* b) {
        a->i64 = a->i64 > b->i64;
    }

    inline void applyBinaryOperatorLessThanOrEqual(Value* a, Value* b) {
        a->i64 = a->i64 <= b->i64;
    }

    inline void applyBinaryOperatorGreaterThanOrEqual(Value* a, Value* b) {
        a->i64 = a->i64 >= b->i64;
    }

    inline void applyBinaryOperatorBoolAnd(Value* a, Value* b) {
        a->i64 = a->i64 && b->i64;
    }

    inline void applyBinaryOperatorBoolOr(Value* a, Value* b) {
        a->i64 = a->i64 || b->i64;
    }



    inline void applyUnaryOperatorAddress(Value* operand) {
    }

    inline void applyBinaryOperatorSubscript(Value* a, Value* b) {
    }

    inline void applyBinaryOperatorMemberSelection(Value* a, Value* b) {
    }

    inline void applyBinaryOperatorMemberSelectionEnum(Value* a, Value* b) {
    }

    inline void applyBinaryOperatorMemberSelectionPointer(Value* a, Value* b) {
    }


    inline int evaluate(Variable* var) {

        Expression* ex = var->expression;

        if (var->expression == NULL) {

        }

        switch (ex->type) {

            case EXT_UNARY : {
                UnaryExpression* uex = (UnaryExpression*) ex;
                if (uex->operand) evaluate(uex->operand);
                // applyOperator(uex->operType, uex->operand);
                // icopy(op, uex->operand);
                break;
            }

            case EXT_BINARY : {
                BinaryExpression* bex = (BinaryExpression*) ex;
                if (bex->left) evaluate(bex->left);
                if (bex->right) evaluate(bex->right);
                // applyOperator(bex->operType, bex->left, bex->right);
                // icopy(op, bex->left);
                break;
            }

            case EXT_TERNARY : {
                break;
            }

            case EXT_FUNCTION_CALL : {
                FunctionCall* fex = (FunctionCall*) ex;
                Function* const fcn = fex->fcn;
                for (int i = 0; i < fcn->prototype.inArgCount; i++) {

                    //evaluate(fex->inArgs[i]);

                    //Value* value = readValue(fex->inArgs[i]);
                    //value->hasValue = contextId + 1;

                    //const int tmp = fex->inArgs[i]->ivalue.hasValue;
                    //fex->inArgs[i]->ivalue.hasValue = contextId + 1;
                    //writeValue(fcn->inArgs[i]->var, *value, fcn->istackIdx + 1);
                    //fex->inArgs[i]->ivalue.hasValue = tmp;

                }
                // execFunction(fcn, op);
                break;
            }
            default: {

            }

        }

        return Err::OK;

    }

    // All exec prefixed functions return index of last
    // node in children array of the input node on sucess
    // otherwise they return error value (which are negative)

    inline int execBranch(Branch* node);
    inline int execFunction(Function* node);
    inline int execVariableAssignment(VariableAssignment* node);
    inline int execVariableDefinition(VariableDefinition* node);
    inline int execReturnStatement(ReturnStatement* node);
    inline int execScope(Scope* node);



    inline int execBranch(Branch* node) {

        for (int i = 0; i < node->expressionCount; i++){
            // evaluate(node->expressions[i]);
            //if (readValue(node->expressions[i])->i32) {
            //    return execScope(node->scopes[i]);
            //}
        }

        if (node->scopeCount > node->expressionCount) {
            //return execScope(node->scopes[node->scopes.size - 1]);;
        }

        return Err::OK;

    }

    inline int execVariableDefinition(VariableDefinition* node) {
        evaluate(node->var);
        //copy(node->var->def->var, node->var);
        return Err::OK;
    }

    inline int execVariableAssignment(VariableAssignment* node) {

        evaluate(node->rvar);
        node->lvar->value = node->rvar->value;
        return Err::OK;

    }

    inline int execReturnStatement(ReturnStatement* node) {

        evaluate(node->var);
        return 0;//RR_RETURN + node->idx;

    }

    inline int execScope(Scope* scope) {

        SyntaxNode** nodes = scope->children;
        for (int i = 0; i < scope->childrenCount; i++) {

            SyntaxNode* const node = nodes[i];

            int err = Err::OK;
            switch (node->type) {

                case NT_BRANCH :
                    err = execBranch((Branch*) node);
                    break;

                case NT_VARIABLE_DEFINITION :
                    err = execVariableDefinition((VariableDefinition*) node);
                    break;

                case NT_VARIABLE_ASSIGNMENT :
                    err = execVariableAssignment((VariableAssignment*) node);
                    break;

                case NT_RETURN_STATEMENT :
                    err = execReturnStatement((ReturnStatement*) node);
                    break;

                default:
                    break;

            }

            if (err > Err::OK) return err;

        }

        return Err::OK;

    }

    template<typename T>
    bool isZero(T value) {
        if constexpr (std::is_floating_point_v<T>) {
            // TODO : ? abs(value) < eps;
            return value == 0.0 || value == -0.0;
        } else {
            return value == 0;
        }
    }

    template<typename T>
    T applyArithmetic(AstContext* ast, Span* span, OperatorEnum op, T left, T right) {
        if (op == OP_DIVISION || op == OP_MODULO) {
            if (isZero(right)) {
                if constexpr (std::is_integral_v<T>) {
                    Diag::report(ast, span, Err::DIVISION_BY_ZERO, SEV_ERROR);
                    return (T) 0;
                } else {
                    Diag::report(ast, span, Wrn::DIVISION_BY_ZERO, SEV_WARNING);
                }
            }
        }

        switch (op) {
            case OP_ADDITION:       return left + right;
            case OP_SUBTRACTION:    return left - right;
            case OP_MULTIPLICATION: return left * right;
            case OP_DIVISION:       return left / right;
        }

        if constexpr (std::is_integral_v<T>) {
            switch (op) {
                case OP_MODULO:      return left % right;
                case OP_BITWISE_AND: return left & right;
                case OP_BITWISE_OR:  return left | right;
                case OP_BITWISE_XOR: return left ^ right;
            }
        }

        return 0;
    }

    template<typename T>
    T applyUnary(AstContext* ast, Span* span, OperatorEnum op, T val) {
        switch (op) {
            case OP_UNARY_PLUS:  return +val;
            case OP_UNARY_MINUS: return -val;
            case OP_NEGATION:    return !val ? (T) 1 : (T) 0;
        }

        if constexpr (std::is_integral_v<T>) {
            switch (op) {
                case OP_BITWISE_NEGATION: return ~val;
            }
        }

        return 0;
    }

    template<typename Target>
    Target castValue(Value* val, Type::Kind fromType) {
        switch (fromType) {
            case Type::DT_I8:  return (Target) val->i8;
            case Type::DT_U8:  return (Target) val->u8;
            case Type::DT_I16: return (Target) val->i16;
            case Type::DT_U16: return (Target) val->u16;
            case Type::DT_I32: return (Target) val->i32;
            case Type::DT_U32: return (Target) val->u32;
            case Type::DT_I64: return (Target) val->i64;
            case Type::DT_U64: return (Target) val->u64;
            case Type::DT_F32: return (Target) val->f32;
            case Type::DT_F64: return (Target) val->f64;
                // TODO: case Type::DT_POINTER: return (Target) val->ptr;
            default: return 0;
        }
    }

    void applyOperator(AstContext* ctx, Span* span, OperatorEnum op, Variable* leftVar, Variable* rightVar, Value* result) {
        Value* left = &leftVar->value;
        Value* right = &rightVar->value;

        switch (left->type->kind) {
            case Type::DT_I8: {
                result->i8 = applyArithmetic(ctx, span, op, left->i8, right->i8);
                break;
            }

            case Type::DT_U8: {
                result->u8 = applyArithmetic(ctx, span, op, left->u8, right->u8);
                break;
            }

            case Type::DT_I16: {
                result->i16 = applyArithmetic(ctx, span, op, left->i16, right->i16);
                break;
            }

            case Type::DT_U16: {
                result->u16 = applyArithmetic(ctx, span, op, left->u16, right->u16);
                break;
            }

            case Type::DT_I32: {
                result->i32 = applyArithmetic(ctx, span, op, left->i32, right->i32);
                break;
            }

            case Type::DT_U32: {
                result->u32 = applyArithmetic(ctx, span, op, left->u32, right->u32);
                break;
            }

            case Type::DT_I64: {
                result->i64 = applyArithmetic(ctx, span, op, left->i64, right->i64);
                break;
            }

            case Type::DT_U64: {
                result->u64 = applyArithmetic(ctx, span, op, left->u64, right->u64);
                break;
            }

            case Type::DT_F32: {
                result->f32 = applyArithmetic(ctx, span, op, left->f32, right->f32);
                break;
            }

            case Type::DT_F64: {
                result->f64 = applyArithmetic(ctx, span, op, left->f64, right->f64);
                break;
            }
        }
    }

    void applyOperator(AstContext* ctx, Span* span, OperatorEnum op, Variable* var, Value* result) {
        Value* value = &var->value;

        if (op == OP_NONE) {
            *result = var->value;
            return;
        }

        switch (var->value.type->kind) {
            case Type::DT_I8: {
                result->i8 = applyUnary(ctx, span, op, value->i8);
                break;
            }

            case Type::DT_U8: {
                result->u8 = applyUnary(ctx, span, op, value->u8);
                break;
            }

            case Type::DT_I16: {
                result->i16 = applyUnary(ctx, span, op, value->i16);
                break;
            }

            case Type::DT_U16: {
                result->u16 = applyUnary(ctx, span, op, value->u16);
                break;
            }

            case Type::DT_I32: {
                result->i32 = applyUnary(ctx, span, op, value->i32);
                break;
            }

            case Type::DT_U32: {
                result->u32 = applyUnary(ctx, span, op, value->u32);
                break;
            }

            case Type::DT_I64: {
                result->i64 = applyUnary(ctx, span, op, value->i64);
                break;
            }

            case Type::DT_U64: {
                result->u64 = applyUnary(ctx, span, op, value->u64);
                break;
            }

            case Type::DT_F32: {
                result->f32 = applyUnary(ctx, span, op, value->f32);
                break;
            }

            case Type::DT_F64: {
                result->f64 = applyUnary(ctx, span, op, value->f64);
                break;
            }
        }
    }

    void initEval(CompilerState* state) {
    }

    /*
    void flatten(AstContext* ast, Variable* var) {
        if (!var || !var->expression) return;

        switch (var->value.type->kind) {
            case Type::DT_STRUCT: {
                Variable* unwrappedVar = unwrap(var);

                if (unwrappedVar->expression->type != EXT_TYPE_INITIALIZATION) {
                    // TODO : error/assert
                    return;
                }

                TypeInitialization* init = (TypeInitialization*) unwrappedVar->expression;
                TypeDefinition* td = (TypeDefinition*) unwrappedVar->value.def;

                for (uint32_t i = 0; i < td->varCount; i++) {
                    init->attributes[i];

                    if (i < init->attributeCount) {
                        td->vars[i]->value = init->attributes[i]->value;
                        td->vars[i]->expression = init->attributes[i]->expression;
                    } else if (init->fillVar) {
                        td->vars[i]->value = init->fillVar->value;
                        td->vars[i]->expression = init->fillVar->expression;
                    }
                }

                break;
            }

            default: break;
        }

        // TODO : tag for the linker/optimizer as constant
    }
    */

    // TODO : we need to track enqountered variables to prevent locking
    Err::Err eval(Validator::ValidationContext* ctx, Variable* var) {
        if (!var || var->value.hasValue) return Err::OK;

        Err::Err err = Err::OK;

        Expression* ex = var->expression;
        if (!ex && var->def) {
            // We are variable in expression, so we have to eval
            // definition...
            VariableDefinition* def = var->def;

            // First we ensure that semantic check was done.
            err = Validator::ensureValidated(ctx, (SyntaxNode*) def);
            if (err != Err::OK) return err;

            // Now evaluate...
            AcquireNodeReturn ans =
                acquireNode(&def->base.cmpStatus, &def->base.workerId, ctx->workerId, true);

            if (ans == ANR_ACQUIRED_FOR_WORK) {
                err = eval(ctx, def->var);
                if (err != Err::OK) {
                    releaseNode(&def->base.cmpStatus, err == Err::OK);
                    return err;
                } else {
                    // flatten(ctx->unit->ast, def->var);
                    releaseNode(&def->base.cmpStatus, err == Err::OK);
                }
            } else if (ans == ANR_ALREADY_ACQUIRED_BY_CALLER) {
                Diag::report(ctx->unit->ast, def->base.span, Err::UNEXPECTED_ERROR, Diag::Format {
                    "TODO : Node being validated is already on stack! Causing circular dependency!"
                });
                return Err::UNEXPECTED_ERROR;
            }

            var->value = def->var->value;

            return Err::OK;
        }

        if (!ex) {
            return Err::OK;
        }

        switch (ex->type) {
            case EXT_UNARY : {
                UnaryExpression* uex = (UnaryExpression*) ex;
                if (!uex->operand) return Err::CANNOT_EVALUATE;

                err = eval(ctx, uex->operand);
                if (err != Err::OK) return err;

                applyOperator(ctx->unit->ast, var->base.span, uex->base.opType, uex->operand, &var->value);
                var->value.hasValue = true;

                break;
            }

            case EXT_BINARY : {
                BinaryExpression* bex = (BinaryExpression*) ex;
                if (!bex->left || !bex->right) return Err::CANNOT_EVALUATE;

                err = eval(ctx, bex->left);
                if (err != Err::OK) return err;

                err = eval(ctx, bex->right);
                if (err != Err::OK) return err;

                applyOperator(ctx->unit->ast, var->base.span, bex->base.opType, bex->left, bex->right, &var->value);
                var->value.hasValue = true;

                break;
            }

            case EXT_FUNCTION_CALL : {
                FunctionCall* call = (FunctionCall*) ex;
                Function* const fcn = call->fcn;

                TaskSystem::dispatchLocalTask(fcn, true);
                // Interpreter::print(fcn);

                for (uint32_t i = 0; i < call->inArgCount; i++) {
                    err = eval(ctx, call->inArgs[i]);
                    if (err != Err::OK) return err;
                }

                if (fcn->base.flags & IS_EXTERN) {
                    Extern::Abi::Driver* abi = Extern::Abi::getTargetDriver();
                    Extern::compile(ctx->unit->ast, abi, fcn);
                    Extern::invoke(ctx->unit->ast, abi, fcn, call->inArgs, call->inArgCount, var);
                } else {
                    err = Interpreter::exec(ctx->unit->ast, fcn, call->inArgs, call->inArgCount, var);
                    if (err != Err::OK) return err;
                }

                var->value.hasValue = true;
                break;
            }

            case EXT_TYPE_INITIALIZATION: {
                TypeInitialization* init = (TypeInitialization*) ex;

                for (int i = 0; i <  init->attributeCount; i++) {
                    err = eval(ctx, init->attributes[i]);
                    if (err != Err::OK) return err;
                }

                err = eval(ctx, init->fillVar);
                if (err != Err::OK) return err;

                var->value.hasValue = true;

                break;
            }

            case EXT_CAST: {
                Cast* castExpr = (Cast*) ex;

                err = eval(ctx, castExpr->operand);
                if (err != Err::OK) return err;

                Value* srcValue = &castExpr->operand->value;
                Type::Kind srcType = srcValue->type->kind;
                Type::Kind dstType = castExpr->target->kind;

                switch (dstType) {
                    case Type::DT_I8:
                        var->value.i8 = castValue<int8_t>(srcValue, srcType);
                        break;
                    case Type::DT_U8:
                        var->value.u8 = castValue<uint8_t>(srcValue, srcType);
                        break;
                    case Type::DT_I16:
                        var->value.i16 = castValue<int16_t>(srcValue, srcType);
                        break;
                    case Type::DT_U16:
                        var->value.u16 = castValue<uint16_t>(srcValue, srcType);
                        break;
                    case Type::DT_I32:
                        var->value.i32 = castValue<int32_t>(srcValue, srcType);
                        break;
                    case Type::DT_U32:
                        var->value.u32 = castValue<uint32_t>(srcValue, srcType);
                        break;
                    case Type::DT_I64:
                        var->value.i64 = castValue<int64_t>(srcValue, srcType);
                        break;
                    case Type::DT_U64:
                        var->value.u64 = castValue<uint64_t>(srcValue, srcType);
                        break;
                    case Type::DT_F32:
                        var->value.f32 = castValue<float>(srcValue, srcType);
                        break;
                    case Type::DT_F64:
                        var->value.f64 = castValue<double>(srcValue, srcType);
                        break;
                        // TODO: case Type::DT_POINTER:
                        //var->value.ptr = castValue<uint64_t>(srcValue, srcType); // Pointers are usually uint64_t under the hood
                        //break;
                    default:
                        Diag::report(ctx->unit->ast, var->base.span, Err::UNEXPECTED_ERROR,
                            Diag::Format {
                                "TODO: cast error"
                            }
                        );
                        break;
                }

                var->value.hasValue = true;
                break;
            }

            default: {
                return Err::NOT_YET_IMPLEMENTED;
            }
        }

        return Err::OK;
    }

}
