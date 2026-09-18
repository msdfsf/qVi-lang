#pragma once

#include "string.h"
#include "task_status.h"
#include <atomic>
#include <cstdint>



// This serves as a compile-time interface for file handling,
// as different might need to represent files differently.
namespace FileSystem {

    constexpr size_t MAX_FILE_PATH = 4096;

    typedef void* Handle;
    constexpr Handle null = NULL;

    // Used to signal in which state the file processing is.
    // Assumed to be used for concurrency handling.
    enum FileStatus : int {
        FS_DIRTY = TS_PENDING,
        FS_BUSY  = TS_RUNNING,
        FS_READY = TS_READY
    };

    struct Timestamp {
        uint64_t low;
        uint64_t high;
    };

    enum PathFlag : uint8_t {
        PF_ABSOLUTE      = 1 << 0,
        PF_NORMALIZED    = 1 << 1,
        PF_HAS_EXT       = 1 << 2,
        PF_HAS_NAME      = 1 << 3,
        PF_IS_DIR        = 1 << 4,
        PF_IS_FILE       = 1 << 5,
    };

    struct Path {
        char     buffer[MAX_FILE_PATH];
        uint16_t bufferLen;
        uint16_t nameOff;
        uint16_t extensionOff;
        uint8_t  flags;
    };

    struct FileInfo {
        String    name;
        Path*     absPath;
        Path*     relativePath; // has to be computed
        uint64_t  sizeBytes;
        Timestamp modifiedTime;
        void*     astRoot;
        void*     userData;

        std::atomic<FileStatus> status;
    };

    // We need capability to know form which source was
    // file loading requested.
    typedef uint32_t Origin;

    // Reserved ranges for the Origin ID
    namespace Origins {
        // Reserved for Compiler (0 - 10)
        constexpr Origin COMPILER_ARBITRARY = 0;
        constexpr Origin COMPILER_SOURCE    = 1;
        constexpr Origin COMPILER_ASSET     = 2;

        // Start of user specific IDs (11+)
        // The LSP or any other tool should define its own IDs
        // starting from here.
        constexpr Origin USER_START       = 11;
    }

    // init/release the FileSystem itself
    void init   ();
    void clear  ();
    void release();

    // load/unload file from the FileSystem
    Handle load(String fname, Origin origin);
    Handle load(String fpath, String fname, Origin origin);

    // For test purposes, but maybe evolve later to something
    // meaningful.
    Handle loadBuffer(String fname, String data, Origin origin);

    void   unload(Handle file, Origin origin);

    const char* getBuffer   (Handle file);
    FileInfo*   getFileInfo (Handle flhnd);
    String      getDirectory(Handle file);
    Handle      getHandle   (String absPath);

    void getFileDir(Path* filePath, Path* outDir);

    void* getUserData(Handle flhnd);
    void  setUserData(Handle flhnd, void* dataPtr);

    // Path procedures
    Path* makePath      (String str);
    Path* initPath      (Path* path);
    void  releasePath   (Path* path);
    void  annotatePath  (Path* path);
    bool  toAbsolutePath(Path* path);
    void  toParentPath  (Path* path);
    void  uriToPath     (String uri, Path* out);
    bool  combinePath   (Path* base, String appendix, Path* out);
    int   computeRelativePath(Path* abs, String root, Path* out);
    Path* computeRelativePath(Handle flhnd, String str);

    bool isPath(String name);

    String catPaths(Path* base, const char* const appendix);

    Path* getExePath();

}
