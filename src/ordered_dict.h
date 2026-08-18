#pragma once

#include "array_list.h"
#include "string.h"
#include <cstdint>

namespace OrderedDict {

    enum Flags {
        COPY_STRINGS  = 1 << 0,
        KEY_IS_INDEX  = 1 << 1,
        KEY_IS_STRING = 1 << 2,
    };

    struct Pair {
        union Key {
            String   str;
            uint64_t idx;
        } key;
        void* data;
    };

    struct Container {
        DArray::Container pairs;
        uint64_t flags;
        uint64_t it;
    };

    void init(Container* dict, size_t initialSize);

    void* get(Container* dict, String key);
    void* get(Container* dict, uint64_t key);

    int set(Container* dict, String key, void* dataPtr);
    int set(Container* dict, uint64_t key, void* dataPtr);

    Pair* getNext(Container* dict);

    void resetIterator(Container* dict);

    void clear(Container* dict);

    Container* tightCopy(Container* src);

}
