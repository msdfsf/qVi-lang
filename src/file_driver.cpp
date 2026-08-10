#include "file_driver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace FileDriver {

    #ifdef _WIN32
        #include <direct.h>
        #include <errno.h>
        int newDir(const String path) {
            // _mkdir needs a NUL-terminated C string; build one in a
            // stack buffer if the path doesn't happen to be terminated.
            char buf[260];
            if (path.len < sizeof(buf)) {
                std::memcpy(buf, path.buff, path.len);
                buf[path.len] = '\0';
                if (_mkdir(buf)) return (errno != EEXIST);
                return 0;
            }
            // Fallback for unusually long paths: leak-friendlier than the
            // old fixed 256-byte buffer that silently truncated.
            char* heap = (char*) malloc(path.len + 1);
            std::memcpy(heap, path.buff, path.len);
            heap[path.len] = '\0';
            const int res = _mkdir(heap) ? (errno != EEXIST) : 0;
            free(heap);
            return res;
        }
    #else
        #include <sys/stat.h>
        int newDir(const String path) {
            char buf[260];
            if (path.len < sizeof(buf)) {
                std::memcpy(buf, path.buff, path.len);
                buf[path.len] = '\0';
                return mkdir(buf, 0777);
            }
            char* heap = (char*) malloc(path.len + 1);
            std::memcpy(heap, path.buff, path.len);
            heap[path.len] = '\0';
            const int res = mkdir(heap, 0777);
            free(heap);
            return res;
        }
    #endif

    // Internal helper: open a NUL-terminated C string under `mode`.
    static FILE* openCStr(const char* cstr, const char* const mode) {
        return std::fopen(cstr, mode);
    }

    FILE* openFile(const String name, const char* const mode) {
        // Many paths that arrive here are already NUL-terminated (they
        // came straight from a C-string literal or a parser-managed
        // buffer with one byte of slack). Detect that case cheaply and
        // skip the copy.
        if (name.buff != NULL && name.buff[name.len] == '\0') {
            return openCStr(name.buff, mode);
        }

        char* buf = (char*) malloc(name.len + 1);
        std::memcpy(buf, name.buff, name.len);
        buf[name.len] = '\0';

        FILE* file = openCStr(buf, mode);
        free(buf);
        return file;
    }

    FILE* openFile(const String name, const String dir, const char* const mode) {
        newDir(dir);

        // "dir/name" with one separator and a NUL terminator. Worst-case
        // path lengths large enough to overflow size_t are nonsensical
        // here, so a saturating add is safe.
        const uint64_t total = dir.len + 1 + name.len + 1;

        char* buf = (char*) malloc(total);
        if (!buf) return NULL;

        std::memcpy(buf, dir.buff, dir.len);
        buf[dir.len] = '/';
        std::memcpy(buf + dir.len + 1, name.buff, name.len);
        buf[dir.len + 1 + name.len] = '\0';

        FILE* file = openCStr(buf, mode);
        free(buf);
        return file;
    }

    int64_t readFile(const String path, char** buffer) {
        // Earlier implementation used a stack-copied NUL-termination
        // (with a fixed 256-byte buffer). Reuse the openFile() code path
        // so we don't duplicate that logic.
        FILE* file = openFile(path, "rb");
        if (!file) return -1;

        std::fseek(file, 0, SEEK_END);
        const int64_t fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        // The original code allocated fileSize + 1 and fread that many
        // bytes, then NUL-terminated at [fileSize]. preserve that
        // behaviour for downstream string-construction callers that
        // expect a NUL terminator.
        char* buf = (char*) malloc((size_t) (fileSize + 1));
        if (!buf) {
            std::fclose(file);
            return -1;
        }

        const size_t read = std::fread(buf, 1, (size_t) fileSize, file);
        buf[read] = '\0';

        std::fclose(file);

        *buffer = buf;
        return (int64_t) read;
    }

    int writeFile(FILE* file, const String buffer) {
        if (!file) return -1;
        const size_t written = std::fwrite(buffer.buff, 1, buffer.len, file);
        return (int) written == buffer.len ? 0 : -1;
    }

    int createDirectory(const String path) {
        // std::filesystem::create_directory requires a std::filesystem::path
        // that is NUL-terminable; build one regardless of whether the
        // underlying String is NUL-terminated.
        char* buf = (char*) malloc(path.len + 1);
        std::memcpy(buf, path.buff, path.len);
        buf[path.len] = '\0';
        const bool ok = std::filesystem::create_directory(buf);
        free(buf);
        return ok ? 0 : 1;
    }

    int doesFileExists(const String path) {
        char* buf = (char*) malloc(path.len + 1);
        std::memcpy(buf, path.buff, path.len);
        buf[path.len] = '\0';
        const bool exists = std::filesystem::exists(buf);
        free(buf);
        return exists ? 1 : 0;
    }

}
