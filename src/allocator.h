#pragma once

// By default, I don’t need to manage memory in any clever way. The code
// is not very suitable for reuse as, let’s say, a library for building tools
// like an LSP. To address this, I added a simple compile-time allocation interface.
//
// I wanted to keep the ability to inline functions and avoid using macros,
// since they are not very debugger or LSP friendly. Templates were also avoided,
// because they are verbose and too C++.
//
// I considered using general handles instead of pointers, but that
// would only complicate how objects are handled in the code. So I ended up
// using the default approach with pointers. If handles are needed, they must be
// part of an outer system capable of mapping pointers.
//
// For now, this is the provided solution. I am not quite happy or satisfied with it,
// but at least I can be sure that it works.
//
// If a custom allocator is needed, define _CUSTOM_ALLOCATOR_
// and provide implementations for the alloc/dealloc functions.
//
// Note: This is the default allocator used for general cases.
// It exists to be included where importing the 'AST' symbols
// and their allocator would be wasteful.
//

// TODO: think of a namespace
#if !defined(_CUSTOM_ALLOCATOR_)

    #include "dynamic_arena.h"

    #ifndef ALLOC_INIT_BUFFER_SIZE
        // 32 MB
        #define ALLOC_INIT_BUFFER_SIZE (1024 * 1024 * 32)
    #endif

    typedef Arena::Marker AllocatorMarker;

    typedef Arena::Container Allocator;
    inline thread_local Allocator allocator {};

    inline thread_local bool gAllocIsInitialized = false;



    inline bool allocIsInitialized() {
        return gAllocIsInitialized;
    }

    inline void allocInit() {
        if (!gAllocIsInitialized) {
            Arena::init(&allocator, ALLOC_INIT_BUFFER_SIZE);
            gAllocIsInitialized = true;
        }
    }

    inline void allocRelease() {
        if (gAllocIsInitialized) {
            Arena::release(&allocator);
            gAllocIsInitialized = false;
        }
    }

    inline void allocClear() {
        Arena::clear(&allocator);
    }



    inline void* alloc(size_t bytes, size_t align) {
        return Arena::push(&allocator, bytes, align);
    }

    inline void dealloc(void* ptr) {
        Arena::rollback(&allocator, ptr);
    }



    inline AllocatorMarker allocMark() {
        return Arena::getMarker(&allocator);
    }

    inline void allocRollback(AllocatorMarker marker) {
        Arena::rollback(&allocator, marker);
    }

#else

    typedef void* AllocatorMarker;
    typedef void* AllocatorHandle;
    extern thread_local AllocatorHandle alc;

    extern bool allocIsInitialized();
    extern void allocInit();
    extern void allocRelease();
    extern void allocClear();

    extern void* alloc   (size_t size, size_t align);
    extern void  dealloc (void* ptr);

    extern AllocatorMarker allocMark();
    extern void allocRollback(AllocatorMarker marker);

#endif

template <typename T>
inline T* alloc(size_t count = 1) {
    return (T*) alloc(sizeof(T) * count, alignof(T));
}
