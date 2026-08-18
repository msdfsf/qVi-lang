#include "runtime.h"
#include "stdio.h"
#include "schubfach_table.h"

#include <cstdint>
#include <cstdio>
#include <cstring>



typedef SchubfachU128 u128;
void printF64(double f);



#define max(a, b) ((a) > (b) ? (a) : (b))



inline void printString(Runtime::_String str) {
    fwrite(str.buff, 1, str.len, stdout);
}

void printAsHex(uint64_t val) {
    #define HEX_DIGIT(v) ((v) < 10 ? '0' + (v) : 'A' + (v) - 10)

    fputs("0x", stdout);

    fputc(HEX_DIGIT((val >> 60) & 0xF), stdout);
    fputc(HEX_DIGIT((val >> 56) & 0xF), stdout);

    fputc(HEX_DIGIT((val >> 52) & 0xF), stdout);
    fputc(HEX_DIGIT((val >> 48) & 0xF), stdout);

    fputc(HEX_DIGIT((val >> 44) & 0xF), stdout);
    fputc(HEX_DIGIT((val >> 40) & 0xF), stdout);

    fputc(HEX_DIGIT((val >> 36) & 0xF), stdout);
    fputc(HEX_DIGIT((val >> 32) & 0xF), stdout);

    fputc(HEX_DIGIT((val >> 28) & 0xF), stdout);
    fputc(HEX_DIGIT((val >> 24) & 0xF), stdout);

    fputc(HEX_DIGIT((val >> 20) & 0xF), stdout);
    fputc(HEX_DIGIT((val >> 16) & 0xF), stdout);

    fputc(HEX_DIGIT((val >> 12) & 0xF), stdout);
    fputc(HEX_DIGIT((val >> 8)  & 0xF), stdout);

    fputc(HEX_DIGIT((val >> 4)  & 0xF), stdout);
    fputc(HEX_DIGIT((val >> 0)  & 0xF), stdout);

}

void printI64(uint64_t val, int sign) {

    uint8_t buff[20];
    uint8_t idx = 19;

    do {
        int64_t tmp = val;
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

void printI64(int64_t val) {
    printI64(val, val < 0);
}

void printU64(uint64_t val) {
    printI64(val, 0);
}

// TODO
struct FloatFormat {
    uint8_t preDotDigits;
    uint8_t postDotDigits;
};

void printFloat(bool sign, uint64_t mantissa, int64_t exp, FloatFormat format) {
    uint8_t buff[64];
    uint8_t idx = 63;

    uint64_t val = mantissa;

    // remove trailing zeros
    while (val > 0) {
        if (val % 10) break;
        val /= 10;
        exp++;
    }

    do {
        buff[idx] = (val % 10) + '0';
        val /= 10;
        idx--;
    } while (val > 0);

    idx++;

    const int valLen = 64 - idx;
    const bool expSign = exp < 0;

    const int intLen = valLen + exp;
    const int decLen = valLen - intLen;

    fwrite(buff + idx, 1, intLen, stdout);
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
void printF32(float f) {
    // TODO
    printF64((double) f);
}

void printF64(double f) {
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
    const FloatFormat format = { .preDotDigits = 6, .postDotDigits = 6 };

    // magic 10 is the base of the result
    if (ms10 >= 10) {
        const uint64_t ms1010 = (ms10 / 10) * 10;
        const uint64_t mt1010 = ms1010 + 10;

        if (ml10 + eIsEven <= 4 * ms1010) {
            printFloat(sign, ms1010, e10, format);
            return;
        }

        if (4 * mt1010 + eIsEven <= mr10) {
            printFloat(sign, mt1010, e10, format);
            return;
        }
    }

    // If coresponding mantissas are in rounding interval
    bool leftIsIn = ml10 + eIsEven <= 4 * ms10;
    bool rightIsIn = 4 * mt10 + eIsEven <= mr10;

    if (leftIsIn && !rightIsIn) {
        printFloat(sign, ml10, e10, format);
        return;
    }

    if (!leftIsIn && rightIsIn) {
        printFloat(sign, mr10, e10, format);
        return;
    }

    // Both in interval, decide by distance
    bool leftIsCloser = ms10 < 2 * (ms10 + mt10);
    bool rightIsCloser = ms10 > 2 * (ms10 + mt10);
    if (leftIsCloser || (!rightIsCloser && ms10 & 1)) {
        printFloat(sign, ml10, e10, format);
    } else {
        printFloat(sign, mr10, e10, format);
    }
}

void printString(Runtime::_ArrayInfo* str, Runtime::_Slice* slice) {

    if (str->element->kind == Type::DT_U8) {
        fwrite(slice->ptr, 1, slice->len, stdout);
    } else {
        fwrite("TODO", 1, 4, stdout);
    }

}

void printArray(Runtime::_ArrayInfo* arr, Runtime::_Slice* slice) {

    const uint64_t count = slice->len;
    const uint64_t stride = arr->element->size;

    printf("[");

    for (uint64_t i = 0; i < count; i++) {

        Runtime::_Any element;
        element.info = arr->element;

        uint8_t* elementAddr = (uint8_t*) slice->ptr + (i * stride);

        if (isPrimitive(element.info->kind)) {
            element.u = 0;
            memcpy(&element.u, elementAddr, stride);
        } else {
            element.p = elementAddr;
        }

        Runtime::printValue(element);

        if (i < count - 1) printf(", ");

    }

    printf("]");

}

void printStruct(Runtime::_StructInfo* info, uint8_t* data) {
    const uint64_t count = info->memberCount;

    printString(info->name);
    printf("{\n");

    uint8_t* basePtr = data;

    for (int i=0; i < count; i++) {
        Runtime::_StructMemberInfo* memberInfo = info->members + i;

        Runtime::_Any member;
        member.info = memberInfo->type;

        // TODO : we need a way to distinguish in typeinfo
        //        between fixed size array and runtime-array
        if (Type::isPrimitive(memberInfo->type->kind)) {
            memcpy(&member.u, basePtr + memberInfo->offset, memberInfo->type->size);
        } else {
            member.p = (basePtr + memberInfo->offset);
        }

        printf("  ");
        Runtime::printValue(member);

        if (i < count - 1) printf(",\n");
        else { printf("\n"); }
    }

    printf("}\n");
}

void Runtime::printValue(_Any val) {
    switch (val.info->kind) {
        case Type::DT_I8: {
            printI64((int64_t) (int8_t) val.i);
            break;
        }
        case Type::DT_I16: {
            printI64((int64_t) (int16_t) val.i);
            break;
        }
        case Type::DT_I32: {
            printI64((int64_t) (int32_t) val.i);
            break;
        }
        case Type::DT_I64: {
            printI64(val.i);
            break;
        }

        case Type::DT_U8: {
            printI64((uint64_t) (uint8_t) val.u);
            break;
        }
        case Type::DT_U16: {
            printI64((uint64_t) (uint16_t) val.u);
            break;
        }
        case Type::DT_U32: {
            printI64((uint64_t) (uint32_t) val.u);
            break;
        }
        case Type::DT_U64: {
            printI64(val.u);
            break;
        }

        case Type::DT_F32: {
            printF32(*(float*) ((void*) &val.f));
            break;
        }

        case Type::DT_F64: {
            printF64(val.f);
            break;
        }

        case Type::DT_STRUCT: {
            printStruct((_StructInfo*) val.info, val.bp);
            break;
        }

        case Type::DT_POINTER: {
            printAsHex(val.u);
            break;
        }

        case Type::DT_ARRAY: {
            printArray((_ArrayInfo*) val.info, val.s);
            break;
        }

        default: {
            printf("TODO");
            break;
        }
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
            printValue(args[argIdx]);

            argIdx++;
            beginIdx = idx + 1;
        }

    }

    fwrite(fmt + beginIdx, 1, idx - beginIdx, stdout);

}
