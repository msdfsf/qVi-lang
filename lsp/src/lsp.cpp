#include "lsp.h"
#include "../../src/task_system.h"
#include "compiler_worker.h"
#include "lsp_render.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>



constexpr double             GROW_COEF      = 1.5;
constexpr FileSystem::Origin ORIGIN         = FileSystem::Origins::USER_START;
constexpr uint32_t           INVALID_OFFSET = UINT32_MAX;

void print(const Lsp::FileData* data);

enum WalkControl {
    WK_CONTINUE,      // Process node and proceed into children
    WK_SKIP_CHILDREN, // Skip recursing into this node's children
    WK_ONLY_CHILDREN, // Process node's children and abort
    WK_ABORT
};

template <typename F>
bool forEachNode(SyntaxNode* node, F&& callback);

template <typename F>
bool forEachExpression(Expression* expr, F&& callback);

template <typename F>
bool astWalk(SyntaxNode* root, F&& visitor);



// Helpers
// ===

// allocates sufficient initial buffer for given string
// and copies data
Lsp::FileString makeFileString(String str) {
    Lsp::FileString out;
    out.capacity = str.len * GROW_COEF;
    out.buff = (char*) malloc(out.capacity);
    memcpy(out.buff, str.buff, str.len);
    out.len = str.len;
    return out;
}

void releaseFileString(Lsp::FileString str) {
    free(str.buff);
    str.len = 0;
}

void resizeFileString(Lsp::FileString* str, size_t newSize) {
    if (newSize <= str->capacity) {
        str->len = newSize;
        return;
    }

    str->buff = (char*) realloc(str->buff, newSize);
    if (!str->buff) {
        Lsp::panic({ Lsp::Err::ALLOC, { 0 } });
    }
    str->len = newSize;
}



uint32_t countLines(String str) {
    uint32_t cnt = 0;
    for (size_t i = 0; i < str.len; i++) {
        if (str.buff[i] == '\n') cnt++;
    }
    return cnt;
}



// Full file recompute
void updateLineOffsets(Lsp::FileData* data, String text) {
    clear(&data->lineOffsets);

    push(&data->lineOffsets, (Lsp::LineOffset) 0);

    for (size_t i = 0; i < text.len; i++) {
        if (text.buff[i] == '\n') {
            push(&data->lineOffsets, (Lsp::LineOffset)(i + 1));
        }
    }
}

void createLineOffsets(Lsp::FileData* data) {
    init(&data->lineOffsets, 1024);
    updateLineOffsets(data, data->data);
}

// Incremental update
void updateLineOffsets(
    Lsp::FileData* data,
    uint32_t  startLine,
    uint32_t  endLine,
    uint32_t  startOffset,
    String    newText,
    int64_t   diffOffset
) {
    Array<Lsp::LineOffset>* const offsets = &data->lineOffsets;

    for (size_t i = endLine + 1; i < offsets->size; i++) {
        offsets->data[i] += diffOffset;
    }

    uint32_t newLinesCount = countLines(newText);
    int64_t  lineDiff      = (int64_t) newLinesCount - (int64_t)(endLine - startLine);
    uint32_t tailSize      = (uint32_t)(offsets->size - endLine - 1);
    size_t   oldSize       = offsets->size;

    if (lineDiff > 0) resize(offsets, oldSize + lineDiff);

    memmove(offsets->data + startLine + 1 + newLinesCount,
            offsets->data + endLine + 1,
            tailSize * sizeof(Lsp::LineOffset));

    if (lineDiff < 0) resize(offsets, oldSize + lineDiff);

    uint32_t lineIdx = startLine + 1;
    for (size_t i = 0; i < newText.len; i++) {
        if (newText.buff[i] == '\n') {
            offsets->data[lineIdx++] = (Lsp::LineOffset)(startOffset + i + 1);
        }
    }
}



Lsp::T::SemanticTokenTypes nodeToLspToken(SyntaxNode* node) {
    if (!node) return Lsp::T::SEMANTIC_TOKEN_TYPES_NONE;

    switch (node->type) {
        case NT_TYPE_DEFINITION:
        case NT_UNION:
            return Lsp::T::SEMANTIC_TOKEN_TYPES_CLASS;

        case NT_ENUMERATOR:
            return Lsp::T::SEMANTIC_TOKEN_TYPES_ENUM;

        case NT_FUNCTION:
            return Lsp::T::SEMANTIC_TOKEN_TYPES_FUNCTION;

        case NT_VARIABLE_DEFINITION:
        case NT_VARIABLE:
            return Lsp::T::SEMANTIC_TOKEN_TYPES_VARIABLE;

        case NT_NAMESPACE:
            return Lsp::T::SEMANTIC_TOKEN_TYPES_NAMESPACE;

        case NT_BRANCH:
        case NT_SWITCH_CASE:
        case NT_LOOP:
        case NT_RETURN_STATEMENT:
        case NT_CONTINUE_STATEMENT:
        case NT_BREAK_STATEMENT:
        case NT_GOTO_STATEMENT:
        case NT_USING:
        case NT_IMPORT:
        case NT_STATEMENT:
            return Lsp::T::SEMANTIC_TOKEN_TYPES_KEYWORD;

        default:
            return Lsp::T::SEMANTIC_TOKEN_TYPES_NONE;
    }
}



void createSemanticTokens(Lsp::FileData* data) {
    // TODO: magic number to a config
    init(&data->semanticTokens, 512);
    init(&data->semanticTokensOld, 512);
}

inline Pos toLspPos(Lsp::FileData* data, Pos pos) {
    Pos lspPos;
    lspPos.ln = pos.ln - 1;
    lspPos.idx = data->lineOffsets.size <= lspPos.ln ?
        0 : pos.idx - data->lineOffsets.data[lspPos.ln];

    return lspPos;
}

// Shall be called after compilation
void Lsp::updateSemanticTokens(Lsp::FileData* data) {
    if (!data || !data->unit->ast || !data->unit->ast->root) return;

    clear(&data->semanticTokens);

    SyntaxNode* root = (SyntaxNode*) data->unit->ast->root;
    astWalk(root, [&](SyntaxNode* node) -> WalkControl {
        if (!node->span) return WK_CONTINUE;

        Lsp::SemanticToken token;

        token.type = nodeToLspToken(node);
        if (token.type == Lsp::T::SEMANTIC_TOKEN_TYPES_NONE) {
            return WK_CONTINUE;
        }

        Span* nameSpan = Ast::Node::getNameSpan(node);
        if (nameSpan) {
            Pos pos = toLspPos(data, nameSpan->start);
            token.ch  = pos.idx;
            token.ln  = pos.ln;
            token.len = nameSpan->end.idx - nameSpan->start.idx;
        } else {
            Pos pos = toLspPos(data, node->span->start);
            token.ch  = pos.idx;
            token.ln  = pos.ln;
            token.len = Ast::Node::getTokenLen(node);
        }

        token.mod = 0;
        if (node->type == NT_VARIABLE_DEFINITION) {
            token.mod |= Lsp::T::SEMANTIC_TOKEN_MODIFIERS_DECLARATION;

            TypeSpecifier* spec = &((VariableDefinition*) node)->type;
            if (spec->qualifier) {
                token.mod |= Lsp::T::SEMANTIC_TOKEN_MODIFIERS_READONLY;
            }

            if (token.len > 0) {
                push(&data->semanticTokens, token);
            }

            Pos pos = toLspPos(data, spec->span->start);
            token.ch  = pos.idx;
            token.ln  = pos.ln;
            token.len = spec->span->end.idx - spec->span->start.idx;
            token.type = Lsp::T::SEMANTIC_TOKEN_TYPES_TYPE;
            token.mod = 0;
        }

        if (token.len > 0) {
            push(&data->semanticTokens, token);
        }

        return WK_CONTINUE;
    });

    // TODO: We sort in case that some nodes may be in different order than
    //       in actual source. Ex. some expressions with unique interpretation.
    //       We may specify that compiler has to preserve source order.
    std::sort(data->semanticTokens.data, data->semanticTokens.data + data->semanticTokens.size,
        [](const Lsp::SemanticToken& a, const Lsp::SemanticToken& b) {
            if (a.ln != b.ln) return a.ln < b.ln;
            if (a.ch != b.ch) return a.ch < b.ch;
            return a.len > b.len;
        }
    );

}

// Copy current tokens into oldTokens
void archiveSemanticTokens(Lsp::FileData* data) {
    clear(&data->semanticTokensOld);
    reserve(&data->semanticTokensOld, data->semanticTokens.size);

    memcpy(data->semanticTokensOld.data,
           data->semanticTokens.data,
           data->semanticTokens.size * sizeof(Lsp::SemanticToken));

    data->semanticTokensOld.size = data->semanticTokens.size;
}

Lsp::FileData* makeFileData() {
    Lsp::FileData* data = (Lsp::FileData*) calloc(1, sizeof(Lsp::FileData));
    if (!data) Lsp::panic({ Lsp::Err::ALLOC, { 0 } });

    data->data = { 0 };

    init(&data->lineOffsets, 512);
    init(&data->semanticTokens, 512);
    init(&data->semanticTokensOld, 512);

    int arenaCount = sizeof(Lsp::FileData::arenas) / sizeof(Arena::Container);
    for (int i = 0; i < arenaCount; i++) {
        Arena::init(data->arenas + i, 1024 * 1024 * 32);

        // We use global compiler allocator as we reset local arenas
        // but this has to persist
        data->unit[i].ast = alloc<AstContext>();
        data->unit[i].reg = alloc<AstRegistry>();

        Ast::init(data->unit[i].ast);
        Ast::init(data->unit[i].reg);
    }

    return data;
}

void releaseFileData(Lsp::FileData* data) {
    releaseFileString(data->data);
    release(&data->lineOffsets);
    release(&data->semanticTokens);
    release(&data->semanticTokensOld);
    free(data);
}

// Converts a position to a byte offset, clamping out-of-bounds values
uint64_t getOffset(Lsp::FileData* data, Lsp::T::Position pos) {
    if (pos.line >= data->lineOffsets.size) {
        return data->data.len;
    }

    size_t lineStart = data->lineOffsets.data[pos.line];
    size_t lineEnd   = pos.line + 1 < data->lineOffsets.size
                        ? data->lineOffsets.data[pos.line + 1]
                        : data->data.len;

    size_t lineLength = lineEnd - lineStart;
    if (pos.character > lineLength) {
        return lineEnd;
    }

    return lineStart + pos.character;
}

// Converts a position to a byte offset, returning INVALID_OFFSET if out of bounds
uint64_t getStrictOffset(Lsp::FileData* data, Lsp::T::Position pos) {
    if (pos.line >= data->lineOffsets.size) {
        return INVALID_OFFSET;
    }

    size_t lineStart = data->lineOffsets.data[pos.line];
    size_t lineEnd   = pos.line + 1 < data->lineOffsets.size
                        ? data->lineOffsets.data[pos.line + 1]
                        : data->data.len;

    size_t lineLength = lineEnd - lineStart;
    if (pos.character > lineLength) {
        return INVALID_OFFSET;
    }

    return lineStart + pos.character;
}

Lsp::Err::Kind
applyChanges(
    Lsp::FileData* data,
    Lsp::T::Slice<Lsp::T::TextDocumentContentChangeEvent*> changes
) {
    for (int i = 0; i < changes.size; i++) {
        auto* change = changes.data[i];

        if (change->range) {
            uint32_t sOffset = getStrictOffset(data, *change->range->start);
            uint32_t eOffset = getStrictOffset(data, *change->range->end);
            if (sOffset == INVALID_OFFSET || eOffset == INVALID_OFFSET) {
                return Lsp::Err::SYNC;
            }

            int64_t diff = change->text.len - (eOffset - sOffset);
            uint32_t oldTotalLen = data->data.len;
            uint32_t newTotalLen = oldTotalLen + diff;

            if (diff > 0) resizeFileString(&data->data, newTotalLen);
            memmove(data->data + sOffset + change->text.len,
                    data->data + eOffset,
                    oldTotalLen - eOffset);
            memcpy(data->data + sOffset, change->text.buff, change->text.len);
            if (diff < 0) resizeFileString(&data->data, newTotalLen);

            updateLineOffsets(
                data,
                change->range->start->line,
                change->range->end->line,
                sOffset,
                change->text,
                diff
            );
        } else {
            resizeFileString(&data->data, change->text.len);
            memcpy(data->data, change->text, change->text.len);
            updateLineOffsets(data, change->text);
        }
    }

    return Lsp::Err::OK;
}



// Basic
// ===

// NOTE:
// We set compilers allocator each time before file processing in task system.
// Each file has its own two arenas it switches to always provide ready to read
// buffer.
//
// But we may need to manipulate with compilers global state. To do so we have
// to also provide global allocator to switch in global context. We may use the
// lsp-allocator, but life time of compiler/lsp data may differ, may certainly
// differ I shall say, as we may want to reset lsp-allocator per request, while
// keeping all global compiler data at least till we are enforced to do a full
// recompilation.
// TODO: we have to think of either fat pointers, or overall compiler design that
// would prevent global state to point to potentially invalid 'local' memory.
//
//
Arena::Container gMainCompilerAllocator;
thread_local Allocator allocator = NULL;

void Lsp::setAndClearCompilerAllocator(Arena::Container* arena) {
    allocator = arena;
    Arena::clear(arena);
}

bool allocIsInitialized() {
    return allocator != NULL;
}

void allocInit() {
}

void allocRelease() {
}

void allocClear() {
    Arena::clear((Arena::Container*) allocator);
}



void* alloc(size_t bytes, size_t align) {
    return Arena::push((Arena::Container*) allocator, bytes, align);
}

void dealloc(void* ptr) {
    Arena::rollback((Arena::Container*) allocator, ptr);
}



AllocatorMarker allocMark() {
    Arena::Container* arena = (Arena::Container*) allocator;
    return arena->tail->data + arena->tail->pos;
}

void allocRollback(AllocatorMarker marker) {
    Arena::rollback((Arena::Container*) allocator, marker);
}



void nallocInit() {
}

void nallocRelease() {
}

void* nalloc(AllocType type) {
    return alloc(nodeTypeSize[type], nodeTypeSize[type]);
}

void* nalloc(AllocType type, size_t count) {
    return alloc(nodeTypeSize[type] * count, nodeTypeSize[type]);
}

void ndealloc(void* ptr) {
}



void Lsp::init() {
    DArray::init(&Lsp::stack, 1024, sizeof(void*));
    Arena::init(&gMainCompilerAllocator, 1024 * 1024 * 16);
    allocator = &gMainCompilerAllocator;

    FileSystem::init();
    CompilerWorker::init(Lsp::State::comm, Lsp::State::compilerThreadCount);
}

void Lsp::release() {
    DArray::release(&Lsp::stack);
    Arena::release(&gMainCompilerAllocator);

    FileSystem::release();
}



// TextDocument handles
// ===

Lsp::Err::Kind
Lsp::handle(Lsp::TextDocument::DidOpen* method) {
    if (!method || !method->textDocument) {
        return report({ Err::INVALID_PARAMS, { 0 } });
    }

    Lsp::T::TextDocumentItem* item = method->textDocument;

    FileSystem::Handle hnd = FileSystem::load(item->uri, ORIGIN);
    if (!hnd) return Err::LOAD_FILE;

    FileSystem::FileInfo* info = FileSystem::getFileInfo(hnd);
    FileData* data = (FileData*) info->userData;

    if (!data) {
        data = makeFileData();
        info->userData = data;
    }

    releaseFileString(data->data);
    data->data = makeFileString(item->text);
    data->version = item->version;

    updateLineOffsets(data, data->data);

    // We parse in sync at the beginning to have at least
    // something to query...
    TaskSystem::beginGroup();
    TaskSystem::dispatchParse(hnd);
    TaskSystem::wait();

    info->status = FileSystem::FS_DIRTY;
    CompilerWorker::enqueueCompilation(hnd);

    return Err::OK;
}

Lsp::Err::Kind
Lsp::handle(Lsp::TextDocument::DidClose *method) {
    if (!method || !method->textDocument) {
        return report({ Err::INVALID_PARAMS, { 0 } });
    }

    Lsp::T::TextDocumentIdentifier* id = method->textDocument;

    FileSystem::Path path;
    FileSystem::uriToPath(id->uri, &path);

    void* hnd = FileSystem::getHandle({ path.buffer, path.bufferLen });
    if (!hnd) return Err::OK;

    FileSystem::FileInfo* info = FileSystem::getFileInfo(hnd);
    FileData* data = (FileData*) info->userData;
    print(data);

    if (info->userData) {
        releaseFileData(data);
        info->userData = NULL;
    }

    FileSystem::unload(hnd, ORIGIN);
    return Err::OK;
}

Lsp::Err::Kind
Lsp::handle(TextDocument::DidChange* method) {
    if (!method || !method->textDocument) {
        return report({ Err::INVALID_PARAMS, { 0 } });
    }

    int version = method->textDocument->version;

    FileSystem::Path path;
    FileSystem::uriToPath(method->textDocument->uri, &path);

    void* hnd = FileSystem::getHandle({ path.buffer, path.bufferLen });
    if (!hnd) return Err::OK;

    FileSystem::FileInfo* info = FileSystem::getFileInfo(hnd);
    FileData* data = (FileData*) info->userData;

    if (method->textDocument->version <= data->version) {
        return Err::OK;
    }
    data->version = version;

    applyChanges(data, method->contentChanges);

    print(data);
    return Err::OK;
}

template<typename T, typename F>
SyntaxNode* handleQuery(Lsp::FileData* data, T* method, F fcn) {
    int idx;
    while (true) {
        // Check-in
        idx = data->committedIdx.load(std::memory_order_acquire);
        data->readerCount[idx].fetch_add(1, std::memory_order_relaxed);

        // But theoretically there is a chance, that buffers was switched and new
        // work already started as readerCount was not updated in time. We have to
        // validate.
        if (idx == data->committedIdx.load(std::memory_order_acquire)) {
            break;
        }

        // Unlucky. Check-out and try again
        int remaining = data->readerCount[idx].fetch_sub(1, std::memory_order_release);
        if (remaining == 1) data->readerCount[idx].notify_one();
    }

    SyntaxNode* result = fcn(data, method);

    // Check-out
    if (data->readerCount[idx].fetch_sub(1, std::memory_order_release) == 1) {
        data->readerCount[idx].notify_one();
    }

    return result;
}

Pos toPos(Lsp::FileData* fdata, Lsp::T::Position* lspPos) {
    if (fdata->lineOffsets.size <= lspPos->line) {
        return { -1, -1 };
    }

    return {
        .idx = (int) (fdata->lineOffsets.data[lspPos->line] + lspPos->character),
        .ln = (int) lspPos->line
    };
}

Lsp::T::Position* toLspPosition(Lsp::FileData* fdata, Pos pos) {
   using namespace Lsp;
   T::Position* position = alloc<T::Position>(&Lsp::State::allocator);

   const int line = pos.ln - 1;

   position->line = line;
   if (fdata->lineOffsets.size <= line) {
       position->character = 0;
   } else {
       position->character = pos.idx - fdata->lineOffsets.data[line];
   }

   return position;
}

Lsp::T::Range* toLspRange(Lsp::FileData* fdata, Span* span) {
    using namespace Lsp;
    T::Range* range = alloc<T::Range>(&Lsp::State::allocator);

    range->start = toLspPosition(fdata, span->start);
    range->end = toLspPosition(fdata, span->end);

    return range;
}

bool isInside(Span* span, Pos pos) {
    return (span->start.idx <= pos.idx) && (span->end.idx >= pos.idx);
}

Lsp::T::Hover*
Lsp::handle(Lsp::TextDocument::Hover* method) {
    FileSystem::Handle fhnd = FileSystem::getHandle(method->textDocument->uri);
    if (!fhnd) return NULL;

    FileData* data = (FileData*) FileSystem::getUserData(fhnd);

    SyntaxNode* node = handleQuery(data, method, [] (auto data, auto method) {
        SyntaxNode* bestNode = (SyntaxNode*) data->unit->ast->root;
        astWalk(bestNode, [&](SyntaxNode* node) -> WalkControl {
            if (!node->span) return WK_CONTINUE;

            if (isInside(node->span, toPos(data, method->position))) {
                bestNode = node;
                return WK_ONLY_CHILDREN;
            } else {
                return WK_SKIP_CHILDREN;
            }
        });

        return bestNode;
    });

    if (!node) return NULL;

    T::Hover* result = alloc<T::Hover>(&State::allocator);
    result->range = toLspRange(data, node->span);
    result->contents = alloc<T::MarkupContent>(&State::allocator);
    result->contents->kind = Lsp::T::MARKUP_KIND_MARKDOWN;
    result->contents->value = Lsp::Render::hover(node);

    return result;
}

// We have to send it in relative offsets...
Lsp::T::UInt* semanticTokensToFull(Array<Lsp::SemanticToken>* src) {
    Lsp::T::UInt* dest = alloc<Lsp::T::UInt>(&Lsp::State::allocator,
        src->size * 5);

    uint32_t prevLn = 0;
    uint32_t prevCh = 0;

    for (size_t i = 0; i < src->size; i++) {
        Lsp::SemanticToken token = src->data[i];

        const int offset = i * 5;
        dest[offset + 0] = token.ln - prevLn;
        dest[offset + 1] = token.ch - (prevLn == token.ln ? prevCh : 0);
        dest[offset + 2] = token.len;
        dest[offset + 3] = token.type;
        dest[offset + 4] = token.mod;

        prevLn = token.ln;
        prevCh = token.ch;
    }

    return dest;
}

// We have to send a diff of relative offsets...
Lsp::Slice<Lsp::T::SemanticTokensEdit*> semanticTokensToDelta(
    Array<Lsp::SemanticToken>* oldTokens,
    Array<Lsp::SemanticToken>* newTokens
) {
    Lsp::Slice<Lsp::T::SemanticTokensEdit*> edits;

    // If no previous tokens exist, fallback to replacing everything
    if (!oldTokens || oldTokens->size == 0) {
        auto editArray = alloc<Lsp::T::SemanticTokensEdit*>(&Lsp::State::allocator, 1);

        auto edit = alloc<Lsp::T::SemanticTokensEdit>(&Lsp::State::allocator);
        edit->start       = 0;
        edit->deleteCount = 0;
        edit->data        = {
            .data = semanticTokensToFull(newTokens),
            .size = newTokens->size
        };

        editArray[0] = edit;
        edits.data = editArray;
        edits.size = 1;

        return edits;
    }

    // Find common prefix
    size_t prefix = 0;
    while (
        prefix < oldTokens->size &&
        prefix < newTokens->size &&
        memcmp(oldTokens->data + prefix,
               newTokens->data + prefix,
               sizeof(Lsp::SemanticToken)) == 0
    ) {
        prefix++;
    }

    // Find common suffix
    size_t oldSuffix = oldTokens->size;
    size_t newSuffix = newTokens->size;
    while (
        oldSuffix > prefix &&
        newSuffix > prefix &&
        memcmp(oldTokens->data + oldSuffix - 1,
               newTokens->data + newSuffix - 1,
               sizeof(Lsp::SemanticToken)) == 0
    ) {
        oldSuffix--;
        newSuffix--;
    }

    Array<Lsp::SemanticToken> diff;
    diff.data     = newTokens->data + prefix;
    diff.size     = newSuffix - prefix;
    diff.capacity = diff.size;

    auto editArray = alloc<Lsp::T::SemanticTokensEdit*>(&Lsp::State::allocator, 1);

    auto edit = alloc<Lsp::T::SemanticTokensEdit>(&Lsp::State::allocator);
    edit->start       = (uint32_t) (prefix * 5);
    edit->deleteCount = (uint32_t) ((oldSuffix - prefix) * 5);
    edit->data.data   = semanticTokensToFull(&diff);
    edit->data.size   = edit->deleteCount;

    editArray[0] = edit;
    edits.data = editArray;

    return edits;
}

Lsp::T::SemanticTokens*
Lsp::handle(Lsp::TextDocument::SemanticTokensfull* method) {
    FileSystem::Handle fhnd = FileSystem::getHandle(method->textDocument->uri);
    if (!fhnd) return NULL;

    FileData* data = (FileData*) FileSystem::getUserData(fhnd);
    if (!data || data->semanticTokens.size == 0) {
        // TODO: we may want to try to compute it on demand
        return NULL;
    }

    size_t size = 0;
    Lsp::T::UInt* dest = NULL;

    handleQuery(data, method, [&](auto data, auto method) {
        size = data->semanticTokens.size * 5;
        dest = semanticTokensToFull(&data->semanticTokens);
        return (SyntaxNode*) NULL;
    });

    auto result = alloc<T::SemanticTokens>(&State::allocator);
    result->data.data = dest;
    result->data.size = size;

    return result;
}

Lsp::T::SemanticTokensDelta*
Lsp::handle(Lsp::TextDocument::SemanticTokensfulldelta* method) {
    FileSystem::Handle fhnd = FileSystem::getHandle(method->textDocument->uri);
    if (!fhnd) return NULL;

    FileData* data = (FileData*) FileSystem::getUserData(fhnd);
    if (!data || data->semanticTokens.size == 0) {
        // TODO: we may want to try to compute it on demand
        return NULL;
    }

    Slice<T::SemanticTokensEdit*> dest;

    handleQuery(data, method, [&](auto data, auto method) {
        dest = semanticTokensToDelta(&data->semanticTokensOld, &data->semanticTokens);
        return (SyntaxNode*) NULL;
    });

    auto result = alloc<T::SemanticTokensDelta>(&State::allocator);
    result->edits = dest;

    return result;
}



// Generic Tree Walker TODO: move to the top?
//

// callback returns false if we have to abort, so to not
// wrap everything with ifs or using macro we go full cpp
template <typename F, typename... Nodes>
inline bool visitNodes(F&& callback, Nodes*... children) {
    return ((children ? callback((SyntaxNode*) children) : true) && ...);
}

template <typename F, typename Node>
inline bool visitArray(F&& callback, Node** array, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (array[i] && !callback((SyntaxNode*) array[i])) {
            return false;
        }
    }
    return true;
}

template <typename F>
bool forEachNode(SyntaxNode* node, F&& callback) {
    if (!node) return true;

    switch (node->type) {
        case NT_NAMESPACE:
        case NT_SCOPE: {
            Scope* scope = (Scope*) node;
            return visitArray(callback, scope->children, scope->childrenCount);
        }

        case NT_CODE_BLOCK: {
            CodeBlock* node = (CodeBlock*) node;
            break;
        }

        case NT_STATEMENT: {
            Statement* stmt = (Statement*) node;
            return visitNodes(callback, stmt->operand);
        }

        case NT_VARIABLE_DEFINITION: {
            VariableDefinition* def = (VariableDefinition*) node;
            if (!visitNodes(callback, def->var)) return false;

            for (uint32_t i = 0; i < def->type.decoratorCount; i++) {
                TypeDecorator* dec = def->type.decorators[i];
                if (dec && dec->len) {
                    if (!callback((SyntaxNode*) dec->len)) return false;
                }
            }

            break;
        }

        case NT_VARIABLE_ASSIGNMENT: {
            VariableAssignment* ass = (VariableAssignment*) node;
            return visitNodes(callback, ass->lvar, ass->rvar);
        }

        case NT_VARIABLE: {
            Variable* var = (Variable*) node;
            if (var->expression) {
                astForEachExpression(var->expression, callback);
            }
            break;
        }

        case NT_ENUMERATOR: {
            Enumerator* en = (Enumerator*) node;
            return visitArray(callback, en->vars, en->varCount);
        }

        case NT_TYPE_DEFINITION:
        case NT_UNION: {
            TypeDefinition* tdef = (TypeDefinition*) node;
            return visitArray(callback, tdef->vars, tdef->varCount);
        }

        case NT_TYPE_INITIALIZATION: {
            TypeInitialization* init = (TypeInitialization*) node;
            return visitArray(callback, init->attributes, init->attributeCount) &&
                   visitNodes(callback, init->fillVar);
        }

        case NT_FUNCTION: {
            Function* fcn = (Function*) node;
            return visitArray(callback, fcn->prototype.inArgs, fcn->prototype.inArgCount) &&
                   visitNodes(callback, fcn->prototype.outArg, fcn->bodyScope);
        }

        case NT_BRANCH: {
            Branch* branch = (Branch*) node;

            uint32_t i = 0;
            for (; i < branch->expressionCount; i++) {
                if (!visitNodes(callback, branch->expressions[i],
                    branch->scopes[i])) {
                    return false;
                }
            }

            return visitNodes(callback, branch->scopes[i]);
        }

        case NT_SWITCH_CASE: {
            SwitchCase* sc = (SwitchCase*) node;

            if (!visitNodes(callback, sc->switchExp)) return false;

            for (uint32_t i = 0; i < sc->caseExpCount; i++) {
                if (!visitNodes(callback, sc->casesExp[i], sc->cases[i])) {
                    return false;
                }
            }

            return visitNodes(callback, sc->elseCase);
        }

        case NT_LOOP: {
            Loop* loop = (Loop*) node;

            bool exit;
            if (loop->arg.kind == Loop::Arg::EXPRESSION) {
                exit = visitNodes(callback, loop->arg.exp);
            } else if (loop->arg.kind == Loop::Arg::RANGE) {
                exit = visitNodes(callback, loop->arg.range);
            }

            return exit && visitNodes(callback, loop->item, loop->index.var, loop->bodyScope);
        }

        case NT_RETURN_STATEMENT: {
            ReturnStatement* ret = (ReturnStatement*) node;
            return visitNodes(callback, ret->var, ret->err);
        }

        case NT_BREAK_STATEMENT:
        case NT_CONTINUE_STATEMENT: {
            break;
        }

        case NT_ERROR: {
            ErrorSet* errSet = (ErrorSet*) node;
            return visitArray(callback, errSet->vars, errSet->varCount);
        }

        case NT_USING: {
            Using* usg = (Using*) node;
            return visitNodes(callback, usg->var);
        }

        case NT_IMPORT: {
            break;
        }

        case NT_LABEL:
        case NT_GOTO_STATEMENT: {
            break;
        }

        case NT_COUNT:
            break;
    }

    return true;
}

template <typename F>
bool astForEachExpression(Expression* expr, F&& callback) {
    if (!expr) return true;

    switch (expr->type) {
        case EXT_UNARY: {
            UnaryExpression* uex = (UnaryExpression*) expr;
            if (uex->operand) callback((SyntaxNode*) uex->operand);
            break;
        }

        case EXT_BINARY: {
            BinaryExpression* bex = (BinaryExpression*) expr;
            if (bex->left)  callback((SyntaxNode*) bex->left);
            if (bex->right) callback((SyntaxNode*) bex->right);
            break;
        }

        case EXT_TERNARY: {
            TernaryExpression* tex = (TernaryExpression*) expr;
            if (tex->condition) callback((SyntaxNode*) tex->condition);
            if (tex->trueExp)   callback((SyntaxNode*) tex->trueExp);
            if (tex->falseExp)  callback((SyntaxNode*) tex->falseExp);
            break;
        }

        case EXT_RANGE: {
            RangeExpression* range = (RangeExpression*) expr;
            if (range->bidx) callback((SyntaxNode*) range->bidx);
            if (range->eidx) callback((SyntaxNode*) range->eidx);
            if (range->step) callback((SyntaxNode*) range->step);
            break;
        }

        case EXT_TYPE_INITIALIZATION: {
            TypeInitialization* init = (TypeInitialization*) expr;
            for (uint32_t i = 0; i < init->attributeCount; i++) {
                if (init->attributes[i]) callback((SyntaxNode*) init->attributes[i]);
            }
            if (init->fillVar) callback((SyntaxNode*) init->fillVar);
            break;
        }

        case EXT_ARRAY_INITIALIZATION: {
            ArrayInitialization* init = (ArrayInitialization*) expr;
            for (uint32_t i = 0; i < init->attributeCount; i++) {
                if (init->attributes[i]) callback((SyntaxNode*) init->attributes[i]);
            }
            break;
        }

        case EXT_STRING_INITIALIZATION:
            break;

        case EXT_ALLOC: {
            Alloc* a = (Alloc*) expr;
            if (a->def) callback((SyntaxNode*) a->def);
            break;
        }

        case EXT_FREE: {
            Free* f = (Free*) expr;
            if (f->var) callback((SyntaxNode*) f->var);
            break;
        }

        case EXT_GET_LENGTH: {
            GetLength* gl = (GetLength*) expr;
            if (gl->arr) callback((SyntaxNode*) gl->arr);
            break;
        }

        case EXT_CAST: {
            Cast* cast = (Cast*) expr;
            if (cast->operand) callback((SyntaxNode*) cast->operand);
            break;
        }

        case EXT_CATCH: {
            Catch* c = (Catch*) expr;
            if (c->call)  callback((SyntaxNode*) c->call);
            if (c->err)   callback((SyntaxNode*) c->err);
            if (c->scope) callback((SyntaxNode*) c->scope);
            break;
        }

        default:
            break;
    }

    return true;
}

template <typename F>
bool astWalk(SyntaxNode* root, F&& visitor) {
    if (!root) return true;

    WalkControl wc = visitor(root);
    if (wc == WK_ABORT) return false;

    if (wc == WK_CONTINUE || wc == WK_ONLY_CHILDREN) {
        bool shouldIStayOrShouldIGo = true;

        forEachNode(root, [&](SyntaxNode* child) -> bool {
            shouldIStayOrShouldIGo = astWalk(child, visitor);
            return shouldIStayOrShouldIGo;
        });

        return shouldIStayOrShouldIGo && (wc != WK_ONLY_CHILDREN);
    }

    return true;
}



// Logger
// ===
// TODO : reuse somehow compilers logger?

#include "../../src/ansi_colors.h"
#define LSP_TAG "LSP-Vi"

static void printTag() {
    fprintf(stderr, AC_BOLD "[LSP-Vi]" AC_RESET);
}

static void printFileName(String name) {
    if (name.buff && name.len > 0) {
        fprintf(stderr, AC_BRIGHT_BLACK "[%.*s]" AC_RESET, (int) name.len, name.buff);
    }
}

static void printTag(Lsp::Inf::Kind kind) {
    switch (kind) {
        case Lsp::Inf::DEBUG:
            fprintf(stderr, AC_BOLD_CYAN "[DEBUG]" AC_RESET);
            break;
        case Lsp::Inf::INFO:
        default:
            fprintf(stderr, AC_BOLD_GREEN "[INFO]" AC_RESET);
            break;
    }
}

Lsp::Inf::Kind
Lsp::report(Lsp::Inf::Info inf, const char* format, ...) {
    printTag();
    printTag(inf.kind);

    va_list argList;
    va_start(argList, format);
    vfprintf(stderr, format, argList);
    va_end(argList);

    printFileName(inf.file);
    fprintf(stderr, "\n");

    return inf.kind;
}

Lsp::Err::Kind
Lsp::report(Err::Info err, ...) {
    if (err.kind == Err::OK) return Err::OK;

    printTag();
    fprintf(stderr, AC_BOLD_RED "[ERROR]" AC_RESET);
    fprintf(stderr, ": ");

    va_list argList;
    va_start(argList, err);
    vfprintf(stderr, Err::str(err.kind), argList);
    va_end(argList);

    printFileName(err.file);
    fprintf(stderr, "\n");

    return err.kind;
}

void Lsp::panic(Err::Info err) {
    report(err);
    std::abort();
}



// Debug functions
// ===

constexpr int MAX_TEXT_PREVIEW   = 1024;
constexpr int MAX_OFFSET_PREVIEW = 16;

void print(const Lsp::FileData* data) {
    if (!data) {
        fprintf(stderr, AC_ERROR "\nFileData: [NULL]" AC_RESET "\n");
        return;
    }

    fprintf(stderr, AC_SECTION "\n=== FileData (LSP State) ===" AC_RESET "\n");
    fprintf(stderr, "  " AC_VAR "Version:      " AC_RESET AC_NUMBER "%u" AC_RESET "\n", data->version);

    fprintf(stderr, "  " AC_VAR "Content:      " AC_RESET AC_NUMBER "%zu" AC_RESET AC_SEPARATOR "/" AC_RESET AC_NUMBER "%zu" AC_RESET " bytes (Used/Cap)\n",
           data->data.len, data->data.capacity);

    fprintf(stderr, "  " AC_VAR "Line Offsets: " AC_RESET AC_NUMBER "%u" AC_RESET AC_SEPARATOR "/" AC_RESET AC_NUMBER "%u" AC_RESET " lines (Used/Cap)\n",
           data->lineOffsets.size, data->lineOffsets.capacity);

    // Visualizing the offset array
    fprintf(stderr, "  " AC_VAR "Offset Map:   " AC_RESET AC_SEPARATOR "[" AC_RESET);
    uint32_t previewCount = data->lineOffsets.size > MAX_OFFSET_PREVIEW ? MAX_OFFSET_PREVIEW : data->lineOffsets.size;
    for (uint32_t i = 0; i < previewCount; i++) {
        fprintf(stderr, AC_NUMBER "%u" AC_RESET "%s",
               data->lineOffsets.data[i],
               (i == previewCount - 1) ? "" : AC_SEPARATOR ", " AC_RESET);
    }
    if (data->lineOffsets.size > MAX_OFFSET_PREVIEW) fprintf(stderr, AC_SEPARATOR ", ..." AC_RESET);
    fprintf(stderr, AC_SEPARATOR "]" AC_RESET "\n");

    // Preview the actual text content
    if (data->data.buff) {
        int previewLen = data->data.len > MAX_TEXT_PREVIEW ? MAX_TEXT_PREVIEW : (int)data->data.len;
        fprintf(stderr, "  " AC_VAR "Text Preview:\n" AC_RESET "---\n" AC_TYPE "%.*s" AC_RESET AC_SEPARATOR "%s" AC_RESET "\n---\n",
               previewLen, data->data.buff,
               data->data.len > MAX_TEXT_PREVIEW ? AC_BRIGHT_BLACK "..." : "");
    }

    fprintf(stderr, "\n");
}
