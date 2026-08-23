#include "data_types.h"
#include "allocator.h"
#include "dynamic_arena.h"
#include "globals.h"
#include "io.h"
#include "set.h"
#include "string.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>



namespace Type {

    TypeInfo basicTypes[] = {
        {
            .kind  = DT_VOID,
            .rank  = 0,
            .align = 1,
            .size  = 0,
        },

        {
            .kind  = DT_I8,
            .rank  = 1,
            .align = 1,
            .size  = 1,
        },

        {
            .kind  = DT_I16,
            .rank  = 2,
            .align = 2,
            .size  = 2,
        },

        {
            .kind  = DT_I32,
            .rank  = 3,
            .align = 4,
            .size  = 4,
        },

        {
            .kind  = DT_I64,
            .rank  = 4,
            .align = 8,
            .size  = 8,
        },

        {
            .kind  = DT_U8,
            .rank  = 1,
            .align = 1,
            .size  = 1,
        },

        {
            .kind  = DT_U16,
            .rank  = 2,
            .align = 2,
            .size  = 2,
        },

        {
            .kind  = DT_U32,
            .rank  = 3,
            .align = 4,
            .size  = 4,
        },

        {
            .kind  = DT_U64,
            .rank  = 4,
            .align = 8,
            .size  = 8,
        },

        {
            .kind  = DT_F32,
            .rank  = 6,
            .align = 4,
            .size  = 4,
        },

        {
            .kind  = DT_F64,
            .rank  = 7,
            .align = 8,
            .size  = 8,
        }
    };

    // TODO : to config
    constexpr uint64_t typeSetInitSize = 16 * 1024;

    // to allocate placeholder nodes, so we can
    // reset it independently from main allocator
    Arena::Container tmpArena;

    Set::Container setPointer;
    Set::Container setArray;
    Set::Container setSlice;

    std::atomic_bool setPointerLock;
    std::atomic_bool setArrayLock;
    std::atomic_bool setSliceLock;

    struct SetSlot {
        uint64_t  hash;
        TypeInfo* type;
    };



    void lockSet(std::atomic_bool* key) {
        while (true) {
            bool expected = false;
            key->compare_exchange_strong(expected, true, std::memory_order_acquire);

            if (expected) {
                key->wait(true, std::memory_order_relaxed);
            } else {
                return;
            }
        }
    }

    void unlockSet(std::atomic_bool* key) {
        key->store(false, std::memory_order_relaxed);
        key->notify_one();
    }

    void init() {
        Set::init(&setPointer, typeSetInitSize);
        setPointer.hashMethod = Set::HM_FNV1A;
        setPointer.keyStorage = Set::KS_POINTER;
        setPointer.keyOffset  = offsetof(SetSlot, hash);

        Set::init(&setArray, typeSetInitSize);
        setArray.hashMethod = Set::HM_FNV1A;
        setArray.keyStorage = Set::KS_POINTER;
        setArray.keyOffset  = offsetof(SetSlot, hash);

        Set::init(&setSlice, typeSetInitSize);
        setSlice.hashMethod = Set::HM_FNV1A;
        setSlice.keyStorage = Set::KS_POINTER;
        setSlice.keyOffset  = offsetof(SetSlot, hash);

        Arena::init(&tmpArena, 1024);
    }

    void release() {
        Set::release(&setPointer);
        Set::release(&setArray);
        Set::release(&setSlice);
        setPointerLock.store(false);
        setArrayLock.store(false);
        setSliceLock.store(false);
    }

    inline uint64_t combineHash(uint64_t a, uint64_t b) {
        return a + (b << 48);
    }

    inline SetSlot* makeSetSlot(TypeInfo* type, uint64_t hash) {
        SetSlot* set = alloc<SetSlot>();
        set->type = type;
        set->hash = hash;
        return set;
    }

    TypeInfo* tmpMakePointer(TypeInfo* element) {
        TypeInfoEx* type = (TypeInfoEx*) Arena::push(&tmpArena, sizeof(TypeInfoEx));
        type->base.kind   = DT_POINTER;
        type->base.size   = 8;
        type->base.align  = 8;
        type->ptr.element = element;

        return (TypeInfo*) type;
    }

    TypeInfo* tmpMakeArray(TypeInfo* element, int64_t len) {
        TypeInfoEx* type = (TypeInfoEx*) Arena::push(&tmpArena, sizeof(TypeInfoEx));
        type->base.kind        = DT_ARRAY;
        type->base.size        = 8;
        type->base.align       = 8;
        type->arr.element      = element;
        type->arr.elementCount = len;

        return (TypeInfo*) type;
    }

    TypeInfo* tmpMakeSlice(TypeInfo* element, uint64_t flags) {
        TypeInfoEx* type = (TypeInfoEx*) Arena::push(&tmpArena, sizeof(TypeInfoEx));
        type->base.kind   = DT_SLICE;
        type->base.size   = 16;
        type->base.align  = 8;
        type->slc.element = element;
        type->slc.flags   = flags;

        return (TypeInfo*) type;
    }

    bool isTmp(TypeInfo* type) {
        return Arena::contain(&tmpArena, type);
    }

    void tmpClear() {
        Arena::clear(&tmpArena);
    }

    TypeInfo* makePointer(TypeInfo* element) {
        PointerInfo* type = NULL;
        uint64_t hash = (uint64_t) element;

        lockSet(&setPointerLock);

        SetSlot* slot = (SetSlot*) Set::find(&setPointer, hash);
        if (!slot) {
            type = (PointerInfo*) alloc<TypeInfoEx>();
            type->base.kind = DT_POINTER;
            type->base.size = 8;
            type->base.align = 8;
            type->element = element;

            slot = makeSetSlot((TypeInfo*) type, hash);
            Set::insert(&setPointer, (uint8_t*)slot);
        } else {
            type = (PointerInfo*) slot->type;
        }

        unlockSet(&setPointerLock);

        return (TypeInfo*) type;
    }

    TypeInfo* makeArray(TypeInfo* element, int64_t len) {
        ArrayInfo* type = NULL;
        uint64_t hash = combineHash((uint64_t) element, len);

        lockSet(&setArrayLock);

        SetSlot* slot = (SetSlot*) Set::find(&setArray, hash);
        if (!slot) {
            type = (ArrayInfo*) alloc<TypeInfoEx>();
            type->base.kind = DT_ARRAY;
            type->base.size = element->size * len;
            type->base.align = element->align;
            type->element = element;
            type->elementCount = len;

            slot = makeSetSlot((TypeInfo*) type, hash);
            Set::insert(&setArray, (uint8_t*)slot);
        } else {
            type = (ArrayInfo*) slot->type;
        }

        unlockSet(&setArrayLock);

        return (TypeInfo*) type;
    }

    TypeInfo* makeSlice(TypeInfo* element, uint64_t flags) {
        SliceInfo* type = NULL;
        uint64_t hash = combineHash((uint64_t) element, flags);

        lockSet(&setSliceLock);

        SetSlot* slot = (SetSlot*) Set::find(&setSlice, hash);
        if (!slot) {
            type = (SliceInfo*) alloc<TypeInfoEx>();
            type->base.kind = DT_SLICE;
            type->base.size = 16;
            type->base.align = 8;
            type->element = element;
            type->flags = flags;

            slot = makeSetSlot((TypeInfo*) type, hash);
            Set::insert(&setSlice, (uint8_t*)slot);
        } else {
            type = (SliceInfo*) slot->type;
        }

        unlockSet(&setSliceLock);

        return (TypeInfo*) type;
    }

    // TODO : abstract
    EnumMemberInfo* findMember(EnumInfo* type, String name, int* idx) {
        EnumInfo* eType = (EnumInfo*) type;

        for (uint64_t i = 0; i < eType->memberCount; i++) {
            EnumMemberInfo* member = &eType->members[i];

            if (cstrcmp({ member->name.buff, member->name.len }, name)) {
                *idx = i;
                return member;
            }
        }

        *idx = 0;
        return NULL;
    }

    EnumMemberInfo* findMember(EnumInfo* type, String* name, int* idx) {
        return findMember(type, *name, idx);
    }

    EnumMemberInfo* findMember(EnumInfo* type, String name) {
        int idx;
        return findMember(type, name, &idx);
    }

    EnumMemberInfo* findMember(EnumInfo* type, String* name) {
        return findMember(type, *name);
    }

    // Returns NULL if not found
    StructMemberInfo* findMember(StructInfo* type, String name, int* idx) {
        StructInfo* sType= (StructInfo*) type;

        for (uint64_t i = 0; i < sType->memberCount; i++) {
            StructMemberInfo* member = &sType->members[i];

            if (cstrcmp({ member->name.buff, member->name.len }, name)) {
                *idx = i;
                return member;
            }
        }

        *idx = 0;
        return NULL;
    }

    StructMemberInfo* findMember(StructInfo* type, String* name, int* idx) {
        return findMember(type, *name, idx);
    }

    StructMemberInfo* findMember(StructInfo* type, String name) {
        int idx;
        return findMember(type, name, &idx);
    }

    StructMemberInfo* findMember(StructInfo* type, String* name) {
        return findMember(type, *name);
    }

    const char* str(Kind kind) {
        switch (kind) {
            case Type::DT_VOID: return "void";
            case Type::DT_I8:   return "i8";
            case Type::DT_I16:  return "i16";
            case Type::DT_I32:  return "i32";
            case Type::DT_I64:  return "i64";
            case Type::DT_U8:   return "u8";
            case Type::DT_U16:  return "u16";
            case Type::DT_U32:  return "u32";
            case Type::DT_U64:  return "u64";
            case Type::DT_F32:  return "f32";
            case Type::DT_F64:  return "f64";

            case Type::DT_POINTER: return "ptr";
            case Type::DT_ARRAY:   return "array";
            case Type::DT_SLICE: return "slice";
            case Type::DT_RANGE:   return "range";

            case Type::DT_STRUCT:  return "struct";
            case Type::DT_UNION:   return "union";
            case Type::DT_ERROR:   return "error";
            case Type::DT_ENUM:    return "enum";
            case Type::DT_MEMBER:  return "<member>";
            case Type::DT_FUNCTION: return "function";

            case Type::DT_UNDEFINED:      return "<undefined>";
            case Type::DT_MULTIPLE_TYPES: return "<varargs>";

            case Type::DT_COUNT: return "<count>";
        }

        return "Unknown";
    }

    const char* str(TypeInfo* type) {
        return str(type->kind);
    }

    void writeTypeName(IO::Stream* stream, Type::TypeInfo* type) {
        if (!type) {
            IO::write(stream, "<null type>");
            return;
        }

        const Type::Kind typeKind = type->kind;

        if (Type::isPrimitive(typeKind)) {
            IO::write(stream, Type::str(typeKind));
            return;
        }

        switch (typeKind) {
            case Type::DT_ARRAY: {
                ArrayInfo* aType= (ArrayInfo*) type;

                writeTypeName(stream, aType->element);
                IO::writef(stream, "[%llu]", (unsigned long long) aType->elementCount);

                break;
            }

            case Type::DT_SLICE: {
                SliceInfo* sType= (SliceInfo*) type;

                writeTypeName(stream, sType->element);

                const char* qualifier = "";
                if (sType->flags & IS_CONST) {
                    qualifier = "const";
                } else if (sType->flags & IS_DYNAMIC) {
                    qualifier = "auton";
                }

                IO::writef(stream, "[%s]", qualifier);

                break;
            }

            case Type::DT_POINTER: {
                PointerInfo* pType= (PointerInfo*) type;

                writeTypeName(stream, pType->element);
                IO::write(stream, '^');

                break;
            }

            case Type::DT_FUNCTION: {
                FunctionInfo* fType= (FunctionInfo*) type;

                IO::write(stream, "fcn(");
                for (uint64_t i = 0; i < fType->argCount; i++) {
                    writeTypeName(stream, fType->argTypes[i]);
                    if (i + 1 < fType->argCount) {
                        IO::write(stream, ", ");
                    }
                }
                IO::write(stream, ") -> ");

                if (fType->retType) {
                    writeTypeName(stream, fType->retType);
                } else {
                    IO::write(stream, "void");
                }

                break;
            }

            case Type::DT_STRUCT:
            case Type::DT_UNION: {
                StructInfo* sType= (StructInfo*) type;

                if (sType->name.buff && sType->name.len > 0) {
                    IO::writef(stream, "%.*s", (int) sType->name.len, sType->name.buff);
                } else {
                    IO::write(stream, "<anonymous struct>");
                }

                break;
            }

            case Type::DT_ENUM: {
                EnumInfo* eType= (EnumInfo*) type;

                if (eType->name.buff && eType->name.len > 0) {
                    IO::writef(stream, "%.*s", (int) eType->name.len, eType->name.buff);
                } else {
                    IO::write(stream, "<anonymous enum>");
                }

                break;
            }

            case Type::DT_RANGE: {
                IO::write(stream, "<range>");
                break;
            }

            case Type::DT_MULTIPLE_TYPES: {
                IO::write(stream, "...");
                break;
            }

            case Type::DT_MEMBER: {
                IO::write(stream, "<member>");
                break;
            }

            case Type::DT_ERROR: {
                IO::write(stream, "<error_type>");
                break;
            }

            case Type::DT_UNDEFINED: {
                IO::write(stream, "<undefined>");
                break;
            }

            default: {
                IO::writef(stream, "<unknown_type:%u>", (unsigned int) typeKind);
                break;
            }
        }
    }

};
