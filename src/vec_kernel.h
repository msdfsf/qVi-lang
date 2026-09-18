#pragma once
#include "data_types.h"
#include "operators.h"
#include "syntax.h"
#include <cstdint>
#include <stdint.h>



typedef void (*VecFunctionBinary)(void* out, void* a, void* b, const int len);
typedef void (*VecFunctionScalar)(void* out, void* a, uint64_t s, const int len);
typedef void (*VecFunctionUnary) (void* out, void* a, const int len);
typedef void (*VecFunctionCast)  (void* out, void* a, const int len);
typedef void (*VecFunctionFill)  (void* out, uint64_t val, const int len);

VecFunctionBinary vecGetBinary (Type::Kind dtype, OperatorEnum oper);
VecFunctionScalar vecGetScalarR(Type::Kind dtype, OperatorEnum oper);
VecFunctionScalar vecGetScalarL(Type::Kind dtype, OperatorEnum oper);
VecFunctionUnary  vecGetUnary  (Type::Kind dtype, OperatorEnum oper);
VecFunctionCast   vecGetCast   (Type::Kind dest, Type::Kind src);
VecFunctionFill   vecGetFill   (Type::Kind dtype);
