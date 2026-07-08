#pragma once

#include "allocator.h"
#include "string.h"
#include "file_system.h"

#include <cstdint>


// 64-bit IDs (top 8 bits = thread ID, lower bits = index)
static constexpr uint64_t THREAD_MASK = 0xFF00000000000000;
static constexpr uint64_t INDEX_MASK  = 0x00FFFFFFFFFFFFFF;

typedef uint64_t Id;

typedef Id VarId;
typedef Id ArrId;
typedef Id ErrId;
typedef Id DefId;

// TODO
typedef float float_t;
typedef double double_t;

typedef uint64_t Flags;

typedef size_t MemberOffset;

typedef String INamed;

struct Pos;
struct Span;
struct LogInfo;



char* const stageTag = NULL;

static uint8_t stageId;
struct UserFileData {
    uint8_t id;
    void* data;
};



#define getMemberOffset(type, member) ((MemberOffset) &(((type*) NULL)->member))
#define getMember(ptr, member) (((char*) (ptr)) + (member))



constexpr uint16_t toDoubleChar(char first, char second) {
    return (static_cast<uint16_t>(first) << 8) | static_cast<uint16_t>(second);
}



// TODO : better name
enum State : uint64_t {
    IS_CONST = 1 << 0,
    IS_CMP_TIME = 1 << 1,
    IS_DYNAMIC = 1 << 2,
    IS_EMBEDED = 1 << 3, // LOOK AT: wtf is this

    IS_UNARY = 1 << 4,
    IS_BINARY = 1 << 5,
    IS_TERNARY = 1 << 6,

    IS_ONE_CHAR = 1 << 7,
    IS_TWO_CHAR = 1 << 8,
    IS_THREE_CHAR = 1 << 9,
    IS_FOUR_CHAR = 1 << 10,

    IS_ARRAY = 1 << 11,
    IS_ARRAY_LIST = 1 << 12,

    IS_ALLOCATED = 1 << 13,

    IS_STRING  = 1 << 14,

    IS_ALLOCATION = 1 << 15,
    IS_LENGTH = 1 << 16,
    IS_SIZE = 1 << 17,

    // TODO : better name
    // For now here. Sometimes we may need to know
    // what ordering constraints are applicable to 'us'.
    IS_UNORDERED = 1 << 18,

    // If node is 'anonymous' and actualy is product
    // of an import statement.
    IS_FROM_IMPORT = 1 << 20,

    IS_EXTERN = 1 << 21,

    IS_RENDERED = 1 << 24,

    // TODO : better name
    // If children are unique. Where both, children and unique,
    // are relative to the specific node type.
    IS_UNIQUE  = 1 << 30,
};

struct Pos {
    int idx;
    int ln;
};

static const Pos INVALID_POS = { -1, -1 };
static const uint64_t NULL_FLAG = (uint64_t) (0);

struct Span {
    // store file data directly, as we may need to look up them frequently
    FileSystem::FileInfo* fileInfo;
    const char* str;
    Pos start;
    Pos end;
};

// Keeps track of a location and the original start of a parsed rule
struct SpanEx : Span {
    Pos super;
};

struct LogInfo {
    Span loc;
};

// sometimes we need to store locations of INamed stuff that
// is not describing advanced stuff
// stuff... stuff...
struct INamedLoc {
    char* buff;
    uint64_t len;
    Span* span;
};

// LOOK AT : maybe better name
struct INamedEx {
    char* buff;
    uint64_t len;
    Span* span;
    Id id;
};

inline void init(INamedEx* name) {
    name->id = 0;
    name->len = 0;
    name->buff = NULL;
    name->span = NULL;
}


// TODO : change to something like 'IAmAnInternalFunction(Function* fcn)'
static inline int isValidFunctionIdx(int idx) {
    return idx >= 0;
}

static inline int isValidPos(Pos pos) {
    return pos.ln != INVALID_POS.ln;
}

static inline Span* getSpanStamp(Span* span) {

    Span* stamp = (Span*) alloc(alc, sizeof(Span));
    if (!stamp) return NULL;

    memcpy(stamp, span, sizeof(Span));

    return stamp;

}

static inline SpanEx markSpanStart(Span* span) {
    SpanEx spanEx;
    (Span&) spanEx = *span;
    spanEx.super = span->start;
    return spanEx;
}

static inline Span* finalizeSpan(SpanEx* lspan, Span* span) {

    lspan->start = lspan->super;
    span->end = lspan->end;

    return getSpanStamp((Span*) lspan);

}

static inline void syncStartToEnd(Span* const span) {
    span->start = { span->end.idx + 1, span->end.ln};
}

static inline void syncEnd(Span* const dest, Span* const src) {
    dest->start = src->end;
}

static inline void freeSpanStamp(Span* loc) {
    dealloc(alc, loc);
}
