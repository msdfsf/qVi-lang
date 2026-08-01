#include "data_types.h"
#include "diagnostic.h"
#include "dynamic_arena.h"
#include "file_system.h"
#include "globals.h"
#include "interpreter.h"
#include "registry.h"
#include "syntax.h"
#include "task_status.h"
#include "utils.h"
#include "validator.h"
#include "foreign_code.h"

#include <cstdint>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif



// TODO : linux version
namespace Extern {

    // TODO : be able to use allocator here instead of arena
    //        for now not universal...
    String utf8ToWChar(Arena::Container* arena, String str) {
        int wcharCount = MultiByteToWideChar(
            CP_UTF8,
            0,
            str.buff,
            str.len,
            NULL,
            0
        );

        if (wcharCount <= 0) {
            return String { NULL, 0 };
        }

        wchar_t* result = (wchar_t*) Arena::push(
            arena,
            wcharCount * sizeof(wchar_t),
            sizeof(wchar_t)
        );

        int converted = MultiByteToWideChar(
            CP_UTF8,
            0,
            str.buff,
            str.len,
            result,
            wcharCount
        );

        return converted == 0 ?
            String { NULL, 0 } :
            String { (char*) result, wcharCount * sizeof(wchar_t) };
    }

    String wcharToUtf8(Arena::Container* arena, String str) {
        int utf8Size = WideCharToMultiByte(
            CP_UTF8,
            0,
            (wchar_t*) str.buff,
            str.len,
            NULL,
            0,
            NULL,
            NULL
        );

        if (utf8Size <= 0) {
            return String { NULL, 0 };
        }

        char* result = (char*) Arena::push(
            arena,
            utf8Size,
            1
        );

        int converted = WideCharToMultiByte(
            CP_UTF8,
            0,
            (wchar_t*) str.buff,
            str.len,
            result,
            utf8Size,
            NULL,
            NULL
        );

        return converted == 0 ?
            String { NULL, 0 } :
            String { result, (uint64_t) utf8Size };
    }

    char* toCString(Arena::Container* arena, String str) {
        char* cstr = (char*) Arena::push(arena, str.len + 1);

        memcpy(cstr, str.buff, str.len);
        cstr[str.len] = '\0';

        return cstr;
    }




    std::atomic<TaskStatus> gLibSetLock;
    Set::Container gLibSet;

    // TODO : to a context or something
    thread_local char osErrBuff[512];

    struct LibraryId {
        uint32_t volume;
        uint32_t indexH;
        uint32_t indexL;
    };

    struct LibrarySlot {
        LibraryId id;
        Library lib;
    };

    void lockLibSet() {
        while (true) {
            TaskStatus expected = TS_PENDING;

            if (gLibSetLock.compare_exchange_strong(expected, TS_RUNNING, std::memory_order_acquire)) {
                return;
            }

            if (expected == TS_RUNNING) {
                gLibSetLock.wait(TS_RUNNING, std::memory_order_relaxed);
            }
        }
    }

    void unlockLibSet() {
        gLibSetLock.store(TS_PENDING, std::memory_order_release);
        gLibSetLock.notify_all();
    }



    void init() {
        gLibSet.hashMethod = Set::HM_12_FNV1A;
        gLibSet.keyOffset = getMemberOffset(LibrarySlot, id);
        Set::init(&gLibSet, 512);
    }



    // Windows
    //

    void formatOsError(int code, char* buff, const int buffSize) {
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buff,
            buffSize,
            NULL
        );
    }

    Err::Err loadLibrary(Validator::ValidationContext* ctx, String name, LibraryLoadLevel level, LibraryHandle* out) {
        HMODULE hnd = NULL;

        String wname = utf8ToWChar(&ctx->stringArena, name);

        if (level == LL_INSPECT) {
            hnd = LoadLibraryExW((wchar_t*) wname.buff, NULL,
                DONT_RESOLVE_DLL_REFERENCES);
        } else if (level == LL_EXECUTE) {
            hnd = LoadLibraryW((wchar_t*) wname.buff);
        } else {
            // TODO :
            return Err::UNEXPECTED_ERROR;
        }

        Arena::rollback(&ctx->stringArena, wname.buff);

        if (!hnd) {
            formatOsError(GetLastError(), osErrBuff, sizeof(osErrBuff));

            Diag::report(ctx->unit->ast, NULL, Err::LIBRARY_LOAD_FAILED,
                Diag::Format {
                    "Failed to map native library into compiler memory.\n"
                    "  Target: %.*s\n"
                    "  System Error: %s"
                },
                name.len, name.buff, osErrBuff
            );

            return Err::LIBRARY_LOAD_FAILED;
        }

        wchar_t* fullPath = (wchar_t*) Arena::push(&ctx->stringArena, FileSystem::MAX_FILE_PATH);
        DWORD resultSize = GetModuleFileNameW(hnd, fullPath, FileSystem::MAX_FILE_PATH);
        DWORD errCode = GetLastError();
        if (resultSize == 0 || errCode == ERROR_INSUFFICIENT_BUFFER) {
            FreeLibrary(hnd);

            formatOsError(errCode, osErrBuff, sizeof(osErrBuff));

            Diag::report(ctx->unit->ast, NULL, Err::LIBRARY_LOAD_FAILED,
                Diag::Format{
                    "Failed to resolve full library path.\n"
                    "  Target: %.*s\n"
                    "  System Error: %s"
                },
                name.len, name.buff, osErrBuff
            );

            Arena::rollback(&ctx->stringArena, fullPath);

            return Err::UNEXPECTED_ERROR;
        }

        HANDLE hFile = CreateFileW(
            fullPath,
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            formatOsError(GetLastError(), osErrBuff, sizeof(osErrBuff));

            FreeLibrary(hnd);

            Diag::report(ctx->unit->ast, NULL, Err::UNEXPECTED_ERROR,
                Diag::Format{
                    "Internal Error: Unable to open native library for identity verification.\n"
                    "  Resolved Path: %ls\n"
                    "  System Error: %s"
                },
                fullPath, osErrBuff
            );

            Arena::rollback(&ctx->stringArena, fullPath);

            return Err::UNEXPECTED_ERROR;
        }

        LibraryId libId = { 0 };

        BY_HANDLE_FILE_INFORMATION info;
        if (!GetFileInformationByHandle(hFile, &info)) {
            formatOsError(GetLastError(), osErrBuff, sizeof(osErrBuff));

            CloseHandle(hFile);
            FreeLibrary((HMODULE) hnd);

            Diag::report(ctx->unit->ast, NULL, Err::UNEXPECTED_ERROR,
                Diag::Format{
                    "Internal Error: Failed to retrieve unique identity for native library.\n"
                    "  The compiler cannot verify if this file is already loaded.\n"
                    "  Path: %ls\n"
                    "  System Error: %s"
                },
                fullPath, osErrBuff
            );

            Arena::rollback(&ctx->stringArena, fullPath);

            return Err::UNEXPECTED_ERROR;
        }

        libId.volume = info.dwVolumeSerialNumber;
        libId.indexH = info.nFileIndexHigh;
        libId.indexL = info.nFileIndexLow;

        CloseHandle(hFile);

        // TODO : abstract to a function as we dont need to rly on os here I guess
        lockLibSet();

        LibrarySlot* libSlot = (LibrarySlot*) Set::find(&gLibSet, (uint64_t) &libId);

        if (!libSlot) {
            libSlot = (LibrarySlot*) alloc(alc, sizeof(LibrarySlot));
            libSlot->id = libId;
            libSlot->lib.dllPath = wcharToUtf8(alc, String { (char*) fullPath, resultSize });
            libSlot->lib.osHandle = (void*) hnd;
            libSlot->lib.loadLevel = level;

            Set::insert(&gLibSet, (uint8_t*) libSlot);
            *out = (LibraryHandle) &libSlot->lib;
        } else {
            if (libSlot->lib.loadLevel == LL_INSPECT && level == LL_EXECUTE) {
                // 'Upgrade' from INSPECT to EXECUTE
                FreeLibrary((HMODULE) libSlot->lib.osHandle);
                libSlot->lib.osHandle = LoadLibraryW(fullPath);
                libSlot->lib.loadLevel = LL_EXECUTE;

                if (!libSlot->lib.osHandle) {
                    unlockLibSet();
                    formatOsError(GetLastError(), osErrBuff, sizeof(osErrBuff));

                    Arena::rollback(&ctx->stringArena, wname.buff);

                    Diag::report(ctx->unit->ast, NULL, Err::LIBRARY_LOAD_FAILED,
                        Diag::Format{
                            "Failed to initialize native library for compile-time execution.\n"
                            "  The library was previously verified but could not be loaded as code.\n"
                            "  Path: %.*s\n"
                            "  System Error: %s\n"
                        },
                        libSlot->lib.dllPath.len, libSlot->lib.dllPath.buff, osErrBuff
                    );

                    return Err::LIBRARY_LOAD_FAILED;
                }
            }

            if (hnd != (HMODULE) libSlot->lib.osHandle) {
                FreeLibrary(hnd);
            }
            *out = (LibraryHandle) &libSlot->lib;
        }

        unlockLibSet();

        Arena::rollback(&ctx->stringArena, fullPath);

        return Err::OK;
    }

    Err::Err ensureFunctionExists(Validator::ValidationContext* ctx, LibraryHandle hLib, Function* fcn) {
        Library* lib = hLib;

        char* cstr = toCString(&ctx->stringArena, String { fcn->name.buff, fcn->name.len });
        FARPROC addr = GetProcAddress((HMODULE) lib->osHandle, cstr);
        if (!addr) {
            formatOsError(GetLastError(), osErrBuff, sizeof(osErrBuff));

            Diag::report(ctx->unit->ast, fcn->base.span, Err::SYMBOL_NOT_FOUND,
                Diag::Format{
                    "Failed to bind native symbol '%.*s'.\n"
                    "  The symbol '%s' was not found in the loaded library.\n"
                    "  Library: %.*s\n"
                    "  System Error: %s\n"
                },
                fcn->name.len, fcn->name.buff,
                cstr,
                lib->dllPath.len, lib->dllPath.buff,
                osErrBuff
            );

            return Err::SYMBOL_NOT_FOUND;
        }

        fcn->externAddress = (void*) addr;
        Arena::rollback(&ctx->stringArena, cstr);

        return Err::OK;
    }

    Err::Err bindFunction(Validator::ValidationContext* ctx, Library* lib, Function* fcn) {
        if (!lib->osHandle || lib->loadLevel != LL_EXECUTE) {
            LibraryHandle hDummy;
            Err::Err err = loadLibrary(ctx, lib->name, LL_EXECUTE, &hDummy);
            if (err != Err::OK) return err;
        }

        char stackBuffer[256];
        char* cstr = stackBuffer;
        if (fcn->name.len >= 256) {
            cstr = (char*) Arena::push(&ctx->stringArena, fcn->name.len + 1);
        }
        memcpy(cstr, fcn->name.buff, fcn->name.len);
        cstr[fcn->name.len] = '\0';

        void* addr = NULL;
        addr = (void*) GetProcAddress((HMODULE) lib->osHandle, cstr);

        if (!addr) {
            formatOsError(GetLastError(), osErrBuff, sizeof(osErrBuff));

            Diag::report(ctx->unit->ast, fcn->base.span, Err::SYMBOL_NOT_FOUND,
                Diag::Format{
                    "Failed to bind native function '%.*s'.\n"
                    "  The symbol '%s' was not found in the library.\n"
                    "  Library Path: %.*s\n"
                    "  System Error: %s\n"
                },
                fcn->name.len, fcn->name.buff, cstr,
                lib->dllPath.len, lib->dllPath.buff, osErrBuff
            );

            if (cstr != stackBuffer) free(cstr);
            return Err::SYMBOL_NOT_FOUND;
        }

        fcn->externAddress = addr;
        fcn->base.cmpStatus = TS_READY;

        if (cstr != stackBuffer) free(cstr);
        return Err::OK;
    }

    Err::Err VariableToStack(AstContext* ast, uint8_t* buff, int64_t buffSize, Variable* var) {
        Value* val = &var->value;

        switch (val->typeKind) {
            case Type::DT_I8:
            case Type::DT_U8:
            case Type::DT_I16:
            case Type::DT_U16:
            case Type::DT_I32:
            case Type::DT_U32:
            case Type::DT_I64:
            case Type::DT_U64:
            case Type::DT_F32:
            case Type::DT_F64: {
                const int size = (Type::basicTypes + val->typeKind)->size;
                if (buffSize < size) {
                    Diag::report(ast, var->base.span,
                        Err::UNEXPECTED_ERROR, Diag::Format {
                            "Unexpected Comptime Memory Error: Attempted to write %d bytes to VM stack, "
                            "but only %lld bytes are available in the current frame."
                        },
                        size, buffSize);
                    return Err::UNEXPECTED_ERROR;
                }

                memset(buff, 0, size);
                memcpy(buff, &val->u64, size);
                break;
            }

            // TODO : sanity check for memberCount == varCount?
            case Type::DT_CUSTOM: {
                TypeDefinition* def = val->def;
                Type::StructInfo* sInfo = (Type::StructInfo*) def->typeInfo;

                var = unwrap(var);
                if (var->def) var = unwrap(var->def->var);

                // TODO : for now assuming that it can be only init
                if (!var->expression || var->expression->type != EXT_TYPE_INITIALIZATION) {
                    Diag::report(ast, var->base.span, Err::UNEXPECTED_SYMBOL,
                        "Expected struct initialization expression.");
                    return Err::UNEXPECTED_ERROR;
                }

                TypeInitialization* init = (TypeInitialization*) var->expression;
                for (int i = 0; i < sInfo->memberCount; i++) {
                    Type::StructMemberInfo* mInfo = sInfo->members + i;

                    Variable* mVar = NULL;
                    if (i < init->attributeCount) {
                        mVar = init->attributes[i];
                    } else if (init->fillVar) {
                        mVar = init->fillVar;
                    } else {
                        mVar = def->vars[i];
                    }

                    uint8_t* mBuff = buff + mInfo->offset;
                    int64_t  mSize = buffSize - mInfo->offset;

                    Err::Err err = VariableToStack(ast, mBuff, mSize, mVar);
                    if (err != Err::OK) return err;
                }

                break;
            }

            case Type::DT_SLICE:
            case Type::DT_ERROR:
            case Type::DT_FUNCTION:
            case Type::DT_COUNT:
            case Type::DT_MULTIPLE_TYPES:
            case Type::DT_ARRAY: {
                Diag::report(ast, var->base.span,
                    Err::NOT_YET_IMPLEMENTED, Diag::Format {
                        "Compile-time conversion for %s not yet implemented"
                    }, Type::str(val->typeKind));
                return Err::NOT_YET_IMPLEMENTED;
            }

            default: {
                Diag::report(ast, var->base.span,
                    Err::UNEXPECTED_ERROR, Diag::Format {
                        "Invalid type kind (%i) in ValueToStack"
                    }, val->typeKind);
                return Err::UNEXPECTED_ERROR;
            }
        }

        return Err::OK;
    }

    // TODO : better var names
    // TODO : [fix] size/alignment is not recorded
    Err::Err compile(Validator::ValidationContext* ctx, Abi::Driver* abi, Function* fcn) {
        if (fcn->abiCtx) return Err::OK;

        const uint32_t argCount = fcn->prototype.inArgCount;
        VariableDefinition** args = fcn->prototype.inArgs;

        uint32_t  stackOffset = 0;
        uint32_t  stackSize;

        uint32_t iRegUsed = 0;
        uint32_t fRegUsed = 0;

        Abi::Arg* abiArgs = (Abi::Arg*) alloc(alc, sizeof(Abi::Arg) * argCount);
        for (uint32_t i = 0; i < argCount; i++) {
            Value* src = &args[i]->var->value;
            Abi::Arg* dest = abiArgs + i;

            abi->classify(dest, src);
            if (Abi::isRegFloat(dest->pass)) {
                if (fRegUsed < abi->fRegCount) {
                    dest->offset = fRegUsed++;
                    if (abi->isUniform) iRegUsed++;
                    continue;
                }
            } else if (Abi::isRegInt(dest->pass)) {
                if (iRegUsed < abi->iRegCount) {
                    dest->offset = iRegUsed++;
                    if (abi->isUniform) fRegUsed++;
                    continue;
                }
            }

            dest->offset = stackOffset;

            uintptr_t ptr = Utils::alignForward(stackOffset, abi->stackAlign);

            if (Type::isStructLike(src->typeKind)) {
                Abi::ensureTypeInfoReady(ctx, src->def, abi);
                dest->size = src->def->typeInfoAbi->info->base.size;
            } else {
                dest->size = Type::basicTypes[src->typeKind].size;
            }
        }

        // prepare and cache ctx
        Abi::CallContext* abiCtx = (Abi::CallContext*) alloc(alc, sizeof(Abi::CallContext));
        abiCtx->args = abiArgs;
        abiCtx->argCount = argCount;
        abiCtx->target = fcn->externAddress;
        abiCtx->argStackSize = stackOffset;
        abiCtx->abi = abi;

        fcn->abiCtx = abiCtx;

        // and also precompute return info
        Value* retVal = &fcn->prototype.outArg->var->value;
        abi->classify(&abiCtx->retArg, retVal);

        return Err::OK;
    }

    Err::Err invoke(AstContext* ast, Abi::Driver* abi, Function* fcn, Variable** args, uint32_t argCount, Variable* out) {
        Abi::fillArgs(ast, fcn->abiCtx, args, argCount);
        abi->invoke(fcn->abiCtx);

        Err::Err err = Abi::stackToVar(ast, out, (uint8_t*) fcn->abiCtx->retArg.data.ptr);
        if (err != Err::OK) return err;

        return Err::OK;
    }

}

namespace Extern::Abi {

    Driver* getTargetDriver() {
        #ifdef _WIN32
            return &Abi::win64;
        #else
            return &Abi::sysV;
        #endif
    }

    bool isRegFloat(ArgPassKind pass) {
        return pass == PK_REG_FLOAT;
    }

    bool isRegInt(ArgPassKind pass) {
        return pass == PK_REG_INT;
    }

    Err::Err stackToVar(AstContext* ast, Variable* var, uint8_t* stack) {
        Value* val = &var->value;

        switch (val->typeKind) {
            case Type::DT_I8:
            case Type::DT_U8:
            case Type::DT_I16:
            case Type::DT_U16:
            case Type::DT_I32:
            case Type::DT_U32:
            case Type::DT_I64:
            case Type::DT_U64:
            case Type::DT_F32:
            case Type::DT_F64: {
                val->u64 = (uint64_t) stack;
                break;
            }

            case Type::DT_CUSTOM: {
                Type::StructInfo* sInfo = (Type::StructInfo*)val->def->typeInfo;

                Variable* currentVar = unwrap(var);
                if (currentVar->def) currentVar = unwrap(currentVar->def->var);

                if (!currentVar->expression || currentVar->expression->type != EXT_TYPE_INITIALIZATION) {
                    Diag::report(ast, var->base.span, Err::UNEXPECTED_SYMBOL,
                        "Expected struct initialization expression to populate from stack.");
                    return Err::UNEXPECTED_ERROR;
                }

                TypeInitialization* init = (TypeInitialization*)currentVar->expression;

                for (int i = 0; i < sInfo->memberCount; i++) {
                    Type::StructMemberInfo* mInfo = sInfo->members + i;

                    Variable* mVar = NULL;
                    if (i < init->attributeCount) {
                        mVar = init->attributes[i];
                    }
                    else if (init->fillVar) {
                        mVar = init->fillVar;
                    }
                    else {
                        mVar = val->def->vars[i];
                    }

                    uint8_t* mBuff = stack + mInfo->offset;

                    Err::Err err = stackToVar(ast, mVar, mBuff);
                    if (err != Err::OK) return err;
                }
                break;
            }

            case Type::DT_SLICE:
            case Type::DT_ARRAY:
            case Type::DT_FUNCTION: {
                Diag::report(ast, var->base.span,
                    Err::NOT_YET_IMPLEMENTED, Diag::Format{
                        "Stack-to-Variable conversion for %s not yet implemented"
                    }, Type::str(val->typeKind));
                return Err::NOT_YET_IMPLEMENTED;
            }

            default: {
                Diag::report(ast, var->base.span,
                    Err::UNEXPECTED_ERROR, Diag::Format{
                        "Invalid type kind (%i) in stackToVar"
                    }, val->typeKind);
                return Err::UNEXPECTED_ERROR;
            }
        }

        return Err::OK;
    }

    // stack size has to be able to fit the value
    Err::Err varToStack(AstContext* ast, Variable* var, uint8_t* stack) {
        Value* val = &var->value;

        switch (val->typeKind) {
            case Type::DT_I8:
            case Type::DT_U8:
            case Type::DT_I16:
            case Type::DT_U16:
            case Type::DT_I32:
            case Type::DT_U32:
            case Type::DT_I64:
            case Type::DT_U64:
            case Type::DT_F32:
            case Type::DT_F64: {
                const int size = (Type::basicTypes + val->typeKind)->size;
                memcpy(stack, &val->u64, size);
                break;
            }

            case Type::DT_CUSTOM: {
                Abi::TypeInfo* info = val->def->typeInfoAbi;
                Type::StructInfo* sInfo = (Type::StructInfo*) val->def->typeInfo;

                var = unwrap(var);
                if (var->def) var = unwrap(var->def->var);

                // TODO : for now assuming that it can be only init
                if (!var->expression || var->expression->type != EXT_TYPE_INITIALIZATION) {
                    Diag::report(ast, var->base.span, Err::UNEXPECTED_SYMBOL,
                        "Expected struct initialization expression.");
                    return Err::UNEXPECTED_ERROR;
                }

                TypeInitialization* init = (TypeInitialization*) var->expression;
                for (int i = 0; i < sInfo->memberCount; i++) {
                    Type::StructMemberInfo* mInfo = sInfo->members + i;

                    Variable* mVar = NULL;
                    if (i < init->attributeCount) {
                        mVar = init->attributes[i];
                    } else if (init->fillVar) {
                        mVar = init->fillVar;
                    } else {
                        mVar = val->def->vars[i];
                    }

                    uint8_t* mBuff = stack + mInfo->offset;

                    Err::Err err = varToStack(ast, mVar, mBuff);
                    if (err != Err::OK) return err;
                }

                break;
            }

            case Type::DT_SLICE:
            case Type::DT_ERROR:
            case Type::DT_FUNCTION:
            case Type::DT_COUNT:
            case Type::DT_MULTIPLE_TYPES:
            case Type::DT_ARRAY: {
                Diag::report(ast, var->base.span,
                    Err::NOT_YET_IMPLEMENTED, Diag::Format {
                        "Compile-time conversion for %s not yet implemented"
                    }, Type::str(val->typeKind));
                return Err::NOT_YET_IMPLEMENTED;
            }

            default: {
                Diag::report(ast, var->base.span,
                    Err::UNEXPECTED_ERROR, Diag::Format {
                        "Invalid type kind (%i) in ValueToStack"
                    }, val->typeKind);
                return Err::UNEXPECTED_ERROR;
            }

        }

        return Err::OK;
    }

    void fillArgs(AstContext* ast, Abi::CallContext* ctx, Variable** args, uint32_t argCount) {
        uint32_t  stackSize;
        uintptr_t stack = Interpreter::getExeFramePointer(ctx->abi->stackAlign, &stackSize);
        if (ctx->argStackSize > stackSize) {
            Diag::report(ast, NULL, Err::UNEXPECTED_ERROR,
                "Native call failed: stack argument space exceeded."
            );
            return;
        }

        for (uint32_t i = 0; i < argCount; i++) {
            Variable* src = args[i];
            Abi::Arg* dest = &ctx->args[i];

            switch (dest->pass) {
                case Abi::PK_REG_INT:
                case Abi::PK_REG_FLOAT: {
                    dest->data.i64 = src->value.i64;
                    break;
                }

                case Abi::PK_MEM_STRUCT_REFERENCE:
                case Abi::PK_REG_STRUCT_REFERENCE: {
                    stack = Utils::alignForward(stack, ctx->abi->indirectAlignment);
                    Abi::varToStack(ast, src, (uint8_t*) stack);
                    dest->data.i64 = stack;

                    stack += ctx->abi->layout.wordSize;
                    break;
                }

                case Abi::PK_REG_STRUCT: {
                    stack = Utils::alignForward(stack, ctx->abi->layout.wordSize);

                    const size_t offset = ctx->abi->layout.wordSize - dest->size;
                    Abi::varToStack(ast, src, ((uint8_t*) stack) + offset);

                    memset((void*) stack, 0, offset);
                    dest->data.i64 = *((int64_t*) stack);

                    stack += ctx->abi->layout.wordSize;
                    break;
                }

                case Abi::PK_MEM_STRUCT: {
                    Abi::TypeInfo* info = src->value.def->typeInfoAbi;

                    stack = Utils::alignForward(stack, info->info->base.align);
                    Abi::varToStack(ast, src, (uint8_t*) stack);

                    dest->data.i64 = *((int64_t*) stack);

                    stack += info->info->base.size;
                    break;
                }

                default: {
                    // Error: not yet implemented
                }
            }
        }
    }

    // TODO : for now, to make this function work reqursivly, we return pointer
    // which it may be allocate. Therefore user cannot cleanly use this ex. to
    // set structs that utilize Type::TypeInfo not as pointer... This is because
    // primitive types are predefined and used uniformly as pointers...
    // So, just think if it needs to be 'solved', or its just fine of limitation...
    Type::TypeInfo* computeTypeInfo(Abi::LayoutConfig* cfg, Type::TypeInfo*  tempInfo) {
        if (Type::isPrimitive(tempInfo->kind)) {
            return tempInfo;
        }

        Type::TypeInfoEx*  ansInfo = (Type::TypeInfoEx*) alloc(alc, sizeof(Type::TypeInfoEx));

        ansInfo->base.kind = tempInfo->kind;
        ansInfo->base.rank = tempInfo->rank;

        switch (tempInfo->kind) {
            case Type::DT_CUSTOM: {
                Type::StructInfo* tempStruct = (Type::StructInfo*) tempInfo;

                uint64_t memberCount = tempStruct->memberCount;
                ansInfo->str.name = tempStruct->name;
                ansInfo->str.memberCount = memberCount;
                ansInfo->str.members = (Type::StructMemberInfo*) alloc(alc,
                    sizeof(Type::StructMemberInfo) * memberCount);

                uint64_t currentOffset = 0;
                uint32_t maxAlignFound = cfg->minAlign;

                for (uint64_t i = 0; i < memberCount; i++) {
                    Type::StructMemberInfo* member = tempStruct->members + i;
                    ansInfo->str.members[i].name = member->name;
                    ansInfo->str.members[i].type =
                        (Type::TypeInfo*) computeTypeInfo(cfg, member->type);


                    uint32_t mAlign = member->type->align;
                    uint32_t mSize  = member->type->size;

                    if (mAlign > maxAlignFound) maxAlignFound = mAlign;

                    currentOffset = Utils::alignForward(currentOffset, mAlign);
                    ansInfo->str.members[i].offset = currentOffset;

                    currentOffset += mSize;
                }

                ansInfo->base.align = maxAlignFound;
                ansInfo->base.size  = Utils::alignForward(currentOffset, maxAlignFound);

                break;
            }

            case Type::DT_ARRAY: {
                Type::ArrayInfo* tempArray = (Type::ArrayInfo*) tempInfo;

                Type::TypeInfo* element = computeTypeInfo(cfg, tempArray->element);
                ansInfo->arr.element = element;
                ansInfo->arr.elementCount = tempArray->elementCount;

                ansInfo->base.align = element->align;
                ansInfo->base.size = element->size * tempArray->elementCount;

                break;
            }

            default: {
                // Error : not yet implemented
                break;
            }
        }

        return (Type::TypeInfo*) ansInfo;
    }

    // TODO : we reuse for now cmpStatus, but later think of either
    // implementing custom task states or add linear task states levels
    // ex TS_READY_2, TS_RUNNING_2 etc.
    Err::Err ensureTypeInfoReady(Validator::ValidationContext* ctx, TypeDefinition* td, Driver* driver) {
        Err::Err err = Err::OK;

        if (td->base.cmpStatus == TS_READY) return Err::OK;

        AcquireNodeReturn ans =
            acquireNode(&td->base.cmpStatus, &td->base.workerId, ctx->workerId, true);

        if (ans == ANR_ACQUIRED_FOR_WORK) {
            td->typeInfoAbi->info = (Type::TypeInfoEx*) computeTypeInfo(&driver->layout, &td->typeInfo->base);
            releaseNode(&td->base.cmpStatus, true);
        } else if (ans == ANR_ALREADY_ACQUIRED_BY_CALLER) {
            // TODO : Proper Errors
            Diag::report(ctx->unit->ast, td->base.span, Err::UNEXPECTED_ERROR, Diag::Format {
                "TODO : Node being validated is already on stack! Causing circular dependency!"
            });
            return Err::UNEXPECTED_ERROR;
        }

        return err;
    }

}
