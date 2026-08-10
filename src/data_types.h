#pragma once

#include "io.h"
#include "string.h"
#include <cstdint>



// TODO :
//   The entire 'type system' feels weird.
//   Conceptually, it feels correct to create explicit Type objects only
//   for fundamental types (int/float), user-defined structs/unions,
//   and enums.
//
//   In the case of fixed-length arrays, defining a unique Type object for each
//   size (int[5] vs int[6]) feels wasteful and incorrect. However,
//   avoiding this leads to a fragmented implementation where we cannot
//   simply inspect the Type object itself but must juggle extra metadata
//   (like AST nodes) to calculate basic properties like size.
//
//   I might refactor the 'type system' later, perhaps by adding cache nodes,
//   a lookup mechanism, or something else to make the code feel better.
//   But until then, I shall excuse myself and simply write what works to
//   get the job done.

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
        DT_POINTER,
        DT_STRING,
        DT_ARRAY,
        DT_SLICE,
        DT_MULTIPLE_TYPES,
        DT_CUSTOM, // TODO : rename as DT_STRUCT
        DT_UNION,
        DT_ERROR,
        DT_ENUM,
        DT_FUNCTION, // FunctionPrototype
        DT_MEMBER,
        DT_UNDEFINED,
        DT_COUNT,

        DT_INT  = DT_I64, // TODO : delete?
        DT_BOOL = DT_U64,
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
        uint32_t size;
        uint32_t align;
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

    struct ArrayInfo {
        TypeInfo  base;
        TypeInfo* element;
        uint64_t  elementCount;
    };

    struct PointerInfo {
        TypeInfo  base;
        TypeInfo* element;
    };

    struct TypeInfoEx {
        union {
            TypeInfo base;
            StructInfo  str; // TODO : better name?
            ArrayInfo   arr;
            PointerInfo ptr;
        };
    };

    // Definition for each 'enum Kind' value
    extern TypeInfo basicTypes[DT_COUNT];
    extern TypeInfoEx* usersTypes;



    inline int isInt(int x) {
        return x >= DT_I8 && x <= DT_U64;
    }

    inline int isSignedInt(int x) {
        return x >= DT_I8 && x <= DT_I64;
    }

    inline int isUnsignedInt(int x) {
        return x >= DT_U8 && x <= DT_U64;
    }

    inline int isFloat(int x) {
        return x >= DT_F32 && x <= DT_F64;
    }

    inline int isTruthy(int x) {
        return isInt(x);
    }

    inline int isPrimitive(int x) {
        return (x >= DT_I8 && x <= DT_F64) || x == DT_POINTER;
    }

    inline int isBasic(int x) {
        return (x > Type::DT_VOID && x <= Type::DT_F64);
    }

    inline int isStructLike(int x) {
        return x == DT_CUSTOM || x == DT_UNION;
    }

    inline int isIndexable(int x) {
        return x == DT_POINTER || x == DT_ARRAY || x == DT_SLICE;
    }

    Type::TypeInfo* getDtype(void* data, Type::Kind dtype);

    const char* str(Kind kind);

    void writeTypeName(IO::Stream* stream, void* type, Type::Kind typeKind);

};
