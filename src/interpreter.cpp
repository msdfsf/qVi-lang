// Habitat of 'global' vm related stuff
//

#include "interpreter.h"
#include "data_types.h"
#include "operators.h"
#include <cstdint>


namespace Interpreter {

    vmword encodeVecDescriptor(const VecDescriptor desc) {
        return (
              (((uint64_t) desc.type        & 0xFF)   << DE_TYPE_SHIFT)
            | (((uint64_t) desc.oper        & 0xFF)   << DE_OPER_SHIFT)
            | (((uint64_t) desc.srcType     & 0xFF)   << DE_SRC_TYPE_SHIFT)
            | (((uint64_t) desc.flags       & 0xFF)   << DE_FLAGS_SHIFT)
            | (((uint64_t) desc.dstElemSize & 0xFFFF) << DE_DST_SIZE_SHIFT)
            | (((uint64_t) desc.srcElemSize & 0xFFFF) << DE_SRC_SIZE_SHIFT)
        );
    }

    VecDescriptor decodeVecDescriptor(const vmword word) {
        return {
            .type        = (Type::Kind)   ((word & DE_TYPE_MASK)     >> DE_TYPE_SHIFT),
            .oper        = (OperatorEnum) ((word & DE_OPER_MASK)     >> DE_OPER_SHIFT),
            .srcType     = (Type::Kind)   ((word & DE_SRC_TYPE_MASK) >> DE_SRC_TYPE_SHIFT),
            .flags       = (uint8_t)      ((word & DE_FLAGS_MASK)    >> DE_FLAGS_SHIFT),
            .dstElemSize = (uint16_t)     ((word & DE_DST_SIZE_MASK) >> DE_DST_SIZE_SHIFT),
            .srcElemSize = (uint16_t)     ((word & DE_SRC_SIZE_MASK) >> DE_SRC_SIZE_SHIFT),
        };
    }

    bool vdIsDestTmp(VecDescriptor desc)   { return (desc.flags & DE_F_DEST) != 0; }
    bool vdIsLeftTmp(VecDescriptor desc)   { return (desc.flags & DE_F_LEFT) != 0; }
    bool vdIsRightTmp(VecDescriptor desc)  { return (desc.flags & DE_F_RIGHT) != 0; }
    bool vdIsDestStack(VecDescriptor desc) { return (desc.flags & DE_F_IS_DEST_STACK) != 0; }

}
