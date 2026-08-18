#include "set.h"
#include "string.h"
#include "utils.h"

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>

namespace Set {

    uint64_t hash(HashMethod method, uint64_t data) {
        switch (method) {
            case HM_FIBONACCI: {
                return data * 11400714819323198485llu;
            }

            case HM_MURMUR3: {
                data ^= data >> 33;
                data *= 0xff51afd7ed558ccdLLU;
                data ^= data >> 33;
                data *= 0xc4ceb9fe1a85ec53LLU;
                data ^= data >> 33;
                return data;
            }

            case HM_FNV1A: {
                uint64_t hash = 0xcbf29ce484222325ULL;
                for (int i = 0; i < 8; i++) {
                    hash ^= (data & 0xFF);
                    hash *= 0x100000001b3ULL;
                    data >>= 8;
                }
                return hash;
            }

            case HM_12_FNV1A: {
                uint8_t* str = (uint8_t*) data;
                uint64_t hash = 0xcbf29ce484222325ULL;
                for (int i = 0; i < 12; i++) {
                    hash ^= (uint64_t) *str;
                    hash *= 0x100000001b3ULL;
                    str++;
                }
                return hash;
            }

            case HM_STRING_FNV1A: {
                uint8_t* str = (uint8_t*) data;
                uint64_t hash = 0xcbf29ce484222325ULL;
                while (*str) {
                    hash ^= (uint64_t) *str;
                    hash *= 0x100000001b3ULL;
                    str++;
                }
                return hash;
            }

            case HM_STRING_STRUCT_FNV1A: {
                String* str = (String*) data;
                uint64_t hash = 0xcbf29ce484222325ULL;
                for (int i = 0; i < str->len; i++) {
                    hash ^= (uint64_t) str->buff[i];
                    hash *= 0x100000001b3ULL;
                }
                return hash;
            }

            case HM_IDENTITY:
            default: {
                return data;
            }
        }
    };

    uint64_t getKey(Container* const set, uint64_t data) {
        if (set->keyStorage == KS_POINTER) {
            return (uint64_t) * (uint64_t**)(data + set->keyOffset);
        }
        else {
            return (uint64_t)(uint64_t**)(data + set->keyOffset);
        }
    }

    inline Slot* allocTable(const uint64_t size) {
        Slot* table = (Slot*) malloc(size * sizeof(Slot));
        if (!table) {
            // TODO
            exit(45354);
        }

        memset(table, 0, size * sizeof(Slot));

        return table;
    }

    void resize (Container* set, uint64_t newSize) {

        newSize = Utils::getPow2Ceil(newSize);
        if (newSize <= set->tableSize) return;

        Slot* oldTable = set->table;
        uint64_t oldTableSize = set->tableSize;

        set->table = allocTable(newSize);
        set->tableSize = newSize;

        const uint64_t mask = set->tableSize - 1;
        for (uint64_t i = 0; i < oldTableSize; i++) {
            if (oldTable[i].type == ST_OCCUPIED) {
                const uint64_t key = getKey(set, (uint64_t) oldTable[i].data);
                uint64_t idx = hash(set->hashMethod, key) & mask;

                while (set->table[idx].type != ST_EMPTY) {
                    idx = (idx + 1) & mask;
                }

                set->table[idx].data = oldTable[i].data;
                set->table[idx].type = ST_OCCUPIED;
            }
        }

       free(oldTable);

    }

    void init(Container* set, uint64_t tableSize) {
        tableSize = Utils::getPow2Ceil(tableSize);

        set->table = allocTable(tableSize);
        set->tableSize = tableSize;
        set->usedSize = 0;
        set->keyOffset = 0;
        set->hashMethod = HM_MURMUR3;
        set->keyStorage = KS_VALUE;
    }

    void release(Container* set) {
        free(set->table);
    }

    bool cmpKeys(Container* set, uint64_t keyA, uint64_t keyB) {
        return (
            keyA == keyB || (
                keyA && keyB &&
                (
                    set->hashMethod >= HM_STRING_START &&
                    set->hashMethod < HM_STRING_STRUCT_START &&
                    strcmp((char*) keyA, (char*) keyB) == 0
                ) ||
                (
                    set->hashMethod >= HM_STRING_STRUCT_START &&
                    cstrcmp((String*) keyA, (String*) keyB)
                )
            )
        );
    }

    bool insert(Container* set, uint8_t* data) {
        const uint64_t key = getKey(set, (uint64_t) data);

        if (set->usedSize * 10 >= set->tableSize * 7) {
            if (set->tableSize * 2 > set->tableSize) {
                resize(set, set->tableSize * 2);
            }
            // TODO : handle max size, or not)
        }

        const uint64_t mask = set->tableSize - 1;

        uint64_t deletedIdx = UINT64_MAX;
        uint64_t idx = hash(set->hashMethod, key) & mask;

        for (uint64_t i = 0; i < set->tableSize; i++) {
            const Slot slot = set->table[idx];
            if (slot.type == ST_EMPTY) break;

            if (slot.type == ST_OCCUPIED) {
                uint64_t slotKey = getKey(set, slot.data);
                if (cmpKeys(set, slotKey, key)) return false;
            } else {
                if (deletedIdx == UINT64_MAX) {
                    deletedIdx = idx;
                }
            }

            idx = (idx + 1) & mask;
        }

        idx = deletedIdx == UINT64_MAX ? idx : deletedIdx;
        set->table[idx] = {
            .data = (uint64_t) data,
            .type = ST_OCCUPIED
        };
        set->usedSize++;

        return true;
    }

    bool remove(Container* set, uint64_t key) {
        if (set->tableSize == 0) return false;

        const uint64_t mask = set->tableSize - 1;
        uint64_t idx = hash(set->hashMethod, key) & mask;

        for (uint64_t i = 0; i < set->tableSize; i++) {
            Slot slot = set->table[idx];
            if (slot.type == ST_EMPTY) break;

            if (slot.type == ST_OCCUPIED) {
                uint64_t slotKey = getKey(set, slot.data);
                if (cmpKeys(set, slotKey, key)) {
                    // Whatever
                    set->table[idx].type = ST_DELETED;
                    set->usedSize--;
                    return true;
                }
            }

            idx = (idx + 1) & mask;
        }

        return false;
    }

    uint8_t* find(Container* set, uint64_t key) {
        if (set->tableSize == 0) return NULL;

        const uint64_t mask = set->tableSize - 1;
        uint64_t idx = hash(set->hashMethod, key) & mask;

        for (uint64_t i = 0; i < set->tableSize; i++) {
            Slot slot = set->table[idx];
            if (slot.type == ST_EMPTY) break;

            if (slot.type == ST_OCCUPIED) {
                uint64_t slotKey = getKey(set, slot.data);
                if (cmpKeys(set, slotKey, key)) {
                    return (uint8_t*) slot.data;
                }
            }

            idx = (idx + 1) & mask;
        }

        return NULL;
    }



    // String interface
    //

    struct StringSetSlot {
        String key;
        uint8_t* data;
    };

    bool insert(Container* set, String key, uint8_t* data) {
        StringSetSlot slot = {
            .key = key,
            .data = data
        };

        return insert(set, (uint8_t*) &slot);
    }

    bool remove(Container* set, String key) {
        return remove(set, (uint64_t) &key);
    }

    uint8_t* find(Container* set, String key) {
        return find(set, (uint64_t) &key);
    }



    void clear(Container *set) {
        memset(set->table, 0, set->tableSize * sizeof(Slot));
        set->usedSize = 0;
    }

}
