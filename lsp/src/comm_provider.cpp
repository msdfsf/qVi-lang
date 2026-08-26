#include "comm_provider.h"
#include <stdlib.h>
#include <string.h>
#include <string>



namespace CommProvider {

    Err init(Info* info, CommType type) {
        if (!info) return ERR_UNKNOWN;

        info->type = type;
        info->buffer.baselineCapacity = DEFAULT_BASELINE_CAPACITY;
        info->buffer.maxCapacity      = DEFAULT_MAX_CAPACITY;
        info->buffer.capacity         = DEFAULT_BASELINE_CAPACITY;

        info->buffer.data = (char*) malloc(info->buffer.capacity);
        if (!info->buffer.data) {
            return ERR_MALLOC;
        }

        if (type == CT_FILE) {
            info->file.handle = NULL;
            info->file.path   = NULL;
        }

        return OK;
    }

    void release(Info* info) {
        if (!info) return;

        if (info->buffer.data) {
            free(info->buffer.data);
            info->buffer.data = NULL;
            info->buffer.capacity = 0;
        }

        if (info->type == CT_FILE && info->file.handle) {
            fclose(info->file.handle);
            info->file.handle = NULL;
        }
    }

    static FILE* getStream(Info* info, const char* fileMode, Err* outErr) {
        switch (info->type) {
            case CT_STD:
                return (fileMode[0] == 'r') ? stdin : stdout;

            case CT_FILE:
                if (!info->file.handle) {
                    info->file.handle = fopen(info->file.path, fileMode);
                    if (!info->file.handle) {
                        *outErr = ERR_FILE_NOT_FOUND;
                        return nullptr;
                    }
                }
                return info->file.handle;

            case CT_TCP:
                *outErr = ERR_NOT_YET_IMPLEMENTED;
                return nullptr;

            default:
                *outErr = ERR_UNKNOWN;
                return nullptr;
        }
    }

    // Ensures buffer can hold at least 'size' bytes
    static bool ensureSize(Buffer* buf, size_t size) {
        if (size <= buf->capacity) return true;
        if (size > buf->maxCapacity) return false;

        char* next = (char*) realloc(buf->data, size + 1);
        if (!next) return false;

        buf->data     = next;
        buf->capacity = size + 1;
        return true;
    }

    Err read(Info* info, Message* msg) {
        if (!info || !msg) return ERR_UNKNOWN;

        Err err = OK;
        FILE* stream = getStream(info, "rb", &err);
        if (!stream) return err;

        // Parse header till '\r\n\r\n', using std::string as tmp local dynamic buffer..
        std::string header;
        size_t      headerLen = 0;
        uint32_t    marker    = 0;

        char ch;
        while ((ch = fgetc(stream)) != EOF) {
            header += ch;
            marker = (marker << 8) | (uint8_t) ch;
            if (marker == 0x0D0A0D0A) break; // detected \r\n\r\n
        }

        if (ch == EOF && headerLen == 0) return ERR_CLOSED;
        if (marker != 0x0D0A0D0A) return ERR_PARSE;

        // For now only extract Content-Length
        constexpr char labelContentLength[] = "Content-Length:";
        // TODO: we may want to use a custom function for case insensitive substr search...
        const char* lenPtr = strstr(header.c_str(), labelContentLength);
        if (!lenPtr) if (!lenPtr) return ERR_PARSE;

        lenPtr += sizeof(labelContentLength) - 1;
        while (*lenPtr == ' ') lenPtr++;

        size_t contentLength = (size_t) strtoull(lenPtr, NULL, 10);
        if (contentLength == 0) return ERR_PARSE;

        // Need to drain stream, in case of oversized payloads
        if (contentLength > info->buffer.maxCapacity) {
            for (size_t i = 0; i < contentLength; i++) fgetc(stream);
            return ERR_PAYLOAD_TOO_LARGE;
        }

        if (!ensureSize(&info->buffer, contentLength)) {
            return ERR_MALLOC;
        }

        size_t bytesRead = fread(info->buffer.data, 1, contentLength, stream);
        if (bytesRead != contentLength) {
            return ERR_IO;
        }

        info->buffer.data[contentLength] = '\0';

        msg->body.data = info->buffer.data;
        msg->body.len  = (uint32_t) contentLength;

        return OK;
    }

    void releaseMessage(Info* info) {
        if (!info) return;

        if (info->buffer.capacity > info->buffer.baselineCapacity) {
            char* baselineData = (char*) realloc(info->buffer.data, info->buffer.baselineCapacity);
            if (baselineData) {
                info->buffer.data     = baselineData;
                info->buffer.capacity = info->buffer.baselineCapacity;
            } else {
                // TODO: ?
            }
        }
    }

    Err write(Info* info, JsonString body) {
        if (!info) return ERR_UNKNOWN;

        Err err = OK;
        FILE* stream = getStream(info, "wb", &err);
        if (!stream) return err;

        if (fprintf(stream, "Content-Length: %u\r\n\r\n", body.len) < 0) {
            return ERR_IO;
        }

        size_t wrote = fwrite(body.data, 1, body.len, stream);
        if (wrote != body.len) {
            return ERR_IO;
        }

        if (fflush(stream) != 0) {
            return ERR_IO;
        }

        return OK;
    }

    const char* str(Err code) {
        switch (code) {
            case OK:                      return "OK";
            case ERR_CLOSED:              return "ERR_CLOSED";
            case ERR_TIMEOUT:             return "ERR_TIMEOUT";
            case ERR_IO:                  return "ERR_IO";
            case ERR_PARSE:               return "ERR_PARSE";
            case ERR_MALLOC:              return "ERR_MALLOC";
            case ERR_NOT_READY:           return "ERR_NOT_READY";
            case ERR_FILE_NOT_FOUND:      return "ERR_FILE_NOT_FOUND";
            case ERR_PERMISSION_DENIED:   return "ERR_PERMISSION_DENIED";
            case ERR_NOT_YET_IMPLEMENTED: return "ERR_NOT_YET_IMPLEMENTED";
            case ERR_PAYLOAD_TOO_LARGE:   return "ERR_PAYLOAD_TOO_LARGE";
            case ERR_UNKNOWN:             return "ERR_UNKNOWN";
            default:                      return "INVALID_ERR_CODE";
        }
    }

}
