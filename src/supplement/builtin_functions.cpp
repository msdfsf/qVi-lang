#include "runtime.h"
#include "stdio.h"
#include "schubfach_table.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>



thread_local IO::Stream gStream = {
    .kind = IO::Stream::SK_C_STREAM,
    .cstream = stdout
};

const char        gIndentString[] = "               ";
constexpr uint8_t gMaxIndentLevel = sizeof(gIndentString);

typedef SchubfachU128 u128;
void printF64(Runtime::_PrintFormat* format, double f);



#define max(a, b) ((a) > (b) ? (a) : (b))



inline void printString(Runtime::_String str) {
    fwrite(str.buff, 1, str.len, stdout);
}

void printRaw(const char* ptr, const uint64_t len) {
    IO:write(&gStream, ptr, len);
}

void printHex(uint8_t val) {
    #define HEX_DIGIT(v) ((v) < 10 ? '0' + (v) : 'A' + (v) - 10)

    IO::write(&gStream, (char) HEX_DIGIT(val >> 4));
    IO::write(&gStream, (char) HEX_DIGIT(val & 0xF));
}

void printAsHex(Runtime::_PrintFormat* format, uint8_t val) {
    IO::write(&gStream, "0x");
    printHex(val);
}

void printAsHex(Runtime::_PrintFormat* format, uint16_t val) {
    IO::write(&gStream, "0x");
    printHex((uint8_t) (val >> 8));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) val);
}

void printAsHex(Runtime::_PrintFormat* format, uint32_t val) {
    IO::write(&gStream, "0x");
    printHex((uint8_t) (val >> 24));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) (val >> 16));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) (val >> 8));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) val);
}

void printAsHex(Runtime::_PrintFormat* format, uint64_t val) {
    IO::write(&gStream, "0x");
    printHex((uint8_t) (val >> 56));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) (val >> 48));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) (val >> 40));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) (val >> 32));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) (val >> 24));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) (val >> 16));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) (val >> 8));
    if (format->pretty) IO::write(&gStream, '_');
    printHex((uint8_t) val);
}

void printAsBin(Runtime::_PrintFormat* format, uint8_t val) {
    char buffer[8];
    for (int i = 7; i >= 0; i--) {
        buffer[8 - i - 1] = '0' + ((val >> i) & 0x01);
    }

    IO::write(&gStream, buffer, 8);
}

void printAsBin(Runtime::_PrintFormat* format, uint16_t val) {
    printAsBin(format, (uint8_t) (val >> 8));
    if (format->pretty) IO::write(&gStream, '_');
    printAsBin(format, (uint8_t) val);
}

void printAsBin(Runtime::_PrintFormat* format, uint32_t val) {
    printAsBin(format, (uint16_t) (val >> 16));
    if (format->pretty) IO::write(&gStream, '_');
    printAsBin(format, (uint16_t) val);
}

void printAsBin(Runtime::_PrintFormat* format, uint64_t val) {
    printAsBin(format, (uint32_t) (val >> 32));
    if (format->pretty) IO::write(&gStream, '_');
    printAsBin(format, (uint32_t) val);
}

void printI64(uint64_t val, int sign) {

    uint8_t buff[20];
    uint8_t idx = 19;

    do {
        uint64_t tmp = val;
        val /= 10;
        buff[idx] = (tmp - val * 10) + '0';
        idx--;
    } while (val);

    if (sign) {
        buff[idx] = '-';
        idx--;
    }

    fwrite(buff + idx + 1, 1, sizeof(buff) - idx - 1, stdout);

}

void printI64(Runtime::_PrintFormat* format, int64_t val) {
    if (val < 0) {
        printI64(-val, true);
    } else {
        if (format->showSign) IO::write(&gStream, '+');
        printI64(val, false);
    }
}

// TODO
struct FloatFormat {
    uint8_t preDotDigits;
    uint8_t postDotDigits;
};

void printFloat(bool sign, uint64_t mantissa, int64_t exp, FloatFormat format) {
    uint8_t buff[64 + 3];
    uint8_t idx = 63;

    uint64_t val = mantissa;

    // TODO: compare later speed of this and target size loops
    memset(buff, '0', 64);

    // remove trailing zeros
    while (val > 0) {
        if (val % 10) break;
        val /= 10;
        exp++;
    }

    if (exp > 0) {
        idx -= exp;
        exp = 0;
    }

    do {
        buff[idx] = (val % 10) + '0';
        val /= 10;
        idx--;
    } while (val > 0);
    buff[idx++] = sign ? '-' : ' ';

    const int valLen = 64 - idx;

    const int intLen = valLen + exp;
    const int decLen = valLen - intLen;

    if (intLen < 0) {
        fputc('0', stdout);
    }else {
        fwrite(buff + idx, 1, intLen, stdout);
    }

    if (decLen > 0) {
        buff[idx + intLen - 1] = '.';
        fwrite(buff + idx + intLen - 1, 1, decLen + 1, stdout);
    }
}

uint64_t u128MultHigh2(u128 a, uint64_t b) {
    unsigned __int128 al = a.l;
    unsigned __int128 ah = a.h;
    unsigned __int128 b128 = b;

    // Bits [0-127]
    unsigned __int128 low_part = al * b128;
    // Bits [64-191]
    unsigned __int128 high_part = ah * b128 + (uint64_t)(low_part >> 64);

    // Return bits [128-191]
    return (uint64_t)(high_part >> 64);
}

uint64_t u128MultHigh(u128 a, uint64_t b) {
    constexpr bool hasInt128 =
    #if defined(__SIZEOF_INT128__)
        true;
    #else
        false;
    #endif

    if constexpr (hasInt128) {
        unsigned __int128 al   = a.l;
        unsigned __int128 ah   = a.h;
        unsigned __int128 b128 = b;

        unsigned __int128 low  = al * b128;
        unsigned __int128 high = ah * b128 + (uint64_t) (low >> 64);

        return (uint64_t) (high >> 64);
    } else {

        u128 ans;

        constexpr int sz = 8 * sizeof(uint32_t);

        const uint64_t a1 = (uint32_t) (a.h >> sz);
        const uint64_t a2 = (uint32_t) (a.h);
        const uint64_t a3 = (uint32_t) (a.l >> sz);
        const uint64_t a4 = (uint32_t) (a.l);

        const uint64_t b1 = (uint32_t) (b >> sz);
        const uint64_t b2 = (uint32_t) (b);

        uint64_t carry = 0;
        uint64_t p1    = 0;
        uint64_t p2    = 0;
        uint64_t tmp1  = 0;
        uint64_t tmp2  = 0;

        // r4
        p1 = a4 * b2;

        ans.l = (uint32_t) p1;
        carry = p1 >> sz;

        // r3
        p1 = a4 * b1;
        p2 = a3 * b2;

        tmp2 = ((uint32_t) p1) + ((uint32_t) p2) + carry;
        tmp1 = (p1 >> sz) + (p2 >> sz) + (tmp2 >> sz);

        ans.l |= tmp2 << sz;
        carry = tmp1 >> sz;

        // r2
        p1 = a3 * b1;
        p2 = a2 * b2;

        tmp2 = ((uint32_t) p1) + ((uint32_t) p2) + carry;
        tmp1 = (p1 >> sz) + (p2 >> sz) + (tmp2 >> sz);

        ans.h = (uint32_t) tmp2;
        carry = tmp1 >> sz;

        // r1
        p1 = a2 * b1;
        p2 = a1 * b2;

        tmp2 = ((uint32_t) p1) + ((uint32_t) p2) + carry;
        tmp1 = (p1 >> sz) + (p2 >> sz) + (tmp2 >> sz);

        ans.h |= tmp2 << sz;

        carry = a1 * b1 + tmp1;
        return carry;
    }
}

// TODO : go through few more time to make
//        it make more sence...
//
// Based on:
// The Schubfach way to render doubles
// Raffaello Giulietti
//
// Var namings:
// 'e' for exponent as prefix
// 'm' for mantissa as prefix
// '2'/'10' as postfix to denote base mantissa/exponent are related to
//
// Trivia as I get it:
// f is consisted of m and e in binary unnormalized form.
// We want to normalize it so:
//  f = m2 * 2^e2
// Then we want to search for nearest boundaries, but we need to
// do so in base 10, as our final result has to be in such base
// if we want to print it.
//
// So, arbitrary float definition becomes:
//  f = m10 * 2^e10
// Therefore arbitrary m10 equals:
//  m10 = m2 * 2^e2 / 10^e10
// or
//  m10 = m2 * 2^e2 * 10^(-e10)
//
// As resolving 10^(-e10) at runtime is kinda slow,
// a lookup table of u128 values is used. Powers
// in the table are stored with implicitly baked exponent:
// table[e10] = floor(2^(-r) * 10^(-e10)) + 1
// which allows for direct uint computations.
//
// To store powers with high precision table values have
// to utilize u128 as much as they can, therefore while
// defining the exponent r width of a number is assumed
// as 125 (not full 128, so there is some room to not overflow
// while used in computations):
//  10^(-e10) = table[e10] * 2^(r)
// therefore:
//  r = log_2(10^(-e10)/table[e10])
// or
//  r = log_2(10^(-e10)) - log_2(table[e10])
// with assumption of 125 width number:
//  r = log_2(10^(-e10)) - log_2(2^125)
// or
//  r = log_2(10^(-e10)) - 125
//
// Now we kinda can convert any float to base 10 representation
// in the code while being efficient...
//
// Because float representation in base 10 is not exactly aligned
// with base 2, nor is the input float necessarily aligned with the
// real value it represents, we have to find the most suitable value.
// Basically we choose the right and left boundaries of rounding interval
// and proceed to convert them with input float to the base 10 and then
// decide which number in such interval suits the best. Which is basically
// truncated to only 4 possible floats, 3 of which we already computed and
// one is just the adjacent one from the input one.
//
// After following rules from the paper, we end up with hopefully the
// most accurate representation of the input float in base 10 in form
// of u64 mantissa, int exponent, bool sign. Which allows us to easily
// print it as we can treat it as integer printing...
//
void printF32(Runtime::_PrintFormat* format, float f) {
    // TODO
    printF64(format, (double) f);
}

void printF64(Runtime::_PrintFormat* format, double f) {
    const FloatFormat floatFormat = { .preDotDigits = 6, .postDotDigits = 6 };

    constexpr uint64_t SIGN_MASK     = 0x8000000000000000;
    constexpr uint64_t EXPONENT_MASK = 0x7FF0000000000000;
    constexpr uint64_t MANTISSA_MASK = 0x000FFFFFFFFFFFFF;
    constexpr int32_t  MANTISSA_LEN  = 52;
    constexpr int32_t  BIAS          = 1023;

    uint64_t bits;
    memcpy(&bits, &f, sizeof(uint64_t));

    bool sign = (bits & SIGN_MASK) != 0;
    int64_t e = (bits & EXPONENT_MASK) >> 52;
    uint64_t m = bits & MANTISSA_MASK;

    if ((e == 0x00) && (m == 0)) {
        // Null
        printFloat(sign, 0, 0, floatFormat);
        //fwrite({ '0' }, 1, 1, stdout);
        return;
    } else if ((e == 0xFF) && (m == 0)) {
        // Inf
        //fwrite({ 'I', 'n', 'f' }, 1, 4, stdout);
        return;
    }
    else if ((e == 0xFF) && (m != 0)) {
        // Nan
        //fwrite({ 'N', 'a', 'N' }, 1, 4, stdout);
        return;
    }

    const bool isSpacingRegular = (m != 0 || e == 0);

    uint64_t m2 = (e != 0 ? 1LL << MANTISSA_LEN : 0) + m;
    int64_t e2 = (e != 0 ? e : 1) - BIAS - MANTISSA_LEN;

    // We search for such boundaries of a rounding interval R_f:
    //  10^k <= width(R_f) < 10^(k + 1)
    // where k is the unique int defining the concrete interval:
    //  k = log10(width(R_f))
    // k can obey two forms depending on R_f spacing:
    // either floor(log10(2^e2))       if spacing is regular
    // or     floor(log10(3/4 * 2^e2)) if spacing is irregular
    //
    // Note: we shift boundaries so further computations are over integers
    //
    int64_t e10 = (e2 * 661'971'961'083LL + (isSpacingRegular ? 0 : -274'743'187'321LL)) >> 41;

    uint64_t ml2 = 4 * m2 - (isSpacingRegular ? 2 : 1);
    uint64_t mr2 = 4 * m2 + 2;
    uint64_t ms2 = 4 * m2;

    // We will apply all needed powers of 2 at once.
    // We need to:
    //  shift by e2 - 2,
    //  compensate for r in the lookup table,
    //  ? custom adjustment for table lookup
    //    (not in the paper, but for some reason we are a bit off)
    //
    // Note: we compute r using magic numbers instead of logs
    // Note: as later we will drop the bottom 128 bits of the multiplication
    //       result, we are virtually already shifted right by 128
    // Note: the paper states that 2 <= shift <= 5
    //
    const uint64_t shift = e2 + (((-e10) * 913'124'641'741LL) >> 38) + 2 + 1;

    // Going base 10
    //
    const u128 invPow10Scaled = SchubfachTable[-e10 + 292];

    // this should always fit into 64bit
    ml2 <<= shift;
    mr2 <<= shift;
    ms2 <<= shift;

    uint64_t tmp = u128MultHigh2(invPow10Scaled, ms2);

    uint64_t ml10 = u128MultHigh(invPow10Scaled, ml2);
    uint64_t mr10 = u128MultHigh(invPow10Scaled, mr2);
    uint64_t ms10 = u128MultHigh(invPow10Scaled, ms2) >> 2;
    uint64_t mt10 = ms10 + 1;

    // Geting the answer
    //
    const uint64_t eIsEven = e & 1;

    // magic 10 is the base of the result
    if (ms10 >= 10) {
        const uint64_t ms1010 = (ms10 / 10) * 10;
        const uint64_t mt1010 = ms1010 + 10;

        if (ml10 + eIsEven <= 4 * ms1010) {
            printFloat(sign, ms1010, e10, floatFormat);
            return;
        }

        if (4 * mt1010 + eIsEven <= mr10) {
            printFloat(sign, mt1010, e10, floatFormat);
            return;
        }
    }

    // If coresponding mantissas are in rounding interval
    bool leftIsIn = ml10 + eIsEven <= 4 * ms10;
    bool rightIsIn = 4 * mt10 + eIsEven <= mr10;

    if (leftIsIn && !rightIsIn) {
        printFloat(sign, ml10, e10, floatFormat);
        return;
    }

    if (!leftIsIn && rightIsIn) {
        printFloat(sign, mr10, e10, floatFormat);
        return;
    }

    // Both in interval, decide by distance
    bool leftIsCloser = ms10 < 2 * (ms10 + mt10);
    bool rightIsCloser = ms10 > 2 * (ms10 + mt10);
    if (leftIsCloser || (!rightIsCloser && ms10 & 1)) {
        printFloat(sign, ml10, e10, floatFormat);
    } else {
        printFloat(sign, mr10, e10, floatFormat);
    }
}

void printString(Runtime::_PrintFormat* format, Runtime::_ArrayInfo* str, Runtime::_Slice* slice) {

    if (str->element->kind == Type::DT_U8) {
        fwrite(slice->ptr, 1, slice->len, stdout);
    } else {
        fwrite("TODO", 1, 4, stdout);
    }

}

void printGenericArray(Runtime::_PrintFormat* format, Runtime::_PointerInfo* type, const uint64_t count, Runtime::_Buffer buffer) {
    const uint64_t stride = type->element->size;

    if (format->raw) {
        printRaw((char*) buffer, stride * count);
        return;
    }

    printf("[");

    for (uint64_t i = 0; i < count; i++) {
        Runtime::_Any element;
        element.info = type->element;

        uint8_t* elementAddr = buffer + (i * stride);

        if (isPrimitive(element.info->kind)) {
            element.u = 0;
            memcpy(&element.u, elementAddr, stride);
        }
        else {
            element.p = elementAddr;
        }

        Runtime::printValue(format, element);

        if (i < count - 1) printf(", ");
    }

    printf("]");
}

void printArray(Runtime::_PrintFormat* format, Runtime::_ArrayInfo* type, Runtime::_Buffer buffer) {
    printGenericArray(format, (Runtime::_PointerInfo*) type, type->elementCount, buffer);
}

void printSlice(Runtime::_PrintFormat* format, Runtime::_SliceInfo* type, Runtime::_Slice* slice) {
    // TODO:
    printGenericArray(format, (Runtime::_PointerInfo*) type, slice->len, (Runtime::_Buffer) slice->ptr);
}

// returns next indent level
void printIndent(uint64_t level) {
    level = level > gMaxIndentLevel ? gMaxIndentLevel : level;
    IO::write(&gStream, gIndentString, level);
}

void printStruct(Runtime::_PrintFormat* format, Runtime::_StructInfo* info, uint8_t* data, uint64_t indentLevel) {
    const uint64_t count = info->memberCount;

    if (format->raw) {
        printRaw((char*) data, info->base.size);
        return;
    }

    if (format->pretty) {
        IO::write(&gStream, "{\n");
    } else {
        IO::write(&gStream, "{");
    }

    uint8_t* basePtr = data;

    for (int i=0; i < count; i++) {
        Runtime::_StructMemberInfo* memberInfo = info->members + i;

        Runtime::_Any member;
        member.info = memberInfo->type;

        if (Type::isPrimitive(memberInfo->type)) {
            memcpy(&member.u, basePtr + memberInfo->offset, memberInfo->type->size);
        } else {
            member.p = (basePtr + memberInfo->offset);
        }

        if (format->pretty) printIndent(indentLevel);

        IO::write(&gStream, memberInfo->name.buff, memberInfo->name.len);
        IO::write(&gStream, ": ");

        if (Type::isStructLike(memberInfo->type)) {
            printStruct(format, (Runtime::_StructInfo*) memberInfo->type, member.b, indentLevel + 1);
        } else {
            Runtime::printValue(format, member);
        }

        if (i < count - 1) IO::write(&gStream, ", ");
        if (format->pretty) IO::write(&gStream, "\n");
    }

    IO::write(&gStream, '}');
    if (format->pretty) IO::write(&gStream, '\n');
}

template<typename T>
void printInt(Runtime::_PrintFormat* format, T val) {
    static_assert(std::is_integral_v<T>);
    using U = std::make_unsigned_t<T>;

    if (format->binary)   printAsBin(format, (U) val);
    else if (format->hex) printAsHex(format, (U) val);
    else if (format->raw) printRaw((char*) &val, sizeof(T));
    else                  printI64(format, val);
}

// TODO: we may want to generate lookup table for each decently dense enum
//       and use binary search as fallback. Also we can use offset for consecutive
//       enums with non-zero start.
void printEnum(Runtime::_PrintFormat* format, Runtime::_EnumInfo* type, int64_t val) {
    Runtime::_String* name = NULL;
    if (val >= 0 && val < type->memberCount && type->members[val].value == val) {
        name = &type->members[val].name;
    } else {
        // TODO: binary search
        for (uint32_t i = 0; i < type->memberCount; i++) {
            if (type->members[i].value == val) {
                name = &type->members[i].name;
                break;
            }
        }
    }

    if (name) {
        IO::write(&gStream, name->buff, name->len);

        if (format->pretty) {
            IO::write(&gStream, '(');
            printInt(format, val);
            IO::write(&gStream, ')');
        }
    } else {
        printInt(format, val);
    }
}

void Runtime::printValue(_PrintFormat* format, _Any val) {
    switch (val.info->kind) {
        case Type::DT_I8:  printInt(format, (int8_t) val.i);  break;
        case Type::DT_I16: printInt(format, (int16_t) val.i); break;
        case Type::DT_I32: printInt(format, (int32_t) val.i); break;
        case Type::DT_I64: printInt(format, (int64_t) val.i); break;

        case Type::DT_U8:  printInt(format, (uint8_t) val.i);  break;
        case Type::DT_U16: printInt(format, (uint16_t) val.i); break;
        case Type::DT_U32: printInt(format, (uint32_t) val.i); break;
        case Type::DT_U64: printInt(format, (uint64_t) val.i); break;

        case Type::DT_F32: {
            if (format->binary)   printAsBin(format, *(uint32_t*) &val.f);
            else if (format->hex) printAsHex(format, *(uint32_t*) &val.f);
            else if (format->raw) printRaw((char*) &val.f, sizeof(float));
            else                  printF32(format, *(float*) &val.f);
            break;
        }

        case Type::DT_F64: {
            if (format->binary)   printAsBin(format, *(uint64_t*) &val.f);
            else if (format->hex) printAsHex(format, *(uint64_t*) &val.f);
            else if (format->raw) printRaw((char*) &val.f, sizeof(double));
            else                  printF64(format, val.f);
            break;
        }

        case Type::DT_STRUCT: {
            printStruct(format, (_StructInfo*) val.info, val.b, 1);
            break;
        }

        case Type::DT_POINTER: {
            printAsHex(format, val.u);
            break;
        }

        case Type::DT_ARRAY: {
            printArray(format, (_ArrayInfo*) val.info, val.b);
            break;
        }

        case Type::DT_SLICE: {
            printSlice(format, (_SliceInfo*) val.info, val.s);
            break;
        }

        case Type::DT_ENUM: {
            printEnum(format, (_EnumInfo*) val.info, val.i);
            break;
        }

        default: {
            printf("TODO");
            break;
        }
    }
}

void Runtime::printArg(_PrintFormat* format, _Any val) {
    if (format->crop || format->width) {
        // TODO: prepare IO buffer to hold result
    }

    if (format->printValue) {
        printValue(format, val);
    }

    if (format->printType) {
        if (format->printValue) {
            IO::write(&gStream, ':');
        }
        Type::writeTypeName(&gStream, val.info);
    }
}

void Runtime::print(char* fmt, int fmtLen, int argsCnt, _Any* args) {
    int idx = 0;
    int argIdx = 0;
    int beginIdx = 0;
    for (; idx < fmtLen; idx++) {
        const char ch = fmt[idx];
        if (ch == '%') {
            fwrite(fmt + beginIdx, 1, idx - beginIdx, stdout);
            // TODO: we need to receive format descriptor
            printArg(NULL, args[argIdx]);

            argIdx++;
            beginIdx = idx + 1;
        }
    }

    fwrite(fmt + beginIdx, 1, idx - beginIdx, stdout);
}
