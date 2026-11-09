// Definitions of the thread local state for tmp_string_allocator.h.
//
// The state lives in a single translation unit: MinGW ld cannot merge
// the 'TLS init function' GCC emits for a dynamically initialized
// 'inline thread_local' variable, so a second definition of it (i.e.
// another file including the header) fails to link with
// "multiple definition of TLS init function".

#include "tmp_string_allocator.h"



alignas(8) thread_local char gTmpStringBuffer[TMP_STRING_STATIC_CAPACITY];

thread_local TmpStringAllocator gTmpStringAllocator = {
    gTmpStringBuffer,
    0,
    TMP_STRING_STATIC_CAPACITY,
    0,
    nullptr,
    0
};
