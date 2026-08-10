
#include "data_types.h"
#include "dynamic_arena.h"
#include "foreign_code.h"
#include "interpreter.h"
#include "io.h"
#include "registry.h"
#include "supplement/runtime.h"

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>

#include "logger.h"
#include "syntax.h"
#include "diagnostic.h"

#include "utils.h"
#include "vec_kernel.h"

#include "debug_helper.h"



namespace Interpreter {

    Logger::Type logErr = { Logger::ERROR };
    Logger::Type LogWrn = { Logger::WARNING };

    vmword* stack;
    uint64_t stackSize;

    // TODO : for now
    Arena::Container heap;

    // for now, and maybe forever, we just simply push to arena
    struct VecContext {
        Arena::Container arena;
    };
    thread_local VecContext vecContext;



    // TODO : refactor to getVM and create struct
    vmword* getFreeStack() {
        // TODO for now no concurrency
        return stack;
    }

    thread_local uint8_t* gFramePointer;
    uintptr_t getExeFramePointer(uint32_t alignment, uint32_t* outStackSize) {
        uintptr_t fp = (uintptr_t) gFramePointer;
        fp = Utils::alignForward(fp, alignment);

        *outStackSize = stackSize - (fp - (uintptr_t) stack);
        return fp;
    }

    void initExec(CompilerState* state) {
        stackSize = 1024 * 1024;
        stack = (vmword*) alloc(alc, stackSize, 8);
        Arena::init(&heap, stackSize);
        Arena::init(&vecContext.arena, stackSize);
    }

    Err::Err StackToVariable(AstContext* ast, uint8_t* buff, int64_t buffSize, Variable* var) {
        Value* val = &var->value;

        switch (val->typeKind) {
            case Type::DT_I8:
            case Type::DT_U8:
            case Type::DT_I16:
            case Type::DT_U16:
            case Type::DT_I32:
            case Type::DT_U32:
            case Type::DT_I64:
            case Type::DT_U64:
            case Type::DT_F32:
            case Type::DT_F64:
            case Type::DT_POINTER: {
                const int size = (Type::basicTypes + val->typeKind)->size;
                if (buffSize < size) {
                    Diag::report(ast, var->base.span,
                        Err::UNEXPECTED_ERROR, Diag::Format {
                            "Unexpected Comptime Memory Error: Attempted to read %d bytes from VM stack, "
                            "but only %lld bytes are available in the current frame."
                        },
                        size, buffSize);
                    return Err::UNEXPECTED_ERROR;
                }

                val->u64 = 0;
                memcpy(&val->u64, buff, size);
                val->hasValue = true;
                break;
            }

            case Type::DT_CUSTOM: {
                TypeDefinition* def = val->def;
                Type::StructInfo* sInfo = (Type::StructInfo*) def->typeInfo;

                TypeInitialization* init = Ast::Node::makeTypeInitialization();
                init->attributeCount = sInfo->memberCount;
                init->attributes = (Variable**) alloc(alc, init->attributeCount * sizeof(Variable*));

                for (int i = 0; i < (int)sInfo->memberCount; i++) {
                    Type::StructMemberInfo* mInfo = sInfo->members + i;

                    Variable* mVar = Ast::Node::makeVariable();
                    mVar->value.typeKind = mInfo->type->kind;
                    mVar->value.def =
                        mInfo->type->kind == Type::DT_CUSTOM ?
                        (TypeDefinition*) mInfo->type : NULL;

                    uint8_t* mBuff = buff + mInfo->offset;
                    int64_t  mSize = buffSize - mInfo->offset;

                    Err::Err err = StackToVariable(ast, mBuff, mSize, mVar);
                    if (err != Err::OK) return err;

                    init->attributes[i] = mVar;
                }

                var->expression = (Expression*) init;
                break;
            }

            case Type::DT_ARRAY:
            case Type::DT_SLICE: {
                Diag::report(ast, var->base.span,
                    Err::NOT_YET_IMPLEMENTED, Diag::Format {
                        "Compile-time conversion for %s not yet implemented"
                    }, Type::str(val->typeKind));
                return Err::NOT_YET_IMPLEMENTED;
            }

            default: {
                Diag::report(ast, var->base.span,
                    Err::UNEXPECTED_ERROR, Diag::Format {
                        "Invalid type kind (%i) in StackToValue"
                    }, val->typeKind);
                return Err::UNEXPECTED_ERROR;
            }
        }

        return Err::OK;
    }

    Err::Err VariableToStack(AstContext* ast, uint8_t* buff, int64_t buffSize, Variable* var) {
        Value* val = &var->value;

        switch (val->typeKind) {
            case Type::DT_I8:
            case Type::DT_U8:
            case Type::DT_I16:
            case Type::DT_U16:
            case Type::DT_I32:
            case Type::DT_U32:
            case Type::DT_I64:
            case Type::DT_U64:
            case Type::DT_F32:
            case Type::DT_F64: {
                const int size = (Type::basicTypes + val->typeKind)->size;
                if (buffSize < size) {
                    Diag::report(ast, var->base.span,
                        Err::UNEXPECTED_ERROR, Diag::Format {
                            "Unexpected Comptime Memory Error: Attempted to write %d bytes to VM stack, "
                            "but only %lld bytes are available in the current frame."
                        },
                        size, buffSize);
                    return Err::UNEXPECTED_ERROR;
                }

                memset(buff, 0, size);
                memcpy(buff, &val->u64, size);
                break;
            }

            // TODO : sanity check for memberCount == varCount?
            case Type::DT_CUSTOM: {
                TypeDefinition* def = val->def;
                Type::StructInfo* sInfo = (Type::StructInfo*) def->typeInfo;

                var = unwrap(var);
                if (var->def) var = unwrap(var->def->var);

                // TODO : for now assuming that it can be only init
                if (!var->expression || var->expression->type != EXT_TYPE_INITIALIZATION) {
                    Diag::report(ast, var->base.span, Err::UNEXPECTED_SYMBOL,
                        "Expected struct initialization expression.");
                    return Err::UNEXPECTED_ERROR;
                }

                TypeInitialization* init = (TypeInitialization*) var->expression;
                for (int i = 0; i < sInfo->memberCount; i++) {
                    Type::StructMemberInfo* mInfo = sInfo->members + i;

                    Variable* mVar = NULL;
                    if (i < init->attributeCount) {
                        mVar = init->attributes[i];
                    } else if (init->fillVar) {
                        mVar = init->fillVar;
                    } else {
                        mVar = def->vars[i];
                    }

                    uint8_t* mBuff = buff + mInfo->offset;
                    int64_t  mSize = buffSize - mInfo->offset;

                    Err::Err err = VariableToStack(ast, mBuff, mSize, mVar);
                    if (err != Err::OK) return err;
                }

                break;
            }

            case Type::DT_SLICE:
            case Type::DT_ERROR:
            case Type::DT_FUNCTION:
            case Type::DT_COUNT:
            case Type::DT_MULTIPLE_TYPES:
            case Type::DT_ARRAY: {
                Diag::report(ast, var->base.span,
                    Err::NOT_YET_IMPLEMENTED, Diag::Format {
                        "Compile-time conversion for %s not yet implemented"
                    }, Type::str(val->typeKind));
                return Err::NOT_YET_IMPLEMENTED;
            }

            default: {
                Diag::report(ast, var->base.span,
                    Err::UNEXPECTED_ERROR, Diag::Format {
                        "Invalid type kind (%i) in ValueToStack"
                    }, val->typeKind);
                return Err::UNEXPECTED_ERROR;
            }
        }

        return Err::OK;
    }


    // some useful functions to not copy-paste that much
    // hopefully they get optimized

    template <typename Dtype>
    inline Dtype _unaligned_fetch(uint8_t*& buffer) {
        Dtype val;
        memcpy(&val, buffer, sizeof(Dtype));
        buffer += sizeof(Dtype);
        return val;
    }

    template <typename Dtype>
    inline Dtype _aligned_fetch(uint8_t*& buffer) {
        Dtype val = *(Dtype*) buffer;
        buffer -= sizeof(Dtype);
        return val;
    }

    inline void _push(vmword*& stack, vmword word) {
        memcpy(stack, &word, sizeof(vmword));
        stack++;
    }

    // macros so we can change the implementation
    // for platforms/build modes

    #define GROW(stack, size) ((stack) += BYTES_TO_WORDS(size))
    #define GROW_IN_WORDS(stack, size) ((stack) += (size))
    #define DROP(stack, size) ((stack) -= (BYTES_TO_WORDS(size)))
    #define DROP_IN_WORDS(stack, size) ((stack) -= (size))

    #define FETCH(buffer, dtype) (_unaligned_fetch<dtype>(buffer))
    #define FETCH_ALIGN(buffer, dtype) (_aligned_fetch<dtype>(buffer))
    #define PUSH(stack, word) (_push(stack, word))
    #define POP(stack) (*(stack -= 1))

    #define BINARY_EXP(dtype, op) BINARY_EXP_EX(dtype, dtype, op)
    #define BINARY_EXP_EX(dtype, resultCast, op) \
        dtype right = (dtype) POP(sp); \
        dtype left = (dtype) POP(sp); \
        dtype ans = left op right; \
        PUSH(sp, (resultCast) ans);

    #define CAST(dest, src) CAST_EX(dest, src, uint64_t)
    #define CAST_EX(dest, src, store) \
        dest val = (dest) (*(src*) (sp - 1)); \
        *(sp - 1) = *(store*) &val;


    // TODO : add Err::RUNTIME_ERROR
    static Err::Err reportUninitializedGlobal(AstContext* ast, ExeBlock* exe, uint8_t* ip, ExeBlock* owner, uint64_t offset) {
        // ip - 1 points to the opcode that just failed
        uint64_t opcodeOffset = (uint64_t) (ip - 1 - exe->bytecode);
        LineInfo* line = findLineForOffset(exe, opcodeOffset);

        String name = Ast::Node::getName(owner->node);
        Diag::report(ast, line ? &line->span : NULL, Err::UNEXPECTED_ERROR,
            Diag::Format {
                "Runtime Error: Access to uninitialized global memory.\n"
                "  Owner Unit: %.*s\n"
                "  Memory Offset: %llu bytes"
            },
            name.len, name.buff, offset
        );

        return Err::UNEXPECTED_ERROR;
    }

    template<typename T>
    inline static Err::Err execGetGlobal(AstContext* ast, ExeBlock* exe, uint8_t*& ip, vmword*& sp) {
        VariableDefinition* def = FETCH(ip, VariableDefinition*);

        ExeBlock* owner = def->vmOwnerExe;
        uint64_t  offset = FETCH(ip, uint64_t);

        if (!owner->liveFp) {
            return reportUninitializedGlobal(ast, exe, ip, owner, offset);
        }

        T val = *(T*) ((uint8_t*) owner->liveFp + offset);
        PUSH(sp, val);

        return Err::OK;
    }

    template<typename T>
    inline static Err::Err execSetGlobal(AstContext* ast, ExeBlock* exe, uint8_t*& ip, vmword*& sp) {
        VariableDefinition* def = FETCH(ip, VariableDefinition*);
        
        ExeBlock* owner  = def->vmOwnerExe;
        uint64_t  offset = FETCH(ip, uint64_t);

        if (!owner->liveFp) {
            return reportUninitializedGlobal(ast, exe, ip, owner, offset);
        }

        vmword word = POP(sp);
        memcpy(owner->liveFp + offset, &word, sizeof(T));

        return Err::OK;
    }

    int execPrint(ExeBlock* exe, uint8_t* fp, vmword* sp) {

        uint64_t argsCnt = POP(sp);
        DROP(sp, argsCnt * 2 * sizeof(vmword));

        uint64_t fmtLen = POP(sp);
        char* fmt = (char*) POP(sp);// (char*) (exe->rawData + POP(sp));

        int idx = 0;
        int argIdx = 1;
        int beginIdx = 0;
        for (; idx < fmtLen; idx++) {

            const char ch = fmt[idx];
            if (ch == '%') {

                if (argIdx >= argsCnt + 1) {
                    printf("<MISSING ARG>");
                    beginIdx = idx + 1;
                    continue;
                }

                Runtime::_Any arg;
                arg.info = (Runtime::_TypeInfo*) sp[2 * argIdx];
                arg.u = sp[2 * argIdx + 1];

                fwrite(fmt + beginIdx, 1, idx - beginIdx, stdout);
                printValue(arg);

                argIdx++;
                beginIdx = idx + 1;

            }

        }

        fwrite(fmt + beginIdx, 1, idx - beginIdx, stdout);

        return 2 + argsCnt * 2 + 1;

    }

    int execAlloc(ExeBlock* exe, uint8_t* fp, vmword* sp) {

        uint64_t bytes = POP(sp);

        void* ptr = Arena::push(&heap, bytes);
        PUSH(sp, (vmword) ptr);

        return 2 + 1;

    }

    // returns the size of the stack frame in words
    int internalCall(ExeBlock* exe, uint8_t* fp, vmword* sp, Ast::Internal::FunctionType ft) {

        switch (ft) {

            case Ast::Internal::IF_PRINTF: {
                return execPrint(exe, fp, sp);
            }

            case Ast::Internal::IF_ALLOC: {
                return execAlloc(exe, fp, sp);
            }

            default: {
                printf("<UNIMPLEMENTED>");
            }

        }

    }



    // --- Some vectorization functions
    //

    VecInfo vecFetchInfo(uint8_t** ip) {
        uint64_t desc = FETCH(*ip, uint64_t);
        uint64_t dest = FETCH(*ip, uint64_t);
        return {
            .dest = dest,
            .desc = decodeVecDescriptor(desc)
        };
    }

    void* vecGetPtr(VecInfo info, uint64_t len, uint8_t* fp) {
        if (!(info.desc.flags & DE_F_DEST)) {
            return fp + info.dest;
        }
        return Arena::push(&vecContext.arena, len, sizeof(vmword));
    }

    void* vecAlloc(const int len) {
        return Arena::push(&vecContext.arena, len, sizeof(vmword));
    }

    void vecResetMemory() {
        Arena::clear(&vecContext.arena);
    }



    uint64_t getStrideFromStoreOpcode(Opcode opcode) {
        switch (opcode) {
            case OC_STORE_I8:
            case OC_STORE_U8:  return 1;
            case OC_STORE_I16:
            case OC_STORE_U16: return 2;
            case OC_STORE_I32:
            case OC_STORE_U32:
            case OC_STORE_F32: return 4;
            case OC_STORE_I64:
            case OC_STORE_U64:
            case OC_STORE_F64:
            case OC_STORE_PTR: return 8;
            default: return 1;
        }
    }

    // We can unite them with Store function
    uint64_t getStrideFromLoadOpcode(Opcode opcode) {
        switch (opcode) {
            case OC_LOAD_I8:
            case OC_LOAD_U8:  return 1;
            case OC_LOAD_I16:
            case OC_LOAD_U16: return 2;
            case OC_LOAD_I32:
            case OC_LOAD_U32:
            case OC_LOAD_F32: return 4;
            case OC_LOAD_I64:
            case OC_LOAD_U64:
            case OC_LOAD_F64:
            case OC_LOAD_PTR: return 8;
            default: return 1;
        }
    }



    Err::Err exec(AstContext* ast, Function* fcn, Variable** args, uint64_t argCount, Variable* out) {
        Arena::clear(&heap);

        ExeBlock* rootBlock = fcn->exe;
        ExeBlock* currBlock = rootBlock;

        vmword* sp = getFreeStack(); // operand stack pointer
        uint8_t* ip = currBlock->bytecode; // bytecode instruction pointer

        // setup 'fake' call with exit
        // for now assuming only void
        uint8_t trap[] = { OC_HALT };
        PUSH(sp, (uint64_t) (trap));
        PUSH(sp, 0);

        uint8_t* fp = (uint8_t*) sp; // current frame on operand stack
        GROW(sp, currBlock->fixedSize);
        GROW(sp, currBlock->localsSize);
        memcpy(fp, currBlock->locals, currBlock->localsSize);

        gFramePointer = fp;

        // push actual args
        {
            int64_t offset = 0;
            for (int i = 0; i < argCount; i++) {
                const int64_t size = getTypeInfo(args[i])->size;
                if (offset + size > currBlock->fixedSize) {
                    Diag::report(ast, args[i]->base.span, Err::UNEXPECTED_ERROR, Diag::Format{
                        "Internal Compiler Error: Argument '%i' at offset %llu exceeds "
                        "function frame size %llu"
                    }, i, offset, currBlock->fixedSize);
                    return Err::UNEXPECTED_ERROR;
                };

                VariableToStack(ast, fp + offset, size, args[i]);
                offset += size;
            }
        }

        while (1) {
            Opcode opcode = (Opcode) *ip;
            ip += sizeof(Opcode);

            switch(opcode) {
                case OC_PUSH_I8: {
                    PUSH(sp, FETCH(ip, int8_t));
                    break;
                }

                case OC_PUSH_U8: {
                    PUSH(sp, FETCH(ip, uint8_t));
                    break;
                }

                case OC_PUSH_I16: {
                    PUSH(sp, FETCH(ip, int16_t));
                    break;
                }

                case OC_PUSH_U16: {
                    PUSH(sp, FETCH(ip, uint16_t));
                    break;
                }

                case OC_PUSH_I32: {
                    PUSH(sp, FETCH(ip, int32_t));
                    break;
                }

                case OC_PUSH_U32:
                case OC_PUSH_F32: {
                    PUSH(sp, FETCH(ip, uint32_t));
                    break;
                }

                case OC_PUSH_I64:
                case OC_PUSH_U64:
                case OC_PUSH_F64:
                case OC_PUSH_PTR: {
                    PUSH(sp, FETCH(ip, uint64_t));
                    break;
                }

                case OC_PUSH_BLOB: {
                   const uint64_t offset = FETCH(ip, uint64_t);
                   const uint64_t size = FETCH(ip, uint64_t);

                   GROW(sp, size);
                   memcpy(sp, fp + offset, size);

                   break;
                }

                case OC_SET_I8:
                case OC_SET_U8: {
                    const uint64_t offset = FETCH(ip, uint64_t);

                    vmword word = POP(sp);
                    memcpy(fp + offset, &word, sizeof(uint8_t));

                    break;
                }

                case OC_SET_I16:
                case OC_SET_U16: {
                    const uint64_t offset = FETCH(ip, uint64_t);

                    vmword word = POP(sp);
                    memcpy(fp + offset, &word, sizeof(uint16_t));

                    break;
                }

                case OC_SET_I32:
                case OC_SET_U32:
                case OC_SET_F32: {
                    const uint64_t offset = FETCH(ip, uint64_t);

                    vmword word = POP(sp);
                    memcpy(fp + offset, &word, sizeof(uint32_t));

                    break;
                }

                case OC_SET_I64:
                case OC_SET_U64:
                case OC_SET_F64:
                case OC_SET_PTR: {
                    const uint64_t offset = FETCH(ip, uint64_t);

                    vmword word = POP(sp);
                    memcpy(fp + offset, &word, sizeof(uint64_t));

                    break;
                }

                case OC_SET_BLOB: {
                    const uint64_t size = FETCH(ip, uint64_t);
                    const uint64_t offset = FETCH(ip, uint64_t);

                    DROP(sp, size);
                    memcpy(fp + offset, sp, size);

                    break;
                }

                case OC_GET_I8: {
                    const uint64_t offset = FETCH(ip, uint64_t);
                    PUSH(sp, *(int8_t*) (fp + offset));
                    break;
                }

                case OC_GET_U8: {
                    const uint64_t offset = FETCH(ip, uint64_t);
                    PUSH(sp, *(uint8_t*) (fp + offset));
                    break;
                }

                case OC_GET_I16: {
                    const uint64_t offset = FETCH(ip, uint64_t);
                    PUSH(sp, *(int16_t*) (fp + offset));
                    break;
                }

                case OC_GET_U16: {
                    const uint64_t offset = FETCH(ip, uint64_t);
                    PUSH(sp, *(uint16_t*) (fp + offset));
                    break;
                }

                case OC_GET_I32: {
                    const uint64_t offset = FETCH(ip, uint64_t);
                    PUSH(sp, *(int32_t*) (fp + offset));
                    break;
                }

                case OC_GET_U32:
                case OC_GET_F32: {
                    const uint64_t offset = FETCH(ip, uint64_t);
                    PUSH(sp, *(uint32_t*) (fp + offset));
                    break;
                }

                case OC_GET_I64:
                case OC_GET_U64:
                case OC_GET_F64:
                case OC_GET_PTR: {
                    const uint64_t offset = FETCH(ip, uint64_t);
                    PUSH(sp, *(uint64_t*) (fp + offset));
                    break;
                }

                case OC_GET_BLOB: {
                    const uint64_t size = FETCH(ip, uint64_t);
                    const uint64_t offset = FETCH(ip, uint64_t);

                    vmword* dest = sp;
                    GROW(sp, size);
                    memcpy(dest, fp + offset, size);

                    break;
                }



                case OC_SET_GLOBAL_I8:
                case OC_SET_GLOBAL_U8: {
                    Err::Err err = execSetGlobal<uint8_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_SET_GLOBAL_I16:
                case OC_SET_GLOBAL_U16: {
                    Err::Err err = execSetGlobal<uint16_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_SET_GLOBAL_I32:
                case OC_SET_GLOBAL_U32:
                case OC_SET_GLOBAL_F32: {
                    Err::Err err = execSetGlobal<float>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_SET_GLOBAL_I64:
                case OC_SET_GLOBAL_U64:
                case OC_SET_GLOBAL_F64:
                case OC_SET_GLOBAL_PTR: {
                    Err::Err err = execSetGlobal<uintptr_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_SET_GLOBAL_BLOB: {
                    ExeBlock* owner = FETCH(ip, ExeBlock*);
                    uint64_t offset = FETCH(ip, uint64_t);
                    uint64_t size   = FETCH(ip, uint64_t);

                    sp -= BYTES_TO_WORDS(size);
                    void* dest = (uint8_t*) owner->liveFp + offset;
                    memcpy(dest, sp, size);

                    break;
                }



                case OC_GET_GLOBAL_I8: {
                    Err::Err err = execGetGlobal<int8_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_GET_GLOBAL_U8: {
                    Err::Err err = execGetGlobal<uint8_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_GET_GLOBAL_I16: {
                    Err::Err err = execGetGlobal<int16_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_GET_GLOBAL_U16: {
                    Err::Err err = execGetGlobal<uint16_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_GET_GLOBAL_I32: {
                    Err::Err err = execGetGlobal<int32_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_GET_GLOBAL_U32: {
                    Err::Err err = execGetGlobal<uint32_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_GET_GLOBAL_F32: {
                    Err::Err err = execGetGlobal<float>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_GET_GLOBAL_I64:
                case OC_GET_GLOBAL_U64:
                case OC_GET_GLOBAL_F64:
                case OC_GET_GLOBAL_PTR: {
                    Err::Err err = execGetGlobal<uintptr_t>(ast, currBlock, ip, sp);
                    if (err != Err::OK) return err;
                    break;
                }

                case OC_GET_GLOBAL_BLOB: {
                    ExeBlock* owner = FETCH(ip, ExeBlock*);
                    uint64_t offset = FETCH(ip, uint64_t);
                    uint64_t size   = FETCH(ip, uint64_t);

                    void* src = (uint8_t*)owner->liveFp + offset;
                    memcpy(sp, src, size);
                    sp += BYTES_TO_WORDS(size);

                    break;
                }



                case OC_LEA: {
                    const uint64_t offset = FETCH(ip, uint64_t);
                    PUSH(sp, (vmword) (fp + offset));
                    break;
                }

                case OC_LEA_CONST: {
                    const uint64_t offset = FETCH(ip, uint64_t);
                    PUSH(sp, (vmword) (currBlock->rawData + offset));
                    break;
                }

                case OC_LEA_GLOBAL: {
                    ExeBlock* owner = FETCH(ip, ExeBlock*);
                    uint64_t offset = FETCH(ip, uint64_t);

                    PUSH(sp, (vmword) (owner->liveFp + offset));
                    break;
                }

                case OC_PTR_IDX: {
                    const uint64_t stride = FETCH(ip, uint64_t);
                    const uint64_t idx = POP(sp);
                    const uint64_t ptr = POP(sp);

                    uint8_t* ans = ((uint8_t*) ptr) + stride * idx;
                    PUSH(sp, (vmword) ans);

                    break;
                }



                // DUMAT: can align > 8 occur on my operand
                //        stack if I am on meta level
                case OC_LOAD_I8: {
                    uintptr_t ptr = POP(sp);
                    PUSH(sp, *(int8_t*) ptr);
                    break;
                }

                case OC_LOAD_U8: {
                    uintptr_t ptr = POP(sp);
                    PUSH(sp, *(uint8_t*) ptr);
                    break;
                }

                case OC_LOAD_I16: {
                    uintptr_t ptr = POP(sp);
                    PUSH(sp, *(int16_t*) ptr);
                    break;
                }

                case OC_LOAD_U16: {
                    uintptr_t ptr = POP(sp);
                    PUSH(sp, *(uint16_t*) ptr);
                    break;
                }

                case OC_LOAD_I32: {
                    uintptr_t ptr = POP(sp);
                    PUSH(sp, *(int32_t*) ptr);
                    break;
                }

                case OC_LOAD_U32:
                case OC_LOAD_F32: {
                    uintptr_t ptr = POP(sp);
                    PUSH(sp, *(uint32_t*) ptr);
                    break;
                }

                case OC_LOAD_I64:
                case OC_LOAD_U64:
                case OC_LOAD_F64:
                case OC_LOAD_PTR: {
                    uintptr_t ptr = POP(sp);
                    PUSH(sp, *(uint64_t*) ptr);
                    break;
                }

                case OC_LOAD_BLOB: {
                    const uint64_t size = FETCH(ip, uint64_t);
                    const void* ptr = (void*) POP(sp);

                    void* dest = sp;
                    GROW(sp, size);
                    memcpy(dest, ptr, size);

                    break;
                }



                case OC_STORE_I8:
                case OC_STORE_U8: {
                    int64_t val = POP(sp);
                    uint8_t* ptr = (uint8_t*) POP(sp);

                    *ptr = (uint8_t) val;
                    break;
                }

                case OC_STORE_I16:
                case OC_STORE_U16: {
                    int64_t val = POP(sp);
                    uint16_t* ptr = (uint16_t*) POP(sp);

                    *ptr = (uint16_t) val;
                    break;
                }

                case OC_STORE_I32:
                case OC_STORE_U32:
                case OC_STORE_F32: {
                    int64_t val = POP(sp);
                    uint32_t* ptr = (uint32_t*) POP(sp);

                    *ptr = (uint32_t) val;
                    break;
                }

                case OC_STORE_I64:
                case OC_STORE_U64:
                case OC_STORE_F64:
                case OC_STORE_PTR: {
                    int64_t val = POP(sp);
                    uint64_t* ptr = (uint64_t*) POP(sp);

                    *ptr = (uint64_t) val;
                    break;
                }

                case OC_STORE_BLOB: {
                    const uint64_t size = FETCH(ip, uint64_t);

                    DROP(sp, size);

                    void* src = sp;
                    void* dest = (void*) POP(sp);
                    memcpy(dest, src, size);

                    break;
                }



                case OC_NEG_I32: {
                    ((vmvalue*) (sp - 1))->i32 = -((vmvalue*) (sp - 1))->i32;
                    break;
                }

                case OC_NEG_U32: {
                    ((vmvalue*) (sp - 1))->u32 = -((vmvalue*) (sp - 1))->u32;
                    break;
                }

                case OC_NEG_I64: {
                    ((vmvalue*) (sp - 1))->i64 = -((vmvalue*) (sp - 1))->i64;
                    break;
                }

                case OC_NEG_U64: {
                    ((vmvalue*) (sp - 1))->u64 = -((vmvalue*) (sp - 1))->u64;
                    break;
                }

                case OC_NEG_F32: {
                    ((vmvalue*) (sp - 1))->f32 = -((vmvalue*) (sp - 1))->f32;
                    break;
                }

                case OC_NEG_F64: {
                    ((vmvalue*) (sp - 1))->f64 = -((vmvalue*) (sp - 1))->f64;
                    break;
                }



                case OC_ADD_I32: {
                    BINARY_EXP(int32_t, +);
                    break;
                }

                case OC_ADD_U32: {
                    BINARY_EXP(uint32_t, +);
                    break;
                }

                case OC_ADD_I64: {
                    BINARY_EXP(int64_t, +);
                    break;
                }

                case OC_ADD_U64: {
                    BINARY_EXP(uint64_t, +);
                    break;
                }

                case OC_ADD_F32: {
                    BINARY_EXP_EX(float, uint32_t, +);
                    break;
                }

                case OC_ADD_F64: {
                    BINARY_EXP_EX(double, uint64_t, +)
                    break;
                }



                case OC_SUB_I32: {
                    BINARY_EXP(int32_t, -);
                    break;
                }

                case OC_SUB_U32: {
                    BINARY_EXP(uint32_t, -);
                    break;
                }

                case OC_SUB_I64: {
                    BINARY_EXP(int64_t, -);
                    break;
                }

                case OC_SUB_U64: {
                    BINARY_EXP(uint64_t, -);
                    break;
                }

                case OC_SUB_F32: {
                    BINARY_EXP_EX(float, uint32_t, -);
                    break;
                }

                case OC_SUB_F64: {
                    BINARY_EXP_EX(double, uint64_t, -);
                    break;
                }



                case OC_MUL_I32: {
                    BINARY_EXP(int32_t, *);
                    break;
                }

                case OC_MUL_U32: {
                    BINARY_EXP(uint32_t, *);
                    break;
                }

                case OC_MUL_I64: {
                    BINARY_EXP(int64_t, *);
                    break;
                }

                case OC_MUL_U64: {
                    BINARY_EXP(uint64_t, *);
                    break;
                }

                case OC_MUL_F32: {
                    BINARY_EXP_EX(float, uint32_t, *);
                    break;
                }

                case OC_MUL_F64: {
                    BINARY_EXP_EX(double, uint64_t, *);
                    break;
                }



                case OC_DIV_I32: {
                    BINARY_EXP(int32_t, /);
                    break;
                }

                case OC_DIV_U32: {
                    BINARY_EXP(uint32_t, /);
                    break;
                }

                case OC_DIV_I64: {
                    BINARY_EXP(int64_t, /);
                    break;
                }

                case OC_DIV_U64: {
                    BINARY_EXP(uint64_t, /);
                    break;
                }

                case OC_DIV_F32: {
                    BINARY_EXP_EX(float, uint32_t, /);
                    break;
                }

                case OC_DIV_F64: {
                    BINARY_EXP_EX(double, uint64_t, /);
                    break;
                }



                case OC_AND_I32: {
                    BINARY_EXP(int32_t, &);
                    break;
                }

                case OC_AND_U32: {
                    BINARY_EXP(uint32_t, &);
                    break;
                }

                case OC_AND_I64: {
                    BINARY_EXP(int64_t, &);
                    break;
                }

                case OC_AND_U64: {
                    BINARY_EXP(uint64_t, &);
                    break;
                }



                case OC_OR_I32: {
                    BINARY_EXP(int32_t, &);
                    break;
                }

                case OC_OR_U32: {
                    BINARY_EXP(uint32_t, &);
                    break;
                }

                case OC_OR_I64: {
                    BINARY_EXP(int64_t, &);
                    break;
                }

                case OC_OR_U64: {
                    BINARY_EXP(uint64_t, &);
                    break;
                }



                case OC_XOR_I32: {
                    BINARY_EXP(int32_t, &);
                    break;
                }

                case OC_XOR_U32: {
                    BINARY_EXP(uint32_t, &);
                    break;
                }

                case OC_XOR_I64: {
                    BINARY_EXP(int64_t, &);
                    break;
                }

                case OC_XOR_U64: {
                    BINARY_EXP(uint64_t, &);
                    break;
                }



                case OC_SHL_I32: {
                    BINARY_EXP(int32_t, <<);
                    break;
                }

                case OC_SHL_U32: {
                    BINARY_EXP(uint32_t, <<);
                    break;
                }

                case OC_SHL_I64: {
                    BINARY_EXP(int64_t, <<);
                    break;
                }

                case OC_SHL_U64: {
                    BINARY_EXP(uint64_t, <<);
                    break;
                }



                case OC_SHR_I32: {
                    BINARY_EXP(int32_t, >>);
                    break;
                }

                case OC_SHR_U32: {
                    BINARY_EXP(uint32_t, >>);
                    break;
                }

                case OC_SHR_I64: {
                    BINARY_EXP(int64_t, >>);
                    break;
                }

                case OC_SHR_U64: {
                    BINARY_EXP(uint64_t, >>);
                    break;
                }



                case OC_MOD_I32: {
                    BINARY_EXP(int32_t, %);
                    break;
                }

                case OC_MOD_U32: {
                    BINARY_EXP(uint32_t, %);
                    break;
                }

                case OC_MOD_I64: {
                    BINARY_EXP(int64_t, %);
                    break;
                }

                case OC_MOD_U64: {
                    BINARY_EXP(uint64_t, %);
                    break;
                }



                case OC_EQ_I32: {
                    BINARY_EXP(int32_t, ==);
                    break;
                }

                case OC_EQ_I64: {
                    BINARY_EXP(int64_t, ==);
                    break;
                }

                case OC_EQ_F32: {
                    BINARY_EXP(float, ==);
                    break;
                }

                case OC_EQ_F64: {
                    BINARY_EXP(double, ==);
                    break;
                }



                case OC_NE_I32: {
                    BINARY_EXP(int32_t, !=);
                    break;
                }

                case OC_NE_I64: {
                    BINARY_EXP(int64_t, !=);
                    break;
                }

                case OC_NE_F32: {
                    BINARY_EXP(float, !=);
                    break;
                }

                case OC_NE_F64: {
                    BINARY_EXP(double, !=);
                    break;
                }



                case OC_LT_I32: {
                    BINARY_EXP(int32_t, <);
                    break;
                }

                case OC_LT_I64: {
                    BINARY_EXP(int64_t, <);
                    break;
                }

                case OC_LT_U32: {
                    BINARY_EXP(uint32_t, <);
                    break;
                }

                case OC_LT_U64: {
                    BINARY_EXP(uint64_t, <);
                    break;
                }

                case OC_LT_F32: {
                    BINARY_EXP(float, <);
                    break;
                }

                case OC_LT_F64: {
                    BINARY_EXP(double, <);
                    break;
                }



                case OC_LE_I32: {
                    BINARY_EXP(int32_t, <=);
                    break;
                }

                case OC_LE_I64: {
                    BINARY_EXP(int64_t, <=);
                    break;
                }

                case OC_LE_U32: {
                    BINARY_EXP(uint32_t, <=);
                    break;
                }

                case OC_LE_U64: {
                    BINARY_EXP(uint64_t, <=);
                    break;
                }

                case OC_LE_F32: {
                    BINARY_EXP(float, <=);
                    break;
                }

                case OC_LE_F64: {
                    BINARY_EXP(double, <=);
                    break;
                }



                case OC_GE_I32: {
                    BINARY_EXP(int32_t, >=);
                    break;
                }

                case OC_GE_I64: {
                    BINARY_EXP(int64_t, >=);
                    break;
                }

                case OC_GE_U32: {
                    BINARY_EXP(uint32_t, >=);
                    break;
                }

                case OC_GE_U64: {
                    BINARY_EXP(uint64_t, >=);
                    break;
                }

                case OC_GE_F32: {
                    BINARY_EXP(float, >=);
                    break;
                }

                case OC_GE_F64: {
                    BINARY_EXP(double, >=);
                    break;
                }



                case OC_GT_I32: {
                    BINARY_EXP(int32_t, >);
                    break;
                }

                case OC_GT_I64: {
                    BINARY_EXP(int64_t, >);
                    break;
                }

                case OC_GT_U32: {
                    BINARY_EXP(uint32_t, >);
                    break;
                }

                case OC_GT_U64: {
                    BINARY_EXP(uint64_t, >);
                    break;
                }

                case OC_GT_F32: {
                    BINARY_EXP(float, >);
                    break;
                }

                case OC_GT_F64: {
                    BINARY_EXP(double, >);
                    break;
                }



                case OC_NOT_BOOL: {
                    vmword word = POP(sp);
                    PUSH(sp, !word);
                    break;
                }

                case OC_BOOL_I32: {
                    CAST(bool, uint32_t);
                    break;
                }

                case OC_BOOL_F32: {
                    CAST(bool, float);
                    break;
                }

                case OC_BOOL_I64: {
                    CAST(bool, uint64_t);
                    break;
                }

                case OC_BOOL_F64: {
                    CAST(bool, double);
                    break;
                }



                case OC_SEXT_32_TO_64: {
                    CAST(int32_t, int64_t);
                    break;
                }

                case OC_ZEXT_32_TO_64: {
                    CAST(uint32_t, uint64_t);
                    break;
                }

                case OC_TRUNC_64_TO_32: {
                    CAST(uint64_t, int32_t);
                    break;
                }



                case OC_CAST_I32_TO_U32: {
                    CAST(int32_t, uint32_t);
                    break;
                }

                case OC_CAST_I32_TO_F32: {
                    CAST(int32_t, float);
                    break;
                }

                case OC_CAST_I32_TO_F64: {
                    CAST(int32_t, double);
                    break;
                }

                case OC_CAST_U32_TO_I32: {
                    CAST_EX(uint32_t, int32_t, int64_t);
                    break;
                }

                case OC_CAST_U32_TO_F32: {
                    CAST(uint32_t, float);
                    break;
                }

                case OC_CAST_U32_TO_F64: {
                    CAST(uint32_t, double);
                    break;
                }

                case OC_CAST_I64_TO_U64: {
                    CAST(int64_t, uint64_t);
                    break;
                }

                case OC_CAST_I64_TO_F32: {
                    CAST(int64_t, float);
                    break;
                }

                case OC_CAST_I64_TO_F64: {
                    CAST(int64_t, double);
                    break;
                }

                case OC_CAST_U64_TO_I64: {
                    CAST(uint64_t, int64_t);
                    break;
                }

                case OC_CAST_U64_TO_F32: {
                    CAST(uint64_t, float);
                    break;
                }

                case OC_CAST_U64_TO_F64: {
                    CAST(uint64_t, double);
                    break;
                }

                case OC_CAST_F32_TO_I32: {
                    CAST_EX(float, int32_t, int64_t);
                    break;
                }

                case OC_CAST_F32_TO_I64: {
                    CAST(float, int64_t);
                    break;
                }

                case OC_CAST_F32_TO_U32: {
                    CAST(float, uint32_t);
                    break;
                }

                case OC_CAST_F32_TO_U64: {
                    CAST(float, uint64_t);
                    break;
                }

                case OC_CAST_F32_TO_F64: {
                    CAST(float, double);
                    break;
                }

                case OC_CAST_F64_TO_I32: {
                    CAST_EX(double, int32_t, int64_t);
                    break;
                }

                case OC_CAST_F64_TO_I64: {
                    CAST(double, int64_t);
                    break;
                }

                case OC_CAST_F64_TO_U32: {
                    CAST(double, uint32_t);
                    break;
                }

                case OC_CAST_F64_TO_U64: {
                    CAST(double, uint64_t);
                    break;
                }

                case OC_CAST_F64_TO_F32: {
                    CAST(float, uint32_t);
                    break;
                }



                case OC_JUMP: {
                    int64_t target = FETCH(ip, int64_t);
                    ip += target;
                    break;
                }

                case OC_JUMP_IF_TRUE: {
                    int64_t target = FETCH(ip, int64_t);

                    if (POP(sp)) {
                        ip += target;
                    }

                    break;
                }

                case OC_JUMP_IF_FALSE: {
                    int64_t target = FETCH(ip, int64_t);

                    if (POP(sp) == 0) {
                        ip += target;
                    }

                    break;
                }



                case OC_POP: {
                    POP(sp);
                    break;
                }

                case OC_POP_N: {
                    uint64_t n = FETCH(ip, uint64_t);
                    DROP_IN_WORDS(sp, n);
                    break;
                }

                case OC_DUP: {
                    PUSH(sp, sp[-1]);
                    break;
                }

                case OC_CROP: {
                    uint64_t blobSize     = FETCH(ip, uint64_t);
                    uint64_t memberOffset = FETCH(ip, uint64_t);
                    uint64_t memberSize   = FETCH(ip, uint64_t);

                    vmword* base = sp - blobSize;
                    uint8_t* src = (uint8_t*) base + memberOffset;

                    memmove(base, src, memberSize);

                    uint64_t memberWords = BYTES_TO_WORDS(memberSize);

                    if (memberWords * sizeof(vmword) > memberSize) {
                        memset((uint8_t*)base + memberSize, 0, memberWords * sizeof(vmword) - memberSize);
                    }

                    sp = base + memberWords;
                    break;
                }

                case OC_SWAP: {
                    if (((uint8_t*) sp) - fp < 2 * sizeof(vmword)) {
                        // TODO : what shall happen when there is only one word on stack?
                        break;
                    }

                    const vmword tmp = *sp;
                    *sp = *(sp - 1);
                    *(sp - 1) = tmp;

                    break;
                }



                case OC_CALL: {
                    Function* fcn = (Function*) FETCH(ip, uint64_t);
                    FETCH(ip, uint64_t); // vararg count, we dont need here
                    ExeBlock* exe = fcn->exe;

                    if (fcn->base.flags & IS_EXTERN) {
                        Extern::Abi::Driver* abi = Extern::Abi::getTargetDriver();
                        Extern::compile(ast, abi, fcn);

                        uint8_t* out = (uint8_t*) sp;
                        Type::TypeInfo* outInfo = getTypeInfo(fcn->prototype.outArg->var);
                        GROW(sp, outInfo->size);

                        Extern::invoke(ast, abi, fcn, out);

                        //Type::TypeInfo* outInfo = getTypeInfo(fcn->prototype.outArg->var);
                        //VariableToStack(ast, (uint8_t*) sp, min((uint64_t) outInfo->size, stackSize), &out);

                        break;
                    }

                    if (isValidFunctionIdx(fcn->internalIdx)) {
                        int fSize = internalCall(currBlock, fp, sp, (Ast::Internal::FunctionType) fcn->internalIdx);
                        DROP_IN_WORDS(sp, 2 + fSize);
                        break;
                    }

                    // NOTE: when nested function definitions land, liveFp must
                    //       be saved/restored across OC_CALL/OC_RET as 3rd
                    //       linkage slot.
                    //       For now it's safe to do nothing because no non-local
                    //       variable can target the same ExeBlock twice, as the
                    //       global block cannot be called...
                    exe->liveFp = fp;

                    // sizes are in bytes
                    //
                    const uint64_t fixedSize   = exe->fixedSize;
                    const uint64_t varargSize = exe->isVariadic ?
                        sizeof(vmword) * (2 * (*(sp - 1)) + 1) : 0;

                    const uint64_t scopeSize = exe->localsSize - exe->defaultArgsSize;
                    uint8_t* scopeLocals = ((uint8_t*) sp) - varargSize;

                    GROW(sp, scopeSize);
                    memmove(scopeLocals + scopeSize, scopeLocals, varargSize);
                    memcpy(scopeLocals, exe->locals + exe->defaultArgsSize, scopeSize);

                    uint64_t retAddrOffset = (fixedSize + varargSize) / sizeof(vmword) + 2;
                    *(sp - retAddrOffset)     = (vmword) ip;
                    *(sp - retAddrOffset + 1) = (vmword) fp;

                    ip = exe->bytecode;
                    fp = ((uint8_t*) sp) - fixedSize - varargSize;

                    gFramePointer = fp;

                    break;
                }

                case OC_RET: {
                    uint64_t size = FETCH(ip, uint64_t);

                    const uint64_t avaliableSize = ((sp - (vmword*) fp) + 2) * sizeof(vmword);
                    if (avaliableSize < size) {
                        GROW(sp, size - avaliableSize);
                    }

                    DROP(sp, size);
                    void* returnData = sp;

                    sp = ((uint64_t*) fp) - 2;
                    fp = (uint8_t*) sp[1];
                    ip = (uint8_t*) sp[0];

                    memmove(sp, returnData, size);
                    GROW(sp, size);

                    break;
                }



                case OC_GROW: {
                    GROW(sp, FETCH(ip, uint64_t));
                    break;
                }

                case OC_HALT: {
                    goto loopEnd;
                    break;
                }



                case OC_VEC_VV: {
                    uint64_t lenB = POP(sp);
                    uint64_t ptrB = POP(sp);
                    uint64_t lenA = POP(sp);
                    uint64_t ptrA = POP(sp);

                    if (lenA != lenB) {
                        // TODO
                    }

                    VecInfo info = vecFetchInfo(&ip);
                    void* out = vecGetPtr(info, lenA, fp);

                    VecFunctionBinary fcn = vecGetBinary(info.desc.dtype, info.desc.oper);
                    fcn(out, (void*) ptrA, (void*) ptrB, lenA);

                    PUSH(sp, (vmword) out);
                    PUSH(sp, lenA);
                    break;
                }

                case OC_VEC_VS: {
                    uint64_t val = POP(sp);
                    uint64_t len = POP(sp);
                    uint64_t ptr = POP(sp);

                    VecInfo info = vecFetchInfo(&ip);
                    void* out = vecGetPtr(info, len, fp);

                    VecFunctionScalar fcn = vecGetScalarR(info.desc.dtype, info.desc.oper);
                    fcn(out, (void*)ptr, val, len);

                    PUSH(sp, (vmword)out);
                    PUSH(sp, len);
                    break;
                }

                case OC_VEC_SV: {
                    uint64_t len = POP(sp);
                    uint64_t ptr = POP(sp);
                    uint64_t val = POP(sp);

                    VecInfo info = vecFetchInfo(&ip);
                    void* out = vecGetPtr(info, len, fp);

                    VecFunctionScalar fcn = vecGetScalarL(info.desc.dtype, info.desc.oper);
                    fcn(out, (void*) ptr, val, len);

                    PUSH(sp, (vmword) out);
                    PUSH(sp, len);
                    break;
                }

                case OC_VEC_UNARY: {
                    uint64_t len = POP(sp);
                    uint64_t ptr = POP(sp);

                    VecInfo info = vecFetchInfo(&ip);
                    void* out = vecGetPtr(info, len, fp);

                    VecFunctionUnary fcn = vecGetUnary(info.desc.dtype, info.desc.oper);
                    fcn(out, (void*) ptr, len);

                    PUSH(sp, (vmword) out);
                    PUSH(sp, len);
                    break;
                }

                case OC_VEC_CAST: {
                    uint64_t len = POP(sp);
                    uint64_t ptr = POP(sp);

                    VecInfo info = vecFetchInfo(&ip);
                    void* out = vecGetPtr(info, len, fp);

                    VecFunctionCast fcn = vecGetCast(info.desc.dtype, info.desc.srcDtype);
                    fcn(out, (void*) ptr, len);

                    PUSH(sp, (vmword) out);
                    PUSH(sp, len);
                    break;
                }

                case OC_VEC_LOAD_INDIRECT: {
                    // TODO
                    break;
                }

                case OC_VEC_STORE_INDIRECT: {
                    // TODO
                    break;
                }

                case OC_VEC_CAT: {
                    uint64_t lenB = POP(sp);
                    uint64_t ptrB = POP(sp);
                    uint64_t lenA = POP(sp);
                    uint64_t ptrA = POP(sp);

                    VecInfo info = vecFetchInfo(&ip);
                    void* out = vecGetPtr(info, lenA + lenB, fp);

                    memcpy(out, (void*) ptrA, lenA);
                    memcpy(((uint8_t*) out) + lenA, (void*) ptrB, lenB);

                    PUSH(sp, (vmword) out);
                    PUSH(sp, lenA + lenB);
                    break;
                }

                case OC_VEC_COPY: {
                    uint64_t len = POP(sp);
                    uint64_t ptr = POP(sp);

                    VecInfo info = vecFetchInfo(&ip);
                    void* out = vecGetPtr(info, len, fp);

                    int dtypeSize = Type::basicTypes[info.desc.dtype].size;
                    memcpy(out, (void*) ptr, len * dtypeSize);

                    PUSH(sp, (vmword) out);
                    PUSH(sp, len);
                    break;
                }

                case OC_VEC_FILL: {
                    uint64_t len = POP(sp);
                    uint64_t val = POP(sp);

                    VecInfo info = vecFetchInfo(&ip);
                    void* out = vecGetPtr(info, len, fp);

                    VecFunctionFill fcn = vecGetFill(info.desc.dtype);
                    fcn(out, val, len);

                    PUSH(sp, (vmword) out);
                    PUSH(sp, len);
                    break;
                }

                case OC_VEC_ALLOC: {
                    uint64_t len    = POP(sp);
                    uint64_t stride = FETCH(ip, uint64_t);

                    PUSH(sp, (vmword) vecAlloc(len * stride));
                    break;
                }

                case OC_VEC_TO_REF: {
                    uint64_t len = POP(sp);
                    uint64_t ptr = POP(sp);

                    Runtime::_Slice* slot = (Runtime::_Slice*) Arena::push(&vecContext.arena, sizeof(Runtime::_Slice));
                    slot->ptr = (char*) ptr;
                    slot->len = len;

                    PUSH(sp, (vmword) slot);
                    break;
                }

                case OC_VEC_MEM_RESET: {
                    vecResetMemory();
                    break;
                }

                case OC_VEC_RESET: {
                    DROP(sp, 2 * sizeof(vmword));
                    vecResetMemory();
                    break;
                }

                case OC_NOP: {
                    break;
                }

                default: {
                    Logger::log(logErr, "Opcode %s (%i) not yet implemented.\n", NULL, toStr(opcode), opcode);
                    return Err::NOT_YET_IMPLEMENTED;
                }
            }

        }
        loopEnd:

        ip = currBlock->bytecode + currBlock->bytecodeSize - sizeof(uint64_t);

        uint64_t ansSize = FETCH(ip, uint64_t);
        DROP(sp, ansSize);
        vmword* ans = sp;

        return out ? StackToVariable(ast, (uint8_t*)ans, ansSize, out) : Err::OK;
    }

    Err::Err exec(Reg::Unit* unit) {
        Function* main = Ast::Node::makeFunction();
        main->name.buff = NULL;
        main->name.len = 0;
        main->exe = unit->exe;
        main->base.scope = unit->ast->root;

        main->prototype.inArgCount = 0;
        main->prototype.inArgs = NULL;
        main->prototype.outArg = NULL;

        Scope mainScopeMem;
        Ast::Node::init(&mainScopeMem);
        main->bodyScope = &mainScopeMem;
        main->bodyScope->base.scope = unit->ast->root;

        Err::Err err = Interpreter::exec(unit->ast, main, NULL, 0, NULL);
        if (err != Err::OK) {
            Diag::report(unit->ast, nullptr, Err::UNEXPECTED_ERROR,
                Diag::Format { "Runtime error during top-level execution." });
        }

        return Err::OK;
    }

}
