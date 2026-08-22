#include "array_list.h"
#include "data_types.h"
#include "dynamic_arena.h"

#include <cstddef>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>


const char* const EM_MALLOC = "Malloc? Malloc? Maaaalloc!";
const char* const EM_INDEX = "Index out of bounds!";

const size_t DEFAULT_INITIAL_SIZE = 4;
const size_t DEFAULT_CONST_COEF = 2;
const size_t DEFAULT_CONST_TERM = 0;



void errHandler(const char* msg) {
    fprintf(stderr, "Array error: %s\n", msg);
    exit(1);
}

void resize(DArray::Container* arr, size_t newSize) {

    void* tmp = realloc(arr->buffer, arr->elementSize * newSize);
    if (tmp == NULL) errHandler(EM_MALLOC);

    arr->buffer = tmp;
    arr->allocSize = newSize;

}

void DArray::init(Container* arr, size_t initialSize, size_t elementSize) {

    arr->size = 0;
    arr->allocSize = initialSize > 0 ? initialSize : DEFAULT_INITIAL_SIZE;
    arr->elementSize = elementSize;

    arr->constCoef = DEFAULT_CONST_COEF;
    arr->constTerm = DEFAULT_CONST_TERM;

    arr->buffer = malloc(arr->allocSize * elementSize);
    if (arr->buffer == NULL) errHandler(EM_MALLOC);

}

void DArray::release(Container* arr) {

    if (arr->buffer) free(arr->buffer);
    arr->buffer = NULL;
    arr->size = 0;
    arr->allocSize = 0;
    arr->elementSize = 0;

}

void DArray::push(Container* arr, void* element) {

    if (arr->size + 1 > arr->allocSize) {
        resize(arr, arr->allocSize * arr->constCoef + arr->constTerm);
    }

    memcpy((char*) arr->buffer + arr->size * arr->elementSize, element, arr->elementSize);
    arr->size++;

}

void DArray::pushFront(Container* arr, void* element) {

    if (arr->size + 1 > arr->allocSize) {
        resize(arr, arr->allocSize * arr->constCoef + arr->constTerm);
    }

    memmove((char*) arr->buffer + arr->elementSize, arr->buffer, arr->size * arr->elementSize);
    memcpy(arr->buffer, element, arr->elementSize);
    arr->size++;

}

// TODO : shouldn it return?
void DArray::pop(Container* arr) {

    if (arr->size > 0) {
        arr->size--;
    }

}

void DArray::clear(Container* arr) {

    arr->size = 0;

}

void DArray::set(Container* arr, size_t idx, void* element) {

    if (idx >= arr->size) errHandler(EM_INDEX);
    memcpy((char*) arr->buffer + idx * arr->elementSize, element, arr->elementSize);

}

void DArray::get(Container* arr, size_t idx, void* out) {

    if (idx >= arr->size) errHandler(EM_INDEX);
    memcpy(out, (char*) arr->buffer + idx * arr->elementSize, arr->elementSize);

}

void* DArray::get(Container* arr, size_t idx) {

    if (idx >= arr->size && idx < 0) return NULL; //errHandler(EM_INDEX);
    return ((char*) arr->buffer) + idx * arr->elementSize;

}

void* DArray::getLast(Container* arr) {

    return DArray::get(arr, arr->size - 1);

}

void DArray::shiftRight(Container* arr, size_t idx) {

    if (arr->size + 1 > arr->allocSize) {
        resize(arr, arr->allocSize * arr->constCoef + arr->constTerm);
    }

    memmove(
        ((uint8_t*) arr->buffer) + arr->elementSize * (idx + 1),
        ((uint8_t*) arr->buffer) + arr->elementSize * idx,
        (arr->size - idx) * arr->elementSize
    );

    arr->size++;

}
