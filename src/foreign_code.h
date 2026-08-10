#pragma once

#include "data_types.h"
#include "syntax.h"
#include "diagnostic.h"
#include <cstdint>



// namespace Reg { struct Unit; }

namespace Extern {

    namespace Abi {

        // This is more 'self ABI' info, that may simplify
        // asm code, as it can be used as contract just between
        // driver functions, compiler will not rely on it and
        // use it ex as debug info
        enum ArgKind : uint8_t {
            AK_INT,
            AK_INT_UP,
            AK_FLOAT,
            AK_FLOAT_UP,
            AK_AGGREGATE,
            AK_NONE,
        };

        // This is foremost description for the compiler how
        // argument shall be passed, how they have to be set
        // for the call (into already compiled ffi context).
        //
        // ABI drivers internally does not have to follow
        // these descriptions in the most accurate way, or
        // build around them, its just a way to define form
        // of input arguments..
        enum ArgPassKind : uint8_t {
            PK_REG_INT,
            PK_REG_INT_UP,
            PK_REG_FLOAT,
            PK_REG_FLOAT_UP,

            PK_REG_STRUCT,
            PK_REG_STRUCT_SPLIT,
            PK_REG_STRUCT_REFERENCE,

            PK_MEM_STRUCT,
            PK_MEM_STRUCT_REFERENCE,
        };

        struct Arg {
            union {
                uint64_t i64;
                double   f64;
                void*    ptr; // points to the struct data
            } data;
            uint32_t size;    // size of the data in bytes

            uint32_t offset; // either stack offset or reg id/number

            ArgKind     kind;
            ArgPassKind pass;
        };

        // TODO : we are reusing internal type system definition,
        // which may allow for some convenient usage, but I am not
        // sure if this has to be the case, as it may lead to weird
        // behaviour, when, although same interface, the interpretation,
        // of data may slightly differ for and between abis
        struct TypeInfo {
            Type::TypeInfoEx* info;
            Abi::ArgKind      argKind;
        };

        struct LayoutConfig {
            uint32_t wordSize;
            uint32_t minAlign;
            uint32_t maxAlign;
        };

        struct Driver {
            uint8_t  iRegCount; // i as integer
            uint8_t  fRegCount; // f as float
            bool     isUniform;
            uint32_t stackAlign;
            uint32_t indirectAlignment; // alignment of memory provided by a caller, like passing a struct by reference

            LayoutConfig layout;

            void        (*classify) (Arg* arg, Value* value);
            void        (*invoke)   (CallContext* ctx);
        };

        struct CallContext {
            Arg*     args;
            uint32_t argCount;

            uint32_t argStackSize; // stack space needed for all args for this call

            Arg      retArg;

            void*    target;

            Driver*  abi;
        };

        const char* str(ArgPassKind pass);

        bool isRegFloat(ArgPassKind pass);
        bool isRegInt(ArgPassKind pass);

        Err::Err        ensureTypeInfoReady (AstContext* ast, TypeDefinition* td, Driver* driver);
        Type::TypeInfo* computeTypeInfo     (Abi::LayoutConfig* cfg, Type::TypeInfo*  tempInfo);

        // Fills ctx->args using syntax nodes
        void fillArgs      (AstContext* ast, Abi::CallContext* ctx, Variable** args, uint32_t argCount);
        // Fills ctx->args using vms stack
        void fillArgsFromVM(AstContext* ast, Abi::CallContext* ctx, uintptr_t sp, uint32_t argCount);


        // AST Variable -> ABI Arg
        // 'dest' shall be pre-allocated to fit the data.
        Err::Err marshal(AstContext* ast, Variable* src, uint8_t* dest, Type::TypeInfo* typeInfo);

        // ABI Arg -> AST Variable
        // 'dest' shall be pre-allocated to fit the data.
        Err::Err unmarshal(AstContext* ast, uint8_t* src, Variable* dest, Type::TypeInfo* typeInfo);

        // ABI Arg A -> ABI Arg B
        // Directly moves data between two ABI representations.
        // Essential for the VM to move a return value from a DLL
        // 'Arg' into a VM Stack 'Arg' layout without using AST as
        // middleman.
        Err::Err transcode(AstContext* ast, uint8_t* src, uint8_t* dest, Type::TypeInfo* srcTypeInfo, Type::TypeInfo* destTypeInfo);

        Driver* getTargetDriver();

        extern Driver win64;
        extern Driver sysV;

    }

    enum LibraryType {
        LT_STATIC,
        LT_SHARED,
    };

    enum LibraryLoadLevel {
        // Library is ready to be examined but no code will be able to run
        LL_INSPECT,
        // Library is fully loaded to memory and ready for execution
        LL_EXECUTE,
    };

    struct Library {
        String name;
        String libPath;  // Path to .lib or .a
        String dllPath;  // Path to .dll or .so
        bool   isStatic; // true if libPath is a 'true' static

        LibraryLoadLevel loadLevel;

        void* osHandle;
    };

    typedef Library* LibraryHandle;



    // Inits globals, so has to be called only once
    void init();

    Err::Err loadLibrary(AstContext* ast, String name, LibraryLoadLevel level, LibraryHandle* out);

    // Ensures that function exists in previously loaded library and
    // binds address in case of LL_EXECUTE
    Err::Err resolveFunction(AstContext* ast, Function* fcn);

    // Upgrades the associated library to LL_EXECUTE, resolves the native
    // address, and builds the ABI CallContext. After this call function
    // is ready to be invoked.
    Err::Err compile(AstContext* ast, Abi::Driver* abi, Function* fcn);

    // Invokes function from previously bound shared library
    //
    // VM -> VM
    // abi args have to be filled beforehand
    Err::Err invoke(AstContext* ast, Abi::Driver* abi, Function* fcn, uint8_t* out);

    // VM -> AST
    // abi args have to be filled beforehand
    Err::Err invoke(AstContext* ast, Abi::Driver* abi, Function* fcn, Variable* out);

    // AST -> AST
    Err::Err invoke(AstContext* ast, Abi::Driver* abi, Function* fcn, Variable** args, uint32_t argCount, Variable* out);

}
