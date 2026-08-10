#pragma once

#include <cstdint>

#include "data_types.h"
#include "dynamic_arena.h"
#include "io.h"
#include "operators.h"
#include "ordered_dict.h"
#include "registry.h"
#include "syntax.h"
#include "diagnostic.h"
#include "validator.h"



/**
 * CALL STACK
 * --------------------------------------
 * This architecture was chosen as a compromise to keep the final bytecode simple
 * for non-variadic cases, while ensuring all local offsets remain fixed and known
 * at compile-time relative to the Frame Pointer (fp).
 *
 * A function's stack frame is divided into four logical groups:
 *   1. Required Arguments
 *   2. Default Arguments
 *   3. Scope Variables (Internal Locals)
 *   4. Variadic Arguments (Varargs)
 *
 * The first three groups have a fixed size known at compile-time and are collectively
 * referred to as 'locals'. Although variadic arguments are technically part of the
 * function’s stack frame, they are not included in this so-called 'definition' as they
 * dont obey this property. Fixed portion of the stack frame remains the same across
 * all calls, and therefore can be determined once at compile time and reused during
 * code generation or exection.
 *
 * DESIGN CHOICE:
 * We anchor 'fp' at the first Required Argument. To keep the layout contiguous,
 * varargs have to be plaace either before or after the locals. The latter was
 * chosen.
 *
 * Rationale:
 * For non-variadic functions, this keeps the call/return logic trivial.
 * Variadic functions require slightly more work at runtime (shifting the vararg
 * section to insert scope locals), but this was deemed an acceptable trade-off
 * since variadic calls are less frequent during compile-time execution. While
 * it is possible to insert a stack-grow instruction to allocate space naturally,
 * this approach becomes non-trivial for one-pass compilation if recursive calls
 * has to be handled.
 *
 * COMPILER METADATA:
 * During compilation, the following properties are recorded in the ExeBlock:
 *   - exe->locals:          A template array containing initial values for
 *                           Default Args and Scope Locals. Used for fast dispatch
 *                           via memcpy.
 *
 *                           Required arguments are not included, since they must
 *                           always be pushed explicitly onto the stack and are
 *                           therefore inherently call-specific.
 *   - exe->fixedSize:       Byte size of the (Req Args + Def Args + Scope Vars).
 *   - exe->defaultArgsSize: Byte size of the Default Arguments section.
 *
 * STACK STATE: IMMEDIATELY BEFORE OC_CALL
 * ------------------------------------------
 * (Address Increases Downwards)
 *
 *  Offset | Content                   | Comment
 * --------|---------------------------|------------------------------
 *         | [ Reserved Slot (IP)    ] | Empty slots reserved by the
 *         | [ Reserved Slot (FP)    ] | caller for VM metadata.
 * --------|---------------------------|------------------------------
 *         | [ Required Argument 1   ] |
 *         | [ ...                   ] | Arguments pushed by the
 * --------|---------------------------| caller onto the operand stack
 *         | [ Default Argument 1    ] |
 *         | [ ...                   ] |
 * --------|---------------------------|------------------------------
 *         | [ Vararg 1 (Any)        ] | Varargs are pushed as 'Any'
 *         | [ Vararg N (Any)        ] | pairs:
 *  sp     | [ <vararg count>        ] | {u64: TypeInfo*, u64: Value}
 * --------|---------------------------|------------------------------
 *
 *
 * STACK STATE: AFTER OC_CALL EXECUTION
 * ---------------------------------------
 *
 *  Offset | Content                   | Comment
 * --------|---------------------------|--------------------
 *  fp - 2 | [ Saved IP              ] | Restored on OC_RET
 *  fp - 1 | [ Saved FP              ] |
 * --------|---------------------------|--------------------   -[ exe:
 *  fp     | [ Required Argument 1   ] | Existing data          [ f
 *         | [ ...                   ] |                        [ i
 * --------|---------------------------|--------------- -[ exe: [ x  -[ exe:
 *         | [ Default Argument 1    ] | Existing data   [ l    [ e   [ defaultArgsSize
 *         | [ ...                   ] |                 [ o    [ d   [
 * --------|---------------------------|---------------  [ c    [ S  -[
 *         | [ Scope Local 1         ] | Initialized via [ a    [ i
 *         | [ ...                   ] | memcpy by VM    [ l    [ z
 * --------|---------------------------|--------------- -[ s   -[ e
 *         | [ Vararg 1 (Any)        ] | Shifted "down" in memory
 *         | [ Vararg N (Any)        ] | by the VM to make room
 *  sp - 1 | [ <vararg count>        ] | for Scope Locals
 * --------|---------------------------|-----------------------------
 */



namespace Interpreter {

    typedef uint64_t vmword;
    #define BYTES_TO_WORDS(bsize) (((bsize) + 7) >> 3)

    union vmvalue {
        int32_t  i32;
        uint32_t u32;
        int64_t  i64;
        uint64_t u64;
        float    f32;
        double   f64;
    };

    // TODO : sync with Type::Kind
    // order of dtypes matters, see DtypeOffset
    enum Opcode : uint8_t {
        OC_PUSH_I8,
        OC_PUSH_U8,
        OC_PUSH_I16,
        OC_PUSH_U16,
        OC_PUSH_I32,
        OC_PUSH_U32,
        OC_PUSH_I64,
        OC_PUSH_U64,
        OC_PUSH_F32,
        OC_PUSH_F64,
        OC_PUSH_PTR,
        OC_PUSH_BLOB,

        OC_SET_I8,
        OC_SET_U8,
        OC_SET_I16,
        OC_SET_U16,
        OC_SET_I32,
        OC_SET_U32,
        OC_SET_I64,
        OC_SET_U64,
        OC_SET_F32,
        OC_SET_F64,
        OC_SET_PTR,
        OC_SET_BLOB,

        OC_GET_I8,
        OC_GET_U8,
        OC_GET_I16,
        OC_GET_U16,
        OC_GET_I32,
        OC_GET_U32,
        OC_GET_I64,
        OC_GET_U64,
        OC_GET_F32,
        OC_GET_F64,
        OC_GET_PTR,
        OC_GET_BLOB,

        OC_LOAD_I8,
        OC_LOAD_U8,
        OC_LOAD_I16,
        OC_LOAD_U16,
        OC_LOAD_I32,
        OC_LOAD_U32,
        OC_LOAD_I64,
        OC_LOAD_U64,
        OC_LOAD_F32,
        OC_LOAD_F64,
        OC_LOAD_PTR,
        OC_LOAD_BLOB,

        OC_STORE_I8,
        OC_STORE_U8,
        OC_STORE_I16,
        OC_STORE_U16,
        OC_STORE_I32,
        OC_STORE_U32,
        OC_STORE_I64,
        OC_STORE_U64,
        OC_STORE_F32,
        OC_STORE_F64,
        OC_STORE_PTR,
        OC_STORE_BLOB,

        // TODO : to docs:
        //        stack:    u64: local offsets
        //        operands: u64: ExeBlock ptr
        OC_SET_GLOBAL_I8,
        OC_SET_GLOBAL_U8,
        OC_SET_GLOBAL_I16,
        OC_SET_GLOBAL_U16,
        OC_SET_GLOBAL_I32,
        OC_SET_GLOBAL_U32,
        OC_SET_GLOBAL_I64,
        OC_SET_GLOBAL_U64,
        OC_SET_GLOBAL_F32,
        OC_SET_GLOBAL_F64,
        OC_SET_GLOBAL_PTR,
        OC_SET_GLOBAL_BLOB,

        OC_GET_GLOBAL_I8,
        OC_GET_GLOBAL_U8,
        OC_GET_GLOBAL_I16,
        OC_GET_GLOBAL_U16,
        OC_GET_GLOBAL_I32,
        OC_GET_GLOBAL_U32,
        OC_GET_GLOBAL_I64,
        OC_GET_GLOBAL_U64,
        OC_GET_GLOBAL_F32,
        OC_GET_GLOBAL_F64,
        OC_GET_GLOBAL_PTR,
        OC_GET_GLOBAL_BLOB,

        OC_LEA,
        OC_LEA_CONST,
        OC_LEA_GLOBAL,
        OC_PTR_IDX,

        OC_POP,
        OC_POP_N,
        OC_DUP,
        OC_CROP,
        OC_SWAP,

        OC_NEG_I32,
        OC_NEG_U32,
        OC_NEG_I64,
        OC_NEG_U64,
        OC_NEG_F32,
        OC_NEG_F64,

        OC_ADD_I32,
        OC_ADD_U32,
        OC_ADD_I64,
        OC_ADD_U64,
        OC_ADD_F32,
        OC_ADD_F64,

        OC_SUB_I32,
        OC_SUB_U32,
        OC_SUB_I64,
        OC_SUB_U64,
        OC_SUB_F32,
        OC_SUB_F64,

        OC_MUL_I32,
        OC_MUL_U32,
        OC_MUL_I64,
        OC_MUL_U64,
        OC_MUL_F32,
        OC_MUL_F64,

        OC_DIV_I32,
        OC_DIV_U32,
        OC_DIV_I64,
        OC_DIV_U64,
        OC_DIV_F32,
        OC_DIV_F64,

        OC_MOD_I32,
        OC_MOD_U32,
        OC_MOD_I64,
        OC_MOD_U64,

        OC_AND_I32,
        OC_AND_U32,
        OC_AND_I64,
        OC_AND_U64,

        OC_OR_I32,
        OC_OR_U32,
        OC_OR_I64,
        OC_OR_U64,

        OC_XOR_I32,
        OC_XOR_U32,
        OC_XOR_I64,
        OC_XOR_U64,

        OC_SHL_I32,
        OC_SHL_U32,
        OC_SHL_I64,
        OC_SHL_U64,

        OC_SHR_I32,
        OC_SHR_U32,
        OC_SHR_I64,
        OC_SHR_U64,

        OC_EQ_I32,
        OC_EQ_I64,
        OC_EQ_F32,
        OC_EQ_F64,

        OC_NE_I32,
        OC_NE_I64,
        OC_NE_F32,
        OC_NE_F64,

        OC_LT_I32,
        OC_LT_U32,
        OC_LT_I64,
        OC_LT_U64,
        OC_LT_F32,
        OC_LT_F64,

        OC_LE_I32,
        OC_LE_U32,
        OC_LE_I64,
        OC_LE_U64,
        OC_LE_F32,
        OC_LE_F64,

        OC_GT_I32,
        OC_GT_U32,
        OC_GT_I64,
        OC_GT_U64,
        OC_GT_F32,
        OC_GT_F64,

        OC_GE_I32,
        OC_GE_U32,
        OC_GE_I64,
        OC_GE_U64,
        OC_GE_F32,
        OC_GE_F64,

        OC_NOT_BOOL,

        OC_BOOL_I32, // also for unsigned
        OC_BOOL_I64, // also for unsigned/ptr
        OC_BOOL_F32,
        OC_BOOL_F64,

        OC_SEXT_32_TO_64, // sign extended
        OC_ZEXT_32_TO_64, // zero extended
        OC_TRUNC_64_TO_32,

        OC_CAST_I32_TO_U32,
        OC_CAST_I32_TO_F32,
        OC_CAST_I32_TO_F64,

        OC_CAST_U32_TO_I32,
        OC_CAST_U32_TO_F32,
        OC_CAST_U32_TO_F64,

        OC_CAST_I64_TO_U64,
        OC_CAST_I64_TO_F32,
        OC_CAST_I64_TO_F64,

        OC_CAST_U64_TO_I64,
        OC_CAST_U64_TO_F32,
        OC_CAST_U64_TO_F64,

        OC_CAST_F32_TO_I32,
        OC_CAST_F32_TO_U32,
        OC_CAST_F32_TO_I64,
        OC_CAST_F32_TO_U64,
        OC_CAST_F32_TO_F64,

        OC_CAST_F64_TO_I32,
        OC_CAST_F64_TO_U32,
        OC_CAST_F64_TO_I64,
        OC_CAST_F64_TO_U64,
        OC_CAST_F64_TO_F32,

        OC_JUMP,
        OC_JUMP_IF_TRUE, // pops
        OC_JUMP_IF_FALSE, // pops

        OC_CALL,
        OC_RET, // TODO separate RET_BLOB

        OC_GROW,

        OC_NOP,
        OC_HALT,

        // general fat-vector instructions
        OC_VEC_VV,
        OC_VEC_VS,
        OC_VEC_SV,
        OC_VEC_UNARY,
        OC_VEC_CAT,
        OC_VEC_COPY,
        OC_VEC_FILL,
        OC_VEC_LOAD_INDIRECT,
        OC_VEC_STORE_INDIRECT,
        OC_VEC_CAST,
        OC_VEC_ALLOC,
        OC_VEC_TO_REF,
        OC_VEC_MEM_RESET,
        OC_VEC_RESET,

        OC_COUNT
    };
    static_assert(OC_COUNT <= 256);

    enum DtypeOffset {
        OFF_I8,
        OFF_U8,
        OFF_I16,
        OFF_U16,
        OFF_I32,
        OFF_U32,
        OFF_I64,
        OFF_U64,
        OFF_F32,
        OFF_F64,
        OFF_PTR,
        OFF_GENERIC,
    };

    enum VecDescriptorEncoding : uint64_t {
        DE_DTYPE_SHIFT     = 0,
        DE_OPER_SHIFT      = 8,
        DE_SRC_DTYPE_SHIFT = 16,
        DE_FLAGS_SHIFT     = 32,

        DE_DTYPE_MASK     = ((uint64_t) 0xFF) << DE_DTYPE_SHIFT,
        DE_OPER_MASK      = ((uint64_t) 0xFF) << DE_OPER_SHIFT,
        DE_SRC_DTYPE_MASK = ((uint64_t) 0xFF) << DE_SRC_DTYPE_SHIFT,
        DE_FLAGS_MASK     = ((uint64_t) 0xFFFFFFFF) << DE_FLAGS_SHIFT,

        // F as Flags, as names are pretty long at this point
        DE_F_DEST_SHIFT  = 31,
        DE_F_LEFT_SHIFT  = 30,
        DE_F_RIGHT_SHIFT = 29,

        DE_F_DEST  = 1U << DE_F_DEST_SHIFT,
        DE_F_LEFT  = 1U << DE_F_LEFT_SHIFT,
        DE_F_RIGHT = 1U << DE_F_RIGHT_SHIFT,
    };

    struct VecDescriptor {
        Type::Kind   dtype;
        OperatorEnum oper;
        Type::Kind   srcDtype; // for cast
        uint8_t      reserved;
        uint32_t     flags;
    };
    static_assert(sizeof(VecDescriptor) == sizeof(vmword), "VecDescriptor must match 'vmword' size!");

    struct VecInfo {
        uint64_t      dest;
        VecDescriptor desc;
    };

    struct LocalVarInfo {
        // for now enough
        Variable* var;
        uint64_t  size;
        uint64_t  align;
    };

    struct LineInfo {
        uint64_t ocOffsetStart;
        uint64_t ocOffsetEnd; // TODO : deprecate
        Span span;
    };

    struct VecResult {
        bool isTmp;
    };

    struct CompilerState {
        // We dont want to populate locals with call args
        // as they naturaly grow on stack. But we want
        // keep here default args and scope locals so we
        // can speedup call stack construction.
        //
        // NOTE : localPos is used for dummy offset calculation
        Arena::Container locals;
        //Controls if locals are populated during offset calculation
        bool populateLocals;

        Arena::Container bytecode;

        // stores raw const data such as string literals that
        // bytecode can refer to
        Arena::Container rawData;

        // keys are locals offsets, values are instances of LocalVarInfo
        OrderedDict::Container localsInfoMap;

        // lines spans to map bytecode and lines in source code
        DArray::Container lines;

        // Helper stack
        DArray::Container tmpStack;

        // to track and build line spans
        Span currentLineSpan;
        uint64_t currentOffsetStart;

        uint64_t currentLoopAddress;

        // max encoutered alignment during building locals stack
        uint64_t maxAlign;

        uint64_t fixedSize;
        uint64_t defaultArgsSize;

        VecResult vecResult;

        // vectorized opperations metadata
        uint64_t maxArrayLiteralSize; // so we can prealocated just enough memory
        uint64_t currentArrayLiteralOffset; // offset in prealocated block

        // Pointer of the result instance of the compilation
        // Used to be linked to some symbols during compilation
        // if there is a need to know the owning block.
        ExeBlock* exe;

        Opcode lastOpcode;
    };

    struct ExeBlock {
        // raw bytes for initial local variable state
        uint8_t* locals;
        uint64_t localsSize;

        uint8_t* bytecode;
        uint64_t bytecodeSize;

        uint8_t* rawData;
        uint64_t rawDataSize;

        LineInfo* lines;
        uint64_t linesSize;

        // maps offsets in locals to their info as LocalVarInfo
        OrderedDict::Container* localsInfoMap;

        // for now like bool, later if there will be
        // more characteristics transform to flag
        bool isVariadic;

        // in bytes
        uint64_t fixedSize;
        uint64_t defaultArgsSize;

        // current frame pointer during execution;
        // null when block is not currently being executed
        // liveFp must be saved/restored on call/ret… For now
        // it's safe to do nothing on ret
        uint8_t* liveFp;

        // original 'ast' node its based on
        SyntaxNode* node;
    };

    inline void clear(CompilerState* state) {
        Arena::clear(&state->locals);
        Arena::clear(&state->bytecode);
        Arena::clear(&state->rawData);
        DArray::clear(&state->lines);
        DArray::clear(&state->tmpStack);
        OrderedDict::clear(&state->localsInfoMap);

        state->populateLocals = false;
        state->currentLineSpan = { 0, 0 };
        state->currentOffsetStart = 0;
        state->maxAlign = 0;
        state->fixedSize = 0;
        state->defaultArgsSize = 0;
        state->vecResult = { 0 };
        state->maxArrayLiteralSize = 0;
        state->currentArrayLiteralOffset = 0;
        state->lastOpcode = Interpreter::OC_NOP;
        state->exe = NULL;
    }

    inline ExeBlock* makeExeBlock() {
        ExeBlock* exe = (ExeBlock*) alloc(alc, sizeof(ExeBlock));
        memset(exe, 0, sizeof(ExeBlock));
        return exe;
    }



    // TODO: manage init/clear/release flow better
    void initDebug(CompilerState* state);
    void initBuild(CompilerState* state);
    void initExec (CompilerState* state);
    void initEval (CompilerState* state);
    inline void init(CompilerState* state) {
        initDebug(state);
        initBuild(state);
        initExec (state);
        initEval (state);
    }

    void releaseDebug(CompilerState* state);
    void releaseBuild(CompilerState* state);
    void releaseExec (CompilerState* state);
    void releaseEval (CompilerState* state);
    inline void release(CompilerState* state) {
        releaseDebug(state);
        releaseBuild(state);
        releaseExec (state);
        releaseEval (state);
    }

    Err::Err compile(CompilerState* state, Function* fcn);
    Err::Err compile(CompilerState* state, Reg::Unit* unit);

    Err::Err eval(Validator::ValidationContext* ctx, Variable* var);
    Err::Err exec(AstContext* ast, Function* fcn, Variable** args, uint64_t argCount, Variable* out);
    Err::Err exec(Reg::Unit* unit);

    void print(IO::Stream* stream, Reg::Unit* unit);
    void print(IO::Stream* stream, ExeBlock* block);
    void print(IO::Stream* stream, Function* fcn, uint64_t depth = 0);

    const char* toStr(Opcode opcode);

    uintptr_t getExeFramePointer(uint32_t alignment, uint32_t* stackSize);

    vmword encodeVecDescriptor(const VecDescriptor desc);
    VecDescriptor decodeVecDescriptor(const vmword word);

    LineInfo* findLineForOffset(ExeBlock * block, uint64_t targetOffset);

};
