#pragma once

#include <cstdint>
#include <stdint.h>
#include <cstddef>


namespace Arena {

    struct Block {
        Block* prev;
        Block* next;

        // accumulated logical pos for this chunk
        // note, we start from padding
        uint64_t startLogicalPos;

        // the offset we need to take at the start of the block
        // to simulate alignments of elements if they were
        // pushed to 'one big block'
        uint8_t padding;

        uint64_t pos; // basicaly represent used size of the block
        alignas(std::max_align_t) uint8_t data[];
    };

    struct Container {
        Block* head;
        Block* tail;

        // the 'virtual' position
        // tracks where we are in the hypothetical 'one big buffer'
        uint64_t logicalPos;

        // track the strictest alignment ever requested
        size_t maxAlignSeen;

        // TODO: better name?
        uint64_t blockPayloadSize;
        uint64_t blockCount;
    };

    struct Marker {
        Block* block;
        uint64_t pos;
    };

    void init(Container* arena, uint64_t initialSize);
    void release(Container* arena);

    void* push(Container* arena, size_t size);
    void* push(Container* arena, size_t size, size_t align);

    Marker getMarker(Container* arena);
    // rollbacks to a previously obtained marker
    void rollback(Container* arena, Marker marker, bool freeMemory = false);
    // rollbacks to any previously allocated pointer
    void rollback(Container* arena, void* ptr, bool freeMemory = false);
    // rollbacks back 'size' in logical pos
    void rollback(Container* arena, uint64_t size, bool freeMemory = false);

    bool contain(Container* arena, void* ptr);

    void clear(Container* arena);

    uint8_t* getPointerToLogicalOffset(Container* arena, uint64_t logicalPos);

    uint64_t getFlatSize(Container* arena);
    uint64_t getMaxAlign(Container* arena);

    void flatCopy(Container* arena, uint8_t* dest);

}
