// TODO : all path stuff is messy...

#include "file_system.h"
#include "file_driver.h"
#include "array_list.h"
#include "allocator.h"
#include "logger.h"
#include "set.h"
#include "config.h"
#include "string.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>

#if _WIN32
#include <windows.h>
#undef ERROR
#endif



namespace FileSystem {

    #if _WIN32
        constexpr int PATH_SEP = '\\';
    #elif
        constexpr int PATH_SEP = '/';
    #endif


    Logger::Type logErr = { .level = Logger::ERROR, .tag = stageTag };



    struct File {
        String   data;
        FileInfo info;
        Origin   origin;
        int      idx; // idx in filesData
        bool     isOpened;
    };

    DArray::Container filesData;
    Set::Container    filesSet;



    bool isAnySlash(const char ch) {
        return (ch == '\\' || ch == '/');
    }

    Path* makePath(Path* src) {

        Path* path = alloc<Path>();

        path->bufferLen = src->bufferLen;
        memcpy(path->buffer, src->buffer, src->bufferLen);
        path->buffer[path->bufferLen] = '\0';

        annotatePath(path);

        return path;

    }

    String nameFromPath(Path* path) {
        return String(
            path->buffer + path->nameOff,
            path->bufferLen - path->nameOff
        );
    }

    void goBackPath(String* path, int count) {
        if (!path || path->len <= 0 || count <= 0) return;

        int idx = path->len;

        while (count > 0) {
            // Skip trailing slashes
            while (idx >= 0 && isAnySlash(path->buff[idx])) {
                idx--;
            }

            #ifdef _WIN32
            if (idx == 1 && path->buff[idx] == ':') {
                idx = 2;
                break;
            }
            #endif

            // Go till the slash or start of the string
            while (idx >= 0 && !isAnySlash(path->buff[idx])) {
                idx--;
            }

            count--;

            // If the root slash stop and preserve slash
            if (idx == 0 && isAnySlash(path->buff[idx])) {
                idx = 1;
                break;
            }
        }

        path->len = idx;
    }

    void normalizePath(String* str) {
        if (!str || str->len == 0) return;

        int idx = 0;
        int minPathLen = 1;
        bool isAbsolute = false;

        #ifdef _WIN32
            // drive letter (C:/)
            if (str->len >= 2 && str->buff[1] == ':') {
                idx = 3;

                // This may invalidate dir name, but as
                // we want to do transformation in place
                // we cannot repair it here...
                str->buff[2] = PATH_SEP;
                isAbsolute = true;
                minPathLen = 3;
            }
        #endif

        // leading slash (Unix root or Windows network)
        if (idx < str->len && isAnySlash(str->buff[idx])) {
            str->buff[idx] = PATH_SEP;
            idx++;
            isAbsolute = true;
        }

        int namedDirCount = 0;
        int writeIdx = idx;

        while (idx < str->len) {
            const char ch = str->buff[idx];

            if (isAnySlash(ch)) {
                idx++;
                continue;
            }

            int dirStart = idx;
            while (idx < str->len && isAnySlash(ch)) {
                idx++;
            }
            int dirLen = idx - dirStart;

            // We have . dir
            if (dirLen == 1 && str->buff[dirStart] == '.') {
                continue;
            }

            // ../../a/b/../../
            // We have .. dir
            if (
                dirLen == 2 &&
                str->buff[dirStart] == '.' &&
                str->buff[dirStart + 1] == '.'
            ) {
                if (namedDirCount <= 0) {
                    writeIdx += 2;
                    str->buff[writeIdx] = PATH_SEP;
                    continue;
                }

                String tmp = { str->buff, (uint64_t) writeIdx };
                goBackPath(&tmp, 1);
                namedDirCount--;

                continue;
            }

            // We have normal dir
            namedDirCount++;
            str->buff[writeIdx++] = PATH_SEP;

            memmove(str->buff + writeIdx, str->buff + dirStart, dirLen);
            writeIdx += dirLen;
        }

        str->buff[writeIdx] = '\0';
        str->len = writeIdx;
    }

    int countPrefixBackSteps(String path) {
        int count = 0;

        int idx = 0;
        while (idx + 2 < path.len) {
            if (path.buff[idx] == '.' && path.buff[idx + 1] == '.') {
                if (idx + 3 >= path.len || isAnySlash(path.buff[idx + 2])) {
                    count++;
                }
                break;
            }
        }

        return count;
    }

    bool isPathGlobal(String str) {
        if (str.len == 0 || str.buff == NULL) return false;

        // Unix-style
        if (str.buff[0] == '/' || str.buff[0] == '\\') {
            return true;
        }

        // Windows-style
        if (str.len >= 2) {
            const char ch = str.buff[0];
            if (
                ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) &&
                str.buff[1] == ':'
            ) {
                return true;
            }
        }

        // Network / UNC paths
        if (str.len >= 2 && str.buff[0] == '\\' && str.buff[1] == '\\') {
            return true;
        }

        return false;
    }

    bool combinePath(Path* base, String appendix, Path* out) {
        normalizePath(&appendix);

        if (isPathGlobal(appendix)) {
            if (appendix.len > MAX_FILE_PATH) {
                return false;
            }
            memcpy(base->buffer, appendix.buff, appendix.len);
            return true;
        }

        const int backSteps = countPrefixBackSteps(appendix);

        String strPath = { base->buffer, base->bufferLen };
        goBackPath(&strPath, backSteps);

        // As we ensured that appendix is normalized...
        int appendixOffset = 3 * backSteps;
        int appendixLength = appendix.len - appendixOffset;
        if (base->bufferLen + appendixLength > MAX_FILE_PATH) {
            return true;
        }

        memcpy(
            base->buffer + base->bufferLen,
            appendix.buff + appendixOffset,
            appendixLength
        );

        return false;
    }

    String catPaths(Path* base, const char* const appendix) {

        const int appendixLen = cstrlen(appendix);
        const int len = base->nameOff + appendixLen;
        char* const str = alloc<char>(len + 1);
        str[len] = '\0';

        memcpy(str, base->buffer, base->nameOff);
        memcpy(str + base->nameOff, appendix, appendixLen);

        return String(str, len);

    }

    String catPaths(Path* base, Path* appendix) {

        const int len = base->nameOff + appendix->bufferLen;
        char* const str = alloc<char>(len + 1);
        str[len] = '\0';

        memcpy(str, base->buffer, base->nameOff);
        memcpy(str + base->nameOff, appendix->buffer, appendix->bufferLen);

        return String(str, len);

    }

    Path* getExePath() {

        Path* path = alloc<Path>();
        path->bufferLen = MAX_FILE_PATH;

        #ifdef _WIN32
            DWORD result = GetModuleFileNameA(NULL, path->buffer, path->bufferLen);
            if (result > 0) {
                path->bufferLen = strlen(path->buffer);
                annotatePath(path);
                toParentPath(path);
            } else {
                Logger::log(logErr, "SYSTEM: Failed to get executable path! (GetModuleFileNameA failed)");
                return NULL;
            }
        #else
            ssize_t len = readlink("/proc/self/exe", path->buffer, path->bufferLen);
            if (len != -1) {
                path->bufferLen = strlen(path->buffer);
                annotatePath(path);
                toParentPath(path);
            } else {
                Logger::log(logErr, "Failed to get executable path! (readlink failed)");
                return NULL;
            }
        #endif

        return path;

    }

    FileInfo* getFileInfo(Handle fhnd) {
        File* file = (File*) fhnd;
        return &file->info;
    }

    void getFileDir(Path* filePath, Path* outDir) {
        int idx = filePath->bufferLen - 1;
        for (; idx >= 0; idx--) {
            if (isAnySlash(filePath->buffer[idx])) {
                break;
            }
        }
        memcpy(outDir->buffer, filePath->buffer, idx + 1);
    }

    void init() {
        DArray::init(&filesData, Config::fileCount, sizeof(File));
        Set::init(&filesSet, Config::fileCount * 2);
        filesSet.hashMethod = Set::HM_STRING_FNV1A;
        filesSet.keyOffset = offsetof(File, info) +
                             offsetof(FileInfo, absPath) +
                             offsetof(String, buff);
    }

    void release() {
        DArray::release(&filesData);
        Set::release(&filesSet);
    }

    Handle load(Path* absPath, Origin origin) {

        {
            File* file = (File*) Set::find(&filesSet, (uint64_t) absPath->buffer);
            if (file) {
                file->isOpened = true;
                return file;
            }
        }

        char* buffer;
        const int bufferLen = FileDriver::readFile(absPath->buffer, &buffer);
        if (bufferLen < 0) return null;

        File file;
        file.isOpened = true;
        file.origin = origin;
        file.data.buff = buffer;
        file.data.len = bufferLen;
        file.info.modifiedTime = { 0, 0 };
        file.info.sizeBytes = 0;
        file.info.relativePath = NULL;
        file.info.userData = NULL;
        file.info.status = FileStatus::FS_DIRTY;

        file.info.absPath = makePath(absPath);
        annotatePath(file.info.absPath);

        toAbsolutePath(file.info.absPath);
        file.info.name = nameFromPath(file.info.absPath);

        file.idx = filesData.size;
        DArray::push(&filesData, &file);

        return DArray::get(&filesData, file.idx);

    }

    Handle load(String fname, Origin origin) {

        if (fname.len >= MAX_FILE_PATH) {
            return null;
        }

        Path* absPath = alloc<Path>();
        memcpy(absPath->buffer, fname.buff, fname.len);
        absPath->buffer[fname.len] = '\0';
        absPath->bufferLen = fname.len;

        return load(absPath, origin);

    }

    Handle load(String fpath, String fname, Origin origin) {

        if (fname.len + fpath.len + 1 >= MAX_FILE_PATH) {
            return null;
        }

        Path* absPath = alloc<Path>();

        memcpy(absPath->buffer, fpath.buff, fpath.len);
        absPath->buffer[fpath.len] = PATH_SEP;

        memcpy(absPath->buffer + fpath.len + 1, fname.buff, fname.len);
        absPath->buffer[fpath.len + fname.len + 1] = '\0';

        return load(absPath, origin);

    }

    void unload(Handle fhnd, Origin origin) {
        File* file = (File*) fhnd;
        if (file->idx >= filesData.size) return;

        file->isOpened = false;
    }

    const char* getBuffer(Handle fhnd) {
        File* file = (File*) fhnd;
        if (!file->isOpened) return NULL;

        return file->data.buff;
    }

    String getDirectory(Handle fhnd) {
        File* file = (File*) fhnd;
        return { file->info.absPath->buffer, file->info.absPath->nameOff };
    }

    Handle getHandle(String absPath) {
        return (Handle) Set::find(&filesSet, (uint64_t) absPath.buff);
    }

    void* getUserData(Handle fhnd) {
        File* file = (File*) fhnd;
        return file->info.userData;
    }

    void setUserData(Handle fhnd, void *dataPtr) {
        File* file = (File*) fhnd;
        file->info.userData = dataPtr;
    }

    Path* computeRelativePath(Handle fhnd, String root) {

        std::filesystem::path tmpRoot(std::string(root.buff, root.len));
        std::filesystem::path tmpFile(std::string(root.buff, root.len));

        std::filesystem::path relative = std::filesystem::relative(tmpFile, tmpRoot);
        const size_t relativeSize = relative.string().size();
        if (relativeSize > MAX_FILE_PATH) {
            return NULL;
        }

        FileInfo* info = getFileInfo(fhnd);
        if (!info->relativePath) {
            info->relativePath = alloc<Path>();
        }
        memcpy(info->relativePath, relative.c_str(), relativeSize);
        annotatePath(info->relativePath);

        return info->relativePath;

    }

}
