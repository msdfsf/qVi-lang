#include "dynamic_arena.h"
#include "utils.h"

#include <cstddef>
#include <cstdint>
#include <stdlib.h>
#include <cstring>
#include <cstddef>

#define max(a, b) ((a) > (b) ? (a) : (b))

namespace Arena {

    Block* initBlock(Container* arena) {

        Block* block = (Block*) malloc(sizeof(Block) + arena->blockPayloadSize);
        if (!block) {
            // TODO
            exit(1231);
        };

        block->padding = 0;
        block->pos = 0;
        block->prev = NULL;
        block->next = NULL;

        return block;

    }

    void releaseBlock(Block* block) {
        free(block);
    }

    void init(Container* arena, uint64_t initialSize) {

        arena->blockPayloadSize = initialSize;
        arena->head = initBlock(arena);
        arena->tail = arena->head;
        arena->blockCount = 1;
        arena->logicalPos = 0;
        arena->maxAlignSeen = 1;

    }

    void release(Container* arena) {

        Block* block = arena->head;
        while (block) {
            Block* next = block->next;
            free(block);
            block = next;
        }

        arena->head = NULL;
        arena->tail = NULL;
        arena->blockCount = 0;

    }

    void* push(Container* arena, size_t size) {
        return push(arena, size, alignof(std::max_align_t));
    }

    void* push(Container* arena, size_t size, size_t align) {

        // make sure align is 2^n
        align = Utils::getPow2Ceil(align);

        uintptr_t addr = (uintptr_t) arena->tail->data + arena->tail->pos;
        size_t padding = Utils::getPadding(addr, align);

        if (arena->tail->pos + padding + size > arena->blockPayloadSize) {

            if (size > arena->blockPayloadSize) {
                // TODO: block cannot fit - add custom sizes
                exit(12313);
            }

            Block* nextBlock = arena->tail->next;
            if (!nextBlock) {
                nextBlock = initBlock(arena);
                nextBlock->prev = arena->tail;
                arena->tail->next = nextBlock;
                arena->blockCount++;
            }

            arena->tail = nextBlock;

            // computing block padding, we want to preserve layout
            // as it was pushed at one block

            // we ghostly allow alignment higher than max align
            const uint64_t baseAlign = alignof(std::max_align_t);
            const uint64_t syncAlign = max(align, baseAlign);

            arena->tail->padding = arena->logicalPos % syncAlign;
            arena->tail->pos = arena->tail->padding;

            padding = Utils::getPadding((uintptr_t) arena->tail->data, align);

        }

        void* ptr = arena->tail->data + arena->tail->pos + padding;
        arena->tail->pos += padding + size;
        arena->logicalPos += padding + size;

        if (align > arena->maxAlignSeen) arena->maxAlignSeen = align;

        return ptr;

    }

    Marker getMarker(Container* arena) {
        return { .block = arena->tail, .pos = arena->logicalPos };
    }

    void rollback(Container* arena, Marker marker, bool freeMemory) {
        arena->tail = marker.block;
        arena->tail->pos = marker.pos;

        if (freeMemory) {
            Block* block = arena->tail->next;
            while (block) {
                Block* next = block->next;
                free(block);
                block = next;
            }
        }
    }

    void rollback(Container* arena, void* ptr, bool freeMemory) {
        Block* block = arena->tail;
        while (block) {
            uint8_t* start = block->data;
            uint8_t* end = start + arena->blockPayloadSize;

            if (ptr >= start && ptr < end) {
                arena->tail = block;
                arena->tail->pos = (uint8_t*) ptr - start;
                return;
            }

            block = block->prev;

            if (freeMemory) {
                releaseBlock(block->next);
                block->next = NULL;
            }
        }
    }

    void rollback(Container* arena, uint64_t size, bool freeMemory) {
        Block* block = arena->tail;
        while (block) {
            if (arena->tail->pos > size) {
                arena->tail->pos -= size;
                return;
            }

            arena->tail->pos = 0;
            block = block->prev;

            if (freeMemory) {
                releaseBlock(block->next);
                block->next = NULL;
            }
        }
    }

    void clear(Container* arena) {

        if (!arena->head) return;

        // TODO : it seems we are losing allocated blocks
        arena->tail = arena->head;
        arena->tail->pos = 0;
        arena->blockCount = 1;
        arena->logicalPos = 0;

    }

    uint64_t getFlatSize(Arena::Container* arena) {
        return arena->logicalPos;
    }

    uint64_t getMaxAlign(Arena::Container* arena) {
        return alignof(std::max_align_t);
    }

    // dest should be aligned at least at result of getMaxAlign
    // dest should be able to fit at least result of getFlatSize
    void flatCopy(Container* arena, uint8_t* dest) {

        Block* block = arena->head;

        while (block) {
            memcpy(dest, block->data + block->padding, block->pos);
            dest += block->pos;
            block = block->next;
        }

    }

}
