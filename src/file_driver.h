#pragma once

#include <cstdio>

#include "string.h"


namespace FileDriver {

    // 0 on success.
    int newDir(const String path);

    // Opens an existing file at `name` under `mode`.
    // Returns NULL on failure.
    FILE* openFile(const String name, const char* const mode);

    // Creates the directory `dir` if needed and opens `name` inside it.
    // Returns NULL on failure.
    FILE* openFile(const String name, const String dir, const char* const mode);

    // Reads the whole file at `path` into a freshly `malloc`'d buffer.
    // The buffer is NUL-terminated for convenience.
    // Returns the byte count, or a negative value on failure.
    // Caller owns `*buffer` and must `free` it.
    int64_t readFile(const String path, char** buffer);

    int writeFile(FILE* file, const String buffer);

    int createDirectory(const String path);

    int doesFileExists(const String path);

}
