#include "ordered_dict.h"
#include "array_list.h"
#include "allocator.h"
#include "string.h"
#include <cstdint>

namespace OrderedDict {

    void init(Container* dict, size_t initialSize) {
        dict->it = 0;
        dict->flags = 0;
        DArray::init(&dict->pairs, initialSize, sizeof(Pair));
    }

    struct Slot {
        bool match;
        int idx;
    };

    Slot getSlot(Container* dict, Pair::Key key) {
        Pair* pairs = (Pair*) dict->pairs.buffer;

        // binary search
        int left = 0;
        int right = (int) dict->pairs.size - 1;

        while (left <= right) {
            int idx = (left + right) / 2;
            int cmp = 0;

            if (dict->flags & KEY_IS_STRING) {
                cmp = cstracmp((pairs + idx)->key.str, key.str);
            } else {
                cmp = (pairs + idx)->key.idx - key.idx;
            }

            if (cmp == 0) {
                return { .match = 1, .idx = idx };
            } else if (cmp < 0) {
                left = idx + 1;
            } else {
                right = idx - 1;
            }
        }

        return { .match = 0, .idx = left };
    }

    void* get(Container* dict, String key) {
        const Slot slot = getSlot(dict, (Pair::Key) key);
        return slot.match ? (((Pair*) dict->pairs.buffer) + slot.idx)->data : NULL;
    }

    void* get(Container* dict, uint64_t key) {
        const Slot slot = getSlot(dict, { .idx = key });
        return slot.match ? (((Pair*) dict->pairs.buffer) + slot.idx)->data : NULL;
    }

    int set(Container* dict, String key, void* dataPtr) {
        const Slot slot = getSlot(dict, { .str = key });
        if (slot.match) return 0;

        DArray::shiftRight(&dict->pairs, slot.idx);

        Pair pair = {};
        pair.key.str.len = key.len;
        pair.data = dataPtr;

        if (dict->flags & COPY_STRINGS) {
            pair.key.str.buff = (char*) alloc(alc, key.len + 1);
            memcpy(pair.key.str.buff, key.buff, key.len);
            pair.key.str.buff[key.len] = '\0';
        } else {
            pair.key.str.buff = key.buff;
        }

        DArray::set(&dict->pairs, slot.idx, &pair);
        return 1;
    }

    int set(Container* dict, uint64_t key, void* dataPtr) {
        const Slot slot = getSlot(dict, { .idx = key });
        if (slot.match) return 0;

        DArray::shiftRight(&dict->pairs, slot.idx);

        Pair pair = {};
        pair.key.idx = key;
        pair.data = dataPtr;

        DArray::set(&dict->pairs, slot.idx, &pair);
        return 1;
    }

    Pair* getNext(Container* dict) {
        if (dict->pairs.size <= 0) return NULL;

        Pair* pair = ((Pair*) dict->pairs.buffer + dict->it);

        dict->it++;
        if (dict->it >= dict->pairs.size) dict->it = 0;

        return pair;
    }

    void resetIterator(Container* dict) {
        dict->it = 0;
    }

    void clear(Container* dict) {
        DArray::clear(&dict->pairs);
        dict->it = 0;
    }

    Container* tightCopy(Container* src) {
        Container* dest = (Container*) alloc(alc, sizeof(Container));

        DArray::init(&dest->pairs, src->pairs.size, src->pairs.elementSize);
        memcpy(dest->pairs.buffer, src->pairs.buffer, src->pairs.size * src->pairs.elementSize);
        dest->pairs.size = src->pairs.size;

        dest->it = src->it;
        dest->flags = src->flags;


        return dest;
    }

}
