#pragma once

#include "io.h"
#include "keywords.h"
#include "string.h"
#include <cstdint>



struct SyntaxNode;
namespace Extern { namespace Abi { struct TypeInfo; } };

namespace Type {

    // Has to be u8 to be used in bytecode
    enum Kind : uint8_t {
        DT_VOID = 0,

        DT_I8,
        DT_I16,
        DT_I32,
        DT_I64,
        DT_U8,
        DT_U16,
        DT_U32,
        DT_U64,
        DT_F32,
        DT_F64,

        // Scalar Pointer type. Tracks pointer metadata via 'PointerInfo'.
        DT_POINTER,

        // Aggregate Container Types
        DT_ARRAY,   // Fixed-size compile-time array
        DT_SLICE, // Dynamic runtime slice view (pointer + length pair)

        // Meta-type:
        // Range specifier (start:step:end).
        DT_RANGE,

        // Meta-type:
        // Placeholder for variadic arguments
        DT_MULTIPLE_TYPES,

        // User-defined aggregate types
        DT_STRUCT,
        DT_UNION,
        DT_ERROR,
        DT_ENUM,

        // FunctionPrototype
        DT_FUNCTION,

        // Meta-type:
        // represents the right-hand side of a member selection
        // expression (e.g., 'b' in 'a.b'). Holds 'StructMemberInfo'
        // as its payload, while the expression evaluates to the
        // member's resolved type.
        DT_MEMBER,

        // Type was tried to be resolved but failed
        DT_UNDEFINED,

        DT_COUNT,

        DT_INT  = DT_I64, // TODO : delete?
        DT_BOOL = DT_U64,
    };

    enum Qualifier : uint8_t {
        Q_NONE,
        Q_EMBED = 1 << 1,
        Q_CONST = 1 << 2,
        Q_FLUID = 1 << 3,
        Q_ALLOC = 1 << 4
    };

    // Expected to be used during resolving to signal status
    enum ResolutionStatus : int64_t {
        RS_CONCRETE  = 0,
        RS_AMBIGUOUS = 1, // Contains inferred dimensions ([?]), uses tmpArena
    };

    // As we may want to use it in runtime
    // we define own string type, so there is
    // clear and independent version
    struct _String {
        char*    buff;
        uint64_t len;
    };

    struct TypeInfo {
        Kind     kind;
        uint8_t  rank;
        uint8_t  align;
        uint32_t size;

        // NOTE: We also try to put this into TypeInfoEx only
        //       and resolve it via a function, while abis
        //       providing theirs definitions to basic types...
        // NOTE: For now only one ABI against which we may compile
        Extern::Abi::TypeInfo* abi;
    };

    struct StructMemberInfo {
        _String   name;
        TypeInfo* type;
        uint64_t  offset;
    };

    // TODO : when implementing packed structs, add
    // flag to know if fields are aligned or not.
    struct StructInfo {
        TypeInfo base;
        _String  name;
        uint64_t memberCount;
        StructMemberInfo* members;
    };

    struct EnumMemberInfo {
        _String name;
        int64_t value;
    };

    struct EnumInfo {
        TypeInfo base;
        _String  name;
        Kind     memberKind;
        uint64_t memberCount;
        EnumMemberInfo* members;
    };

    struct ArrayInfo {
        TypeInfo  base;
        TypeInfo* element;
        uint64_t  elementCount;
    };

    struct SliceInfo {
        TypeInfo  base;
        TypeInfo* element;
        uint64_t  flags; // to distinguish between [const], [auton], [muton] etc.
    };

    struct PointerInfo {
        TypeInfo  base;
        TypeInfo* element;
    };

    struct FunctionInfo {
        TypeInfo   base;
        TypeInfo** argTypes;
        uint64_t   argCount;
        TypeInfo*  retType;
        uint64_t   flags;
    };

    // TODO : we have to somehow ensure that everything 'indexable'
    //        inherit from pointer
    //        Type::getMember ?

    struct TypeInfoEx {
        union {
            TypeInfo     base;
            StructInfo   str; // TODO : better name?
            ArrayInfo    arr;
            SliceInfo    slc;
            PointerInfo  ptr;
            FunctionInfo fcn;
            EnumInfo     en;
        };
        SyntaxNode* astNode;
    };

    // Definition for each 'enum Kind' value
    extern TypeInfo basicTypes[DT_COUNT];
    extern TypeInfoEx* usersTypes;

    constexpr int64_t ARRAY_LEN_UNKNOWN = -1;



    inline int isInt(int x) {
        return x >= DT_I8 && x <= DT_U64;
    }

    inline int isInt(TypeInfo* x) {
        return isInt(x->kind);
    }

    inline int isSignedInt(int x) {
        return x >= DT_I8 && x <= DT_I64;
    }

    inline int isSignedInt(TypeInfo* x) {
        return isSignedInt(x->kind);
    }

    inline int isUnsignedInt(int x) {
        return x >= DT_U8 && x <= DT_U64;
    }

    inline int isUnsignedInt(TypeInfo* x) {
        return isUnsignedInt(x->kind);
    }

    inline int isFloat(int x) {
        return x >= DT_F32 && x <= DT_F64;
    }

    inline int isFloat(TypeInfo* x) {
        return isFloat(x->kind);
    }

    inline int isTruthy(int x) {
        return isInt(x);
    }

    inline int isTruthy(TypeInfo* x) {
        return isInt(x->kind);
    }

    inline int isPrimitive(int x) {
        return (x >= DT_I8 && x <= DT_F64) || x == DT_POINTER;
    }

    inline int isPrimitive(TypeInfo* x) {
        return isPrimitive(x->kind);
    }

    inline bool isScalar(int x) {
        return isPrimitive(x) || x == DT_ENUM;
    }

    inline bool isScalar(TypeInfo* x) {
        return isScalar(x->kind);
    }

    inline bool isIntegerOrEnum(int x) {
        return (x >= DT_I8 && x <= DT_U64) || x == DT_ENUM;
    }

    inline bool isIntegerOrEnum(TypeInfo* x) {
        return isIntegerOrEnum(x->kind);
    }

    inline int isBasic(int x) {
        return (x > Type::DT_VOID && x <= Type::DT_F64);
    }

    inline int isBasic(TypeInfo* x) {
        return isBasic(x->kind);
    }

    inline int isStructLike(int x) {
        return x == DT_STRUCT || x == DT_UNION;
    }

    inline int isStructLike(TypeInfo* x) {
        return isStructLike(x->kind);
    }

    inline int isArrayLike(int x) {
        return x == DT_ARRAY || x == DT_SLICE;
    }

    inline int isArrayLike(TypeInfo* x) {
        return isArrayLike(x->kind);
    }

    inline int isIndexable(int x) {
        return x == DT_POINTER || x == DT_ARRAY || x == DT_SLICE;
    }

    inline int isIndexable(TypeInfo* x) {
        return isIndexable(x->kind);
    }

    // TODO: better name?
    inline Kind getUnderlyingKind(TypeInfo* type) {
        if (type->kind == DT_ENUM) {
            EnumInfo* eInfo = (EnumInfo*) type;
            return eInfo->memberKind;
        }

        return type->kind;
    }

    void init();
    void release();

    TypeInfo* tmpMakePointer(TypeInfo* info);
    TypeInfo* tmpMakeArray(TypeInfo* info, int64_t len);
    TypeInfo* tmpMakeSlice(TypeInfo* info, uint64_t flags);

    bool isTmp(TypeInfo* type);
    void tmpClear();

    TypeInfo* makePointer(TypeInfo* info);
    TypeInfo* makeArray(TypeInfo* info, int64_t len);
    TypeInfo* makeSlice(TypeInfo* info, uint64_t flags);

    EnumMemberInfo* findMember(EnumInfo* type, String name);
    EnumMemberInfo* findMember(EnumInfo* type, String* name);
    EnumMemberInfo* findMember(EnumInfo* type, String name, int* idx);
    EnumMemberInfo* findMember(EnumInfo* type, String* name, int* idx);

    StructMemberInfo* findMember(StructInfo* type, String name);
    StructMemberInfo* findMember(StructInfo* type, String* name);
    StructMemberInfo* findMember(StructInfo* type, String name, int* idx);
    StructMemberInfo* findMember(StructInfo* type, String* name, int* idx);

    const char* str(Kind kind);
    const char* str(TypeInfo* info);

    void writeTypeName(IO::Stream* stream, Type::TypeInfo* type);

};
