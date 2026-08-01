#include "data_types.h"
#include "io.h"
#include "syntax.h"

namespace Type {

    TypeInfo basicTypes[] = {

        {
            .kind  = DT_VOID,
            .rank  = 0,
            .size  = 0,
            .align = 1,
        },

        {
            .kind  = DT_I8,
            .rank  = 1,
            .size  = 1,
            .align = 1,
        },

        {
            .kind  = DT_I16,
            .rank  = 2,
            .size  = 2,
            .align = 2,
        },

        {
            .kind  = DT_I32,
            .rank  = 3,
            .size  = 4,
            .align = 4,
        },

        {
            .kind  = DT_I64,
            .rank  = 4,
            .size  = 8,
            .align = 8,
        },

        {
            .kind  = DT_U8,
            .rank  = 1,
            .size  = 1,
            .align = 1,
        },

        {
            .kind  = DT_U16,
            .rank  = 2,
            .size  = 2,
            .align = 2,
        },

        {
            .kind  = DT_U32,
            .rank  = 3,
            .size  = 4,
            .align = 4,
        },

        {
            .kind  = DT_U64,
            .rank  = 4,
            .size  = 8,
            .align = 8,
        },

        {
            .kind  = DT_F32,
            .rank  = 6,
            .size  = 4,
            .align = 4,
        },

        {
            .kind  = DT_F64,
            .rank  = 7,
            .size  = 8,
            .align = 8,
        },

        {
            .kind  = DT_STRING,
            .rank  = 5,
            .size  = 16,
            .align = 8,
        },

        {
            .kind  = DT_POINTER,
            .rank  = 5,
            .size  = 8,
            .align = 8,
        },

        {
            .kind  = DT_ARRAY,
            .rank  = 5,
            .size  = 8,
            .align = 8,
        },

        {
            .kind  = DT_SLICE,
            .rank  = 5,
            .size  = 8,
            .align = 8,
        },

        {
            .kind  = DT_MULTIPLE_TYPES,
            .rank  = 0,
            .size  = 0,
            .align = 1,
        },

        {
            .kind  = DT_CUSTOM,
            .rank  = 10,
            .size  = 0,
            .align = 1,
        },

        {
            .kind  = DT_ENUM,
            .rank  = 0,
            .size  = 4,
            .align = 4,
        },

        {
            .kind  = DT_UNDEFINED,
            .rank  = 0,
            .size  = 0,
            .align = 1,
        }

    };

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

            case Type::DT_STRING:  return "string";
            case Type::DT_POINTER: return "ptr";
            case Type::DT_ARRAY:   return "array";
            case Type::DT_SLICE:   return "slice";
            case Type::DT_CUSTOM:  return "DT_CUSTOM";
            case Type::DT_UNION:   return "DT_UNION";
            case Type::DT_ERROR:   return "DT_ERROR";
            case Type::DT_ENUM:    return "DT_ENUM";

            case Type::DT_FUNCTION: return "DT_FUNCTION";

            case Type::DT_UNDEFINED:      return "DT_UNDEFINED";
            case Type::DT_MULTIPLE_TYPES: return "DT_MULTIPLE_TYPES";
        }
        return "Unknown";
    }

    void writeTypeName(IO::Stream* stream, void* type, Type::Kind typeKind) {
        if (!stream) return;

        if (!type && typeKind >= Type::DT_CUSTOM) {
            IO::write(stream, "void");
            return;
        }

        if (Type::isPrimitive(typeKind)) {
            const char* str = Type::str(typeKind);
            IO::write(stream, str);
            return;
        }

        switch (typeKind) {
            case Type::DT_CUSTOM:
            case Type::DT_UNION: {
                TypeDefinition* def = (TypeDefinition*) type;
                IO::writef(stream, "%.*s", (int)def->name.len, def->name.buff);
                break;
            }

            case Type::DT_ENUM: {
                Enumerator* en = (Enumerator*)type;
                IO::writef(stream, "%.*s", (int)en->name.len, en->name.buff);
                break;
            }

            case Type::DT_POINTER: {
                Pointer* ptr = (Pointer*)type;

                // Recurse to the base type first
                writeTypeName(stream, ptr->pointsTo, ptr->pointsToKind);

                // Append the pointer symbol (using the optimized char overload)
                IO::write(stream, '^');
                break;
            }

            case Type::DT_ARRAY: {
                Array* arr = (Array*)type;

                // Recurse to the element type
                writeTypeName(stream, arr->base.pointsTo, arr->base.pointsToKind);

                // Append the array size bracket [N] or [?]
                if (arr->length && arr->length->value.hasValue) {
                    IO::writef(stream, "[%llu]", arr->length->value.u64);
                } else {
                    IO::write(stream, "[?]", 3);
                }
                break;
            }

            case Type::DT_SLICE: {
                Slice* slice = (Slice*)type;

                IO::write(stream, '[');

                // Slices can have typed bounds: e.g. [i32:i32]
                writeTypeName(stream, slice->bidx->value.any, slice->bidx->value.typeKind);
                IO::write(stream, ':');
                writeTypeName(stream, slice->eidx->value.any, slice->eidx->value.typeKind);

                IO::write(stream, ']');
                break;
            }

            default: {
                IO::write(stream, "void", 4);
                break;
            }
        }
    }

};
