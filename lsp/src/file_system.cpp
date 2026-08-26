#include "../../src/file_system.h"
#include "../../src/file_driver.h"
#include "../../src/array_list.h"
#include "../../src/set.h"
#include "lsp.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>



namespace FileSystem {

    struct File {
        String   data;
        FileInfo info;
        Origin   origin;
        int      idx; // idx in FileStorage::data
        bool     isOpened;
    };

    struct FileStorage {
        DArray::Container data;
        Set::Container    set;
    };

    FileStorage filesLsp;
    FileStorage filesDisc;



    String persistString(String str) {
        if (str.buff == NULL || str.len == 0) {
            return { NULL, 0 };
        }

        char* buffer = (char*) malloc(str.len + 1);
        if (!buffer) return { NULL, 0 };

        memcpy(buffer, str.buff, str.len);
        buffer[str.len] = '\0';

        return { buffer, str.len };
    }

    Path* persistPath(const Path* src) {
        if (!src) return NULL;

        Path* dest = (Path*) malloc(sizeof(Path));
        if (!dest) return NULL;

        memcpy(dest, src, sizeof(Path));

        return dest;
    }



    void initFileStorage(FileStorage* files, size_t count) {
        DArray::init(&files->data, count, sizeof(File));
        Set::init(&files->set, 2 * count);
        files->set.keyOffset = offsetof(File, info) +
                               offsetof(FileInfo, absPath);
        files->set.hashMethod = Set::HM_STRING_FNV1A;
        files->set.keyStorage = Set::KS_POINTER;
    }

    void releaseFileStorage(FileStorage* files) {
        DArray::release(&files->data);
        Set::release(&files->set);
    }

    void init() {
        constexpr size_t tableSize = 1024;

        initFileStorage(&filesLsp, tableSize);
        initFileStorage(&filesDisc, tableSize);
    }

    void release() {
        releaseFileStorage(&filesLsp);
        releaseFileStorage(&filesDisc);
    }



    void insertFile(FileStorage* files, File* file) {
        file->idx = files->data.size;
        DArray::push(&files->data, file);
        Set::insert(&files->set, (uint8_t*) file);
    }

    // For now we dont actually remove file, just mark is
    // as not opened
    // TODO: it seems like we actually remove it... but not clear properly data
    void removeFile(FileStorage* files, File* file) {
        file->isOpened = false;
        Set::remove(&files->set, (uint64_t) file->info.name.buff);
    }



    File* makeFile(String fname, Origin origin) {
        File* file = (File*) calloc(1, sizeof(File));
        file->origin = origin;

        char* nameBuffer = (char*) malloc(fname.len + 1);
        memcpy(nameBuffer, fname.buff, fname.len + 1);

        file->info.name.buff = nameBuffer;
        file->info.name.len = fname.len;

        return file;
    }

    void releaseFile(File* file) {
        free(file->data.buff);
        free(file);
    }



    File* findFileLsp(String fname) {
        return (File*) Set::find(&filesLsp.set, (uint64_t) fname.buff);
    }

    File* findFileDisc(String fname) {
        return (File*) Set::find(&filesDisc.set, (uint64_t) fname.buff);
    }



    Handle load(String fname, Origin origin) {
        File* file;

        if (origin >= Origins::USER_START) {
            Path path;
            uriToPath(fname, &path);

            file = findFileLsp(fname);
            if (!file) {
                file = makeFile(fname, origin);
                if (!file) return NULL;

                file->info.absPath = persistPath(&path);
                file->info.name = {
                    file->info.absPath->buffer + path.nameOff,
                    (uint64_t) (path.bufferLen - path.nameOff)
                };

                insertFile(&filesLsp, file);
            }
        } else {
            if (fname.len > MAX_FILE_PATH) {
                return NULL;
            }

            Path path;
            memcpy(path.buffer, fname.buff, fname.len);
            if (!toAbsolutePath(&path)) {
                return NULL;
            }
            annotatePath(&path);

            fname.buff = path.buffer;
            fname.len  = path.bufferLen;

            file = findFileLsp(fname);
            if (!file) file = findFileDisc(fname);

            if (!file) {
                char* buffer;
                size_t size = FileDriver::readFile(fname.buff, &buffer);
                if (size < 0) return NULL;

                file = makeFile(fname, origin);
                file->data = { buffer, size };
                file->origin = origin;

                file->info.absPath = persistPath(&path);
                file->info.name = {
                    path.buffer + path.nameOff,
                    (uint64_t) (path.bufferLen - path.nameOff)
                };

                insertFile(&filesDisc, file);
            }
        }

        return file;
    }

    Handle load(String fpath, String fname) {
        // TODO
        return NULL;
    }

    void unload(Handle handle, Origin origin) {
        File* file = (File*) handle;
        String abs = {
            file->info.absPath->buffer,
            file->info.absPath->bufferLen
        };

        if (origin >= Origins::USER_START) {
            File* file = findFileLsp(abs);
            removeFile(&filesLsp, file);
        } else {
            File* file = findFileDisc(abs);
            removeFile(&filesDisc, file);
        }
    }

    Handle getHandle(String absPath) {
        Path path;
        uriToPath(absPath, &path);

        absPath.buff = path.buffer;
        absPath.len = path.bufferLen;

        File * file = findFileLsp(absPath);
        if (!file) file = findFileDisc(absPath);

        return file;
    }

    bool isAnySlash(const char ch) {
        return (ch == '\\' || ch == '/');
    }
    // TODO
    void getFileDir(Path* filePath, Path* outDir) {
        int idx = filePath->bufferLen - 1;
        for (; idx >= 0; idx--) {
            if (isAnySlash(filePath->buffer[idx])) {
                break;
            }
        }
        memcpy(outDir->buffer, filePath->buffer, idx + 1);
    }

    FileInfo* getFileInfo(Handle hnd) {
        return &((File*) hnd)->info;
    }

    void* getUserData(Handle handle) {
        File* file = (File*) handle;
        return file->info.userData;
    }

    char const* getBuffer(Handle handle) {
        File* file = (File*) handle;
        if (file->origin >= Origins::USER_START) {
            Lsp::FileData* data = (Lsp::FileData*) file->info.userData;
            return data->data;
        }

        return file->data;
    }
}
