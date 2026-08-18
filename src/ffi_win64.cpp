#include "data_types.h"
#include "foreign_code.h"
#include <cstdint>

namespace Extern::Abi {

    void win64Classify(Arg* arg, Abi::TypeInfo* abi) {
        Type::TypeInfoEx* type = abi->type;

        switch (type->base.kind) {
            case Type::DT_F32:
            case Type::DT_F64: {
                arg->kind = AK_FLOAT;
                arg->pass = PK_REG_FLOAT;
                break;
            }

            case Type::DT_U8:
            case Type::DT_I8:
            case Type::DT_U16:
            case Type::DT_I16:
            case Type::DT_U32:
            case Type::DT_I32:
            case Type::DT_U64:
            case Type::DT_I64:
            case Type::DT_POINTER: {
                arg->kind = AK_INT;
                arg->pass = PK_REG_INT;
                break;
            }

            case Type::DT_STRUCT:
            case Type::DT_UNION: {
                // Structs and unions of size 8, 16, 32, or 64 bits,
                // and __m64 types, are passed as if they were integers
                // of the same size.
                // Structs or unions of other sizes are passed as a pointer
                // to 16-byte aligned memory allocated by the caller.

                const uint32_t size = type->base.size;
                if ((size & 1) == 0 && size <= 8) {
                    arg->kind = Abi::AK_INT;
                    arg->pass = Abi::PK_REG_STRUCT;
                } else {
                    arg->kind = Abi::AK_AGGREGATE;
                    arg->pass = Abi::PK_REG_STRUCT_REFERENCE;
                }

                break;
            }

            default: {
                arg->kind = AK_NONE;
                break;
            }
        }
    }

    extern "C" void asm_win64Invoke(CallContext* ctx);

    Driver win64 = {
        .iRegCount = 4,
        .fRegCount = 4,
        .isUniform = true,
        .stackAlign = 16,
        .indirectAlignment = 16,

        .layout = {
            .wordSize = 8,
            .minAlign = 1,
            .maxAlign = 16
        },

        .classify = win64Classify,
        .invoke   = asm_win64Invoke
    };

}
