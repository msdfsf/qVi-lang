#include "runtime.h"
#include "../data_types.h"


namespace Runtime {

    _TypeInfo* toRuntimeType(Type::TypeInfo* type) {
        return (_TypeInfo*) type;
    }

}
