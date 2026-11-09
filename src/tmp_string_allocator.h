#pragma once

// Allocator for temporary strings that are usually
// used within a one function scope.
//
// Allocator is thread local for convenient usage,
// although its may not be the best way of doing
// things.
//
// The 'array-list'-like storage is used.
// TODO: maybe chained system would be better...
//

#include <cstdio>
#include <cstdlib>
#include <cstring>



struct TmpStringAllocator {
    char*  buffer;
    size_t size;
    size_t capacity;
    size_t pos;
    char*  tmp;          // saved buffer
    size_t tmpCapacity;  // saved capacity
};

struct TmpStringMarker {
    size_t pos;
    bool   isStatic;
};

constexpr size_t TMP_STRING_STATIC_CAPACITY = 128;

alignas(8) extern thread_local char gTmpStringBuffer[TMP_STRING_STATIC_CAPACITY];
extern thread_local TmpStringAllocator gTmpStringAllocator;



inline void tsError() {
    fprintf(stderr, "Malloc, Mallooooc, Malloooooooc!\n");
    exit(69);
}

inline void tsGrow(size_t requestedCapacity) {
    size_t capacity = gTmpStringAllocator.capacity * 2;
    if (capacity < requestedCapacity) {
        capacity = requestedCapacity;
    }
    if (capacity < 512) {
        capacity = 512;
    }

    char* buf = NULL;

    if (gTmpStringAllocator.buffer == gTmpStringBuffer) {
        // transition from static buffer to heap
        buf = (char*) malloc(capacity);
        if (!buf) tsError();

        if (gTmpStringAllocator.pos > 0) {
            memcpy(buf, gTmpStringBuffer, gTmpStringAllocator.pos);
        }
    } else {
        // reallocate existing heap buffer
        buf = (char*) realloc(gTmpStringAllocator.buffer, capacity);
        if (!buf) tsError();
    }

    gTmpStringAllocator.buffer = buf;
    gTmpStringAllocator.capacity = capacity;
}

inline char* tsAlloc(size_t bytes, const size_t align = 8) {
    const size_t alignedBytes = (bytes + (align - 1)) & ~(align - 1);

    if (gTmpStringAllocator.pos + alignedBytes > gTmpStringAllocator.capacity) {
        if (gTmpStringAllocator.tmp) {
            gTmpStringAllocator.buffer = gTmpStringAllocator.tmp;
            gTmpStringAllocator.capacity = gTmpStringAllocator.tmpCapacity;
        }

        tsGrow(gTmpStringAllocator.pos + alignedBytes);
    }

    char* ptr = gTmpStringAllocator.buffer + gTmpStringAllocator.pos;
    gTmpStringAllocator.pos += alignedBytes;
    gTmpStringAllocator.size = gTmpStringAllocator.pos;

    return ptr;
}

inline TmpStringMarker tsMark() {
    return {
        gTmpStringAllocator.pos,
        gTmpStringAllocator.buffer == gTmpStringBuffer
    };
}

inline void tsRollback(TmpStringMarker marker) {
    // If we marked on the static buffer, but grew to the heap in between:
    if (marker.isStatic && gTmpStringAllocator.buffer != gTmpStringBuffer) {
        free(gTmpStringAllocator.buffer);
        gTmpStringAllocator.buffer = gTmpStringBuffer;
        gTmpStringAllocator.capacity = TMP_STRING_STATIC_CAPACITY;
    }

    gTmpStringAllocator.pos = marker.pos;
    gTmpStringAllocator.size = marker.pos;
}

// Resets allocator back to the static buffer, saving the active buffer in tmp
inline void tsClear() {
    // if tmp already held an unreleased heap buffer, free it to avoid memory leak
    if (gTmpStringAllocator.tmp && gTmpStringAllocator.tmp != gTmpStringBuffer) {
        free(gTmpStringAllocator.tmp);
    }

    gTmpStringAllocator.tmp = gTmpStringAllocator.buffer;
    gTmpStringAllocator.tmpCapacity = gTmpStringAllocator.capacity;

    gTmpStringAllocator.buffer = gTmpStringBuffer;
    gTmpStringAllocator.capacity = TMP_STRING_STATIC_CAPACITY;
    gTmpStringAllocator.pos = 0;
    gTmpStringAllocator.size = 0;
}

inline void tsInit() {
    tsClear();
}

// Clear and free the saved buffer
inline void tsRelease() {
    tsClear();

    if (gTmpStringAllocator.tmp != gTmpStringBuffer) {
        free(gTmpStringAllocator.tmp);
    }
    gTmpStringAllocator.tmp = NULL;
    gTmpStringAllocator.tmpCapacity = 0;
}
