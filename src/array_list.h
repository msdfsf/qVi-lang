#pragma once

#include <stdint.h>



namespace DArray {

    struct Container {

        void* buffer;

        size_t elementSize;
        size_t allocSize;
        size_t size;

        // size_n = size_n-1 * constCoef + constTerm
        uint32_t constCoef;
        uint32_t constTerm;

    };

    void init(Container* arr, size_t initialSize, size_t elementSize);
    void release(Container* arr);

    void push(Container* arr, void* element);
    void pushFront(Container* arr, void* element);
    void pop(Container* arr);

    void clear(Container* arr);

    void set(Container* arr, size_t index, void* element);
    void get(Container* arr, size_t index, void* out);
    void* get(Container* arr, size_t index);
    void* getLast(Container* arr);

    void shiftRight(Container* arr, size_t idx);

}
