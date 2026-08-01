#pragma once

#include "string.h"
#include <cstdint>
namespace Set {

    enum HashMethod {
        HM_IDENTITY,    // already hash/random id
        HM_FIBONACCI,
        HM_MURMUR3,
        HM_FNV1A,

        // Start of the section where data will be treated as
        // null-terminated strings
        HM_STRING_START,
        HM_STRING_FNV1A = HM_STRING_START,

        // 'key' is treated as a pointer to 12 byte int
        HM_12_FNV1A,

        // Start of the section where data will be treated as
        // string struct { char*, u64 }
        HM_STRING_STRUCT_START,
        HM_STRING_STRUCT_FNV1A = HM_STRING_STRUCT_START,
    };

    enum KeyStorage {
        KS_POINTER,
        KS_VALUE,
    };

    enum SlotType {
        ST_EMPTY,
        ST_OCCUPIED,
        ST_DELETED,
    };

    struct Slot {
        uint64_t data;
        SlotType type;
    };

    struct Container {
        Slot*    table;
        uint64_t tableSize;

        uint64_t usedSize;
        uint64_t keyOffset;

        HashMethod hashMethod;
        KeyStorage keyStorage;
    };

    void init(Container* set, uint64_t tableSize);
    void release(Container* set);

    // true: item was inserted; false: item already exists
    bool insert(Container* set, uint8_t* data);
    // true: item was removed; false: item does not exists
    bool remove(Container* set, uint64_t key);

    uint8_t* find(Container* set, uint64_t key);

    // String interface for convenience
    // NOTE : make sure keyOffset is 0 and hashMethod is
    //        appropriate (>= HM_STRING_STRUCT_START)
    bool insert(Container* set, String key, uint8_t* data);
    bool remove(Container* set, String key);
    uint8_t* find(Container* set, String key);

    void clear(Container* set);

}
