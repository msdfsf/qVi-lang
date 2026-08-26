#pragma once
#include "json.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>



// NOTE: We receive a full contiguous buffer directly from the comm provider.
//       This buffer is managed with a baseline capacity and a max allowed limit.
//       When a request exceeds the baseline size, we temporarily realloc the buffer
//       to fit it, then shrink it back down to the baseline capacity after the
//       request finishes. If a request exceeds the max allowed limit, we treat
//       it as impossible to process.
//
namespace CommProvider {

    // TODO: to config
    constexpr size_t DEFAULT_BASELINE_CAPACITY = 1024 * 1024 * 2;
    constexpr size_t DEFAULT_MAX_CAPACITY      = 1024 * 1024 * 64;

    enum Err {
        OK = 0,
        ERR_CLOSED              = -1,
        ERR_TIMEOUT             = -2,
        ERR_IO                  = -3,
        ERR_PARSE               = -4,
        ERR_MALLOC              = -5,
        ERR_NOT_READY           = -6,
        ERR_FILE_NOT_FOUND      = -7,
        ERR_PERMISSION_DENIED   = -8,
        ERR_NOT_YET_IMPLEMENTED = -9,
        ERR_PAYLOAD_TOO_LARGE   = -10,
        ERR_UNKNOWN             = -11,
    };

    enum CommType {
        CT_STD,
        CT_TCP,
        CT_FILE,
    };

    struct TCPInfo {
        const char* addr;
        int port;
    };

    struct FileInfo {
        FILE* handle;
        const char* path;
    };

    struct StdInfo {};

    struct Buffer {
        char*  data             = nullptr;
        size_t capacity         = 0;
        size_t baselineCapacity = DEFAULT_BASELINE_CAPACITY;
        size_t maxCapacity      = DEFAULT_MAX_CAPACITY;
    };

    struct Info {
        CommType type;
        Buffer   buffer;
        union {
            TCPInfo  tcp;
            FileInfo file;
            StdInfo  std;
        };
    };

    struct Message {
        JsonString body;
    };

    Err  init   (Info* info, CommType type);
    void release(Info* info);

    Err read (Info* info, Message* msg);
    Err write(Info* info, JsonString js);

    // Call after processing a message to shrink back down if it was oversized
    void releaseMessage(Info* info);

    const char* str(Err code);

}
