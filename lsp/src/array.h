#pragma once
#include "stdlib.h"



template <typename Element>
struct Array {
    Element* data     = 0;
    size_t   size     = 0;
    size_t   capacity = 0;

    inline Element& operator [] (const size_t idx) {
        return data[idx];
    }

    inline const Element& operator [] (const size_t idx) const {
        return data[idx];
    }
};

// TODO: for now here
//

template <typename T>
inline void init(Array<T>* arr, size_t initialCapacity) {
    arr->size     = 0;
    arr->capacity = initialCapacity;
    arr->data     = initialCapacity ? (T*) malloc(sizeof(T) * initialCapacity) : nullptr;
}

template <typename T>
inline void release(Array<T>* arr) {
    if (arr->data) {
        free(arr->data);
        arr->data = nullptr;
    }
    arr->size     = 0;
    arr->capacity = 0;
}

template <typename T>
inline void reserve(Array<T>* arr, size_t newCapacity) {
    if (newCapacity <= arr->capacity) return;

    T* next = (T*) realloc(arr->data, sizeof(T) * newCapacity);
    if (next) {
        arr->data     = next;
        arr->capacity = newCapacity;
    }
}

template <typename T>
inline void resize(Array<T>* arr, size_t newSize) {
    if (newSize > arr->capacity) {
        reserve(arr, newSize);
    }
    arr->size = newSize;
}

template <typename T>
inline void clear(Array<T>* arr) {
    arr->size = 0;
}

template <typename T>
inline T* push(Array<T>* arr, const T& item) {
    if (arr->size >= arr->capacity) {
        size_t nextCap = arr->capacity ? (arr->capacity * 2) : 16;
        reserve(arr, nextCap);
    }

    arr->data[arr->size] = item;
    return &arr->data[arr->size++];
}
