// Things to fix:
//  for now json parser inserts '\0' at the end of a string
//  so any given json text is invalidated. We need this, I
//  suppose only for fopen, so the plan is to migrate to
//  normal file open function which supports lenths.


#include "json.h"
#include "lsp.h"
#include "comm_provider.h"

#include <cstdio>
#include <debugapi.h>
#include <synchapi.h>



constexpr JsonString operator ""_js(const char* s, size_t l) {
    return {(char*) s, l};
}

static bool match(JsonString str, const char* lit) {
    return (str.len == (int) strlen(lit) && strncmp(str.data, lit, str.len) == 0);
}

template <typename T>
static JsonString generateResponse(CommProvider::Info* comm, T* result, int id) {
    JsonWriter js;
    // TODO: for now static size, make json return bool if write was successful or not?
    jsonWriterInit(&js, comm->buffer.data, comm->buffer.capacity);

    jsonWriteObjectStart(&js);
    jsonWriteKey(&js, "jsonrpc"_js);
    jsonWriteStr(&js, "2.0"_js);

    jsonWriteKey(&js, "id"_js);
    jsonWriteInt(&js, id);

    jsonWriteKey(&js, "result"_js);
    //jsonWriteObjectStart(js);

    Lsp::serialize(&js, result);

    //jsonWriteObjectEnd(js);
    jsonWriteObjectEnd(&js);

    return jsonWriterCommit(&js);
}

Arena::Container lspAllocatorArena;

#include <fcntl.h>
#include <io.h>
int main() {
    #ifdef _DEBUG
    while (!IsDebuggerPresent()) Sleep(3);
    #else
    Sleep(10000);
    #endif

    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        perror("Cannot set stdin to binary mode");
        return 1;
    }

    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        perror("Cannot set stdout to binary mode");
        return 1;
    }

    CommProvider::Info comm;
    init(&comm, CommProvider::CT_STD);

    Arena::init(&lspAllocatorArena, 1024 * 1024 * 32);
    Lsp::Allocator lspAllocator = {
        .alloc   =(void* (*)(void*, size_t)) static_cast<void* (*) (Arena::Container*, size_t)>(&Arena::push),
        .context = &lspAllocatorArena
    };

    // Prepare global state
    Lsp::State::allocator = lspAllocator;
    Lsp::State::permission = Lsp::P_PARSE;
    Lsp::State::comm = &comm;
    #ifdef _DEBUG
    Lsp::State::compilerThreadCount = 0;
    #else
    Lsp::State::compilerThreadCount = 4;
    #endif
    
    Lsp::init();
    Lsp::report({ Lsp::Inf::INFO }, "Initialization finished.");

    bool beOrNotToBe = true;
    while (beOrNotToBe) {
        JsonLex js = { 0 };
        JsonType token;

        Arena::clear(&lspAllocatorArena);
        CommProvider::releaseMessage(&comm);

        CommProvider::Message msg = { 0 };
        CommProvider::Err err = CommProvider::read(&comm, &msg);

        if (err == CommProvider::ERR_CLOSED) break;
        if (err != CommProvider::OK) continue;

        jsonLexInit(&js, msg.body, "lsp_input"_js);
        fprintf(stderr, "\n%.*s\n", msg.body.len, msg.body.data);

        // Extract:
        // "id": <number>
        // "method": <string>
        // "params": <array> -> as JsonString which
        token = jsonNext(&js);
        if (!jsonMatch(&js, token, JSON_OBJECT_OPEN)) {
            continue;
        }

        int id = -1;
        JsonString methodUri = { 0 };

        while (true) {
            token = jsonNext(&js);
            if (token == JSON_OBJECT_CLOSE) break;
            if (!jsonMatch(&js, token, JSON_KEY)) break;

            JsonString key = js.value.s;

            token = jsonNext(&js);
            if (match(key, "id")) {
                if (jsonMatch(&js, token, JSON_NUMBER)) {
                    id = (int) js.value.n;
                }
                continue;
            } else if (match(key, "method")) {
                if (jsonMatch(&js, token, JSON_STRING)) {
                    methodUri = js.value.s;
                }
                continue;
            } else if (match(key, "params")) {
                // TODO : for now we assume that they go in order
                // as this should be done by design
                break;
            }
            jsonSkipValue(&js, token);
        }


        if (js.errCode != 0) {
            // TODO
        }

        // Convert method uri to enum representation
        Lsp::T::RequestMethod method = Lsp::toRequestMethod(methodUri);

        // Dispatch requested method
        JsonString resp = { 0, 0 };
        switch (method) {

            case Lsp::T::RM_INITIALIZE: {
                Lsp::Initialize* params = Lsp::parse<Lsp::Initialize>(&js, &lspAllocator);
                Lsp::T::InitializeResult result = { 0 };

                // Sync Options
                auto* sync = Lsp::alloc<Lsp::T::TextDocumentSyncOptions>(&lspAllocator);
                sync->openClose = true;
                sync->change    = Lsp::T::TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL;

                // Capabilities
                auto* caps = Lsp::alloc<Lsp::T::ServerCapabilities>(&lspAllocator);

                // Features
                caps->textDocumentSync       = sync;
                caps->hoverProvider          = true;



                // SemanticTokens
                // ---

                caps->semanticTokensProvider = Lsp::alloc<Lsp::T::SemanticTokensOptions>(&lspAllocator);
                caps->semanticTokensProvider->full.kind = Lsp::T::SemanticTokensOptions::Full::_EStruct;
                caps->semanticTokensProvider->full._struct = {
                    .delta = true
                };

                caps->semanticTokensProvider->legend = Lsp::alloc<Lsp::T::SemanticTokensLegend>(&lspAllocator);

                constexpr size_t TOKEN_TYPE_COUNT = 10;

                Lsp::Slice<String>* tokenTypes = &caps->semanticTokensProvider->legend->tokenTypes;
                tokenTypes->size = TOKEN_TYPE_COUNT;
                tokenTypes->data = Lsp::alloc<String>(&lspAllocator, TOKEN_TYPE_COUNT);

                tokenTypes->data[0] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_KEYWORD);
                tokenTypes->data[1] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_TYPE);
                tokenTypes->data[2] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_FUNCTION);
                tokenTypes->data[3] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_VARIABLE);
                tokenTypes->data[4] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_ENUM);
                tokenTypes->data[5] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_NAMESPACE);
                tokenTypes->data[6] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_MODIFIER);
                tokenTypes->data[7] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_OPERATOR);
                tokenTypes->data[8] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_STRING);
                tokenTypes->data[9] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_TYPES_NUMBER);

                constexpr size_t TOKEN_MODIFIER_COUNT = 5;

                Lsp::Slice<String>* tokenModifiers = &caps->semanticTokensProvider->legend->tokenModifiers;
                tokenModifiers->size = TOKEN_MODIFIER_COUNT;
                tokenModifiers->data = Lsp::alloc<String>(&lspAllocator, TOKEN_MODIFIER_COUNT);

                tokenModifiers->data[0] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_MODIFIERS_DECLARATION);
                tokenModifiers->data[1] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_MODIFIERS_READONLY);
                tokenModifiers->data[2] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_MODIFIERS_STATIC);
                tokenModifiers->data[3] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_MODIFIERS_DEPRECATED);
                tokenModifiers->data[4] = Lsp::T::toString(Lsp::T::SEMANTIC_TOKEN_MODIFIERS_MODIFICATION);



                //caps->definitionProvider     = true;
                //caps->documentSymbolProvider = true;
                //caps->declarationProvider    = true;
                //caps->implementationProvider = true;

                // Server Info
                result.serverInfo = Lsp::alloc<Lsp::T::InitializeResult::_ServerInfo>(&lspAllocator);
                result.serverInfo->name    = "qVi-LanguageServer";
                result.serverInfo->version = "0.1.0";

                result.capabilities = caps;

                resp = generateResponse(&comm, &result, id);
                break;
            }

            case Lsp::T::RM_TEXT_DOCUMENT_DID_OPEN: {
                using namespace Lsp::TextDocument;
                Lsp::handle(Lsp::parse<DidOpen>(&js, &lspAllocator));

                break;
            }

            case Lsp::T::RM_TEXT_DOCUMENT_DID_CHANGE: {
                using namespace Lsp::TextDocument;
                Lsp::handle(Lsp::parse<DidChange>(&js, &lspAllocator));

                break;
            }

            case Lsp::T::RM_TEXT_DOCUMENT_DID_CLOSE: {
                using namespace Lsp::TextDocument;
                Lsp::handle(Lsp::parse<DidClose>(&js, &lspAllocator));

                break;
            }

            case Lsp::T::RM_TEXT_DOCUMENT_DOCUMENT_SYMBOL: {

                break;

            }

            case Lsp::T::RM_INITIALIZED: {
                Lsp::State::initialized = true;
                break;
            }

            case Lsp::T::RM_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL: {
                using namespace Lsp::TextDocument;

                Lsp::T::SemanticTokens* result =
                    Lsp::handle(Lsp::parse<SemanticTokensfull>(&js, &lspAllocator));

                resp = generateResponse(&comm, result, id);
                break;
            }

            case Lsp::T::RM_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL_DELTA: {
                using namespace Lsp::TextDocument;

                Lsp::T::SemanticTokensDelta* result =
                    Lsp::handle(Lsp::parse<SemanticTokensfulldelta>(&js, &lspAllocator));

                resp = generateResponse(&comm, result, id);
                break;
            }

            case Lsp::T::RM_TEXT_DOCUMENT_HOVER: {
                using namespace Lsp::TextDocument;

                Lsp::T::Hover* result =
                    Lsp::handle(Lsp::parse<Hover>(&js, &lspAllocator));

                resp = generateResponse(&comm, result, id);
                break;
            }

            case Lsp::T::RM_TEXT_DOCUMENT_DEFINITION: {
                break;
            }

            case Lsp::T::RM_SHUTDOWN: {
                beOrNotToBe = 0;

                JsonWriter jsWr;
                jsonWriterInit(&jsWr, comm.buffer.data, comm.buffer.capacity);

                jsonWriteObjectStart(&jsWr);
                    jsonWriteKey(&jsWr, "jsonrpc"_js); jsonWriteStr(&jsWr, "2.0"_js);
                    jsonWriteKey(&jsWr, "id"_js);      jsonWriteInt(&jsWr, id);
                    jsonWriteKey(&jsWr, "result"_js);  jsonWriteNull(&jsWr);
                jsonWriteObjectEnd(&jsWr);

                JsonString resp = jsonWriterCommit(&jsWr);
                break;
            }

            case Lsp::T::RM_EXIT: {
                return beOrNotToBe ? 0 : 1;
            }

            default:
                break;
        }
        if (resp.len > 0) CommProvider::write(&comm, resp);

    }

    CommProvider::release(&comm);
    Lsp::release();

    return 0;
}
