/*
 * While implementing the LSP type representation, it was decided to represent
 * all non-trivial data types via pointers (As trivial counts strings, integers
 * and booleans). This allows for a convenient and unified interface which also
 * allows for clean way of handling optional fields that are pretty common.
 * Given the use of arenas, the cost of allocating numerous small nodes is
 * negligible.
 *
 * Arrays are modeled using the Slice<T> template, which couples a pointer with
 * a size to keep the structures POD-like. The intended strategy for populating
 * these slices involves using a temporary stack during the parsing to collect
 * all relevant nodes. Once the final count is determined, a single
 * allocation of the exact required size can be performed.
 *
 * To prevent misinterpretation and ensure absolute fidelity to the official
 * specification, all original typenames have been preserved within the Lsp::T
 * namespace. Because the naming conventions in the LSP specification do not
 * always follow a consistent hierarchical logic, keeping the formal names
 * allows them to exist as a source of truth without being forced into a
 * naming scheme that may not fit.
 *
 * The primary user interface is organized around method paths, such as
 * Lsp::TextDocument::DidChange. This hierarchy follows the actual protocol
 * paths, where the final method name is a typedef pointing to the formal
 * parameter type defined in the Lsp::T namespace.
 *
 * To automate the conversion between JSON and Cpp structures, each type
 * provides a static Schema specialization. This allows for less readable
 * but generic parsing and serialization for any protocol node without polluting
 * this codebase with the very fun task of parsing JSON files.
 *
 * This process was automated by vibing ton of JS to generate most of the content
 * in this few lines long file. The thing should be named 'LspMetaGenerator.html'
 * and hopefully laying nearby this source file.
 */



#pragma once

#include "json.h"
#include "comm_provider.h"
#include "array.h"
// #include "../../src/globals.h"
#include "../../src/file_system.h"
#include "../../src/array_list.h"
#include "../../src/dynamic_arena.h"
#include "../../src/registry.h"

#include <tuple>
#include <cstdlib>



// AUTO-GENERATED LSP PROTOCOL DATA STRUCTURES
// ===

namespace Lsp::T {

    typedef String String;
    typedef int32_t    Int;
    typedef uint32_t   UInt;
    typedef bool   Bool;
    typedef double Double;

    typedef void* LSPAny;
    typedef void* LSPObject;

    template<typename T>
    struct Slice {
        T*     data;
        size_t size;
    };

    template<typename T>
    struct Schema;

    template<typename T>
	struct UnionSchema;

    template<typename Class, typename Member>
    struct Field {
        const char*    name;
        Member Class::*ptr;
    };

    template<typename Class, typename Member>
    static constexpr Field<Class, Member> make_field(const char* name, Member Class::*ptr) {
        return { name, ptr };
    }

    template<typename Tag, typename MemberPtr>
	struct VariantField {
		Tag       kind;
		MemberPtr ptr;
	};

	template<typename Tag, typename MemberPtr>
	static constexpr VariantField<Tag, MemberPtr> make_variant(Tag kind, MemberPtr ptr) {
		return { kind, ptr };
	}

    template<> struct Schema<void> {
        static constexpr auto fields = std::make_tuple();
    };

    // Method Count: 93
    // Seed:         257348622
    // Table Size:   256
    // Algorithm:    sdbm
    enum RequestMethod {
        RM_NONE                                     = -1,
        RM_TEXT_DOCUMENT_IMPLEMENTATION             = 204,
        RM_TEXT_DOCUMENT_TYPE_DEFINITION            = 33,
        RM_WORKSPACE_WORKSPACE_FOLDERS              = 251,
        RM_WORKSPACE_CONFIGURATION                  = 100,
        RM_TEXT_DOCUMENT_DOCUMENT_COLOR             = 56,
        RM_TEXT_DOCUMENT_COLOR_PRESENTATION         = 139,
        RM_TEXT_DOCUMENT_FOLDING_RANGE              = 249,
        RM_WORKSPACE_FOLDING_RANGE_REFRESH          = 78,
        RM_TEXT_DOCUMENT_DECLARATION                = 176,
        RM_TEXT_DOCUMENT_SELECTION_RANGE            = 155,
        RM_WINDOW_WORK_DONE_PROGRESS_CREATE         = 178,
        RM_TEXT_DOCUMENT_PREPARE_CALL_HIERARCHY     = 153,
        RM_CALL_HIERARCHY_INCOMING_CALLS            = 218,
        RM_CALL_HIERARCHY_OUTGOING_CALLS            = 110,
        RM_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL       = 30,
        RM_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL_DELTA = 253,
        RM_TEXT_DOCUMENT_SEMANTIC_TOKENS_RANGE      = 67,
        RM_WORKSPACE_SEMANTIC_TOKENS_REFRESH        = 96,
        RM_WINDOW_SHOW_DOCUMENT                     = 146,
        RM_TEXT_DOCUMENT_LINKED_EDITING_RANGE       = 104,
        RM_WORKSPACE_WILL_CREATE_FILES              = 190,
        RM_WORKSPACE_WILL_RENAME_FILES              = 16,
        RM_WORKSPACE_WILL_DELETE_FILES              = 237,
        RM_TEXT_DOCUMENT_MONIKER                    = 215,
        RM_TEXT_DOCUMENT_PREPARE_TYPE_HIERARCHY     = 28,
        RM_TYPE_HIERARCHY_SUPERTYPES                = 123,
        RM_TYPE_HIERARCHY_SUBTYPES                  = 140,
        RM_TEXT_DOCUMENT_INLINE_VALUE               = 85,
        RM_WORKSPACE_INLINE_VALUE_REFRESH           = 42,
        RM_TEXT_DOCUMENT_INLAY_HINT                 = 124,
        RM_INLAY_HINT_RESOLVE                       = 211,
        RM_WORKSPACE_INLAY_HINT_REFRESH             = 198,
        RM_TEXT_DOCUMENT_DIAGNOSTIC                 = 165,
        RM_WORKSPACE_DIAGNOSTIC                     = 127,
        RM_WORKSPACE_DIAGNOSTIC_REFRESH             = 113,
        RM_TEXT_DOCUMENT_INLINE_COMPLETION          = 89,
        RM_CLIENT_REGISTER_CAPABILITY               = 145,
        RM_CLIENT_UNREGISTER_CAPABILITY             = 150,
        RM_INITIALIZE                               = 121,
        RM_SHUTDOWN                                 = 250,
        RM_WINDOW_SHOW_MESSAGE_REQUEST              = 252,
        RM_TEXT_DOCUMENT_WILL_SAVE_WAIT_UNTIL       = 244,
        RM_TEXT_DOCUMENT_COMPLETION                 = 141,
        RM_COMPLETION_ITEM_RESOLVE                  = 130,
        RM_TEXT_DOCUMENT_HOVER                      = 148,
        RM_TEXT_DOCUMENT_SIGNATURE_HELP             = 102,
        RM_TEXT_DOCUMENT_DEFINITION                 = 49,
        RM_TEXT_DOCUMENT_REFERENCES                 = 73,
        RM_TEXT_DOCUMENT_DOCUMENT_HIGHLIGHT         = 103,
        RM_TEXT_DOCUMENT_DOCUMENT_SYMBOL            = 238,
        RM_TEXT_DOCUMENT_CODE_ACTION                = 219,
        RM_CODE_ACTION_RESOLVE                      = 197,
        RM_WORKSPACE_SYMBOL                         = 149,
        RM_WORKSPACE_SYMBOL_RESOLVE                 = 184,
        RM_TEXT_DOCUMENT_CODE_LENS                  = 174,
        RM_CODE_LENS_RESOLVE                        = 135,
        RM_WORKSPACE_CODE_LENS_REFRESH              = 62,
        RM_TEXT_DOCUMENT_DOCUMENT_LINK              = 243,
        RM_DOCUMENT_LINK_RESOLVE                    = 179,
        RM_TEXT_DOCUMENT_FORMATTING                 = 223,
        RM_TEXT_DOCUMENT_RANGE_FORMATTING           = 254,
        RM_TEXT_DOCUMENT_RANGES_FORMATTING          = 37,
        RM_TEXT_DOCUMENT_ON_TYPE_FORMATTING         = 39,
        RM_TEXT_DOCUMENT_RENAME                     = 245,
        RM_TEXT_DOCUMENT_PREPARE_RENAME             = 76,
        RM_WORKSPACE_EXECUTE_COMMAND                = 114,
        RM_WORKSPACE_APPLY_EDIT                     = 247,
        RM_WORKSPACE_DID_CHANGE_WORKSPACE_FOLDERS   = 225,
        RM_WINDOW_WORK_DONE_PROGRESS_CANCEL         = 216,
        RM_WORKSPACE_DID_CREATE_FILES               = 241,
        RM_WORKSPACE_DID_RENAME_FILES               = 34,
        RM_WORKSPACE_DID_DELETE_FILES               = 221,
        RM_NOTEBOOK_DOCUMENT_DID_OPEN               = 171,
        RM_NOTEBOOK_DOCUMENT_DID_CHANGE             = 143,
        RM_NOTEBOOK_DOCUMENT_DID_SAVE               = 64,
        RM_NOTEBOOK_DOCUMENT_DID_CLOSE              = 248,
        RM_INITIALIZED                              = 120,
        RM_EXIT                                     = 200,
        RM_WORKSPACE_DID_CHANGE_CONFIGURATION       = 63,
        RM_WINDOW_SHOW_MESSAGE                      = 246,
        RM_WINDOW_LOG_MESSAGE                       = 50,
        RM_TELEMETRY_EVENT                          = 46,
        RM_TEXT_DOCUMENT_DID_OPEN                   = 109,
        RM_TEXT_DOCUMENT_DID_CHANGE                 = 230,
        RM_TEXT_DOCUMENT_DID_CLOSE                  = 105,
        RM_TEXT_DOCUMENT_DID_SAVE                   = 220,
        RM_TEXT_DOCUMENT_WILL_SAVE                  = 35,
        RM_WORKSPACE_DID_CHANGE_WATCHED_FILES       = 70,
        RM_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS        = 224,
        RM___SET_TRACE                              = 168,
        RM___LOG_TRACE                              = 52,
        RM___CANCEL_REQUEST                         = 12,
        RM___PROGRESS                               = 132,
    };

    static const char* toString(RequestMethod val) {
        switch (val) {
            case RM_TEXT_DOCUMENT_IMPLEMENTATION            : return "textDocument/implementation";
            case RM_TEXT_DOCUMENT_TYPE_DEFINITION           : return "textDocument/typeDefinition";
            case RM_WORKSPACE_WORKSPACE_FOLDERS             : return "workspace/workspaceFolders";
            case RM_WORKSPACE_CONFIGURATION                 : return "workspace/configuration";
            case RM_TEXT_DOCUMENT_DOCUMENT_COLOR            : return "textDocument/documentColor";
            case RM_TEXT_DOCUMENT_COLOR_PRESENTATION        : return "textDocument/colorPresentation";
            case RM_TEXT_DOCUMENT_FOLDING_RANGE             : return "textDocument/foldingRange";
            case RM_WORKSPACE_FOLDING_RANGE_REFRESH         : return "workspace/foldingRange/refresh";
            case RM_TEXT_DOCUMENT_DECLARATION               : return "textDocument/declaration";
            case RM_TEXT_DOCUMENT_SELECTION_RANGE           : return "textDocument/selectionRange";
            case RM_WINDOW_WORK_DONE_PROGRESS_CREATE        : return "window/workDoneProgress/create";
            case RM_TEXT_DOCUMENT_PREPARE_CALL_HIERARCHY    : return "textDocument/prepareCallHierarchy";
            case RM_CALL_HIERARCHY_INCOMING_CALLS           : return "callHierarchy/incomingCalls";
            case RM_CALL_HIERARCHY_OUTGOING_CALLS           : return "callHierarchy/outgoingCalls";
            case RM_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL      : return "textDocument/semanticTokens/full";
            case RM_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL_DELTA: return "textDocument/semanticTokens/full/delta";
            case RM_TEXT_DOCUMENT_SEMANTIC_TOKENS_RANGE     : return "textDocument/semanticTokens/range";
            case RM_WORKSPACE_SEMANTIC_TOKENS_REFRESH       : return "workspace/semanticTokens/refresh";
            case RM_WINDOW_SHOW_DOCUMENT                    : return "window/showDocument";
            case RM_TEXT_DOCUMENT_LINKED_EDITING_RANGE      : return "textDocument/linkedEditingRange";
            case RM_WORKSPACE_WILL_CREATE_FILES             : return "workspace/willCreateFiles";
            case RM_WORKSPACE_WILL_RENAME_FILES             : return "workspace/willRenameFiles";
            case RM_WORKSPACE_WILL_DELETE_FILES             : return "workspace/willDeleteFiles";
            case RM_TEXT_DOCUMENT_MONIKER                   : return "textDocument/moniker";
            case RM_TEXT_DOCUMENT_PREPARE_TYPE_HIERARCHY    : return "textDocument/prepareTypeHierarchy";
            case RM_TYPE_HIERARCHY_SUPERTYPES               : return "typeHierarchy/supertypes";
            case RM_TYPE_HIERARCHY_SUBTYPES                 : return "typeHierarchy/subtypes";
            case RM_TEXT_DOCUMENT_INLINE_VALUE              : return "textDocument/inlineValue";
            case RM_WORKSPACE_INLINE_VALUE_REFRESH          : return "workspace/inlineValue/refresh";
            case RM_TEXT_DOCUMENT_INLAY_HINT                : return "textDocument/inlayHint";
            case RM_INLAY_HINT_RESOLVE                      : return "inlayHint/resolve";
            case RM_WORKSPACE_INLAY_HINT_REFRESH            : return "workspace/inlayHint/refresh";
            case RM_TEXT_DOCUMENT_DIAGNOSTIC                : return "textDocument/diagnostic";
            case RM_WORKSPACE_DIAGNOSTIC                    : return "workspace/diagnostic";
            case RM_WORKSPACE_DIAGNOSTIC_REFRESH            : return "workspace/diagnostic/refresh";
            case RM_TEXT_DOCUMENT_INLINE_COMPLETION         : return "textDocument/inlineCompletion";
            case RM_CLIENT_REGISTER_CAPABILITY              : return "client/registerCapability";
            case RM_CLIENT_UNREGISTER_CAPABILITY            : return "client/unregisterCapability";
            case RM_INITIALIZE                              : return "initialize";
            case RM_SHUTDOWN                                : return "shutdown";
            case RM_WINDOW_SHOW_MESSAGE_REQUEST             : return "window/showMessageRequest";
            case RM_TEXT_DOCUMENT_WILL_SAVE_WAIT_UNTIL      : return "textDocument/willSaveWaitUntil";
            case RM_TEXT_DOCUMENT_COMPLETION                : return "textDocument/completion";
            case RM_COMPLETION_ITEM_RESOLVE                 : return "completionItem/resolve";
            case RM_TEXT_DOCUMENT_HOVER                     : return "textDocument/hover";
            case RM_TEXT_DOCUMENT_SIGNATURE_HELP            : return "textDocument/signatureHelp";
            case RM_TEXT_DOCUMENT_DEFINITION                : return "textDocument/definition";
            case RM_TEXT_DOCUMENT_REFERENCES                : return "textDocument/references";
            case RM_TEXT_DOCUMENT_DOCUMENT_HIGHLIGHT        : return "textDocument/documentHighlight";
            case RM_TEXT_DOCUMENT_DOCUMENT_SYMBOL           : return "textDocument/documentSymbol";
            case RM_TEXT_DOCUMENT_CODE_ACTION               : return "textDocument/codeAction";
            case RM_CODE_ACTION_RESOLVE                     : return "codeAction/resolve";
            case RM_WORKSPACE_SYMBOL                        : return "workspace/symbol";
            case RM_WORKSPACE_SYMBOL_RESOLVE                : return "workspaceSymbol/resolve";
            case RM_TEXT_DOCUMENT_CODE_LENS                 : return "textDocument/codeLens";
            case RM_CODE_LENS_RESOLVE                       : return "codeLens/resolve";
            case RM_WORKSPACE_CODE_LENS_REFRESH             : return "workspace/codeLens/refresh";
            case RM_TEXT_DOCUMENT_DOCUMENT_LINK             : return "textDocument/documentLink";
            case RM_DOCUMENT_LINK_RESOLVE                   : return "documentLink/resolve";
            case RM_TEXT_DOCUMENT_FORMATTING                : return "textDocument/formatting";
            case RM_TEXT_DOCUMENT_RANGE_FORMATTING          : return "textDocument/rangeFormatting";
            case RM_TEXT_DOCUMENT_RANGES_FORMATTING         : return "textDocument/rangesFormatting";
            case RM_TEXT_DOCUMENT_ON_TYPE_FORMATTING        : return "textDocument/onTypeFormatting";
            case RM_TEXT_DOCUMENT_RENAME                    : return "textDocument/rename";
            case RM_TEXT_DOCUMENT_PREPARE_RENAME            : return "textDocument/prepareRename";
            case RM_WORKSPACE_EXECUTE_COMMAND               : return "workspace/executeCommand";
            case RM_WORKSPACE_APPLY_EDIT                    : return "workspace/applyEdit";
            case RM_WORKSPACE_DID_CHANGE_WORKSPACE_FOLDERS  : return "workspace/didChangeWorkspaceFolders";
            case RM_WINDOW_WORK_DONE_PROGRESS_CANCEL        : return "window/workDoneProgress/cancel";
            case RM_WORKSPACE_DID_CREATE_FILES              : return "workspace/didCreateFiles";
            case RM_WORKSPACE_DID_RENAME_FILES              : return "workspace/didRenameFiles";
            case RM_WORKSPACE_DID_DELETE_FILES              : return "workspace/didDeleteFiles";
            case RM_NOTEBOOK_DOCUMENT_DID_OPEN              : return "notebookDocument/didOpen";
            case RM_NOTEBOOK_DOCUMENT_DID_CHANGE            : return "notebookDocument/didChange";
            case RM_NOTEBOOK_DOCUMENT_DID_SAVE              : return "notebookDocument/didSave";
            case RM_NOTEBOOK_DOCUMENT_DID_CLOSE             : return "notebookDocument/didClose";
            case RM_INITIALIZED                             : return "initialized";
            case RM_EXIT                                    : return "exit";
            case RM_WORKSPACE_DID_CHANGE_CONFIGURATION      : return "workspace/didChangeConfiguration";
            case RM_WINDOW_SHOW_MESSAGE                     : return "window/showMessage";
            case RM_WINDOW_LOG_MESSAGE                      : return "window/logMessage";
            case RM_TELEMETRY_EVENT                         : return "telemetry/event";
            case RM_TEXT_DOCUMENT_DID_OPEN                  : return "textDocument/didOpen";
            case RM_TEXT_DOCUMENT_DID_CHANGE                : return "textDocument/didChange";
            case RM_TEXT_DOCUMENT_DID_CLOSE                 : return "textDocument/didClose";
            case RM_TEXT_DOCUMENT_DID_SAVE                  : return "textDocument/didSave";
            case RM_TEXT_DOCUMENT_WILL_SAVE                 : return "textDocument/willSave";
            case RM_WORKSPACE_DID_CHANGE_WATCHED_FILES      : return "workspace/didChangeWatchedFiles";
            case RM_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS       : return "textDocument/publishDiagnostics";
            case RM___SET_TRACE                             : return "$/setTrace";
            case RM___LOG_TRACE                             : return "$/logTrace";
            case RM___CANCEL_REQUEST                        : return "$/cancelRequest";
            case RM___PROGRESS                              : return "$/progress";
            default: return "";
        }
    }

    static RequestMethod toRequestMethod(JsonString val) {
        if (val.len < 0) return RM_NONE;
        uint32_t h = 0;
        for (int i = 0; i < val.len; i++) {
            h = (uint8_t)val.data[i] + (h << 6) + (h << 16) - h;
        }
        h ^= 257348622;
        h ^= (h >> 16);
        h *= 0x85ebca6bU;
        h ^= (h >> 13);
        h *= 0xc2b2ae35U;
        h ^= (h >> 16);
        uint32_t index = h % 256;
        return (RequestMethod) (index);
    }

    // Forward Declarations
    // ===

    struct TextDocumentPositionParams;
    struct ImplementationParams;
    struct Location;
    struct TextDocumentRegistrationOptions;
    struct ImplementationOptions;
    struct ImplementationRegistrationOptions;
    struct TypeDefinitionParams;
    struct TypeDefinitionOptions;
    struct TypeDefinitionRegistrationOptions;
    struct WorkspaceFolder;
    struct DidChangeWorkspaceFoldersParams;
    struct ConfigurationParams;
    struct DocumentColorParams;
    struct ColorInformation;
    struct DocumentColorOptions;
    struct DocumentColorRegistrationOptions;
    struct ColorPresentationParams;
    struct ColorPresentation;
    struct WorkDoneProgressOptions;
    struct FoldingRangeParams;
    struct FoldingRange;
    struct FoldingRangeOptions;
    struct FoldingRangeRegistrationOptions;
    struct DeclarationParams;
    struct DeclarationOptions;
    struct DeclarationRegistrationOptions;
    struct SelectionRangeParams;
    struct SelectionRange;
    struct SelectionRangeOptions;
    struct SelectionRangeRegistrationOptions;
    struct WorkDoneProgressCreateParams;
    struct WorkDoneProgressCancelParams;
    struct CallHierarchyPrepareParams;
    struct CallHierarchyItem;
    struct CallHierarchyOptions;
    struct CallHierarchyRegistrationOptions;
    struct CallHierarchyIncomingCallsParams;
    struct CallHierarchyIncomingCall;
    struct CallHierarchyOutgoingCallsParams;
    struct CallHierarchyOutgoingCall;
    struct SemanticTokensParams;
    struct SemanticTokens;
    struct SemanticTokensPartialResult;
    struct SemanticTokensOptions;
    struct SemanticTokensRegistrationOptions;
    struct SemanticTokensDeltaParams;
    struct SemanticTokensDelta;
    struct SemanticTokensDeltaPartialResult;
    struct SemanticTokensRangeParams;
    struct ShowDocumentParams;
    struct ShowDocumentResult;
    struct LinkedEditingRangeParams;
    struct LinkedEditingRanges;
    struct LinkedEditingRangeOptions;
    struct LinkedEditingRangeRegistrationOptions;
    struct CreateFilesParams;
    struct WorkspaceEdit;
    struct FileOperationRegistrationOptions;
    struct RenameFilesParams;
    struct DeleteFilesParams;
    struct MonikerParams;
    struct Moniker;
    struct MonikerOptions;
    struct MonikerRegistrationOptions;
    struct TypeHierarchyPrepareParams;
    struct TypeHierarchyItem;
    struct TypeHierarchyOptions;
    struct TypeHierarchyRegistrationOptions;
    struct TypeHierarchySupertypesParams;
    struct TypeHierarchySubtypesParams;
    struct InlineValueParams;
    struct InlineValueOptions;
    struct InlineValueRegistrationOptions;
    struct InlayHintParams;
    struct InlayHint;
    struct InlayHintOptions;
    struct InlayHintRegistrationOptions;
    struct DocumentDiagnosticParams;
    struct DocumentDiagnosticReportPartialResult;
    struct DiagnosticServerCancellationData;
    struct DiagnosticOptions;
    struct DiagnosticRegistrationOptions;
    struct WorkspaceDiagnosticParams;
    struct WorkspaceDiagnosticReport;
    struct WorkspaceDiagnosticReportPartialResult;
    struct DidOpenNotebookDocumentParams;
    struct DidChangeNotebookDocumentParams;
    struct DidSaveNotebookDocumentParams;
    struct DidCloseNotebookDocumentParams;
    struct InlineCompletionParams;
    struct InlineCompletionList;
    struct InlineCompletionItem;
    struct InlineCompletionOptions;
    struct InlineCompletionRegistrationOptions;
    struct RegistrationParams;
    struct UnregistrationParams;
    struct _InitializeParams;
    struct WorkspaceFoldersInitializeParams;
    struct InitializeParams;
    struct InitializeResult;
    struct InitializeError;
    struct InitializedParams;
    struct DidChangeConfigurationParams;
    struct DidChangeConfigurationRegistrationOptions;
    struct ShowMessageParams;
    struct ShowMessageRequestParams;
    struct MessageActionItem;
    struct LogMessageParams;
    struct DidOpenTextDocumentParams;
    struct DidChangeTextDocumentParams;
    struct TextDocumentChangeRegistrationOptions;
    struct DidCloseTextDocumentParams;
    struct DidSaveTextDocumentParams;
    struct SaveOptions;
    struct TextDocumentSaveRegistrationOptions;
    struct WillSaveTextDocumentParams;
    struct TextEdit;
    struct DidChangeWatchedFilesParams;
    struct DidChangeWatchedFilesRegistrationOptions;
    struct PublishDiagnosticsParams;
    struct CompletionParams;
    struct CompletionItem;
    struct CompletionList;
    struct CompletionOptions;
    struct CompletionRegistrationOptions;
    struct HoverParams;
    struct Hover;
    struct HoverOptions;
    struct HoverRegistrationOptions;
    struct SignatureHelpParams;
    struct SignatureHelp;
    struct SignatureHelpOptions;
    struct SignatureHelpRegistrationOptions;
    struct DefinitionParams;
    struct DefinitionOptions;
    struct DefinitionRegistrationOptions;
    struct ReferenceParams;
    struct ReferenceOptions;
    struct ReferenceRegistrationOptions;
    struct DocumentHighlightParams;
    struct DocumentHighlight;
    struct DocumentHighlightOptions;
    struct DocumentHighlightRegistrationOptions;
    struct DocumentSymbolParams;
    struct BaseSymbolInformation;
    struct SymbolInformation;
    struct DocumentSymbol;
    struct DocumentSymbolOptions;
    struct DocumentSymbolRegistrationOptions;
    struct CodeActionParams;
    struct Command;
    struct CodeAction;
    struct CodeActionOptions;
    struct CodeActionRegistrationOptions;
    struct WorkspaceSymbolParams;
    struct WorkspaceSymbol;
    struct WorkspaceSymbolOptions;
    struct WorkspaceSymbolRegistrationOptions;
    struct CodeLensParams;
    struct CodeLens;
    struct CodeLensOptions;
    struct CodeLensRegistrationOptions;
    struct DocumentLinkParams;
    struct DocumentLink;
    struct DocumentLinkOptions;
    struct DocumentLinkRegistrationOptions;
    struct DocumentFormattingParams;
    struct DocumentFormattingOptions;
    struct DocumentFormattingRegistrationOptions;
    struct DocumentRangeFormattingParams;
    struct DocumentRangeFormattingOptions;
    struct DocumentRangeFormattingRegistrationOptions;
    struct DocumentRangesFormattingParams;
    struct DocumentOnTypeFormattingParams;
    struct DocumentOnTypeFormattingOptions;
    struct DocumentOnTypeFormattingRegistrationOptions;
    struct RenameParams;
    struct RenameOptions;
    struct RenameRegistrationOptions;
    struct PrepareRenameParams;
    struct ExecuteCommandParams;
    struct ExecuteCommandOptions;
    struct ExecuteCommandRegistrationOptions;
    struct ApplyWorkspaceEditParams;
    struct ApplyWorkspaceEditResult;
    struct WorkDoneProgressBegin;
    struct WorkDoneProgressReport;
    struct WorkDoneProgressEnd;
    struct SetTraceParams;
    struct LogTraceParams;
    struct CancelParams;
    struct ProgressParams;
    struct WorkDoneProgressParams;
    struct PartialResultParams;
    struct LocationLink;
    struct Range;
    struct StaticRegistrationOptions;
    struct WorkspaceFoldersChangeEvent;
    struct ConfigurationItem;
    struct TextDocumentIdentifier;
    struct Color;
    struct Position;
    struct SemanticTokensEdit;
    struct FileCreate;
    struct TextDocumentEdit;
    struct ResourceOperation;
    struct CreateFile;
    struct RenameFile;
    struct DeleteFile;
    struct ChangeAnnotation;
    struct FileOperationFilter;
    struct FileRename;
    struct FileDelete;
    struct InlineValueContext;
    struct InlineValueText;
    struct InlineValueVariableLookup;
    struct InlineValueEvaluatableExpression;
    struct InlayHintLabelPart;
    struct MarkupContent;
    struct FullDocumentDiagnosticReport;
    struct RelatedFullDocumentDiagnosticReport;
    struct UnchangedDocumentDiagnosticReport;
    struct RelatedUnchangedDocumentDiagnosticReport;
    struct PreviousResultId;
    struct NotebookDocument;
    struct TextDocumentItem;
    struct VersionedNotebookDocumentIdentifier;
    struct NotebookDocumentChangeEvent;
    struct NotebookDocumentIdentifier;
    struct InlineCompletionContext;
    struct StringValue;
    struct Registration;
    struct Unregistration;
    struct ServerCapabilities;
    struct VersionedTextDocumentIdentifier;
    struct FileEvent;
    struct FileSystemWatcher;
    struct Diagnostic;
    struct CompletionContext;
    struct CompletionItemLabelDetails;
    struct InsertReplaceEdit;
    struct SignatureHelpContext;
    struct SignatureInformation;
    struct ReferenceContext;
    struct CodeActionContext;
    struct FormattingOptions;
    struct SemanticTokensLegend;
    struct OptionalVersionedTextDocumentIdentifier;
    struct AnnotatedTextEdit;
    struct CreateFileOptions;
    struct RenameFileOptions;
    struct DeleteFileOptions;
    struct FileOperationPattern;
    struct WorkspaceFullDocumentDiagnosticReport;
    struct WorkspaceUnchangedDocumentDiagnosticReport;
    struct NotebookCell;
    struct NotebookCellArrayChange;
    struct SelectedCompletionInfo;
    struct ClientCapabilities;
    struct TextDocumentSyncOptions;
    struct NotebookDocumentSyncOptions;
    struct NotebookDocumentSyncRegistrationOptions;
    struct WorkspaceFoldersServerCapabilities;
    struct FileOperationOptions;
    struct CodeDescription;
    struct DiagnosticRelatedInformation;
    struct ParameterInformation;
    struct NotebookCellTextDocumentFilter;
    struct FileOperationPatternOptions;
    struct ExecutionSummary;
    struct WorkspaceClientCapabilities;
    struct TextDocumentClientCapabilities;
    struct NotebookDocumentClientCapabilities;
    struct WindowClientCapabilities;
    struct GeneralClientCapabilities;
    struct RelativePattern;
    struct WorkspaceEditClientCapabilities;
    struct DidChangeConfigurationClientCapabilities;
    struct DidChangeWatchedFilesClientCapabilities;
    struct WorkspaceSymbolClientCapabilities;
    struct ExecuteCommandClientCapabilities;
    struct SemanticTokensWorkspaceClientCapabilities;
    struct CodeLensWorkspaceClientCapabilities;
    struct FileOperationClientCapabilities;
    struct InlineValueWorkspaceClientCapabilities;
    struct InlayHintWorkspaceClientCapabilities;
    struct DiagnosticWorkspaceClientCapabilities;
    struct FoldingRangeWorkspaceClientCapabilities;
    struct TextDocumentSyncClientCapabilities;
    struct CompletionClientCapabilities;
    struct HoverClientCapabilities;
    struct SignatureHelpClientCapabilities;
    struct DeclarationClientCapabilities;
    struct DefinitionClientCapabilities;
    struct TypeDefinitionClientCapabilities;
    struct ImplementationClientCapabilities;
    struct ReferenceClientCapabilities;
    struct DocumentHighlightClientCapabilities;
    struct DocumentSymbolClientCapabilities;
    struct CodeActionClientCapabilities;
    struct CodeLensClientCapabilities;
    struct DocumentLinkClientCapabilities;
    struct DocumentColorClientCapabilities;
    struct DocumentFormattingClientCapabilities;
    struct DocumentRangeFormattingClientCapabilities;
    struct DocumentOnTypeFormattingClientCapabilities;
    struct RenameClientCapabilities;
    struct FoldingRangeClientCapabilities;
    struct SelectionRangeClientCapabilities;
    struct PublishDiagnosticsClientCapabilities;
    struct CallHierarchyClientCapabilities;
    struct SemanticTokensClientCapabilities;
    struct LinkedEditingRangeClientCapabilities;
    struct MonikerClientCapabilities;
    struct TypeHierarchyClientCapabilities;
    struct InlineValueClientCapabilities;
    struct InlayHintClientCapabilities;
    struct DiagnosticClientCapabilities;
    struct InlineCompletionClientCapabilities;
    struct NotebookDocumentSyncClientCapabilities;
    struct ShowMessageRequestClientCapabilities;
    struct ShowDocumentClientCapabilities;
    struct RegularExpressionsClientCapabilities;
    struct MarkdownClientCapabilities;
    struct Definition;
    struct Declaration;
    struct InlineValue;
    struct DocumentDiagnosticReport;
    struct PrepareRenameResult;
    struct WorkspaceDocumentDiagnosticReport;
    struct TextDocumentContentChangeEvent;
    struct MarkedString;
    struct DocumentFilter;
    struct GlobPattern;
    struct TextDocumentFilter;
    struct NotebookDocumentFilter;

    enum SemanticTokenTypes {
        SEMANTIC_TOKEN_TYPES_NONE           = 0,
        SEMANTIC_TOKEN_TYPES_NAMESPACE      = 1,
        SEMANTIC_TOKEN_TYPES_TYPE           = 2,
        SEMANTIC_TOKEN_TYPES_CLASS          = 3,
        SEMANTIC_TOKEN_TYPES_ENUM           = 4,
        SEMANTIC_TOKEN_TYPES_INTERFACE      = 5,
        SEMANTIC_TOKEN_TYPES_STRUCT         = 6,
        SEMANTIC_TOKEN_TYPES_TYPE_PARAMETER = 7,
        SEMANTIC_TOKEN_TYPES_PARAMETER      = 8,
        SEMANTIC_TOKEN_TYPES_VARIABLE       = 9,
        SEMANTIC_TOKEN_TYPES_PROPERTY       = 10,
        SEMANTIC_TOKEN_TYPES_ENUM_MEMBER    = 11,
        SEMANTIC_TOKEN_TYPES_EVENT          = 12,
        SEMANTIC_TOKEN_TYPES_FUNCTION       = 13,
        SEMANTIC_TOKEN_TYPES_METHOD         = 14,
        SEMANTIC_TOKEN_TYPES_MACRO          = 15,
        SEMANTIC_TOKEN_TYPES_KEYWORD        = 16,
        SEMANTIC_TOKEN_TYPES_MODIFIER       = 17,
        SEMANTIC_TOKEN_TYPES_COMMENT        = 18,
        SEMANTIC_TOKEN_TYPES_STRING         = 19,
        SEMANTIC_TOKEN_TYPES_NUMBER         = 20,
        SEMANTIC_TOKEN_TYPES_REGEXP         = 21,
        SEMANTIC_TOKEN_TYPES_OPERATOR       = 22,
        SEMANTIC_TOKEN_TYPES_DECORATOR      = 23,
    };

    static const char* toString(SemanticTokenTypes val) {
        switch (val) {
            case SEMANTIC_TOKEN_TYPES_NAMESPACE     : return "namespace";
            case SEMANTIC_TOKEN_TYPES_TYPE          : return "type";
            case SEMANTIC_TOKEN_TYPES_CLASS         : return "class";
            case SEMANTIC_TOKEN_TYPES_ENUM          : return "enum";
            case SEMANTIC_TOKEN_TYPES_INTERFACE     : return "interface";
            case SEMANTIC_TOKEN_TYPES_STRUCT        : return "struct";
            case SEMANTIC_TOKEN_TYPES_TYPE_PARAMETER: return "typeParameter";
            case SEMANTIC_TOKEN_TYPES_PARAMETER     : return "parameter";
            case SEMANTIC_TOKEN_TYPES_VARIABLE      : return "variable";
            case SEMANTIC_TOKEN_TYPES_PROPERTY      : return "property";
            case SEMANTIC_TOKEN_TYPES_ENUM_MEMBER   : return "enumMember";
            case SEMANTIC_TOKEN_TYPES_EVENT         : return "event";
            case SEMANTIC_TOKEN_TYPES_FUNCTION      : return "function";
            case SEMANTIC_TOKEN_TYPES_METHOD        : return "method";
            case SEMANTIC_TOKEN_TYPES_MACRO         : return "macro";
            case SEMANTIC_TOKEN_TYPES_KEYWORD       : return "keyword";
            case SEMANTIC_TOKEN_TYPES_MODIFIER      : return "modifier";
            case SEMANTIC_TOKEN_TYPES_COMMENT       : return "comment";
            case SEMANTIC_TOKEN_TYPES_STRING        : return "string";
            case SEMANTIC_TOKEN_TYPES_NUMBER        : return "number";
            case SEMANTIC_TOKEN_TYPES_REGEXP        : return "regexp";
            case SEMANTIC_TOKEN_TYPES_OPERATOR      : return "operator";
            case SEMANTIC_TOKEN_TYPES_DECORATOR     : return "decorator";
            default: return "";
        }
    }

    enum SemanticTokenModifiers {
        SEMANTIC_TOKEN_MODIFIERS_NONE            = 0,
        SEMANTIC_TOKEN_MODIFIERS_DECLARATION     = 1,
        SEMANTIC_TOKEN_MODIFIERS_DEFINITION      = 2,
        SEMANTIC_TOKEN_MODIFIERS_READONLY        = 3,
        SEMANTIC_TOKEN_MODIFIERS_STATIC          = 4,
        SEMANTIC_TOKEN_MODIFIERS_DEPRECATED      = 5,
        SEMANTIC_TOKEN_MODIFIERS_ABSTRACT        = 6,
        SEMANTIC_TOKEN_MODIFIERS_ASYNC           = 7,
        SEMANTIC_TOKEN_MODIFIERS_MODIFICATION    = 8,
        SEMANTIC_TOKEN_MODIFIERS_DOCUMENTATION   = 9,
        SEMANTIC_TOKEN_MODIFIERS_DEFAULT_LIBRARY = 10,
    };

    static const char* toString(SemanticTokenModifiers val) {
        switch (val) {
            case SEMANTIC_TOKEN_MODIFIERS_DECLARATION    : return "declaration";
            case SEMANTIC_TOKEN_MODIFIERS_DEFINITION     : return "definition";
            case SEMANTIC_TOKEN_MODIFIERS_READONLY       : return "readonly";
            case SEMANTIC_TOKEN_MODIFIERS_STATIC         : return "static";
            case SEMANTIC_TOKEN_MODIFIERS_DEPRECATED     : return "deprecated";
            case SEMANTIC_TOKEN_MODIFIERS_ABSTRACT       : return "abstract";
            case SEMANTIC_TOKEN_MODIFIERS_ASYNC          : return "async";
            case SEMANTIC_TOKEN_MODIFIERS_MODIFICATION   : return "modification";
            case SEMANTIC_TOKEN_MODIFIERS_DOCUMENTATION  : return "documentation";
            case SEMANTIC_TOKEN_MODIFIERS_DEFAULT_LIBRARY: return "defaultLibrary";
            default: return "";
        }
    }

    enum DocumentDiagnosticReportKind {
        DOCUMENT_DIAGNOSTIC_REPORT_KIND_NONE      = 0,
        DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL      = 1,
        DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED = 2,
    };

    static const char* toString(DocumentDiagnosticReportKind val) {
        switch (val) {
            case DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL     : return "full";
            case DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED: return "unchanged";
            default: return "";
        }
    }

    enum ErrorCodes {
        ERROR_CODES_NONE                   = 0,
        ERROR_CODES_PARSE_ERROR            = -32700,
        ERROR_CODES_INVALID_REQUEST        = -32600,
        ERROR_CODES_METHOD_NOT_FOUND       = -32601,
        ERROR_CODES_INVALID_PARAMS         = -32602,
        ERROR_CODES_INTERNAL_ERROR         = -32603,
        ERROR_CODES_SERVER_NOT_INITIALIZED = -32002,
        ERROR_CODES_UNKNOWN_ERROR_CODE     = -32001,
    };

    enum LSPErrorCodes {
        LSP_ERROR_CODES_NONE              = 0,
        LSP_ERROR_CODES_REQUEST_FAILED    = -32803,
        LSP_ERROR_CODES_SERVER_CANCELLED  = -32802,
        LSP_ERROR_CODES_CONTENT_MODIFIED  = -32801,
        LSP_ERROR_CODES_REQUEST_CANCELLED = -32800,
    };

    enum FoldingRangeKind {
        FOLDING_RANGE_KIND_NONE    = 0,
        FOLDING_RANGE_KIND_COMMENT = 1,
        FOLDING_RANGE_KIND_IMPORTS = 2,
        FOLDING_RANGE_KIND_REGION  = 3,
    };

    static const char* toString(FoldingRangeKind val) {
        switch (val) {
            case FOLDING_RANGE_KIND_COMMENT: return "comment";
            case FOLDING_RANGE_KIND_IMPORTS: return "imports";
            case FOLDING_RANGE_KIND_REGION : return "region";
            default: return "";
        }
    }

    enum SymbolKind {
        SYMBOL_KIND_NONE           = 0,
        SYMBOL_KIND_FILE           = 1,
        SYMBOL_KIND_MODULE         = 2,
        SYMBOL_KIND_NAMESPACE      = 3,
        SYMBOL_KIND_PACKAGE        = 4,
        SYMBOL_KIND_CLASS          = 5,
        SYMBOL_KIND_METHOD         = 6,
        SYMBOL_KIND_PROPERTY       = 7,
        SYMBOL_KIND_FIELD          = 8,
        SYMBOL_KIND_CONSTRUCTOR    = 9,
        SYMBOL_KIND_ENUM           = 10,
        SYMBOL_KIND_INTERFACE      = 11,
        SYMBOL_KIND_FUNCTION       = 12,
        SYMBOL_KIND_VARIABLE       = 13,
        SYMBOL_KIND_CONSTANT       = 14,
        SYMBOL_KIND_STRING         = 15,
        SYMBOL_KIND_NUMBER         = 16,
        SYMBOL_KIND_BOOLEAN        = 17,
        SYMBOL_KIND_ARRAY          = 18,
        SYMBOL_KIND_OBJECT         = 19,
        SYMBOL_KIND_KEY            = 20,
        SYMBOL_KIND_NULL           = 21,
        SYMBOL_KIND_ENUM_MEMBER    = 22,
        SYMBOL_KIND_STRUCT         = 23,
        SYMBOL_KIND_EVENT          = 24,
        SYMBOL_KIND_OPERATOR       = 25,
        SYMBOL_KIND_TYPE_PARAMETER = 26,
    };

    enum SymbolTag {
        SYMBOL_TAG_NONE       = 0,
        SYMBOL_TAG_DEPRECATED = 1,
    };

    enum UniquenessLevel {
        UNIQUENESS_LEVEL_NONE     = 0,
        UNIQUENESS_LEVEL_DOCUMENT = 1,
        UNIQUENESS_LEVEL_PROJECT  = 2,
        UNIQUENESS_LEVEL_GROUP    = 3,
        UNIQUENESS_LEVEL_SCHEME   = 4,
        UNIQUENESS_LEVEL_GLOBAL   = 5,
    };

    static const char* toString(UniquenessLevel val) {
        switch (val) {
            case UNIQUENESS_LEVEL_DOCUMENT: return "document";
            case UNIQUENESS_LEVEL_PROJECT : return "project";
            case UNIQUENESS_LEVEL_GROUP   : return "group";
            case UNIQUENESS_LEVEL_SCHEME  : return "scheme";
            case UNIQUENESS_LEVEL_GLOBAL  : return "global";
            default: return "";
        }
    }

    enum MonikerKind {
        MONIKER_KIND_NONE   = 0,
        MONIKER_KIND_IMPORT = 1,
        MONIKER_KIND_EXPORT = 2,
        MONIKER_KIND_LOCAL  = 3,
    };

    static const char* toString(MonikerKind val) {
        switch (val) {
            case MONIKER_KIND_IMPORT: return "import";
            case MONIKER_KIND_EXPORT: return "export";
            case MONIKER_KIND_LOCAL : return "local";
            default: return "";
        }
    }

    enum InlayHintKind {
        INLAY_HINT_KIND_NONE      = 0,
        INLAY_HINT_KIND_TYPE      = 1,
        INLAY_HINT_KIND_PARAMETER = 2,
    };

    enum MessageType {
        MESSAGE_TYPE_NONE    = 0,
        MESSAGE_TYPE_ERROR   = 1,
        MESSAGE_TYPE_WARNING = 2,
        MESSAGE_TYPE_INFO    = 3,
        MESSAGE_TYPE_LOG     = 4,
        MESSAGE_TYPE_DEBUG   = 5,
    };

    enum TextDocumentSyncKind {
        TEXT_DOCUMENT_SYNC_KIND_NONE        = 0,
        TEXT_DOCUMENT_SYNC_KIND_FULL        = 1,
        TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL = 2,
    };

    enum TextDocumentSaveReason {
        TEXT_DOCUMENT_SAVE_REASON_NONE        = 0,
        TEXT_DOCUMENT_SAVE_REASON_MANUAL      = 1,
        TEXT_DOCUMENT_SAVE_REASON_AFTER_DELAY = 2,
        TEXT_DOCUMENT_SAVE_REASON_FOCUS_OUT   = 3,
    };

    enum CompletionItemKind {
        COMPLETION_ITEM_KIND_NONE           = 0,
        COMPLETION_ITEM_KIND_TEXT           = 1,
        COMPLETION_ITEM_KIND_METHOD         = 2,
        COMPLETION_ITEM_KIND_FUNCTION       = 3,
        COMPLETION_ITEM_KIND_CONSTRUCTOR    = 4,
        COMPLETION_ITEM_KIND_FIELD          = 5,
        COMPLETION_ITEM_KIND_VARIABLE       = 6,
        COMPLETION_ITEM_KIND_CLASS          = 7,
        COMPLETION_ITEM_KIND_INTERFACE      = 8,
        COMPLETION_ITEM_KIND_MODULE         = 9,
        COMPLETION_ITEM_KIND_PROPERTY       = 10,
        COMPLETION_ITEM_KIND_UNIT           = 11,
        COMPLETION_ITEM_KIND_VALUE          = 12,
        COMPLETION_ITEM_KIND_ENUM           = 13,
        COMPLETION_ITEM_KIND_KEYWORD        = 14,
        COMPLETION_ITEM_KIND_SNIPPET        = 15,
        COMPLETION_ITEM_KIND_COLOR          = 16,
        COMPLETION_ITEM_KIND_FILE           = 17,
        COMPLETION_ITEM_KIND_REFERENCE      = 18,
        COMPLETION_ITEM_KIND_FOLDER         = 19,
        COMPLETION_ITEM_KIND_ENUM_MEMBER    = 20,
        COMPLETION_ITEM_KIND_CONSTANT       = 21,
        COMPLETION_ITEM_KIND_STRUCT         = 22,
        COMPLETION_ITEM_KIND_EVENT          = 23,
        COMPLETION_ITEM_KIND_OPERATOR       = 24,
        COMPLETION_ITEM_KIND_TYPE_PARAMETER = 25,
    };

    enum CompletionItemTag {
        COMPLETION_ITEM_TAG_NONE       = 0,
        COMPLETION_ITEM_TAG_DEPRECATED = 1,
    };

    enum InsertTextFormat {
        INSERT_TEXT_FORMAT_NONE       = 0,
        INSERT_TEXT_FORMAT_PLAIN_TEXT = 1,
        INSERT_TEXT_FORMAT_SNIPPET    = 2,
    };

    enum InsertTextMode {
        INSERT_TEXT_MODE_NONE               = 0,
        INSERT_TEXT_MODE_AS_IS              = 1,
        INSERT_TEXT_MODE_ADJUST_INDENTATION = 2,
    };

    enum DocumentHighlightKind {
        DOCUMENT_HIGHLIGHT_KIND_NONE  = 0,
        DOCUMENT_HIGHLIGHT_KIND_TEXT  = 1,
        DOCUMENT_HIGHLIGHT_KIND_READ  = 2,
        DOCUMENT_HIGHLIGHT_KIND_WRITE = 3,
    };

    enum CodeActionKind {
        CODE_ACTION_KIND_NONE                    = 0,
        CODE_ACTION_KIND_EMPTY                   = 1,
        CODE_ACTION_KIND_QUICK_FIX               = 2,
        CODE_ACTION_KIND_REFACTOR                = 3,
        CODE_ACTION_KIND_REFACTOR_EXTRACT        = 4,
        CODE_ACTION_KIND_REFACTOR_INLINE         = 5,
        CODE_ACTION_KIND_REFACTOR_REWRITE        = 6,
        CODE_ACTION_KIND_SOURCE                  = 7,
        CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS = 8,
        CODE_ACTION_KIND_SOURCE_FIX_ALL          = 9,
    };

    static const char* toString(CodeActionKind val) {
        switch (val) {
            case CODE_ACTION_KIND_EMPTY                  : return "";
			case CODE_ACTION_KIND_QUICK_FIX              : return "quickfix";
			case CODE_ACTION_KIND_REFACTOR               : return "refactor";
			case CODE_ACTION_KIND_REFACTOR_EXTRACT       : return "refactor.extract";
			case CODE_ACTION_KIND_REFACTOR_INLINE        : return "refactor.inline";
			case CODE_ACTION_KIND_REFACTOR_REWRITE       : return "refactor.rewrite";
			case CODE_ACTION_KIND_SOURCE                 : return "source";
			case CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS: return "source.organizeImports";
			case CODE_ACTION_KIND_SOURCE_FIX_ALL         : return "source.fixAll";
			default: return "";
        }
    }

    enum TraceValues {
        TRACE_VALUES_NONE     = 0,
        TRACE_VALUES_OFF      = 1,
        TRACE_VALUES_MESSAGES = 2,
        TRACE_VALUES_VERBOSE  = 3,
    };

    static const char* toString(TraceValues val) {
        switch (val) {
            case TRACE_VALUES_OFF     : return "off";
            case TRACE_VALUES_MESSAGES: return "messages";
            case TRACE_VALUES_VERBOSE : return "verbose";
            default: return "";
        }
    }

    enum MarkupKind {
        MARKUP_KIND_NONE       = 0,
        MARKUP_KIND_PLAIN_TEXT = 1,
        MARKUP_KIND_MARKDOWN   = 2,
    };

    static const char* toString(MarkupKind val) {
        switch (val) {
            case MARKUP_KIND_PLAIN_TEXT: return "plaintext";
            case MARKUP_KIND_MARKDOWN  : return "markdown";
            default: return "";
        }
    }

    enum InlineCompletionTriggerKind {
        INLINE_COMPLETION_TRIGGER_KIND_INVOKED   = 0,
        INLINE_COMPLETION_TRIGGER_KIND_AUTOMATIC = 1,
    };

    enum PositionEncodingKind {
        POSITION_ENCODING_KIND_NONE  = 0,
        POSITION_ENCODING_KIND_UTF8  = 1,
        POSITION_ENCODING_KIND_UTF16 = 2,
        POSITION_ENCODING_KIND_UTF32 = 3,
    };

    static const char* toString(PositionEncodingKind val) {
        switch (val) {
            case POSITION_ENCODING_KIND_UTF8 : return "utf-8";
            case POSITION_ENCODING_KIND_UTF16: return "utf-16";
            case POSITION_ENCODING_KIND_UTF32: return "utf-32";
            default: return "";
        }
    }

    enum FileChangeType {
        FILE_CHANGE_TYPE_NONE    = 0,
        FILE_CHANGE_TYPE_CREATED = 1,
        FILE_CHANGE_TYPE_CHANGED = 2,
        FILE_CHANGE_TYPE_DELETED = 3,
    };

    enum WatchKind {
        WATCH_KIND_NONE   = 0,
        WATCH_KIND_CREATE = 1,
        WATCH_KIND_CHANGE = 2,
        WATCH_KIND_DELETE = 4,
    };

    enum DiagnosticSeverity {
        DIAGNOSTIC_SEVERITY_NONE        = 0,
        DIAGNOSTIC_SEVERITY_ERROR       = 1,
        DIAGNOSTIC_SEVERITY_WARNING     = 2,
        DIAGNOSTIC_SEVERITY_INFORMATION = 3,
        DIAGNOSTIC_SEVERITY_HINT        = 4,
    };

    enum DiagnosticTag {
        DIAGNOSTIC_TAG_NONE        = 0,
        DIAGNOSTIC_TAG_UNNECESSARY = 1,
        DIAGNOSTIC_TAG_DEPRECATED  = 2,
    };

    enum CompletionTriggerKind {
        COMPLETION_TRIGGER_KIND_NONE                               = 0,
        COMPLETION_TRIGGER_KIND_INVOKED                            = 1,
        COMPLETION_TRIGGER_KIND_TRIGGER_CHARACTER                  = 2,
        COMPLETION_TRIGGER_KIND_TRIGGER_FOR_INCOMPLETE_COMPLETIONS = 3,
    };

    enum SignatureHelpTriggerKind {
        SIGNATURE_HELP_TRIGGER_KIND_NONE              = 0,
        SIGNATURE_HELP_TRIGGER_KIND_INVOKED           = 1,
        SIGNATURE_HELP_TRIGGER_KIND_TRIGGER_CHARACTER = 2,
        SIGNATURE_HELP_TRIGGER_KIND_CONTENT_CHANGE    = 3,
    };

    enum CodeActionTriggerKind {
        CODE_ACTION_TRIGGER_KIND_NONE      = 0,
        CODE_ACTION_TRIGGER_KIND_INVOKED   = 1,
        CODE_ACTION_TRIGGER_KIND_AUTOMATIC = 2,
    };

    enum FileOperationPatternKind {
        FILE_OPERATION_PATTERN_KIND_NONE   = 0,
        FILE_OPERATION_PATTERN_KIND_FILE   = 1,
        FILE_OPERATION_PATTERN_KIND_FOLDER = 2,
    };

    static const char* toString(FileOperationPatternKind val) {
        switch (val) {
            case FILE_OPERATION_PATTERN_KIND_FILE  : return "file";
            case FILE_OPERATION_PATTERN_KIND_FOLDER: return "folder";
            default: return "";
        }
    }

    enum NotebookCellKind {
        NOTEBOOK_CELL_KIND_NONE   = 0,
        NOTEBOOK_CELL_KIND_MARKUP = 1,
        NOTEBOOK_CELL_KIND_CODE   = 2,
    };

    enum ResourceOperationKind {
        RESOURCE_OPERATION_KIND_NONE   = 0,
        RESOURCE_OPERATION_KIND_CREATE = 1,
        RESOURCE_OPERATION_KIND_RENAME = 2,
        RESOURCE_OPERATION_KIND_DELETE = 3,
    };

    static const char* toString(ResourceOperationKind val) {
        switch (val) {
            case RESOURCE_OPERATION_KIND_CREATE: return "create";
            case RESOURCE_OPERATION_KIND_RENAME: return "rename";
            case RESOURCE_OPERATION_KIND_DELETE: return "delete";
            default: return "";
        }
    }

    enum FailureHandlingKind {
        FAILURE_HANDLING_KIND_NONE                    = 0,
        FAILURE_HANDLING_KIND_ABORT                   = 1,
        FAILURE_HANDLING_KIND_TRANSACTIONAL           = 2,
        FAILURE_HANDLING_KIND_TEXT_ONLY_TRANSACTIONAL = 3,
        FAILURE_HANDLING_KIND_UNDO                    = 4,
    };

    static const char* toString(FailureHandlingKind val) {
        switch (val) {
            case FAILURE_HANDLING_KIND_ABORT                  : return "abort";
            case FAILURE_HANDLING_KIND_TRANSACTIONAL          : return "transactional";
            case FAILURE_HANDLING_KIND_TEXT_ONLY_TRANSACTIONAL: return "textOnlyTransactional";
            case FAILURE_HANDLING_KIND_UNDO                   : return "undo";
            default: return "";
        }
    }

    enum PrepareSupportDefaultBehavior {
        PREPARE_SUPPORT_DEFAULT_BEHAVIOR_NONE       = 0,
        PREPARE_SUPPORT_DEFAULT_BEHAVIOR_IDENTIFIER = 1,
    };

    enum TokenFormat {
        TOKEN_FORMAT_NONE     = 0,
        TOKEN_FORMAT_RELATIVE = 1,
    };

    static const char* toString(TokenFormat val) {
        switch (val) {
            case TOKEN_FORMAT_RELATIVE: return "relative";
            default: return "";
        }
    }

    /**
     * A parameter literal used in requests to pass a text document and a position inside that
     * document.
     */
    struct TextDocumentPositionParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The position inside the text document.
         */
        Position*                position;
    };

    template<> struct Schema<TextDocumentPositionParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &TextDocumentPositionParams::textDocument),
            make_field("position",     &TextDocumentPositionParams::position)
        );
    };

    struct ImplementationParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<ImplementationParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &ImplementationParams::textDocument),
            make_field("position",     &ImplementationParams::position)
        );
    };

    /**
     * Represents a location inside a resource, such as a line
     * inside a text file.
     */
    struct Location {
        String  uri;
        Range*  range;
    };

    template<> struct Schema<Location> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",   &Location::uri),
            make_field("range", &Location::range)
        );
    };

    /**
     * General text document registration options.
     */
    struct TextDocumentRegistrationOptions {
        /**
         * A document selector to identify the scope of the registration. If set to null
         * the document selector provided on the client side will be used.
         */
        Slice<DocumentFilter*>  documentSelector;
    };

    template<> struct Schema<TextDocumentRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &TextDocumentRegistrationOptions::documentSelector)
        );
    };

    struct ImplementationOptions {
    };

    template<> struct Schema<ImplementationOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    struct ImplementationRegistrationOptions : public TextDocumentRegistrationOptions, public ImplementationOptions {
    };

    template<> struct Schema<ImplementationRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &ImplementationRegistrationOptions::documentSelector)
        );
    };

    struct TypeDefinitionParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<TypeDefinitionParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &TypeDefinitionParams::textDocument),
            make_field("position",     &TypeDefinitionParams::position)
        );
    };

    struct TypeDefinitionOptions {
    };

    template<> struct Schema<TypeDefinitionOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    struct TypeDefinitionRegistrationOptions : public TextDocumentRegistrationOptions, public TypeDefinitionOptions {
    };

    template<> struct Schema<TypeDefinitionRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &TypeDefinitionRegistrationOptions::documentSelector)
        );
    };

    /**
     * A workspace folder inside a client.
     */
    struct WorkspaceFolder {
        /**
         * The associated URI for this workspace folder.
         */
        String  uri;
        /**
         * The name of the workspace folder. Used to refer to this
         * workspace folder in the user interface.
         */
        String  name;
    };

    template<> struct Schema<WorkspaceFolder> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",  &WorkspaceFolder::uri),
            make_field("name", &WorkspaceFolder::name)
        );
    };

    /**
     * The parameters of a `workspace/didChangeWorkspaceFolders` notification.
     */
    struct DidChangeWorkspaceFoldersParams {
        /**
         * The actual workspace folder change event.
         */
        WorkspaceFoldersChangeEvent*  event;
    };

    template<> struct Schema<DidChangeWorkspaceFoldersParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("event", &DidChangeWorkspaceFoldersParams::event)
        );
    };

    /**
     * The parameters of a configuration request.
     */
    struct ConfigurationParams {
        Slice<ConfigurationItem*>  items;
    };

    template<> struct Schema<ConfigurationParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("items", &ConfigurationParams::items)
        );
    };

    /**
     * Parameters for a {@link DocumentColorRequest}.
     */
    struct DocumentColorParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
    };

    template<> struct Schema<DocumentColorParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DocumentColorParams::textDocument)
        );
    };

    /**
     * Represents a color range from a document.
     */
    struct ColorInformation {
        /**
         * The range in the document where this color appears.
         */
        Range*  range;
        /**
         * The actual color value for this color range.
         */
        Color*  color;
    };

    template<> struct Schema<ColorInformation> {
        static constexpr auto fields = std::make_tuple(
            make_field("range", &ColorInformation::range),
            make_field("color", &ColorInformation::color)
        );
    };

    struct DocumentColorOptions {
    };

    template<> struct Schema<DocumentColorOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    struct DocumentColorRegistrationOptions : public TextDocumentRegistrationOptions, public DocumentColorOptions {
    };

    template<> struct Schema<DocumentColorRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &DocumentColorRegistrationOptions::documentSelector)
        );
    };

    /**
     * Parameters for a {@link ColorPresentationRequest}.
     */
    struct ColorPresentationParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The color to request presentations for.
         */
        Color*                   color;
        /**
         * The range where the color would be inserted. Serves as a context.
         */
        Range*                   range;
    };

    template<> struct Schema<ColorPresentationParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &ColorPresentationParams::textDocument),
            make_field("color",        &ColorPresentationParams::color),
            make_field("range",        &ColorPresentationParams::range)
        );
    };

    struct ColorPresentation {
        /**
         * The label of this color presentation. It will be shown on the color
         * picker header. By default this is also the text that is inserted when selecting
         * this color presentation.
         */
        String            label;
        /**
         * An {@link TextEdit edit} which is applied to a document when selecting
         * this presentation for the color.  When `falsy` the {@link ColorPresentation.label label}
         * is used.
         */
        TextEdit*         textEdit;
        /**
         * An optional array of additional {@link TextEdit text edits} that are applied when
         * selecting this color presentation. Edits must not overlap with the main {@link ColorPresentation.textEdit edit} nor with themselves.
         */
        Slice<TextEdit*>  additionalTextEdits;
    };

    template<> struct Schema<ColorPresentation> {
        static constexpr auto fields = std::make_tuple(
            make_field("label",               &ColorPresentation::label),
            make_field("textEdit",            &ColorPresentation::textEdit),
            make_field("additionalTextEdits", &ColorPresentation::additionalTextEdits)
        );
    };

    struct WorkDoneProgressOptions {
        Bool  workDoneProgress;
    };

    template<> struct Schema<WorkDoneProgressOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("workDoneProgress", &WorkDoneProgressOptions::workDoneProgress)
        );
    };

    /**
     * Parameters for a {@link FoldingRangeRequest}.
     */
    struct FoldingRangeParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
    };

    template<> struct Schema<FoldingRangeParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &FoldingRangeParams::textDocument)
        );
    };

    /**
     * Represents a folding range. To be valid, start and end line must be bigger than zero and smaller
     * than the number of lines in the document. Clients are free to ignore invalid ranges.
     */
    struct FoldingRange {
        /**
         * The zero-based start line of the range to fold. The folded area starts after the line's last character.
         * To be valid, the end must be zero or larger and smaller than the number of lines in the document.
         */
        UInt              startLine;
        /**
         * The zero-based character offset from where the folded range starts. If not defined, defaults to the length of the start line.
         */
        UInt              startCharacter;
        /**
         * The zero-based end line of the range to fold. The folded area ends with the line's last character.
         * To be valid, the end must be zero or larger and smaller than the number of lines in the document.
         */
        UInt              endLine;
        /**
         * The zero-based character offset before the folded range ends. If not defined, defaults to the length of the end line.
         */
        UInt              endCharacter;
        /**
         * Describes the kind of the folding range such as `comment' or 'region'. The kind
         * is used to categorize folding ranges and used by commands like 'Fold all comments'.
         * See {@link FoldingRangeKind} for an enumeration of standardized kinds.
         */
        FoldingRangeKind  kind;
        /**
         * The text that the client should show when the specified range is
         * collapsed. If not defined or not supported by the client, a default
         * will be chosen by the client.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        String            collapsedText;
    };

    template<> struct Schema<FoldingRange> {
        static constexpr auto fields = std::make_tuple(
            make_field("startLine",      &FoldingRange::startLine),
            make_field("startCharacter", &FoldingRange::startCharacter),
            make_field("endLine",        &FoldingRange::endLine),
            make_field("endCharacter",   &FoldingRange::endCharacter),
            make_field("kind",           &FoldingRange::kind),
            make_field("collapsedText",  &FoldingRange::collapsedText)
        );
    };

    struct FoldingRangeOptions {
    };

    template<> struct Schema<FoldingRangeOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    struct FoldingRangeRegistrationOptions : public TextDocumentRegistrationOptions, public FoldingRangeOptions {
    };

    template<> struct Schema<FoldingRangeRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &FoldingRangeRegistrationOptions::documentSelector)
        );
    };

    struct DeclarationParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<DeclarationParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DeclarationParams::textDocument),
            make_field("position",     &DeclarationParams::position)
        );
    };

    struct DeclarationOptions {
    };

    template<> struct Schema<DeclarationOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    struct DeclarationRegistrationOptions : public DeclarationOptions, public TextDocumentRegistrationOptions {
    };

    template<> struct Schema<DeclarationRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &DeclarationRegistrationOptions::documentSelector)
        );
    };

    /**
     * A parameter literal used in selection range requests.
     */
    struct SelectionRangeParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The positions inside the text document.
         */
        Slice<Position*>         positions;
    };

    template<> struct Schema<SelectionRangeParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &SelectionRangeParams::textDocument),
            make_field("positions",    &SelectionRangeParams::positions)
        );
    };

    /**
     * A selection range represents a part of a selection hierarchy. A selection range
     * may have a parent selection range that contains it.
     */
    struct SelectionRange {
        /**
         * The {@link Range range} of this selection range.
         */
        Range*           range;
        /**
         * The parent selection range containing this range. Therefore `parent.range` must contain `this.range`.
         */
        SelectionRange*  parent;
    };

    template<> struct Schema<SelectionRange> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",  &SelectionRange::range),
            make_field("parent", &SelectionRange::parent)
        );
    };

    struct SelectionRangeOptions {
    };

    template<> struct Schema<SelectionRangeOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    struct SelectionRangeRegistrationOptions : public SelectionRangeOptions, public TextDocumentRegistrationOptions {
    };

    template<> struct Schema<SelectionRangeRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &SelectionRangeRegistrationOptions::documentSelector)
        );
    };

    struct WorkDoneProgressCreateParams {
        /**
         * The token to be used to report progress.
         */
        String  token;
    };

    template<> struct Schema<WorkDoneProgressCreateParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("token", &WorkDoneProgressCreateParams::token)
        );
    };

    struct WorkDoneProgressCancelParams {
        /**
         * The token to be used to report progress.
         */
        String  token;
    };

    template<> struct Schema<WorkDoneProgressCancelParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("token", &WorkDoneProgressCancelParams::token)
        );
    };

    /**
     * The parameter of a `textDocument/prepareCallHierarchy` request.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CallHierarchyPrepareParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<CallHierarchyPrepareParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &CallHierarchyPrepareParams::textDocument),
            make_field("position",     &CallHierarchyPrepareParams::position)
        );
    };

    /**
     * Represents programming constructs like functions or constructors in the context
     * of call hierarchy.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CallHierarchyItem {
        /**
         * The name of this item.
         */
        String            name;
        /**
         * The kind of this item.
         */
        SymbolKind        kind;
        /**
         * Tags for this item.
         */
        Slice<SymbolTag>  tags;
        /**
         * More detail for this item, e.g. the signature of a function.
         */
        String            detail;
        /**
         * The resource identifier of this item.
         */
        String            uri;
        /**
         * The range enclosing this symbol not including leading/trailing whitespace but everything else, e.g. comments and code.
         */
        Range*            range;
        /**
         * The range that should be selected and revealed when this symbol is being picked, e.g. the name of a function.
         * Must be contained by the {@link CallHierarchyItem.range `range`}.
         */
        Range*            selectionRange;
        /**
         * A data entry field that is preserved between a call hierarchy prepare and
         * incoming calls or outgoing calls requests.
         */
        String            data;
    };

    template<> struct Schema<CallHierarchyItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("name",           &CallHierarchyItem::name),
            make_field("kind",           &CallHierarchyItem::kind),
            make_field("tags",           &CallHierarchyItem::tags),
            make_field("detail",         &CallHierarchyItem::detail),
            make_field("uri",            &CallHierarchyItem::uri),
            make_field("range",          &CallHierarchyItem::range),
            make_field("selectionRange", &CallHierarchyItem::selectionRange),
            make_field("data",           &CallHierarchyItem::data)
        );
    };

    /**
     * Call hierarchy options used during static registration.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CallHierarchyOptions {
    };

    template<> struct Schema<CallHierarchyOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * Call hierarchy options used during static or dynamic registration.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CallHierarchyRegistrationOptions : public TextDocumentRegistrationOptions, public CallHierarchyOptions {
    };

    template<> struct Schema<CallHierarchyRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &CallHierarchyRegistrationOptions::documentSelector)
        );
    };

    /**
     * The parameter of a `callHierarchy/incomingCalls` request.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CallHierarchyIncomingCallsParams {
        CallHierarchyItem*  item;
    };

    template<> struct Schema<CallHierarchyIncomingCallsParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("item", &CallHierarchyIncomingCallsParams::item)
        );
    };

    /**
     * Represents an incoming call, e.g. a caller of a method or constructor.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CallHierarchyIncomingCall {
        /**
         * The item that makes the call.
         */
        CallHierarchyItem*  from;
        /**
         * The ranges at which the calls appear. This is relative to the caller
         * denoted by {@link CallHierarchyIncomingCall.from `this.from`}.
         */
        Slice<Range*>       fromRanges;
    };

    template<> struct Schema<CallHierarchyIncomingCall> {
        static constexpr auto fields = std::make_tuple(
            make_field("from",       &CallHierarchyIncomingCall::from),
            make_field("fromRanges", &CallHierarchyIncomingCall::fromRanges)
        );
    };

    /**
     * The parameter of a `callHierarchy/outgoingCalls` request.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CallHierarchyOutgoingCallsParams {
        CallHierarchyItem*  item;
    };

    template<> struct Schema<CallHierarchyOutgoingCallsParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("item", &CallHierarchyOutgoingCallsParams::item)
        );
    };

    /**
     * Represents an outgoing call, e.g. calling a getter from a method or a method from a constructor etc.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CallHierarchyOutgoingCall {
        /**
         * The item that is called.
         */
        CallHierarchyItem*  to;
        /**
         * The range at which this item is called. This is the range relative to the caller, e.g the item
         * passed to {@link CallHierarchyItemProvider.provideCallHierarchyOutgoingCalls `provideCallHierarchyOutgoingCalls`}
         * and not {@link CallHierarchyOutgoingCall.to `this.to`}.
         */
        Slice<Range*>       fromRanges;
    };

    template<> struct Schema<CallHierarchyOutgoingCall> {
        static constexpr auto fields = std::make_tuple(
            make_field("to",         &CallHierarchyOutgoingCall::to),
            make_field("fromRanges", &CallHierarchyOutgoingCall::fromRanges)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
    };

    template<> struct Schema<SemanticTokensParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &SemanticTokensParams::textDocument)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokens {
        /**
         * An optional result id. If provided and clients support delta updating
         * the client will include the result id in the next semantic token request.
         * A server can then instead of computing all semantic tokens again simply
         * send a delta.
         */
        String       resultId;
        /**
         * The actual tokens.
         */
        Slice<UInt>  data;
    };

    template<> struct Schema<SemanticTokens> {
        static constexpr auto fields = std::make_tuple(
            make_field("resultId", &SemanticTokens::resultId),
            make_field("data",     &SemanticTokens::data)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensPartialResult {
        Slice<UInt>  data;
    };

    template<> struct Schema<SemanticTokensPartialResult> {
        static constexpr auto fields = std::make_tuple(
            make_field("data", &SemanticTokensPartialResult::data)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensOptions {
		/**
		 * The legend used by the server
		 */
		SemanticTokensLegend*  legend;
		/**
		 * Server supports providing semantic tokens for a specific range
		 * of a document.
		 */
		Bool range;
		/**
		 * Server supports providing semantic tokens for a full document.
		 */
		struct Full {
			enum Kind {
				_EBoolean = 0,
				_EStruct = 1,
			} kind;

			struct _Struct {
				Bool delta;
			};

			union {
				Bool _boolean;
				_Struct _struct;
			};
		} full;
	};

	template<> struct Schema<SemanticTokensOptions> {
		static constexpr auto fields = std::make_tuple(
			make_field("legend", &SemanticTokensOptions::legend),
			make_field("range",  &SemanticTokensOptions::range),
			make_field("full",   &SemanticTokensOptions::full)
		);
	};

	template<> struct Schema<SemanticTokensOptions::Full::_Struct> {
		static constexpr auto fields = std::make_tuple(
			make_field("delta", &SemanticTokensOptions::Full::_Struct::delta)
		);
	};

	template<> struct UnionSchema<SemanticTokensOptions::Full> {
		static constexpr auto tag = &SemanticTokensOptions::Full::kind;
		static constexpr auto variants = std::make_tuple(
			make_variant(SemanticTokensOptions::Full::_EBoolean, &SemanticTokensOptions::Full::_boolean),
			make_variant(SemanticTokensOptions::Full::_EStruct, &SemanticTokensOptions::Full::_struct)
		);
	};

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensRegistrationOptions : public TextDocumentRegistrationOptions, public SemanticTokensOptions {
    };

    template<> struct Schema<SemanticTokensRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &SemanticTokensRegistrationOptions::documentSelector),
            make_field("legend",           &SemanticTokensRegistrationOptions::legend),
            make_field("range",            &SemanticTokensRegistrationOptions::range),
            make_field("full",             &SemanticTokensRegistrationOptions::full)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensDeltaParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The result id of a previous response. The result Id can either point to a full response
         * or a delta response depending on what was received last.
         */
        String                   previousResultId;
    };

    template<> struct Schema<SemanticTokensDeltaParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument",     &SemanticTokensDeltaParams::textDocument),
            make_field("previousResultId", &SemanticTokensDeltaParams::previousResultId)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensDelta {
        String                      resultId;
        /**
         * The semantic token edits to transform a previous result into a new result.
         */
        Slice<SemanticTokensEdit*>  edits;
    };

    template<> struct Schema<SemanticTokensDelta> {
        static constexpr auto fields = std::make_tuple(
            make_field("resultId", &SemanticTokensDelta::resultId),
            make_field("edits",    &SemanticTokensDelta::edits)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensDeltaPartialResult {
        Slice<SemanticTokensEdit*>  edits;
    };

    template<> struct Schema<SemanticTokensDeltaPartialResult> {
        static constexpr auto fields = std::make_tuple(
            make_field("edits", &SemanticTokensDeltaPartialResult::edits)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensRangeParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The range the semantic tokens are requested for.
         */
        Range*                   range;
    };

    template<> struct Schema<SemanticTokensRangeParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &SemanticTokensRangeParams::textDocument),
            make_field("range",        &SemanticTokensRangeParams::range)
        );
    };

    /**
     * Params to show a resource in the UI.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct ShowDocumentParams {
        /**
         * The uri to show.
         */
        String  uri;
        /**
         * Indicates to show the resource in an external program.
         * To show, for example, `https://code.visualstudio.com/`
         * in the default WEB browser set `external` to `true`.
         */
        Bool    external;
        /**
         * An optional property to indicate whether the editor
         * showing the document should take focus or not.
         * Clients might ignore this property if an external
         * program is started.
         */
        Bool    takeFocus;
        /**
         * An optional selection range if the document is a text
         * document. Clients might ignore the property if an
         * external program is started or the file is not a text
         * file.
         */
        Range*  selection;
    };

    template<> struct Schema<ShowDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",       &ShowDocumentParams::uri),
            make_field("external",  &ShowDocumentParams::external),
            make_field("takeFocus", &ShowDocumentParams::takeFocus),
            make_field("selection", &ShowDocumentParams::selection)
        );
    };

    /**
     * The result of a showDocument request.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct ShowDocumentResult {
        /**
         * A boolean indicating if the show was successful.
         */
        Bool  success;
    };

    template<> struct Schema<ShowDocumentResult> {
        static constexpr auto fields = std::make_tuple(
            make_field("success", &ShowDocumentResult::success)
        );
    };

    struct LinkedEditingRangeParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<LinkedEditingRangeParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &LinkedEditingRangeParams::textDocument),
            make_field("position",     &LinkedEditingRangeParams::position)
        );
    };

    /**
     * The result of a linked editing range request.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct LinkedEditingRanges {
        /**
         * A list of ranges that can be edited together. The ranges must have
         * identical length and contain identical text content. The ranges cannot overlap.
         */
        Slice<Range*>  ranges;
        /**
         * An optional word pattern (regular expression) that describes valid contents for
         * the given ranges. If no pattern is provided, the client configuration's word
         * pattern will be used.
         */
        String         wordPattern;
    };

    template<> struct Schema<LinkedEditingRanges> {
        static constexpr auto fields = std::make_tuple(
            make_field("ranges",      &LinkedEditingRanges::ranges),
            make_field("wordPattern", &LinkedEditingRanges::wordPattern)
        );
    };

    struct LinkedEditingRangeOptions {
    };

    template<> struct Schema<LinkedEditingRangeOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    struct LinkedEditingRangeRegistrationOptions : public TextDocumentRegistrationOptions, public LinkedEditingRangeOptions {
    };

    template<> struct Schema<LinkedEditingRangeRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &LinkedEditingRangeRegistrationOptions::documentSelector)
        );
    };

    /**
     * The parameters sent in notifications/requests for user-initiated creation of
     * files.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CreateFilesParams {
        /**
         * An array of all files/folders created in this operation.
         */
        Slice<FileCreate*>  files;
    };

    template<> struct Schema<CreateFilesParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("files", &CreateFilesParams::files)
        );
    };

    /**
     * A workspace edit represents changes to many resources managed in the workspace. The edit
     * should either provide `changes` or `documentChanges`. If documentChanges are present
     * they are preferred over `changes` if the client can handle versioned document edits.
     *
     * Since version 3.13.0 a workspace edit can contain resource operations as well. If resource
     * operations are present clients need to execute the operations in the order in which they
     * are provided. So a workspace edit for example can consist of the following two changes:
     * (1) a create file a.txt and (2) a text document edit which insert text into file a.txt.
     *
     * An invalid sequence (e.g. (1) delete file a.txt and (2) insert text into file a.txt) will
     * cause failure of the operation. How the client recovers from the failure is described by
     * the client capability: `workspace.workspaceEdit.failureHandling`
     */
    struct WorkspaceEdit {
        /**
         * Holds changes to existing resources.
         */
        LSPAny                    changes;
        /**
         * Depending on the client capability `workspace.workspaceEdit.resourceOperations` document changes
         * are either an array of `TextDocumentEdit`s to express changes to n different text documents
         * where each text document edit addresses a specific version of a text document. Or it can contain
         * above `TextDocumentEdit`s mixed with create, rename and delete file / folder operations.
         *
         * Whether a client supports versioned document edits is expressed via
         * `workspace.workspaceEdit.documentChanges` client capability.
         *
         * If a client neither supports `documentChanges` nor `workspace.workspaceEdit.resourceOperations` then
         * only plain `TextEdit`s using the `changes` property are supported.
         */
        Slice<TextDocumentEdit*>  documentChanges;
        /**
         * A map of change annotations that can be referenced in `AnnotatedTextEdit`s or create, rename and
         * delete file / folder operations.
         *
         * Whether clients honor this property depends on the client capability `workspace.changeAnnotationSupport`.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        LSPAny                    changeAnnotations;
    };

    template<> struct Schema<WorkspaceEdit> {
        static constexpr auto fields = std::make_tuple(
            make_field("changes",           &WorkspaceEdit::changes),
            make_field("documentChanges",   &WorkspaceEdit::documentChanges),
            make_field("changeAnnotations", &WorkspaceEdit::changeAnnotations)
        );
    };

    /**
     * The options to register for file operations.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct FileOperationRegistrationOptions {
        /**
         * The actual filters.
         */
        Slice<FileOperationFilter*>  filters;
    };

    template<> struct Schema<FileOperationRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("filters", &FileOperationRegistrationOptions::filters)
        );
    };

    /**
     * The parameters sent in notifications/requests for user-initiated renames of
     * files.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct RenameFilesParams {
        /**
         * An array of all files/folders renamed in this operation. When a folder is renamed, only
         * the folder will be included, and not its children.
         */
        Slice<FileRename*>  files;
    };

    template<> struct Schema<RenameFilesParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("files", &RenameFilesParams::files)
        );
    };

    /**
     * The parameters sent in notifications/requests for user-initiated deletes of
     * files.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct DeleteFilesParams {
        /**
         * An array of all files/folders deleted in this operation.
         */
        Slice<FileDelete*>  files;
    };

    template<> struct Schema<DeleteFilesParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("files", &DeleteFilesParams::files)
        );
    };

    struct MonikerParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<MonikerParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &MonikerParams::textDocument),
            make_field("position",     &MonikerParams::position)
        );
    };

    /**
     * Moniker definition to match LSIF 0.5 moniker definition.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct Moniker {
        /**
         * The scheme of the moniker. For example tsc or .Net
         */
        String           scheme;
        /**
         * The identifier of the moniker. The value is opaque in LSIF however
         * schema owners are allowed to define the structure if they want.
         */
        String           identifier;
        /**
         * The scope in which the moniker is unique
         */
        UniquenessLevel  unique;
        /**
         * The moniker kind if known.
         */
        MonikerKind      kind;
    };

    template<> struct Schema<Moniker> {
        static constexpr auto fields = std::make_tuple(
            make_field("scheme",     &Moniker::scheme),
            make_field("identifier", &Moniker::identifier),
            make_field("unique",     &Moniker::unique),
            make_field("kind",       &Moniker::kind)
        );
    };

    struct MonikerOptions {
    };

    template<> struct Schema<MonikerOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    struct MonikerRegistrationOptions : public TextDocumentRegistrationOptions, public MonikerOptions {
    };

    template<> struct Schema<MonikerRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &MonikerRegistrationOptions::documentSelector)
        );
    };

    /**
     * The parameter of a `textDocument/prepareTypeHierarchy` request.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct TypeHierarchyPrepareParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<TypeHierarchyPrepareParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &TypeHierarchyPrepareParams::textDocument),
            make_field("position",     &TypeHierarchyPrepareParams::position)
        );
    };

    /**
     * @since 3.17.0
     * @since 3.17.0
     */
    struct TypeHierarchyItem {
        /**
         * The name of this item.
         */
        String            name;
        /**
         * The kind of this item.
         */
        SymbolKind        kind;
        /**
         * Tags for this item.
         */
        Slice<SymbolTag>  tags;
        /**
         * More detail for this item, e.g. the signature of a function.
         */
        String            detail;
        /**
         * The resource identifier of this item.
         */
        String            uri;
        /**
         * The range enclosing this symbol not including leading/trailing whitespace
         * but everything else, e.g. comments and code.
         */
        Range*            range;
        /**
         * The range that should be selected and revealed when this symbol is being
         * picked, e.g. the name of a function. Must be contained by the
         * {@link TypeHierarchyItem.range `range`}.
         */
        Range*            selectionRange;
        /**
         * A data entry field that is preserved between a type hierarchy prepare and
         * supertypes or subtypes requests. It could also be used to identify the
         * type hierarchy in the server, helping improve the performance on
         * resolving supertypes and subtypes.
         */
        String            data;
    };

    template<> struct Schema<TypeHierarchyItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("name",           &TypeHierarchyItem::name),
            make_field("kind",           &TypeHierarchyItem::kind),
            make_field("tags",           &TypeHierarchyItem::tags),
            make_field("detail",         &TypeHierarchyItem::detail),
            make_field("uri",            &TypeHierarchyItem::uri),
            make_field("range",          &TypeHierarchyItem::range),
            make_field("selectionRange", &TypeHierarchyItem::selectionRange),
            make_field("data",           &TypeHierarchyItem::data)
        );
    };

    /**
     * Type hierarchy options used during static registration.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct TypeHierarchyOptions {
    };

    template<> struct Schema<TypeHierarchyOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * Type hierarchy options used during static or dynamic registration.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct TypeHierarchyRegistrationOptions : public TextDocumentRegistrationOptions, public TypeHierarchyOptions {
    };

    template<> struct Schema<TypeHierarchyRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &TypeHierarchyRegistrationOptions::documentSelector)
        );
    };

    /**
     * The parameter of a `typeHierarchy/supertypes` request.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct TypeHierarchySupertypesParams {
        TypeHierarchyItem*  item;
    };

    template<> struct Schema<TypeHierarchySupertypesParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("item", &TypeHierarchySupertypesParams::item)
        );
    };

    /**
     * The parameter of a `typeHierarchy/subtypes` request.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct TypeHierarchySubtypesParams {
        TypeHierarchyItem*  item;
    };

    template<> struct Schema<TypeHierarchySubtypesParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("item", &TypeHierarchySubtypesParams::item)
        );
    };

    /**
     * A parameter literal used in inline value requests.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValueParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The document range for which inline values should be computed.
         */
        Range*                   range;
        /**
         * Additional information about the context in which inline values were
         * requested.
         */
        InlineValueContext*      context;
    };

    template<> struct Schema<InlineValueParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &InlineValueParams::textDocument),
            make_field("range",        &InlineValueParams::range),
            make_field("context",      &InlineValueParams::context)
        );
    };

    /**
     * Inline value options used during static registration.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValueOptions {
    };

    template<> struct Schema<InlineValueOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * Inline value options used during static or dynamic registration.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValueRegistrationOptions : public InlineValueOptions, public TextDocumentRegistrationOptions {
    };

    template<> struct Schema<InlineValueRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &InlineValueRegistrationOptions::documentSelector)
        );
    };

    /**
     * A parameter literal used in inlay hint requests.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlayHintParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The document range for which inlay hints should be computed.
         */
        Range*                   range;
    };

    template<> struct Schema<InlayHintParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &InlayHintParams::textDocument),
            make_field("range",        &InlayHintParams::range)
        );
    };

    /**
     * Inlay hint information.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlayHint {
        /**
         * The position of this hint.
         *
         * If multiple hints have the same position, they will be shown in the order
         * they appear in the response.
         */
        Position*         position;
        /**
         * The label of this hint. A human readable string or an array of
         * InlayHintLabelPart label parts.
         *
         * *Note* that neither the string nor the label part can be empty.
         */
        String            label;
        /**
         * The kind of this hint. Can be omitted in which case the client
         * should fall back to a reasonable default.
         */
        InlayHintKind     kind;
        /**
         * Optional text edits that are performed when accepting this inlay hint.
         *
         * *Note* that edits are expected to change the document so that the inlay
         * hint (or its nearest variant) is now part of the document and the inlay
         * hint itself is now obsolete.
         */
        Slice<TextEdit*>  textEdits;
        /**
         * The tooltip text when you hover over this item.
         */
        String            tooltip;
        /**
         * Render padding before the hint.
         *
         * Note: Padding should use the editor's background color, not the
         * background color of the hint itself. That means padding can be used
         * to visually align/separate an inlay hint.
         */
        Bool              paddingLeft;
        /**
         * Render padding after the hint.
         *
         * Note: Padding should use the editor's background color, not the
         * background color of the hint itself. That means padding can be used
         * to visually align/separate an inlay hint.
         */
        Bool              paddingRight;
        /**
         * A data entry field that is preserved on an inlay hint between
         * a `textDocument/inlayHint` and a `inlayHint/resolve` request.
         */
        String            data;
    };

    template<> struct Schema<InlayHint> {
        static constexpr auto fields = std::make_tuple(
            make_field("position",     &InlayHint::position),
            make_field("label",        &InlayHint::label),
            make_field("kind",         &InlayHint::kind),
            make_field("textEdits",    &InlayHint::textEdits),
            make_field("tooltip",      &InlayHint::tooltip),
            make_field("paddingLeft",  &InlayHint::paddingLeft),
            make_field("paddingRight", &InlayHint::paddingRight),
            make_field("data",         &InlayHint::data)
        );
    };

    /**
     * Inlay hint options used during static registration.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlayHintOptions {
        /**
         * The server provides support to resolve additional
         * information for an inlay hint item.
         */
        Bool  resolveProvider;
    };

    template<> struct Schema<InlayHintOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("resolveProvider", &InlayHintOptions::resolveProvider)
        );
    };

    /**
     * Inlay hint options used during static or dynamic registration.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlayHintRegistrationOptions : public InlayHintOptions, public TextDocumentRegistrationOptions {
    };

    template<> struct Schema<InlayHintRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("resolveProvider",  &InlayHintRegistrationOptions::resolveProvider),
            make_field("documentSelector", &InlayHintRegistrationOptions::documentSelector)
        );
    };

    /**
     * Parameters of the document diagnostic request.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DocumentDiagnosticParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The additional identifier  provided during registration.
         */
        String                   identifier;
        /**
         * The result id of a previous response if provided.
         */
        String                   previousResultId;
    };

    template<> struct Schema<DocumentDiagnosticParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument",     &DocumentDiagnosticParams::textDocument),
            make_field("identifier",       &DocumentDiagnosticParams::identifier),
            make_field("previousResultId", &DocumentDiagnosticParams::previousResultId)
        );
    };

    /**
     * A partial result for a document diagnostic report.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DocumentDiagnosticReportPartialResult {
        LSPAny  relatedDocuments;
    };

    template<> struct Schema<DocumentDiagnosticReportPartialResult> {
        static constexpr auto fields = std::make_tuple(
            make_field("relatedDocuments", &DocumentDiagnosticReportPartialResult::relatedDocuments)
        );
    };

    /**
     * Cancellation data returned from a diagnostic request.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DiagnosticServerCancellationData {
        Bool  retriggerRequest;
    };

    template<> struct Schema<DiagnosticServerCancellationData> {
        static constexpr auto fields = std::make_tuple(
            make_field("retriggerRequest", &DiagnosticServerCancellationData::retriggerRequest)
        );
    };

    /**
     * Diagnostic options.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DiagnosticOptions {
        /**
         * An optional identifier under which the diagnostics are
         * managed by the client.
         */
        String  identifier;
        /**
         * Whether the language has inter file dependencies meaning that
         * editing code in one file can result in a different diagnostic
         * set in another file. Inter file dependencies are common for
         * most programming languages and typically uncommon for linters.
         */
        Bool    interFileDependencies;
        /**
         * The server provides support for workspace diagnostics as well.
         */
        Bool    workspaceDiagnostics;
    };

    template<> struct Schema<DiagnosticOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("identifier",            &DiagnosticOptions::identifier),
            make_field("interFileDependencies", &DiagnosticOptions::interFileDependencies),
            make_field("workspaceDiagnostics",  &DiagnosticOptions::workspaceDiagnostics)
        );
    };

    /**
     * Diagnostic registration options.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DiagnosticRegistrationOptions : public TextDocumentRegistrationOptions, public DiagnosticOptions {
    };

    template<> struct Schema<DiagnosticRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector",      &DiagnosticRegistrationOptions::documentSelector),
            make_field("identifier",            &DiagnosticRegistrationOptions::identifier),
            make_field("interFileDependencies", &DiagnosticRegistrationOptions::interFileDependencies),
            make_field("workspaceDiagnostics",  &DiagnosticRegistrationOptions::workspaceDiagnostics)
        );
    };

    /**
     * Parameters of the workspace diagnostic request.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct WorkspaceDiagnosticParams {
        /**
         * The additional identifier provided during registration.
         */
        String                    identifier;
        /**
         * The currently known diagnostic reports with their
         * previous result ids.
         */
        Slice<PreviousResultId*>  previousResultIds;
    };

    template<> struct Schema<WorkspaceDiagnosticParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("identifier",        &WorkspaceDiagnosticParams::identifier),
            make_field("previousResultIds", &WorkspaceDiagnosticParams::previousResultIds)
        );
    };

    /**
     * A workspace diagnostic report.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct WorkspaceDiagnosticReport {
        Slice<WorkspaceDocumentDiagnosticReport*>  items;
    };

    template<> struct Schema<WorkspaceDiagnosticReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("items", &WorkspaceDiagnosticReport::items)
        );
    };

    /**
     * A partial result for a workspace diagnostic report.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct WorkspaceDiagnosticReportPartialResult {
        Slice<WorkspaceDocumentDiagnosticReport*>  items;
    };

    template<> struct Schema<WorkspaceDiagnosticReportPartialResult> {
        static constexpr auto fields = std::make_tuple(
            make_field("items", &WorkspaceDiagnosticReportPartialResult::items)
        );
    };

    /**
     * The params sent in an open notebook document notification.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DidOpenNotebookDocumentParams {
        /**
         * The notebook document that got opened.
         */
        NotebookDocument*         notebookDocument;
        /**
         * The text documents that represent the content
         * of a notebook cell.
         */
        Slice<TextDocumentItem*>  cellTextDocuments;
    };

    template<> struct Schema<DidOpenNotebookDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("notebookDocument",  &DidOpenNotebookDocumentParams::notebookDocument),
            make_field("cellTextDocuments", &DidOpenNotebookDocumentParams::cellTextDocuments)
        );
    };

    /**
     * The params sent in a change notebook document notification.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DidChangeNotebookDocumentParams {
        /**
         * The notebook document that did change. The version number points
         * to the version after all provided changes have been applied. If
         * only the text document content of a cell changes the notebook version
         * doesn't necessarily have to change.
         */
        VersionedNotebookDocumentIdentifier*  notebookDocument;
        /**
         * The actual changes to the notebook document.
         *
         * The changes describe single state changes to the notebook document.
         * So if there are two changes c1 (at array index 0) and c2 (at array
         * index 1) for a notebook in state S then c1 moves the notebook from
         * S to S' and c2 from S' to S''. So c1 is computed on the state S and
         * c2 is computed on the state S'.
         *
         * To mirror the content of a notebook using change events use the following approach:
         * - start with the same initial content
         * - apply the 'notebookDocument/didChange' notifications in the order you receive them.
         * - apply the `NotebookChangeEvent`s in a single notification in the order
         * you receive them.
         */
        NotebookDocumentChangeEvent*          change;
    };

    template<> struct Schema<DidChangeNotebookDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("notebookDocument", &DidChangeNotebookDocumentParams::notebookDocument),
            make_field("change",           &DidChangeNotebookDocumentParams::change)
        );
    };

    /**
     * The params sent in a save notebook document notification.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DidSaveNotebookDocumentParams {
        /**
         * The notebook document that got saved.
         */
        NotebookDocumentIdentifier*  notebookDocument;
    };

    template<> struct Schema<DidSaveNotebookDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("notebookDocument", &DidSaveNotebookDocumentParams::notebookDocument)
        );
    };

    /**
     * The params sent in a close notebook document notification.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DidCloseNotebookDocumentParams {
        /**
         * The notebook document that got closed.
         */
        NotebookDocumentIdentifier*     notebookDocument;
        /**
         * The text documents that represent the content
         * of a notebook cell that got closed.
         */
        Slice<TextDocumentIdentifier*>  cellTextDocuments;
    };

    template<> struct Schema<DidCloseNotebookDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("notebookDocument",  &DidCloseNotebookDocumentParams::notebookDocument),
            make_field("cellTextDocuments", &DidCloseNotebookDocumentParams::cellTextDocuments)
        );
    };

    /**
     * A parameter literal used in inline completion requests.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct InlineCompletionParams : public TextDocumentPositionParams {
        /**
         * Additional information about the context in which inline completions were
         * requested.
         */
        InlineCompletionContext*  context;
    };

    template<> struct Schema<InlineCompletionParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &InlineCompletionParams::textDocument),
            make_field("position",     &InlineCompletionParams::position),
            make_field("context",      &InlineCompletionParams::context)
        );
    };

    /**
     * Represents a collection of {@link InlineCompletionItem inline completion items} to be presented in the editor.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct InlineCompletionList {
        /**
         * The inline completion items
         */
        Slice<InlineCompletionItem*>  items;
    };

    template<> struct Schema<InlineCompletionList> {
        static constexpr auto fields = std::make_tuple(
            make_field("items", &InlineCompletionList::items)
        );
    };

    /**
     * An inline completion item represents a text snippet that is proposed inline to complete text that is being typed.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct InlineCompletionItem {
        /**
         * The text to replace the range with. Must be set.
         */
        String    insertText;
        /**
         * A text that is used to decide if this inline completion should be shown. When `falsy` the {@link InlineCompletionItem.insertText} is used.
         */
        String    filterText;
        /**
         * The range to replace. Must begin and end on the same line.
         */
        Range*    range;
        /**
         * An optional {@link Command} that is executed *after* inserting this completion.
         */
        Command*  command;
    };

    template<> struct Schema<InlineCompletionItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("insertText", &InlineCompletionItem::insertText),
            make_field("filterText", &InlineCompletionItem::filterText),
            make_field("range",      &InlineCompletionItem::range),
            make_field("command",    &InlineCompletionItem::command)
        );
    };

    /**
     * Inline completion options used during static registration.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct InlineCompletionOptions {
    };

    template<> struct Schema<InlineCompletionOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * Inline completion options used during static or dynamic registration.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct InlineCompletionRegistrationOptions : public InlineCompletionOptions, public TextDocumentRegistrationOptions {
    };

    template<> struct Schema<InlineCompletionRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &InlineCompletionRegistrationOptions::documentSelector)
        );
    };

    struct RegistrationParams {
        Slice<Registration*>  registrations;
    };

    template<> struct Schema<RegistrationParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("registrations", &RegistrationParams::registrations)
        );
    };

    struct UnregistrationParams {
        Slice<Unregistration*>  unregisterations;
    };

    template<> struct Schema<UnregistrationParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("unregisterations", &UnregistrationParams::unregisterations)
        );
    };

    /**
     * The initialize parameters
     */
    struct _InitializeParams {
        /**
         * The process Id of the parent process that started
         * the server.
         *
         * Is `null` if the process has not been started by another process.
         * If the parent process is not alive then the server should exit.
         */
        Int                  processId;
        /**
         * Information about the client
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        struct _ClientInfo {
            /**
             * The name of the client as defined by the client.
             */
            String  name;
            /**
             * The client's version as defined by the client.
             */
            String  version;
        }* clientInfo;
        /**
         * The locale the client is currently showing the user interface
         * in. This must not necessarily be the locale of the operating
         * system.
         *
         * Uses IETF language tags as the value's syntax
         * (See https://en.wikipedia.org/wiki/IETF_language_tag)
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        String               locale;
        /**
         * The rootPath of the workspace. Is null
         * if no folder is open.
         *
         * @deprecated in favour of rootUri.
         */
        String               rootPath;
        /**
         * The rootUri of the workspace. Is null if no
         * folder is open. If both `rootPath` and `rootUri` are set
         * `rootUri` wins.
         *
         * @deprecated in favour of workspaceFolders.
         */
        String               rootUri;
        /**
         * The capabilities provided by the client (editor or tool)
         */
        ClientCapabilities*  capabilities;
        /**
         * User provided initialization options.
         */
        String               initializationOptions;
        /**
         * The initial trace setting. If omitted trace is disabled ('off').
         */
        TraceValues          trace;
    };

    template<> struct Schema<_InitializeParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("processId",             &_InitializeParams::processId),
            make_field("clientInfo",            &_InitializeParams::clientInfo),
            make_field("locale",                &_InitializeParams::locale),
            make_field("rootPath",              &_InitializeParams::rootPath),
            make_field("rootUri",               &_InitializeParams::rootUri),
            make_field("capabilities",          &_InitializeParams::capabilities),
            make_field("initializationOptions", &_InitializeParams::initializationOptions),
            make_field("trace",                 &_InitializeParams::trace)
        );
    };

    template<> struct Schema<_InitializeParams::_ClientInfo> {
        static constexpr auto fields = std::make_tuple(
            make_field("name",    &_InitializeParams::_ClientInfo::name),
            make_field("version", &_InitializeParams::_ClientInfo::version)
        );
    };

    struct WorkspaceFoldersInitializeParams {
        /**
         * The workspace folders configured in the client when the server starts.
         *
         * This property is only available if the client supports workspace folders.
         * It can be `null` if the client supports workspace folders but none are
         * configured.
         *
         * @since 3.6.0
         * @since 3.6.0
         */
        Slice<WorkspaceFolder*>  workspaceFolders;
    };

    template<> struct Schema<WorkspaceFoldersInitializeParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("workspaceFolders", &WorkspaceFoldersInitializeParams::workspaceFolders)
        );
    };

    struct InitializeParams : public _InitializeParams, public WorkspaceFoldersInitializeParams {
    };

    template<> struct Schema<InitializeParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("processId",             &InitializeParams::processId),
            make_field("clientInfo",            &InitializeParams::clientInfo),
            make_field("locale",                &InitializeParams::locale),
            make_field("rootPath",              &InitializeParams::rootPath),
            make_field("rootUri",               &InitializeParams::rootUri),
            make_field("capabilities",          &InitializeParams::capabilities),
            make_field("initializationOptions", &InitializeParams::initializationOptions),
            make_field("trace",                 &InitializeParams::trace),
            make_field("workspaceFolders",      &InitializeParams::workspaceFolders)
        );
    };

    /**
     * The result returned from an initialize request.
     */
    struct InitializeResult {
        /**
         * The capabilities the language server provides.
         */
        ServerCapabilities*  capabilities;
        /**
         * Information about the server.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        struct _ServerInfo {
            /**
             * The name of the server as defined by the server.
             */
            String  name;
            /**
             * The server's version as defined by the server.
             */
            String  version;
        }* serverInfo;
    };

    template<> struct Schema<InitializeResult> {
        static constexpr auto fields = std::make_tuple(
            make_field("capabilities", &InitializeResult::capabilities),
            make_field("serverInfo",   &InitializeResult::serverInfo)
        );
    };

    template<> struct Schema<InitializeResult::_ServerInfo> {
        static constexpr auto fields = std::make_tuple(
            make_field("name",    &InitializeResult::_ServerInfo::name),
            make_field("version", &InitializeResult::_ServerInfo::version)
        );
    };

    /**
     * The data type of the ResponseError if the
     * initialize request fails.
     */
    struct InitializeError {
        /**
         * Indicates whether the client execute the following retry logic:
         * (1) show the message provided by the ResponseError to the user
         * (2) user selects retry or cancel
         * (3) if user selected retry the initialize method is sent again.
         */
        Bool  retry;
    };

    template<> struct Schema<InitializeError> {
        static constexpr auto fields = std::make_tuple(
            make_field("retry", &InitializeError::retry)
        );
    };

    struct InitializedParams {
    };

    template<> struct Schema<InitializedParams> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * The parameters of a change configuration notification.
     */
    struct DidChangeConfigurationParams {
        /**
         * The actual changed settings
         */
        String  settings;
    };

    template<> struct Schema<DidChangeConfigurationParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("settings", &DidChangeConfigurationParams::settings)
        );
    };

    struct DidChangeConfigurationRegistrationOptions {
        String  section;
    };

    template<> struct Schema<DidChangeConfigurationRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("section", &DidChangeConfigurationRegistrationOptions::section)
        );
    };

    /**
     * The parameters of a notification message.
     */
    struct ShowMessageParams {
        /**
         * The message type. See {@link MessageType}
         */
        MessageType  type;
        /**
         * The actual message.
         */
        String       message;
    };

    template<> struct Schema<ShowMessageParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("type",    &ShowMessageParams::type),
            make_field("message", &ShowMessageParams::message)
        );
    };

    struct ShowMessageRequestParams {
        /**
         * The message type. See {@link MessageType}
         */
        MessageType                type;
        /**
         * The actual message.
         */
        String                     message;
        /**
         * The message action items to present.
         */
        Slice<MessageActionItem*>  actions;
    };

    template<> struct Schema<ShowMessageRequestParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("type",    &ShowMessageRequestParams::type),
            make_field("message", &ShowMessageRequestParams::message),
            make_field("actions", &ShowMessageRequestParams::actions)
        );
    };

    struct MessageActionItem {
        /**
         * A short title like 'Retry', 'Open Log' etc.
         */
        String  title;
    };

    template<> struct Schema<MessageActionItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("title", &MessageActionItem::title)
        );
    };

    /**
     * The log message parameters.
     */
    struct LogMessageParams {
        /**
         * The message type. See {@link MessageType}
         */
        MessageType  type;
        /**
         * The actual message.
         */
        String       message;
    };

    template<> struct Schema<LogMessageParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("type",    &LogMessageParams::type),
            make_field("message", &LogMessageParams::message)
        );
    };

    /**
     * The parameters sent in an open text document notification
     */
    struct DidOpenTextDocumentParams {
        /**
         * The document that was opened.
         */
        TextDocumentItem*  textDocument;
    };

    template<> struct Schema<DidOpenTextDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DidOpenTextDocumentParams::textDocument)
        );
    };

    /**
     * The change text document notification's parameters.
     */
    struct DidChangeTextDocumentParams {
        /**
         * The document that did change. The version number points
         * to the version after all provided content changes have
         * been applied.
         */
        VersionedTextDocumentIdentifier*        textDocument;
        /**
         * The actual content changes. The content changes describe single state changes
         * to the document. So if there are two content changes c1 (at array index 0) and
         * c2 (at array index 1) for a document in state S then c1 moves the document from
         * S to S' and c2 from S' to S''. So c1 is computed on the state S and c2 is computed
         * on the state S'.
         *
         * To mirror the content of a document using change events use the following approach:
         * - start with the same initial content
         * - apply the 'textDocument/didChange' notifications in the order you receive them.
         * - apply the `TextDocumentContentChangeEvent`s in a single notification in the order
         * you receive them.
         */
        Slice<TextDocumentContentChangeEvent*>  contentChanges;
    };

    template<> struct Schema<DidChangeTextDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument",   &DidChangeTextDocumentParams::textDocument),
            make_field("contentChanges", &DidChangeTextDocumentParams::contentChanges)
        );
    };

    /**
     * Describe options to be used when registered for text document change events.
     */
    struct TextDocumentChangeRegistrationOptions : public TextDocumentRegistrationOptions {
        /**
         * How documents are synced to the server.
         */
        TextDocumentSyncKind  syncKind;
    };

    template<> struct Schema<TextDocumentChangeRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &TextDocumentChangeRegistrationOptions::documentSelector),
            make_field("syncKind",         &TextDocumentChangeRegistrationOptions::syncKind)
        );
    };

    /**
     * The parameters sent in a close text document notification
     */
    struct DidCloseTextDocumentParams {
        /**
         * The document that was closed.
         */
        TextDocumentIdentifier*  textDocument;
    };

    template<> struct Schema<DidCloseTextDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DidCloseTextDocumentParams::textDocument)
        );
    };

    /**
     * The parameters sent in a save text document notification
     */
    struct DidSaveTextDocumentParams {
        /**
         * The document that was saved.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * Optional the content when saved. Depends on the includeText value
         * when the save notification was requested.
         */
        String                   text;
    };

    template<> struct Schema<DidSaveTextDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DidSaveTextDocumentParams::textDocument),
            make_field("text",         &DidSaveTextDocumentParams::text)
        );
    };

    /**
     * Save options.
     */
    struct SaveOptions {
        /**
         * The client is supposed to include the content on save.
         */
        Bool  includeText;
    };

    template<> struct Schema<SaveOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("includeText", &SaveOptions::includeText)
        );
    };

    /**
     * Save registration options.
     */
    struct TextDocumentSaveRegistrationOptions : public TextDocumentRegistrationOptions, public SaveOptions {
    };

    template<> struct Schema<TextDocumentSaveRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &TextDocumentSaveRegistrationOptions::documentSelector),
            make_field("includeText",      &TextDocumentSaveRegistrationOptions::includeText)
        );
    };

    /**
     * The parameters sent in a will save text document notification.
     */
    struct WillSaveTextDocumentParams {
        /**
         * The document that will be saved.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The 'TextDocumentSaveReason'.
         */
        TextDocumentSaveReason   reason;
    };

    template<> struct Schema<WillSaveTextDocumentParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &WillSaveTextDocumentParams::textDocument),
            make_field("reason",       &WillSaveTextDocumentParams::reason)
        );
    };

    /**
     * A text edit applicable to a text document.
     */
    struct TextEdit {
        /**
         * The range of the text document to be manipulated. To insert
         * text into a document create a range where start === end.
         */
        Range*  range;
        /**
         * The string to be inserted. For delete operations use an
         * empty string.
         */
        String  newText;
    };

    template<> struct Schema<TextEdit> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",   &TextEdit::range),
            make_field("newText", &TextEdit::newText)
        );
    };

    /**
     * The watched files change notification's parameters.
     */
    struct DidChangeWatchedFilesParams {
        /**
         * The actual file events.
         */
        Slice<FileEvent*>  changes;
    };

    template<> struct Schema<DidChangeWatchedFilesParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("changes", &DidChangeWatchedFilesParams::changes)
        );
    };

    /**
     * Describe options to be used when registered for text document change events.
     */
    struct DidChangeWatchedFilesRegistrationOptions {
        /**
         * The watchers to register.
         */
        Slice<FileSystemWatcher*>  watchers;
    };

    template<> struct Schema<DidChangeWatchedFilesRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("watchers", &DidChangeWatchedFilesRegistrationOptions::watchers)
        );
    };

    /**
     * The publish diagnostic notification's parameters.
     */
    struct PublishDiagnosticsParams {
        /**
         * The URI for which diagnostic information is reported.
         */
        String              uri;
        /**
         * Optional the version number of the document the diagnostics are published for.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Int                 version;
        /**
         * An array of diagnostic information items.
         */
        Slice<Diagnostic*>  diagnostics;
    };

    template<> struct Schema<PublishDiagnosticsParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",         &PublishDiagnosticsParams::uri),
            make_field("version",     &PublishDiagnosticsParams::version),
            make_field("diagnostics", &PublishDiagnosticsParams::diagnostics)
        );
    };

    /**
     * Completion parameters
     */
    struct CompletionParams : public TextDocumentPositionParams {
        /**
         * The completion context. This is only available if the client specifies
         * to send this using the client capability `textDocument.completion.contextSupport === true`
         */
        CompletionContext*  context;
    };

    template<> struct Schema<CompletionParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &CompletionParams::textDocument),
            make_field("position",     &CompletionParams::position),
            make_field("context",      &CompletionParams::context)
        );
    };

    /**
     * A completion item represents a text snippet that is
     * proposed to complete text that is being typed.
     */
    struct CompletionItem {
        /**
         * The label of this completion item.
         *
         * The label property is also by default the text that
         * is inserted when selecting this completion.
         *
         * If label details are provided the label itself should
         * be an unqualified name of the completion item.
         */
        String                       label;
        /**
         * Additional details for the label
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        CompletionItemLabelDetails*  labelDetails;
        /**
         * The kind of this completion item. Based of the kind
         * an icon is chosen by the editor.
         */
        CompletionItemKind           kind;
        /**
         * Tags for this completion item.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Slice<CompletionItemTag>     tags;
        /**
         * A human-readable string with additional information
         * about this item, like type or symbol information.
         */
        String                       detail;
        /**
         * A human-readable string that represents a doc-comment.
         */
        String                       documentation;
        /**
         * Indicates if this item is deprecated.
         * @deprecated Use `tags` instead.
         */
        Bool                         deprecated;
        /**
         * Select this item when showing.
         *
         * *Note* that only one completion item can be selected and that the
         * tool / client decides which item that is. The rule is that the *first*
         * item of those that match best is selected.
         */
        Bool                         preselect;
        /**
         * A string that should be used when comparing this item
         * with other items. When `falsy` the {@link CompletionItem.label label}
         * is used.
         */
        String                       sortText;
        /**
         * A string that should be used when filtering a set of
         * completion items. When `falsy` the {@link CompletionItem.label label}
         * is used.
         */
        String                       filterText;
        /**
         * A string that should be inserted into a document when selecting
         * this completion. When `falsy` the {@link CompletionItem.label label}
         * is used.
         *
         * The `insertText` is subject to interpretation by the client side.
         * Some tools might not take the string literally. For example
         * VS Code when code complete is requested in this example
         * `con<cursor position>` and a completion item with an `insertText` of
         * `console` is provided it will only insert `sole`. Therefore it is
         * recommended to use `textEdit` instead since it avoids additional client
         * side interpretation.
         */
        String                       insertText;
        /**
         * The format of the insert text. The format applies to both the
         * `insertText` property and the `newText` property of a provided
         * `textEdit`. If omitted defaults to `InsertTextFormat.PlainText`.
         *
         * Please note that the insertTextFormat doesn't apply to
         * `additionalTextEdits`.
         */
        InsertTextFormat             insertTextFormat;
        /**
         * How whitespace and indentation is handled during completion
         * item insertion. If not provided the clients default value depends on
         * the `textDocument.completion.insertTextMode` client capability.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        InsertTextMode               insertTextMode;
        /**
         * An {@link TextEdit edit} which is applied to a document when selecting
         * this completion. When an edit is provided the value of
         * {@link CompletionItem.insertText insertText} is ignored.
         *
         * Most editors support two different operations when accepting a completion
         * item. One is to insert a completion text and the other is to replace an
         * existing text with a completion text. Since this can usually not be
         * predetermined by a server it can report both ranges. Clients need to
         * signal support for `InsertReplaceEdits` via the
         * `textDocument.completion.insertReplaceSupport` client capability
         * property.
         *
         * *Note 1:* The text edit's range as well as both ranges from an insert
         * replace edit must be a [single line] and they must contain the position
         * at which completion has been requested.
         * *Note 2:* If an `InsertReplaceEdit` is returned the edit's insert range
         * must be a prefix of the edit's replace range, that means it must be
         * contained and starting at the same position.
         *
         * @since 3.16.0 additional type `InsertReplaceEdit`
         * @since 3.16.0 additional type `InsertReplaceEdit`
         */
        TextEdit*                    textEdit;
        /**
         * The edit text used if the completion item is part of a CompletionList and
         * CompletionList defines an item default for the text edit range.
         *
         * Clients will only honor this property if they opt into completion list
         * item defaults using the capability `completionList.itemDefaults`.
         *
         * If not provided and a list's default range is provided the label
         * property is used as a text.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        String                       textEditText;
        /**
         * An optional array of additional {@link TextEdit text edits} that are applied when
         * selecting this completion. Edits must not overlap (including the same insert position)
         * with the main {@link CompletionItem.textEdit edit} nor with themselves.
         *
         * Additional text edits should be used to change text unrelated to the current cursor position
         * (for example adding an import statement at the top of the file if the completion item will
         * insert an unqualified type).
         */
        Slice<TextEdit*>             additionalTextEdits;
        /**
         * An optional set of characters that when pressed while this completion is active will accept it first and
         * then type that character. *Note* that all commit characters should have `length=1` and that superfluous
         * characters will be ignored.
         */
        Slice<String>                commitCharacters;
        /**
         * An optional {@link Command command} that is executed *after* inserting this completion. *Note* that
         * additional modifications to the current document should be described with the
         * {@link CompletionItem.additionalTextEdits additionalTextEdits}-property.
         */
        Command*                     command;
        /**
         * A data entry field that is preserved on a completion item between a
         * {@link CompletionRequest} and a {@link CompletionResolveRequest}.
         */
        String                       data;
    };

    template<> struct Schema<CompletionItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("label",               &CompletionItem::label),
            make_field("labelDetails",        &CompletionItem::labelDetails),
            make_field("kind",                &CompletionItem::kind),
            make_field("tags",                &CompletionItem::tags),
            make_field("detail",              &CompletionItem::detail),
            make_field("documentation",       &CompletionItem::documentation),
            make_field("deprecated",          &CompletionItem::deprecated),
            make_field("preselect",           &CompletionItem::preselect),
            make_field("sortText",            &CompletionItem::sortText),
            make_field("filterText",          &CompletionItem::filterText),
            make_field("insertText",          &CompletionItem::insertText),
            make_field("insertTextFormat",    &CompletionItem::insertTextFormat),
            make_field("insertTextMode",      &CompletionItem::insertTextMode),
            make_field("textEdit",            &CompletionItem::textEdit),
            make_field("textEditText",        &CompletionItem::textEditText),
            make_field("additionalTextEdits", &CompletionItem::additionalTextEdits),
            make_field("commitCharacters",    &CompletionItem::commitCharacters),
            make_field("command",             &CompletionItem::command),
            make_field("data",                &CompletionItem::data)
        );
    };

    /**
     * Represents a collection of {@link CompletionItem completion items} to be presented
     * in the editor.
     */
    struct CompletionList {
        /**
         * This list it not complete. Further typing results in recomputing this list.
         *
         * Recomputed lists have all their items replaced (not appended) in the
         * incomplete completion sessions.
         */
        Bool                    isIncomplete;
        /**
         * In many cases the items of an actual completion result share the same
         * value for properties like `commitCharacters` or the range of a text
         * edit. A completion list can therefore define item defaults which will
         * be used if a completion item itself doesn't specify the value.
         *
         * If a completion list specifies a default value and a completion item
         * also specifies a corresponding value the one from the item is used.
         *
         * Servers are only allowed to return default values if the client
         * signals support for this via the `completionList.itemDefaults`
         * capability.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        struct _ItemDefaults {
            /**
             * A default commit character set.
             *
             * @since 3.17.0
             * @since 3.17.0
             */
            Slice<String>     commitCharacters;
            /**
             * A default edit range.
             *
             * @since 3.17.0
             * @since 3.17.0
             */
            Range*            editRange;
            /**
             * A default insert text format.
             *
             * @since 3.17.0
             * @since 3.17.0
             */
            InsertTextFormat  insertTextFormat;
            /**
             * A default insert text mode.
             *
             * @since 3.17.0
             * @since 3.17.0
             */
            InsertTextMode    insertTextMode;
            /**
             * A default data value.
             *
             * @since 3.17.0
             * @since 3.17.0
             */
            String            data;
        }* itemDefaults;
        /**
         * The completion items.
         */
        Slice<CompletionItem*>  items;
    };

    template<> struct Schema<CompletionList> {
        static constexpr auto fields = std::make_tuple(
            make_field("isIncomplete", &CompletionList::isIncomplete),
            make_field("itemDefaults", &CompletionList::itemDefaults),
            make_field("items",        &CompletionList::items)
        );
    };

    template<> struct Schema<CompletionList::_ItemDefaults> {
        static constexpr auto fields = std::make_tuple(
            make_field("commitCharacters", &CompletionList::_ItemDefaults::commitCharacters),
            make_field("editRange",        &CompletionList::_ItemDefaults::editRange),
            make_field("insertTextFormat", &CompletionList::_ItemDefaults::insertTextFormat),
            make_field("insertTextMode",   &CompletionList::_ItemDefaults::insertTextMode),
            make_field("data",             &CompletionList::_ItemDefaults::data)
        );
    };

    /**
     * Completion options.
     */
    struct CompletionOptions {
        /**
         * Most tools trigger completion request automatically without explicitly requesting
         * it using a keyboard shortcut (e.g. Ctrl+Space). Typically they do so when the user
         * starts to type an identifier. For example if the user types `c` in a JavaScript file
         * code complete will automatically pop up present `console` besides others as a
         * completion item. Characters that make up identifiers don't need to be listed here.
         *
         * If code complete should automatically be trigger on characters not being valid inside
         * an identifier (for example `.` in JavaScript) list them in `triggerCharacters`.
         */
        Slice<String>     triggerCharacters;
        /**
         * The list of all possible characters that commit a completion. This field can be used
         * if clients don't support individual commit characters per completion item. See
         * `ClientCapabilities.textDocument.completion.completionItem.commitCharactersSupport`
         *
         * If a server provides both `allCommitCharacters` and commit characters on an individual
         * completion item the ones on the completion item win.
         *
         * @since 3.2.0
         * @since 3.2.0
         */
        Slice<String>     allCommitCharacters;
        /**
         * The server provides support to resolve additional
         * information for a completion item.
         */
        Bool              resolveProvider;
        /**
         * The server supports the following `CompletionItem` specific
         * capabilities.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        struct _CompletionItem {
            /**
             * The server has support for completion item label
             * details (see also `CompletionItemLabelDetails`) when
             * receiving a completion item in a resolve call.
             *
             * @since 3.17.0
             * @since 3.17.0
             */
            Bool  labelDetailsSupport;
        }* completionItem;
    };

    template<> struct Schema<CompletionOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("triggerCharacters",   &CompletionOptions::triggerCharacters),
            make_field("allCommitCharacters", &CompletionOptions::allCommitCharacters),
            make_field("resolveProvider",     &CompletionOptions::resolveProvider),
            make_field("completionItem",      &CompletionOptions::completionItem)
        );
    };

    template<> struct Schema<CompletionOptions::_CompletionItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("labelDetailsSupport", &CompletionOptions::_CompletionItem::labelDetailsSupport)
        );
    };

    /**
     * Registration options for a {@link CompletionRequest}.
     */
    struct CompletionRegistrationOptions : public TextDocumentRegistrationOptions, public CompletionOptions {
    };

    template<> struct Schema<CompletionRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector",    &CompletionRegistrationOptions::documentSelector),
            make_field("triggerCharacters",   &CompletionRegistrationOptions::triggerCharacters),
            make_field("allCommitCharacters", &CompletionRegistrationOptions::allCommitCharacters),
            make_field("resolveProvider",     &CompletionRegistrationOptions::resolveProvider),
            make_field("completionItem",      &CompletionRegistrationOptions::completionItem)
        );
    };

    /**
     * Parameters for a {@link HoverRequest}.
     */
    struct HoverParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<HoverParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &HoverParams::textDocument),
            make_field("position",     &HoverParams::position)
        );
    };

    /**
     * The result of a hover request.
     */
    struct Hover {
        /**
         * The hover's content
         */
        MarkupContent*  contents;
        /**
         * An optional range inside the text document that is used to
         * visualize the hover, e.g. by changing the background color.
         */
        Range*          range;
    };

    template<> struct Schema<Hover> {
        static constexpr auto fields = std::make_tuple(
            make_field("contents", &Hover::contents),
            make_field("range",    &Hover::range)
        );
    };

    /**
     * Hover options.
     */
    struct HoverOptions {
    };

    template<> struct Schema<HoverOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * Registration options for a {@link HoverRequest}.
     */
    struct HoverRegistrationOptions : public TextDocumentRegistrationOptions, public HoverOptions {
    };

    template<> struct Schema<HoverRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &HoverRegistrationOptions::documentSelector)
        );
    };

    /**
     * Parameters for a {@link SignatureHelpRequest}.
     */
    struct SignatureHelpParams : public TextDocumentPositionParams {
        /**
         * The signature help context. This is only available if the client specifies
         * to send this using the client capability `textDocument.signatureHelp.contextSupport === true`
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        SignatureHelpContext*  context;
    };

    template<> struct Schema<SignatureHelpParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &SignatureHelpParams::textDocument),
            make_field("position",     &SignatureHelpParams::position),
            make_field("context",      &SignatureHelpParams::context)
        );
    };

    /**
     * Signature help represents the signature of something
     * callable. There can be multiple signature but only one
     * active and only one active parameter.
     */
    struct SignatureHelp {
        /**
         * One or more signatures.
         */
        Slice<SignatureInformation*>  signatures;
        /**
         * The active signature. If omitted or the value lies outside the
         * range of `signatures` the value defaults to zero or is ignored if
         * the `SignatureHelp` has no signatures.
         *
         * Whenever possible implementors should make an active decision about
         * the active signature and shouldn't rely on a default value.
         *
         * In future version of the protocol this property might become
         * mandatory to better express this.
         */
        UInt                          activeSignature;
        /**
         * The active parameter of the active signature. If omitted or the value
         * lies outside the range of `signatures[activeSignature].parameters`
         * defaults to 0 if the active signature has parameters. If
         * the active signature has no parameters it is ignored.
         * In future version of the protocol this property might become
         * mandatory to better express the active parameter if the
         * active signature does have any.
         */
        UInt                          activeParameter;
    };

    template<> struct Schema<SignatureHelp> {
        static constexpr auto fields = std::make_tuple(
            make_field("signatures",      &SignatureHelp::signatures),
            make_field("activeSignature", &SignatureHelp::activeSignature),
            make_field("activeParameter", &SignatureHelp::activeParameter)
        );
    };

    /**
     * Server Capabilities for a {@link SignatureHelpRequest}.
     */
    struct SignatureHelpOptions {
        /**
         * List of characters that trigger signature help automatically.
         */
        Slice<String>  triggerCharacters;
        /**
         * List of characters that re-trigger signature help.
         *
         * These trigger characters are only active when signature help is already showing. All trigger characters
         * are also counted as re-trigger characters.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Slice<String>  retriggerCharacters;
    };

    template<> struct Schema<SignatureHelpOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("triggerCharacters",   &SignatureHelpOptions::triggerCharacters),
            make_field("retriggerCharacters", &SignatureHelpOptions::retriggerCharacters)
        );
    };

    /**
     * Registration options for a {@link SignatureHelpRequest}.
     */
    struct SignatureHelpRegistrationOptions : public TextDocumentRegistrationOptions, public SignatureHelpOptions {
    };

    template<> struct Schema<SignatureHelpRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector",    &SignatureHelpRegistrationOptions::documentSelector),
            make_field("triggerCharacters",   &SignatureHelpRegistrationOptions::triggerCharacters),
            make_field("retriggerCharacters", &SignatureHelpRegistrationOptions::retriggerCharacters)
        );
    };

    /**
     * Parameters for a {@link DefinitionRequest}.
     */
    struct DefinitionParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<DefinitionParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DefinitionParams::textDocument),
            make_field("position",     &DefinitionParams::position)
        );
    };

    /**
     * Server Capabilities for a {@link DefinitionRequest}.
     */
    struct DefinitionOptions {
    };

    template<> struct Schema<DefinitionOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * Registration options for a {@link DefinitionRequest}.
     */
    struct DefinitionRegistrationOptions : public TextDocumentRegistrationOptions, public DefinitionOptions {
    };

    template<> struct Schema<DefinitionRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &DefinitionRegistrationOptions::documentSelector)
        );
    };

    /**
     * Parameters for a {@link ReferencesRequest}.
     */
    struct ReferenceParams : public TextDocumentPositionParams {
        ReferenceContext*  context;
    };

    template<> struct Schema<ReferenceParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &ReferenceParams::textDocument),
            make_field("position",     &ReferenceParams::position),
            make_field("context",      &ReferenceParams::context)
        );
    };

    /**
     * Reference options.
     */
    struct ReferenceOptions {
    };

    template<> struct Schema<ReferenceOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * Registration options for a {@link ReferencesRequest}.
     */
    struct ReferenceRegistrationOptions : public TextDocumentRegistrationOptions, public ReferenceOptions {
    };

    template<> struct Schema<ReferenceRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &ReferenceRegistrationOptions::documentSelector)
        );
    };

    /**
     * Parameters for a {@link DocumentHighlightRequest}.
     */
    struct DocumentHighlightParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<DocumentHighlightParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DocumentHighlightParams::textDocument),
            make_field("position",     &DocumentHighlightParams::position)
        );
    };

    /**
     * A document highlight is a range inside a text document which deserves
     * special attention. Usually a document highlight is visualized by changing
     * the background color of its range.
     */
    struct DocumentHighlight {
        /**
         * The range this highlight applies to.
         */
        Range*                 range;
        /**
         * The highlight kind, default is {@link DocumentHighlightKind.Text text}.
         */
        DocumentHighlightKind  kind;
    };

    template<> struct Schema<DocumentHighlight> {
        static constexpr auto fields = std::make_tuple(
            make_field("range", &DocumentHighlight::range),
            make_field("kind",  &DocumentHighlight::kind)
        );
    };

    /**
     * Provider options for a {@link DocumentHighlightRequest}.
     */
    struct DocumentHighlightOptions {
    };

    template<> struct Schema<DocumentHighlightOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * Registration options for a {@link DocumentHighlightRequest}.
     */
    struct DocumentHighlightRegistrationOptions : public TextDocumentRegistrationOptions, public DocumentHighlightOptions {
    };

    template<> struct Schema<DocumentHighlightRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &DocumentHighlightRegistrationOptions::documentSelector)
        );
    };

    /**
     * Parameters for a {@link DocumentSymbolRequest}.
     */
    struct DocumentSymbolParams {
        /**
         * The text document.
         */
        TextDocumentIdentifier*  textDocument;
    };

    template<> struct Schema<DocumentSymbolParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DocumentSymbolParams::textDocument)
        );
    };

    /**
     * A base for all symbol information.
     */
    struct BaseSymbolInformation {
        /**
         * The name of this symbol.
         */
        String            name;
        /**
         * The kind of this symbol.
         */
        SymbolKind        kind;
        /**
         * Tags for this symbol.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Slice<SymbolTag>  tags;
        /**
         * The name of the symbol containing this symbol. This information is for
         * user interface purposes (e.g. to render a qualifier in the user interface
         * if necessary). It can't be used to re-infer a hierarchy for the document
         * symbols.
         */
        String            containerName;
    };

    template<> struct Schema<BaseSymbolInformation> {
        static constexpr auto fields = std::make_tuple(
            make_field("name",          &BaseSymbolInformation::name),
            make_field("kind",          &BaseSymbolInformation::kind),
            make_field("tags",          &BaseSymbolInformation::tags),
            make_field("containerName", &BaseSymbolInformation::containerName)
        );
    };

    /**
     * Represents information about programming constructs like variables, classes,
     * interfaces etc.
     */
    struct SymbolInformation : public BaseSymbolInformation {
        /**
         * Indicates if this symbol is deprecated.
         *
         * @deprecated Use tags instead
         */
        Bool       deprecated;
        /**
         * The location of this symbol. The location's range is used by a tool
         * to reveal the location in the editor. If the symbol is selected in the
         * tool the range's start information is used to position the cursor. So
         * the range usually spans more than the actual symbol's name and does
         * normally include things like visibility modifiers.
         *
         * The range doesn't have to denote a node range in the sense of an abstract
         * syntax tree. It can therefore not be used to re-construct a hierarchy of
         * the symbols.
         */
        Location*  location;
    };

    template<> struct Schema<SymbolInformation> {
        static constexpr auto fields = std::make_tuple(
            make_field("name",          &SymbolInformation::name),
            make_field("kind",          &SymbolInformation::kind),
            make_field("tags",          &SymbolInformation::tags),
            make_field("containerName", &SymbolInformation::containerName),
            make_field("deprecated",    &SymbolInformation::deprecated),
            make_field("location",      &SymbolInformation::location)
        );
    };

    /**
     * Represents programming constructs like variables, classes, interfaces etc.
     * that appear in a document. Document symbols can be hierarchical and they
     * have two ranges: one that encloses its definition and one that points to
     * its most interesting range, e.g. the range of an identifier.
     */
    struct DocumentSymbol {
        /**
         * The name of this symbol. Will be displayed in the user interface and therefore must not be
         * an empty string or a string only consisting of white spaces.
         */
        String                  name;
        /**
         * More detail for this symbol, e.g the signature of a function.
         */
        String                  detail;
        /**
         * The kind of this symbol.
         */
        SymbolKind              kind;
        /**
         * Tags for this document symbol.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Slice<SymbolTag>        tags;
        /**
         * Indicates if this symbol is deprecated.
         *
         * @deprecated Use tags instead
         */
        Bool                    deprecated;
        /**
         * The range enclosing this symbol not including leading/trailing whitespace but everything else
         * like comments. This information is typically used to determine if the clients cursor is
         * inside the symbol to reveal in the symbol in the UI.
         */
        Range*                  range;
        /**
         * The range that should be selected and revealed when this symbol is being picked, e.g the name of a function.
         * Must be contained by the `range`.
         */
        Range*                  selectionRange;
        /**
         * Children of this symbol, e.g. properties of a class.
         */
        Slice<DocumentSymbol*>  children;
    };

    template<> struct Schema<DocumentSymbol> {
        static constexpr auto fields = std::make_tuple(
            make_field("name",           &DocumentSymbol::name),
            make_field("detail",         &DocumentSymbol::detail),
            make_field("kind",           &DocumentSymbol::kind),
            make_field("tags",           &DocumentSymbol::tags),
            make_field("deprecated",     &DocumentSymbol::deprecated),
            make_field("range",          &DocumentSymbol::range),
            make_field("selectionRange", &DocumentSymbol::selectionRange),
            make_field("children",       &DocumentSymbol::children)
        );
    };

    /**
     * Provider options for a {@link DocumentSymbolRequest}.
     */
    struct DocumentSymbolOptions {
        /**
         * A human-readable string that is shown when multiple outlines trees
         * are shown for the same document.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        String  label;
    };

    template<> struct Schema<DocumentSymbolOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("label", &DocumentSymbolOptions::label)
        );
    };

    /**
     * Registration options for a {@link DocumentSymbolRequest}.
     */
    struct DocumentSymbolRegistrationOptions : public TextDocumentRegistrationOptions, public DocumentSymbolOptions {
    };

    template<> struct Schema<DocumentSymbolRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &DocumentSymbolRegistrationOptions::documentSelector),
            make_field("label",            &DocumentSymbolRegistrationOptions::label)
        );
    };

    /**
     * The parameters of a {@link CodeActionRequest}.
     */
    struct CodeActionParams {
        /**
         * The document in which the command was invoked.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The range for which the command was invoked.
         */
        Range*                   range;
        /**
         * Context carrying additional information.
         */
        CodeActionContext*       context;
    };

    template<> struct Schema<CodeActionParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &CodeActionParams::textDocument),
            make_field("range",        &CodeActionParams::range),
            make_field("context",      &CodeActionParams::context)
        );
    };

    /**
     * Represents a reference to a command. Provides a title which
     * will be used to represent a command in the UI and, optionally,
     * an array of arguments which will be passed to the command handler
     * function when invoked.
     */
    struct Command {
        /**
         * Title of the command, like `save`.
         */
        String         title;
        /**
         * The identifier of the actual command handler.
         */
        String         command;
        /**
         * Arguments that the command handler should be
         * invoked with.
         */
        Slice<String>  arguments;
    };

    template<> struct Schema<Command> {
        static constexpr auto fields = std::make_tuple(
            make_field("title",     &Command::title),
            make_field("command",   &Command::command),
            make_field("arguments", &Command::arguments)
        );
    };

    /**
     * A code action represents a change that can be performed in code, e.g. to fix a problem or
     * to refactor code.
     *
     * A CodeAction must set either `edit` and/or a `command`. If both are supplied, the `edit` is applied first, then the `command` is executed.
     */
    struct CodeAction {
        /**
         * A short, human-readable, title for this code action.
         */
        String              title;
        /**
         * The kind of the code action.
         *
         * Used to filter code actions.
         */
        CodeActionKind      kind;
        /**
         * The diagnostics that this code action resolves.
         */
        Slice<Diagnostic*>  diagnostics;
        /**
         * Marks this as a preferred action. Preferred actions are used by the `auto fix` command and can be targeted
         * by keybindings.
         *
         * A quick fix should be marked preferred if it properly addresses the underlying error.
         * A refactoring should be marked preferred if it is the most reasonable choice of actions to take.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Bool                isPreferred;
        /**
         * Marks that the code action cannot currently be applied.
         *
         * Clients should follow the following guidelines regarding disabled code actions:
         *
         * - Disabled code actions are not shown in automatic [lightbulbs](https://code.visualstudio.com/docs/editor/editingevolved#_code-action)
         * code action menus.
         *
         * - Disabled actions are shown as faded out in the code action menu when the user requests a more specific type
         * of code action, such as refactorings.
         *
         * - If the user has a [keybinding](https://code.visualstudio.com/docs/editor/refactoring#_keybindings-for-code-actions)
         * that auto applies a code action and only disabled code actions are returned, the client should show the user an
         * error message with `reason` in the editor.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        struct _Disabled {
            /**
             * Human readable description of why the code action is currently disabled.
             *
             * This is displayed in the code actions UI.
             */
            String  reason;
        }* disabled;
        /**
         * The workspace edit this code action performs.
         */
        WorkspaceEdit*      edit;
        /**
         * A command this code action executes. If a code action
         * provides an edit and a command, first the edit is
         * executed and then the command.
         */
        Command*            command;
        /**
         * A data entry field that is preserved on a code action between
         * a `textDocument/codeAction` and a `codeAction/resolve` request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        String              data;
    };

    template<> struct Schema<CodeAction> {
        static constexpr auto fields = std::make_tuple(
            make_field("title",       &CodeAction::title),
            make_field("kind",        &CodeAction::kind),
            make_field("diagnostics", &CodeAction::diagnostics),
            make_field("isPreferred", &CodeAction::isPreferred),
            make_field("disabled",    &CodeAction::disabled),
            make_field("edit",        &CodeAction::edit),
            make_field("command",     &CodeAction::command),
            make_field("data",        &CodeAction::data)
        );
    };

    template<> struct Schema<CodeAction::_Disabled> {
        static constexpr auto fields = std::make_tuple(
            make_field("reason", &CodeAction::_Disabled::reason)
        );
    };

    /**
     * Provider options for a {@link CodeActionRequest}.
     */
    struct CodeActionOptions {
        /**
         * CodeActionKinds that this server may return.
         *
         * The list of kinds may be generic, such as `CodeActionKind.Refactor`, or the server
         * may list out every specific kind they provide.
         */
        Slice<CodeActionKind>  codeActionKinds;
        /**
         * The server provides support to resolve additional
         * information for a code action.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool                   resolveProvider;
    };

    template<> struct Schema<CodeActionOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("codeActionKinds", &CodeActionOptions::codeActionKinds),
            make_field("resolveProvider", &CodeActionOptions::resolveProvider)
        );
    };

    /**
     * Registration options for a {@link CodeActionRequest}.
     */
    struct CodeActionRegistrationOptions : public TextDocumentRegistrationOptions, public CodeActionOptions {
    };

    template<> struct Schema<CodeActionRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &CodeActionRegistrationOptions::documentSelector),
            make_field("codeActionKinds",  &CodeActionRegistrationOptions::codeActionKinds),
            make_field("resolveProvider",  &CodeActionRegistrationOptions::resolveProvider)
        );
    };

    /**
     * The parameters of a {@link WorkspaceSymbolRequest}.
     */
    struct WorkspaceSymbolParams {
        /**
         * A query string to filter symbols by. Clients may send an empty
         * string here to request all symbols.
         */
        String  query;
    };

    template<> struct Schema<WorkspaceSymbolParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("query", &WorkspaceSymbolParams::query)
        );
    };

    /**
     * A special workspace symbol that supports locations without a range.
     *
     * See also SymbolInformation.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct WorkspaceSymbol : public BaseSymbolInformation {
        /**
         * The location of the symbol. Whether a server is allowed to
         * return a location without a range depends on the client
         * capability `workspace.symbol.resolveSupport`.
         *
         * See SymbolInformation#location for more details.
         */
        Location*  location;
        /**
         * A data entry field that is preserved on a workspace symbol between a
         * workspace symbol request and a workspace symbol resolve request.
         */
        String     data;
    };

    template<> struct Schema<WorkspaceSymbol> {
        static constexpr auto fields = std::make_tuple(
            make_field("name",          &WorkspaceSymbol::name),
            make_field("kind",          &WorkspaceSymbol::kind),
            make_field("tags",          &WorkspaceSymbol::tags),
            make_field("containerName", &WorkspaceSymbol::containerName),
            make_field("location",      &WorkspaceSymbol::location),
            make_field("data",          &WorkspaceSymbol::data)
        );
    };

    /**
     * Server capabilities for a {@link WorkspaceSymbolRequest}.
     */
    struct WorkspaceSymbolOptions {
        /**
         * The server provides support to resolve additional
         * information for a workspace symbol.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        Bool  resolveProvider;
    };

    template<> struct Schema<WorkspaceSymbolOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("resolveProvider", &WorkspaceSymbolOptions::resolveProvider)
        );
    };

    /**
     * Registration options for a {@link WorkspaceSymbolRequest}.
     */
    struct WorkspaceSymbolRegistrationOptions : public WorkspaceSymbolOptions {
    };

    template<> struct Schema<WorkspaceSymbolRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("resolveProvider", &WorkspaceSymbolRegistrationOptions::resolveProvider)
        );
    };

    /**
     * The parameters of a {@link CodeLensRequest}.
     */
    struct CodeLensParams {
        /**
         * The document to request code lens for.
         */
        TextDocumentIdentifier*  textDocument;
    };

    template<> struct Schema<CodeLensParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &CodeLensParams::textDocument)
        );
    };

    /**
     * A code lens represents a {@link Command command} that should be shown along with
     * source text, like the number of references, a way to run tests, etc.
     *
     * A code lens is _unresolved_ when no command is associated to it. For performance
     * reasons the creation of a code lens and resolving should be done in two stages.
     */
    struct CodeLens {
        /**
         * The range in which this code lens is valid. Should only span a single line.
         */
        Range*    range;
        /**
         * The command this code lens represents.
         */
        Command*  command;
        /**
         * A data entry field that is preserved on a code lens item between
         * a {@link CodeLensRequest} and a {@link CodeLensResolveRequest}
         */
        String    data;
    };

    template<> struct Schema<CodeLens> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",   &CodeLens::range),
            make_field("command", &CodeLens::command),
            make_field("data",    &CodeLens::data)
        );
    };

    /**
     * Code Lens provider options of a {@link CodeLensRequest}.
     */
    struct CodeLensOptions {
        /**
         * Code lens has a resolve provider as well.
         */
        Bool  resolveProvider;
    };

    template<> struct Schema<CodeLensOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("resolveProvider", &CodeLensOptions::resolveProvider)
        );
    };

    /**
     * Registration options for a {@link CodeLensRequest}.
     */
    struct CodeLensRegistrationOptions : public TextDocumentRegistrationOptions, public CodeLensOptions {
    };

    template<> struct Schema<CodeLensRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &CodeLensRegistrationOptions::documentSelector),
            make_field("resolveProvider",  &CodeLensRegistrationOptions::resolveProvider)
        );
    };

    /**
     * The parameters of a {@link DocumentLinkRequest}.
     */
    struct DocumentLinkParams {
        /**
         * The document to provide document links for.
         */
        TextDocumentIdentifier*  textDocument;
    };

    template<> struct Schema<DocumentLinkParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DocumentLinkParams::textDocument)
        );
    };

    /**
     * A document link is a range in a text document that links to an internal or external resource, like another
     * text document or a web site.
     */
    struct DocumentLink {
        /**
         * The range this link applies to.
         */
        Range*  range;
        /**
         * The uri this link points to. If missing a resolve request is sent later.
         */
        String  target;
        /**
         * The tooltip text when you hover over this link.
         *
         * If a tooltip is provided, is will be displayed in a string that includes instructions on how to
         * trigger the link, such as `{0} (ctrl + click)`. The specific instructions vary depending on OS,
         * user settings, and localization.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        String  tooltip;
        /**
         * A data entry field that is preserved on a document link between a
         * DocumentLinkRequest and a DocumentLinkResolveRequest.
         */
        String  data;
    };

    template<> struct Schema<DocumentLink> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",   &DocumentLink::range),
            make_field("target",  &DocumentLink::target),
            make_field("tooltip", &DocumentLink::tooltip),
            make_field("data",    &DocumentLink::data)
        );
    };

    /**
     * Provider options for a {@link DocumentLinkRequest}.
     */
    struct DocumentLinkOptions {
        /**
         * Document links have a resolve provider as well.
         */
        Bool  resolveProvider;
    };

    template<> struct Schema<DocumentLinkOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("resolveProvider", &DocumentLinkOptions::resolveProvider)
        );
    };

    /**
     * Registration options for a {@link DocumentLinkRequest}.
     */
    struct DocumentLinkRegistrationOptions : public TextDocumentRegistrationOptions, public DocumentLinkOptions {
    };

    template<> struct Schema<DocumentLinkRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &DocumentLinkRegistrationOptions::documentSelector),
            make_field("resolveProvider",  &DocumentLinkRegistrationOptions::resolveProvider)
        );
    };

    /**
     * The parameters of a {@link DocumentFormattingRequest}.
     */
    struct DocumentFormattingParams {
        /**
         * The document to format.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The format options.
         */
        FormattingOptions*       options;
    };

    template<> struct Schema<DocumentFormattingParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DocumentFormattingParams::textDocument),
            make_field("options",      &DocumentFormattingParams::options)
        );
    };

    /**
     * Provider options for a {@link DocumentFormattingRequest}.
     */
    struct DocumentFormattingOptions {
    };

    template<> struct Schema<DocumentFormattingOptions> {
        static constexpr auto fields = std::make_tuple(

        );
    };

    /**
     * Registration options for a {@link DocumentFormattingRequest}.
     */
    struct DocumentFormattingRegistrationOptions : public TextDocumentRegistrationOptions, public DocumentFormattingOptions {
    };

    template<> struct Schema<DocumentFormattingRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &DocumentFormattingRegistrationOptions::documentSelector)
        );
    };

    /**
     * The parameters of a {@link DocumentRangeFormattingRequest}.
     */
    struct DocumentRangeFormattingParams {
        /**
         * The document to format.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The range to format
         */
        Range*                   range;
        /**
         * The format options
         */
        FormattingOptions*       options;
    };

    template<> struct Schema<DocumentRangeFormattingParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DocumentRangeFormattingParams::textDocument),
            make_field("range",        &DocumentRangeFormattingParams::range),
            make_field("options",      &DocumentRangeFormattingParams::options)
        );
    };

    /**
     * Provider options for a {@link DocumentRangeFormattingRequest}.
     */
    struct DocumentRangeFormattingOptions {
        /**
         * Whether the server supports formatting multiple ranges at once.
         *
         * @since 3.18.0
         * @proposed
         * @since 3.18.0
         */
        Bool  rangesSupport;
    };

    template<> struct Schema<DocumentRangeFormattingOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("rangesSupport", &DocumentRangeFormattingOptions::rangesSupport)
        );
    };

    /**
     * Registration options for a {@link DocumentRangeFormattingRequest}.
     */
    struct DocumentRangeFormattingRegistrationOptions : public TextDocumentRegistrationOptions, public DocumentRangeFormattingOptions {
    };

    template<> struct Schema<DocumentRangeFormattingRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &DocumentRangeFormattingRegistrationOptions::documentSelector),
            make_field("rangesSupport",    &DocumentRangeFormattingRegistrationOptions::rangesSupport)
        );
    };

    /**
     * The parameters of a {@link DocumentRangesFormattingRequest}.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct DocumentRangesFormattingParams {
        /**
         * The document to format.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The ranges to format
         */
        Slice<Range*>            ranges;
        /**
         * The format options
         */
        FormattingOptions*       options;
    };

    template<> struct Schema<DocumentRangesFormattingParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DocumentRangesFormattingParams::textDocument),
            make_field("ranges",       &DocumentRangesFormattingParams::ranges),
            make_field("options",      &DocumentRangesFormattingParams::options)
        );
    };

    /**
     * The parameters of a {@link DocumentOnTypeFormattingRequest}.
     */
    struct DocumentOnTypeFormattingParams {
        /**
         * The document to format.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The position around which the on type formatting should happen.
         * This is not necessarily the exact position where the character denoted
         * by the property `ch` got typed.
         */
        Position*                position;
        /**
         * The character that has been typed that triggered the formatting
         * on type request. That is not necessarily the last character that
         * got inserted into the document since the client could auto insert
         * characters as well (e.g. like automatic brace completion).
         */
        String                   ch;
        /**
         * The formatting options.
         */
        FormattingOptions*       options;
    };

    template<> struct Schema<DocumentOnTypeFormattingParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &DocumentOnTypeFormattingParams::textDocument),
            make_field("position",     &DocumentOnTypeFormattingParams::position),
            make_field("ch",           &DocumentOnTypeFormattingParams::ch),
            make_field("options",      &DocumentOnTypeFormattingParams::options)
        );
    };

    /**
     * Provider options for a {@link DocumentOnTypeFormattingRequest}.
     */
    struct DocumentOnTypeFormattingOptions {
        /**
         * A character on which formatting should be triggered, like `{`.
         */
        String         firstTriggerCharacter;
        /**
         * More trigger characters.
         */
        Slice<String>  moreTriggerCharacter;
    };

    template<> struct Schema<DocumentOnTypeFormattingOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("firstTriggerCharacter", &DocumentOnTypeFormattingOptions::firstTriggerCharacter),
            make_field("moreTriggerCharacter",  &DocumentOnTypeFormattingOptions::moreTriggerCharacter)
        );
    };

    /**
     * Registration options for a {@link DocumentOnTypeFormattingRequest}.
     */
    struct DocumentOnTypeFormattingRegistrationOptions : public TextDocumentRegistrationOptions, public DocumentOnTypeFormattingOptions {
    };

    template<> struct Schema<DocumentOnTypeFormattingRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector",      &DocumentOnTypeFormattingRegistrationOptions::documentSelector),
            make_field("firstTriggerCharacter", &DocumentOnTypeFormattingRegistrationOptions::firstTriggerCharacter),
            make_field("moreTriggerCharacter",  &DocumentOnTypeFormattingRegistrationOptions::moreTriggerCharacter)
        );
    };

    /**
     * The parameters of a {@link RenameRequest}.
     */
    struct RenameParams {
        /**
         * The document to rename.
         */
        TextDocumentIdentifier*  textDocument;
        /**
         * The position at which this request was sent.
         */
        Position*                position;
        /**
         * The new name of the symbol. If the given name is not valid the
         * request must return a {@link ResponseError} with an
         * appropriate message set.
         */
        String                   newName;
    };

    template<> struct Schema<RenameParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &RenameParams::textDocument),
            make_field("position",     &RenameParams::position),
            make_field("newName",      &RenameParams::newName)
        );
    };

    /**
     * Provider options for a {@link RenameRequest}.
     */
    struct RenameOptions {
        /**
         * Renames should be checked and tested before being executed.
         *
         * @since version 3.12.0
         * @since version 3.12.0
         */
        Bool  prepareProvider;
    };

    template<> struct Schema<RenameOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("prepareProvider", &RenameOptions::prepareProvider)
        );
    };

    /**
     * Registration options for a {@link RenameRequest}.
     */
    struct RenameRegistrationOptions : public TextDocumentRegistrationOptions, public RenameOptions {
    };

    template<> struct Schema<RenameRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentSelector", &RenameRegistrationOptions::documentSelector),
            make_field("prepareProvider",  &RenameRegistrationOptions::prepareProvider)
        );
    };

    struct PrepareRenameParams : public TextDocumentPositionParams {
    };

    template<> struct Schema<PrepareRenameParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &PrepareRenameParams::textDocument),
            make_field("position",     &PrepareRenameParams::position)
        );
    };

    /**
     * The parameters of a {@link ExecuteCommandRequest}.
     */
    struct ExecuteCommandParams {
        /**
         * The identifier of the actual command handler.
         */
        String         command;
        /**
         * Arguments that the command should be invoked with.
         */
        Slice<String>  arguments;
    };

    template<> struct Schema<ExecuteCommandParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("command",   &ExecuteCommandParams::command),
            make_field("arguments", &ExecuteCommandParams::arguments)
        );
    };

    /**
     * The server capabilities of a {@link ExecuteCommandRequest}.
     */
    struct ExecuteCommandOptions {
        /**
         * The commands to be executed on the server
         */
        Slice<String>  commands;
    };

    template<> struct Schema<ExecuteCommandOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("commands", &ExecuteCommandOptions::commands)
        );
    };

    /**
     * Registration options for a {@link ExecuteCommandRequest}.
     */
    struct ExecuteCommandRegistrationOptions : public ExecuteCommandOptions {
    };

    template<> struct Schema<ExecuteCommandRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("commands", &ExecuteCommandRegistrationOptions::commands)
        );
    };

    /**
     * The parameters passed via an apply workspace edit request.
     */
    struct ApplyWorkspaceEditParams {
        /**
         * An optional label of the workspace edit. This label is
         * presented in the user interface for example on an undo
         * stack to undo the workspace edit.
         */
        String          label;
        /**
         * The edits to apply.
         */
        WorkspaceEdit*  edit;
    };

    template<> struct Schema<ApplyWorkspaceEditParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("label", &ApplyWorkspaceEditParams::label),
            make_field("edit",  &ApplyWorkspaceEditParams::edit)
        );
    };

    /**
     * The result returned from the apply workspace edit request.
     *
     * @since 3.17 renamed from ApplyWorkspaceEditResponse
     * @since 3.17 renamed from ApplyWorkspaceEditResponse
     */
    struct ApplyWorkspaceEditResult {
        /**
         * Indicates whether the edit was applied or not.
         */
        Bool    applied;
        /**
         * An optional textual description for why the edit was not applied.
         * This may be used by the server for diagnostic logging or to provide
         * a suitable error for a request that triggered the edit.
         */
        String  failureReason;
        /**
         * Depending on the client's failure handling strategy `failedChange` might
         * contain the index of the change that failed. This property is only available
         * if the client signals a `failureHandlingStrategy` in its client capabilities.
         */
        UInt    failedChange;
    };

    template<> struct Schema<ApplyWorkspaceEditResult> {
        static constexpr auto fields = std::make_tuple(
            make_field("applied",       &ApplyWorkspaceEditResult::applied),
            make_field("failureReason", &ApplyWorkspaceEditResult::failureReason),
            make_field("failedChange",  &ApplyWorkspaceEditResult::failedChange)
        );
    };

    struct WorkDoneProgressBegin {
        LSPAny  kind;
        /**
         * Mandatory title of the progress operation. Used to briefly inform about
         * the kind of operation being performed.
         *
         * Examples: "Indexing" or "Linking dependencies".
         */
        String  title;
        /**
         * Controls if a cancel button should show to allow the user to cancel the
         * long running operation. Clients that don't support cancellation are allowed
         * to ignore the setting.
         */
        Bool    cancellable;
        /**
         * Optional, more detailed associated progress message. Contains
         * complementary information to the `title`.
         *
         * Examples: "3/25 files", "project/src/module2", "node_modules/some_dep".
         * If unset, the previous progress message (if any) is still valid.
         */
        String  message;
        /**
         * Optional progress percentage to display (value 100 is considered 100%).
         * If not provided infinite progress is assumed and clients are allowed
         * to ignore the `percentage` value in subsequent report notifications.
         *
         * The value should be steadily rising. Clients are free to ignore values
         * that are not following this rule. The value range is [0, 100].
         */
        UInt    percentage;
    };

    template<> struct Schema<WorkDoneProgressBegin> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",        &WorkDoneProgressBegin::kind),
            make_field("title",       &WorkDoneProgressBegin::title),
            make_field("cancellable", &WorkDoneProgressBegin::cancellable),
            make_field("message",     &WorkDoneProgressBegin::message),
            make_field("percentage",  &WorkDoneProgressBegin::percentage)
        );
    };

    struct WorkDoneProgressReport {
        LSPAny  kind;
        /**
         * Controls enablement state of a cancel button.
         *
         * Clients that don't support cancellation or don't support controlling the button's
         * enablement state are allowed to ignore the property.
         */
        Bool    cancellable;
        /**
         * Optional, more detailed associated progress message. Contains
         * complementary information to the `title`.
         *
         * Examples: "3/25 files", "project/src/module2", "node_modules/some_dep".
         * If unset, the previous progress message (if any) is still valid.
         */
        String  message;
        /**
         * Optional progress percentage to display (value 100 is considered 100%).
         * If not provided infinite progress is assumed and clients are allowed
         * to ignore the `percentage` value in subsequent report notifications.
         *
         * The value should be steadily rising. Clients are free to ignore values
         * that are not following this rule. The value range is [0, 100].
         */
        UInt    percentage;
    };

    template<> struct Schema<WorkDoneProgressReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",        &WorkDoneProgressReport::kind),
            make_field("cancellable", &WorkDoneProgressReport::cancellable),
            make_field("message",     &WorkDoneProgressReport::message),
            make_field("percentage",  &WorkDoneProgressReport::percentage)
        );
    };

    struct WorkDoneProgressEnd {
        LSPAny  kind;
        /**
         * Optional, a final message indicating to for example indicate the outcome
         * of the operation.
         */
        String  message;
    };

    template<> struct Schema<WorkDoneProgressEnd> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",    &WorkDoneProgressEnd::kind),
            make_field("message", &WorkDoneProgressEnd::message)
        );
    };

    struct SetTraceParams {
        TraceValues  value;
    };

    template<> struct Schema<SetTraceParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("value", &SetTraceParams::value)
        );
    };

    struct LogTraceParams {
        String  message;
        String  verbose;
    };

    template<> struct Schema<LogTraceParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("message", &LogTraceParams::message),
            make_field("verbose", &LogTraceParams::verbose)
        );
    };

    struct CancelParams {
        /**
         * The request id to cancel.
         */
        String  id;
    };

    template<> struct Schema<CancelParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("id", &CancelParams::id)
        );
    };

    struct ProgressParams {
        /**
         * The progress token provided by the client or server.
         */
        String  token;
        /**
         * The progress data.
         */
        String  value;
    };

    template<> struct Schema<ProgressParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("token", &ProgressParams::token),
            make_field("value", &ProgressParams::value)
        );
    };

    struct WorkDoneProgressParams {
        /**
         * An optional token that a server can use to report work done progress.
         */
        String  workDoneToken;
    };

    template<> struct Schema<WorkDoneProgressParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("workDoneToken", &WorkDoneProgressParams::workDoneToken)
        );
    };

    struct PartialResultParams {
        /**
         * An optional token that a server can use to report partial results (e.g. streaming) to
         * the client.
         */
        String  partialResultToken;
    };

    template<> struct Schema<PartialResultParams> {
        static constexpr auto fields = std::make_tuple(
            make_field("partialResultToken", &PartialResultParams::partialResultToken)
        );
    };

    /**
     * Represents the connection of two locations. Provides additional metadata over normal {@link Location locations},
     * including an origin range.
     */
    struct LocationLink {
        /**
         * Span of the origin of this link.
         *
         * Used as the underlined span for mouse interaction. Defaults to the word range at
         * the definition position.
         */
        Range*  originSelectionRange;
        /**
         * The target resource identifier of this link.
         */
        String  targetUri;
        /**
         * The full target range of this link. If the target for example is a symbol then target range is the
         * range enclosing this symbol not including leading/trailing whitespace but everything else
         * like comments. This information is typically used to highlight the range in the editor.
         */
        Range*  targetRange;
        /**
         * The range that should be selected and revealed when this link is being followed, e.g the name of a function.
         * Must be contained by the `targetRange`. See also `DocumentSymbol#range`
         */
        Range*  targetSelectionRange;
    };

    template<> struct Schema<LocationLink> {
        static constexpr auto fields = std::make_tuple(
            make_field("originSelectionRange", &LocationLink::originSelectionRange),
            make_field("targetUri",            &LocationLink::targetUri),
            make_field("targetRange",          &LocationLink::targetRange),
            make_field("targetSelectionRange", &LocationLink::targetSelectionRange)
        );
    };

    /**
     * A range in a text document expressed as (zero-based) start and end positions.
     *
     * If you want to specify a range that contains a line including the line ending
     * character(s) then use an end position denoting the start of the next line.
     * For example:
     * ```ts
     * {
     * start: { line: 5, character: 23 }
     * end : { line 6, character : 0 }
     * }
     * ```
     */
    struct Range {
        /**
         * The range's start position.
         */
        Position*  start;
        /**
         * The range's end position.
         */
        Position*  end;
    };

    template<> struct Schema<Range> {
        static constexpr auto fields = std::make_tuple(
            make_field("start", &Range::start),
            make_field("end",   &Range::end)
        );
    };

    /**
     * Static registration options to be returned in the initialize
     * request.
     */
    struct StaticRegistrationOptions {
        /**
         * The id used to register the request. The id can be used to deregister
         * the request again. See also Registration#id.
         */
        String  id;
    };

    template<> struct Schema<StaticRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("id", &StaticRegistrationOptions::id)
        );
    };

    /**
     * The workspace folder change event.
     */
    struct WorkspaceFoldersChangeEvent {
        /**
         * The array of added workspace folders
         */
        Slice<WorkspaceFolder*>  added;
        /**
         * The array of the removed workspace folders
         */
        Slice<WorkspaceFolder*>  removed;
    };

    template<> struct Schema<WorkspaceFoldersChangeEvent> {
        static constexpr auto fields = std::make_tuple(
            make_field("added",   &WorkspaceFoldersChangeEvent::added),
            make_field("removed", &WorkspaceFoldersChangeEvent::removed)
        );
    };

    struct ConfigurationItem {
        /**
         * The scope to get the configuration section for.
         */
        String  scopeUri;
        /**
         * The configuration section asked for.
         */
        String  section;
    };

    template<> struct Schema<ConfigurationItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("scopeUri", &ConfigurationItem::scopeUri),
            make_field("section",  &ConfigurationItem::section)
        );
    };

    /**
     * A literal to identify a text document in the client.
     */
    struct TextDocumentIdentifier {
        /**
         * The text document's uri.
         */
        String  uri;
    };

    template<> struct Schema<TextDocumentIdentifier> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri", &TextDocumentIdentifier::uri)
        );
    };

    /**
     * Represents a color in RGBA space.
     */
    struct Color {
        /**
         * The red component of this color in the range [0-1].
         */
        Double  red;
        /**
         * The green component of this color in the range [0-1].
         */
        Double  green;
        /**
         * The blue component of this color in the range [0-1].
         */
        Double  blue;
        /**
         * The alpha component of this color in the range [0-1].
         */
        Double  alpha;
    };

    template<> struct Schema<Color> {
        static constexpr auto fields = std::make_tuple(
            make_field("red",   &Color::red),
            make_field("green", &Color::green),
            make_field("blue",  &Color::blue),
            make_field("alpha", &Color::alpha)
        );
    };

    /**
     * Position in a text document expressed as zero-based line and character
     * offset. Prior to 3.17 the offsets were always based on a UTF-16 string
     * representation. So a string of the form `a𐐀b` the character offset of the
     * character `a` is 0, the character offset of `𐐀` is 1 and the character
     * offset of b is 3 since `𐐀` is represented using two code units in UTF-16.
     * Since 3.17 clients and servers can agree on a different string encoding
     * representation (e.g. UTF-8). The client announces it's supported encoding
     * via the client capability [`general.positionEncodings`](https://microsoft.github.io/language-server-protocol/specifications/specification-current/#clientCapabilities).
     * The value is an array of position encodings the client supports, with
     * decreasing preference (e.g. the encoding at index `0` is the most preferred
     * one). To stay backwards compatible the only mandatory encoding is UTF-16
     * represented via the string `utf-16`. The server can pick one of the
     * encodings offered by the client and signals that encoding back to the
     * client via the initialize result's property
     * [`capabilities.positionEncoding`](https://microsoft.github.io/language-server-protocol/specifications/specification-current/#serverCapabilities). If the string value
     * `utf-16` is missing from the client's capability `general.positionEncodings`
     * servers can safely assume that the client supports UTF-16. If the server
     * omits the position encoding in its initialize result the encoding defaults
     * to the string value `utf-16`. Implementation considerations: since the
     * conversion from one encoding into another requires the content of the
     * file / line the conversion is best done where the file is read which is
     * usually on the server side.
     *
     * Positions are line end character agnostic. So you can not specify a position
     * that denotes `\r|\n` or `\n|` where `|` represents the character offset.
     *
     * @since 3.17.0 - support for negotiated position encoding.
     * @since 3.17.0 - support for negotiated position encoding.
     */
    struct Position {
        /**
         * Line position in a document (zero-based).
         *
         * If a line number is greater than the number of lines in a document, it defaults back to the number of lines in the document.
         * If a line number is negative, it defaults to 0.
         */
        UInt  line;
        /**
         * Character offset on a line in a document (zero-based).
         *
         * The meaning of this offset is determined by the negotiated
         * `PositionEncodingKind`.
         *
         * If the character value is greater than the line length it defaults back to the
         * line length.
         */
        UInt  character;
    };

    template<> struct Schema<Position> {
        static constexpr auto fields = std::make_tuple(
            make_field("line",      &Position::line),
            make_field("character", &Position::character)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensEdit {
        /**
         * The start offset of the edit.
         */
        UInt         start;
        /**
         * The count of elements to remove.
         */
        UInt         deleteCount;
        /**
         * The elements to insert.
         */
        Slice<UInt>  data;
    };

    template<> struct Schema<SemanticTokensEdit> {
        static constexpr auto fields = std::make_tuple(
            make_field("start",       &SemanticTokensEdit::start),
            make_field("deleteCount", &SemanticTokensEdit::deleteCount),
            make_field("data",        &SemanticTokensEdit::data)
        );
    };

    /**
     * Represents information on a file/folder create.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct FileCreate {
        /**
         * A file:// URI for the location of the file/folder being created.
         */
        String  uri;
    };

    template<> struct Schema<FileCreate> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri", &FileCreate::uri)
        );
    };

    /**
     * Describes textual changes on a text document. A TextDocumentEdit describes all changes
     * on a document version Si and after they are applied move the document to version Si+1.
     * So the creator of a TextDocumentEdit doesn't need to sort the array of edits or do any
     * kind of ordering. However the edits must be non overlapping.
     */
    struct TextDocumentEdit {
        /**
         * The text document to change.
         */
        OptionalVersionedTextDocumentIdentifier*  textDocument;
        /**
         * The edits to be applied.
         *
         * @since 3.16.0 - support for AnnotatedTextEdit. This is guarded using a
         * client capability.
         * @since 3.16.0 - support for AnnotatedTextEdit. This is guarded using a
client capability.
         */
        Slice<TextEdit*>                          edits;
    };

    template<> struct Schema<TextDocumentEdit> {
        static constexpr auto fields = std::make_tuple(
            make_field("textDocument", &TextDocumentEdit::textDocument),
            make_field("edits",        &TextDocumentEdit::edits)
        );
    };

    /**
     * A generic resource operation.
     */
    struct ResourceOperation {
        /**
         * The resource operation kind.
         */
        String  kind;
        /**
         * An optional annotation identifier describing the operation.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        String  annotationId;
    };

    template<> struct Schema<ResourceOperation> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",         &ResourceOperation::kind),
            make_field("annotationId", &ResourceOperation::annotationId)
        );
    };

    /**
     * Create file operation.
     */
    struct CreateFile : public ResourceOperation {
        /**
         * A create
         */
        LSPAny              kind;
        /**
         * The resource to create.
         */
        String              uri;
        /**
         * Additional options
         */
        CreateFileOptions*  options;
    };

    template<> struct Schema<CreateFile> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",         &CreateFile::kind),
            make_field("annotationId", &CreateFile::annotationId),
            make_field("kind",         &CreateFile::kind),
            make_field("uri",          &CreateFile::uri),
            make_field("options",      &CreateFile::options)
        );
    };

    /**
     * Rename file operation
     */
    struct RenameFile : public ResourceOperation {
        /**
         * A rename
         */
        LSPAny              kind;
        /**
         * The old (existing) location.
         */
        String              oldUri;
        /**
         * The new location.
         */
        String              newUri;
        /**
         * Rename options.
         */
        RenameFileOptions*  options;
    };

    template<> struct Schema<RenameFile> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",         &RenameFile::kind),
            make_field("annotationId", &RenameFile::annotationId),
            make_field("kind",         &RenameFile::kind),
            make_field("oldUri",       &RenameFile::oldUri),
            make_field("newUri",       &RenameFile::newUri),
            make_field("options",      &RenameFile::options)
        );
    };

    /**
     * Delete file operation
     */
    struct DeleteFile : public ResourceOperation {
        /**
         * A delete
         */
        LSPAny              kind;
        /**
         * The file to delete.
         */
        String              uri;
        /**
         * Delete options.
         */
        DeleteFileOptions*  options;
    };

    template<> struct Schema<DeleteFile> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",         &DeleteFile::kind),
            make_field("annotationId", &DeleteFile::annotationId),
            make_field("kind",         &DeleteFile::kind),
            make_field("uri",          &DeleteFile::uri),
            make_field("options",      &DeleteFile::options)
        );
    };

    /**
     * Additional information that describes document changes.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct ChangeAnnotation {
        /**
         * A human-readable string describing the actual change. The string
         * is rendered prominent in the user interface.
         */
        String  label;
        /**
         * A flag which indicates that user confirmation is needed
         * before applying the change.
         */
        Bool    needsConfirmation;
        /**
         * A human-readable string which is rendered less prominent in
         * the user interface.
         */
        String  description;
    };

    template<> struct Schema<ChangeAnnotation> {
        static constexpr auto fields = std::make_tuple(
            make_field("label",             &ChangeAnnotation::label),
            make_field("needsConfirmation", &ChangeAnnotation::needsConfirmation),
            make_field("description",       &ChangeAnnotation::description)
        );
    };

    /**
     * A filter to describe in which file operation requests or notifications
     * the server is interested in receiving.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct FileOperationFilter {
        /**
         * A Uri scheme like `file` or `untitled`.
         */
        String                 scheme;
        /**
         * The actual file operation pattern.
         */
        FileOperationPattern*  pattern;
    };

    template<> struct Schema<FileOperationFilter> {
        static constexpr auto fields = std::make_tuple(
            make_field("scheme",  &FileOperationFilter::scheme),
            make_field("pattern", &FileOperationFilter::pattern)
        );
    };

    /**
     * Represents information on a file/folder rename.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct FileRename {
        /**
         * A file:// URI for the original location of the file/folder being renamed.
         */
        String  oldUri;
        /**
         * A file:// URI for the new location of the file/folder being renamed.
         */
        String  newUri;
    };

    template<> struct Schema<FileRename> {
        static constexpr auto fields = std::make_tuple(
            make_field("oldUri", &FileRename::oldUri),
            make_field("newUri", &FileRename::newUri)
        );
    };

    /**
     * Represents information on a file/folder delete.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct FileDelete {
        /**
         * A file:// URI for the location of the file/folder being deleted.
         */
        String  uri;
    };

    template<> struct Schema<FileDelete> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri", &FileDelete::uri)
        );
    };

    /**
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValueContext {
        /**
         * The stack frame (as a DAP Id) where the execution has stopped.
         */
        Int     frameId;
        /**
         * The document range where execution has stopped.
         * Typically the end position of the range denotes the line where the inline values are shown.
         */
        Range*  stoppedLocation;
    };

    template<> struct Schema<InlineValueContext> {
        static constexpr auto fields = std::make_tuple(
            make_field("frameId",         &InlineValueContext::frameId),
            make_field("stoppedLocation", &InlineValueContext::stoppedLocation)
        );
    };

    /**
     * Provide inline value as text.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValueText {
        /**
         * The document range for which the inline value applies.
         */
        Range*  range;
        /**
         * The text of the inline value.
         */
        String  text;
    };

    template<> struct Schema<InlineValueText> {
        static constexpr auto fields = std::make_tuple(
            make_field("range", &InlineValueText::range),
            make_field("text",  &InlineValueText::text)
        );
    };

    /**
     * Provide inline value through a variable lookup.
     * If only a range is specified, the variable name will be extracted from the underlying document.
     * An optional variable name can be used to override the extracted name.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValueVariableLookup {
        /**
         * The document range for which the inline value applies.
         * The range is used to extract the variable name from the underlying document.
         */
        Range*  range;
        /**
         * If specified the name of the variable to look up.
         */
        String  variableName;
        /**
         * How to perform the lookup.
         */
        Bool    caseSensitiveLookup;
    };

    template<> struct Schema<InlineValueVariableLookup> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",               &InlineValueVariableLookup::range),
            make_field("variableName",        &InlineValueVariableLookup::variableName),
            make_field("caseSensitiveLookup", &InlineValueVariableLookup::caseSensitiveLookup)
        );
    };

    /**
     * Provide an inline value through an expression evaluation.
     * If only a range is specified, the expression will be extracted from the underlying document.
     * An optional expression can be used to override the extracted expression.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValueEvaluatableExpression {
        /**
         * The document range for which the inline value applies.
         * The range is used to extract the evaluatable expression from the underlying document.
         */
        Range*  range;
        /**
         * If specified the expression overrides the extracted expression.
         */
        String  expression;
    };

    template<> struct Schema<InlineValueEvaluatableExpression> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",      &InlineValueEvaluatableExpression::range),
            make_field("expression", &InlineValueEvaluatableExpression::expression)
        );
    };

    /**
     * An inlay hint label part allows for interactive and composite labels
     * of inlay hints.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlayHintLabelPart {
        /**
         * The value of this label part.
         */
        String     value;
        /**
         * The tooltip text when you hover over this label part. Depending on
         * the client capability `inlayHint.resolveSupport` clients might resolve
         * this property late using the resolve request.
         */
        String     tooltip;
        /**
         * An optional source code location that represents this
         * label part.
         *
         * The editor will use this location for the hover and for code navigation
         * features: This part will become a clickable link that resolves to the
         * definition of the symbol at the given location (not necessarily the
         * location itself), it shows the hover that shows at the given location,
         * and it shows a context menu with further code navigation commands.
         *
         * Depending on the client capability `inlayHint.resolveSupport` clients
         * might resolve this property late using the resolve request.
         */
        Location*  location;
        /**
         * An optional command for this label part.
         *
         * Depending on the client capability `inlayHint.resolveSupport` clients
         * might resolve this property late using the resolve request.
         */
        Command*   command;
    };

    template<> struct Schema<InlayHintLabelPart> {
        static constexpr auto fields = std::make_tuple(
            make_field("value",    &InlayHintLabelPart::value),
            make_field("tooltip",  &InlayHintLabelPart::tooltip),
            make_field("location", &InlayHintLabelPart::location),
            make_field("command",  &InlayHintLabelPart::command)
        );
    };

    /**
     * A `MarkupContent` literal represents a string value which content is interpreted base on its
     * kind flag. Currently the protocol supports `plaintext` and `markdown` as markup kinds.
     *
     * If the kind is `markdown` then the value can contain fenced code blocks like in GitHub issues.
     * See https://help.github.com/articles/creating-and-highlighting-code-blocks/#syntax-highlighting
     *
     * Here is an example how such a string can be constructed using JavaScript / TypeScript:
     * ```ts
     * let markdown: MarkdownContent = {
     * kind: MarkupKind.Markdown,
     * value: [
     * '# Header',
     * 'Some text',
     * '```typescript',
     * 'someCode();',
     * '```'
     * ].join('\n')
     * };
     * ```
     *
     * *Please Note* that clients might sanitize the return markdown. A client could decide to
     * remove HTML from the markdown to avoid script execution.
     */
    struct MarkupContent {
        /**
         * The type of the Markup
         */
        MarkupKind  kind;
        /**
         * The content itself
         */
        String      value;
    };

    template<> struct Schema<MarkupContent> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",  &MarkupContent::kind),
            make_field("value", &MarkupContent::value)
        );
    };

    /**
     * A diagnostic report with a full set of problems.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct FullDocumentDiagnosticReport {
        /**
         * A full document diagnostic report.
         */
        LSPAny              kind;
        /**
         * An optional result id. If provided it will
         * be sent on the next diagnostic request for the
         * same document.
         */
        String              resultId;
        /**
         * The actual items.
         */
        Slice<Diagnostic*>  items;
    };

    template<> struct Schema<FullDocumentDiagnosticReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",     &FullDocumentDiagnosticReport::kind),
            make_field("resultId", &FullDocumentDiagnosticReport::resultId),
            make_field("items",    &FullDocumentDiagnosticReport::items)
        );
    };

    /**
     * A full diagnostic report with a set of related documents.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct RelatedFullDocumentDiagnosticReport : public FullDocumentDiagnosticReport {
        /**
         * Diagnostics of related documents. This information is useful
         * in programming languages where code in a file A can generate
         * diagnostics in a file B which A depends on. An example of
         * such a language is C/C++ where marco definitions in a file
         * a.cpp and result in errors in a header file b.hpp.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        LSPAny  relatedDocuments;
    };

    template<> struct Schema<RelatedFullDocumentDiagnosticReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",             &RelatedFullDocumentDiagnosticReport::kind),
            make_field("resultId",         &RelatedFullDocumentDiagnosticReport::resultId),
            make_field("items",            &RelatedFullDocumentDiagnosticReport::items),
            make_field("relatedDocuments", &RelatedFullDocumentDiagnosticReport::relatedDocuments)
        );
    };

    /**
     * A diagnostic report indicating that the last returned
     * report is still accurate.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct UnchangedDocumentDiagnosticReport {
        /**
         * A document diagnostic report indicating
         * no changes to the last result. A server can
         * only return `unchanged` if result ids are
         * provided.
         */
        LSPAny  kind;
        /**
         * A result id which will be sent on the next
         * diagnostic request for the same document.
         */
        String  resultId;
    };

    template<> struct Schema<UnchangedDocumentDiagnosticReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",     &UnchangedDocumentDiagnosticReport::kind),
            make_field("resultId", &UnchangedDocumentDiagnosticReport::resultId)
        );
    };

    /**
     * An unchanged diagnostic report with a set of related documents.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct RelatedUnchangedDocumentDiagnosticReport : public UnchangedDocumentDiagnosticReport {
        /**
         * Diagnostics of related documents. This information is useful
         * in programming languages where code in a file A can generate
         * diagnostics in a file B which A depends on. An example of
         * such a language is C/C++ where marco definitions in a file
         * a.cpp and result in errors in a header file b.hpp.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        LSPAny  relatedDocuments;
    };

    template<> struct Schema<RelatedUnchangedDocumentDiagnosticReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",             &RelatedUnchangedDocumentDiagnosticReport::kind),
            make_field("resultId",         &RelatedUnchangedDocumentDiagnosticReport::resultId),
            make_field("relatedDocuments", &RelatedUnchangedDocumentDiagnosticReport::relatedDocuments)
        );
    };

    /**
     * A previous result id in a workspace pull request.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct PreviousResultId {
        /**
         * The URI for which the client knowns a
         * result id.
         */
        String  uri;
        /**
         * The value of the previous result id.
         */
        String  value;
    };

    template<> struct Schema<PreviousResultId> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",   &PreviousResultId::uri),
            make_field("value", &PreviousResultId::value)
        );
    };

    /**
     * A notebook document.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookDocument {
        /**
         * The notebook document's uri.
         */
        String                uri;
        /**
         * The type of the notebook.
         */
        String                notebookType;
        /**
         * The version number of this document (it will increase after each
         * change, including undo/redo).
         */
        Int                   version;
        /**
         * Additional metadata stored with the notebook
         * document.
         *
         * Note: should always be an object literal (e.g. LSPObject)
         */
        LSPAny                metadata;
        /**
         * The cells of a notebook.
         */
        Slice<NotebookCell*>  cells;
    };

    template<> struct Schema<NotebookDocument> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",          &NotebookDocument::uri),
            make_field("notebookType", &NotebookDocument::notebookType),
            make_field("version",      &NotebookDocument::version),
            make_field("metadata",     &NotebookDocument::metadata),
            make_field("cells",        &NotebookDocument::cells)
        );
    };

    /**
     * An item to transfer a text document from the client to the
     * server.
     */
    struct TextDocumentItem {
        /**
         * The text document's uri.
         */
        String  uri;
        /**
         * The text document's language identifier.
         */
        String  languageId;
        /**
         * The version number of this document (it will increase after each
         * change, including undo/redo).
         */
        Int     version;
        /**
         * The content of the opened text document.
         */
        String  text;
    };

    template<> struct Schema<TextDocumentItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",        &TextDocumentItem::uri),
            make_field("languageId", &TextDocumentItem::languageId),
            make_field("version",    &TextDocumentItem::version),
            make_field("text",       &TextDocumentItem::text)
        );
    };

    /**
     * A versioned notebook document identifier.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct VersionedNotebookDocumentIdentifier {
        /**
         * The version number of this notebook document.
         */
        Int     version;
        /**
         * The notebook document's uri.
         */
        String  uri;
    };

    template<> struct Schema<VersionedNotebookDocumentIdentifier> {
        static constexpr auto fields = std::make_tuple(
            make_field("version", &VersionedNotebookDocumentIdentifier::version),
            make_field("uri",     &VersionedNotebookDocumentIdentifier::uri)
        );
    };

    /**
     * A change event for a notebook document.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookDocumentChangeEvent {
        /**
         * The changed meta data if any.
         *
         * Note: should always be an object literal (e.g. LSPObject)
         */
        LSPAny   metadata;
        /**
         * Changes to cells
         */
        struct _Cells {
            /**
             * Changes to the cell structure to add or
             * remove cells.
             */
            struct _Structure {
                /**
                 * The change to the cell array.
                 */
                NotebookCellArrayChange*        array;
                /**
                 * Additional opened cell text documents.
                 */
                Slice<TextDocumentItem*>        didOpen;
                /**
                 * Additional closed cell text documents.
                 */
                Slice<TextDocumentIdentifier*>  didClose;
            }* structure;
            /**
             * Changes to notebook cells properties like its
             * kind, execution summary or metadata.
             */
            Slice<NotebookCell*>  data;
            /**
             * Changes to the text content of notebook cells.
             */
            Slice<LSPAny>         textContent;
        }* cells;
    };

    template<> struct Schema<NotebookDocumentChangeEvent> {
        static constexpr auto fields = std::make_tuple(
            make_field("metadata", &NotebookDocumentChangeEvent::metadata),
            make_field("cells",    &NotebookDocumentChangeEvent::cells)
        );
    };

    template<> struct Schema<NotebookDocumentChangeEvent::_Cells::_Structure> {
        static constexpr auto fields = std::make_tuple(
            make_field("array",    &NotebookDocumentChangeEvent::_Cells::_Structure::array),
            make_field("didOpen",  &NotebookDocumentChangeEvent::_Cells::_Structure::didOpen),
            make_field("didClose", &NotebookDocumentChangeEvent::_Cells::_Structure::didClose)
        );
    };

    template<> struct Schema<NotebookDocumentChangeEvent::_Cells> {
        static constexpr auto fields = std::make_tuple(
            make_field("structure",   &NotebookDocumentChangeEvent::_Cells::structure),
            make_field("data",        &NotebookDocumentChangeEvent::_Cells::data),
            make_field("textContent", &NotebookDocumentChangeEvent::_Cells::textContent)
        );
    };

    /**
     * A literal to identify a notebook document in the client.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookDocumentIdentifier {
        /**
         * The notebook document's uri.
         */
        String  uri;
    };

    template<> struct Schema<NotebookDocumentIdentifier> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri", &NotebookDocumentIdentifier::uri)
        );
    };

    /**
     * Provides information about the context in which an inline completion was requested.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct InlineCompletionContext {
        /**
         * Describes how the inline completion was triggered.
         */
        InlineCompletionTriggerKind  triggerKind;
        /**
         * Provides information about the currently selected item in the autocomplete widget if it is visible.
         */
        SelectedCompletionInfo*      selectedCompletionInfo;
    };

    template<> struct Schema<InlineCompletionContext> {
        static constexpr auto fields = std::make_tuple(
            make_field("triggerKind",            &InlineCompletionContext::triggerKind),
            make_field("selectedCompletionInfo", &InlineCompletionContext::selectedCompletionInfo)
        );
    };

    /**
     * A string value used as a snippet is a template which allows to insert text
     * and to control the editor cursor when insertion happens.
     *
     * A snippet can define tab stops and placeholders with `$1`, `$2`
     * and `${3:foo}`. `$0` defines the final tab stop, it defaults to
     * the end of the snippet. Variables are defined with `$name` and
     * `${name:default value}`.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct StringValue {
        /**
         * The kind of string value.
         */
        LSPAny  kind;
        /**
         * The snippet string.
         */
        String  value;
    };

    template<> struct Schema<StringValue> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",  &StringValue::kind),
            make_field("value", &StringValue::value)
        );
    };

    /**
     * General parameters to register for a notification or to register a provider.
     */
    struct Registration {
        /**
         * The id used to register the request. The id can be used to deregister
         * the request again.
         */
        String  id;
        /**
         * The method / capability to register for.
         */
        String  method;
        /**
         * Options necessary for the registration.
         */
        String  registerOptions;
    };

    template<> struct Schema<Registration> {
        static constexpr auto fields = std::make_tuple(
            make_field("id",              &Registration::id),
            make_field("method",          &Registration::method),
            make_field("registerOptions", &Registration::registerOptions)
        );
    };

    /**
     * General parameters to unregister a request or notification.
     */
    struct Unregistration {
        /**
         * The id used to unregister the request or notification. Usually an id
         * provided during the register request.
         */
        String  id;
        /**
         * The method to unregister for.
         */
        String  method;
    };

    template<> struct Schema<Unregistration> {
        static constexpr auto fields = std::make_tuple(
            make_field("id",     &Unregistration::id),
            make_field("method", &Unregistration::method)
        );
    };

    /**
     * Defines the capabilities provided by a language
     * server.
     */
    struct ServerCapabilities {
        /**
         * The position encoding the server picked from the encodings offered
         * by the client via the client capability `general.positionEncodings`.
         *
         * If the client didn't provide any position encodings the only valid
         * value that a server can return is 'utf-16'.
         *
         * If omitted it defaults to 'utf-16'.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        PositionEncodingKind              positionEncoding;
        /**
         * Defines how text documents are synced. Is either a detailed structure
         * defining each notification or for backwards compatibility the
         * TextDocumentSyncKind number.
         */
        TextDocumentSyncOptions*          textDocumentSync;
        /**
         * Defines how notebook documents are synced.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        NotebookDocumentSyncOptions*      notebookDocumentSync;
        /**
         * The server provides completion support.
         */
        CompletionOptions*                completionProvider;
        /**
         * The server provides hover support.
         */
        Bool                              hoverProvider;
        /**
         * The server provides signature help support.
         */
        SignatureHelpOptions*             signatureHelpProvider;
        /**
         * The server provides Goto Declaration support.
         */
        Bool                              declarationProvider;
        /**
         * The server provides goto definition support.
         */
        Bool                              definitionProvider;
        /**
         * The server provides Goto Type Definition support.
         */
        Bool                              typeDefinitionProvider;
        /**
         * The server provides Goto Implementation support.
         */
        Bool                              implementationProvider;
        /**
         * The server provides find references support.
         */
        Bool                              referencesProvider;
        /**
         * The server provides document highlight support.
         */
        Bool                              documentHighlightProvider;
        /**
         * The server provides document symbol support.
         */
        Bool                              documentSymbolProvider;
        /**
         * The server provides code actions. CodeActionOptions may only be
         * specified if the client states that it supports
         * `codeActionLiteralSupport` in its initial `initialize` request.
         */
        Bool                              codeActionProvider;
        /**
         * The server provides code lens.
         */
        CodeLensOptions*                  codeLensProvider;
        /**
         * The server provides document link support.
         */
        DocumentLinkOptions*              documentLinkProvider;
        /**
         * The server provides color provider support.
         */
        Bool                              colorProvider;
        /**
         * The server provides workspace symbol support.
         */
        Bool                              workspaceSymbolProvider;
        /**
         * The server provides document formatting.
         */
        Bool                              documentFormattingProvider;
        /**
         * The server provides document range formatting.
         */
        Bool                              documentRangeFormattingProvider;
        /**
         * The server provides document formatting on typing.
         */
        DocumentOnTypeFormattingOptions*  documentOnTypeFormattingProvider;
        /**
         * The server provides rename support. RenameOptions may only be
         * specified if the client states that it supports
         * `prepareSupport` in its initial `initialize` request.
         */
        Bool                              renameProvider;
        /**
         * The server provides folding provider support.
         */
        Bool                              foldingRangeProvider;
        /**
         * The server provides selection range support.
         */
        Bool                              selectionRangeProvider;
        /**
         * The server provides execute command support.
         */
        ExecuteCommandOptions*            executeCommandProvider;
        /**
         * The server provides call hierarchy support.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool                              callHierarchyProvider;
        /**
         * The server provides linked editing range support.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool                              linkedEditingRangeProvider;
        /**
         * The server provides semantic tokens support.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        SemanticTokensOptions*            semanticTokensProvider;
        /**
         * The server provides moniker support.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool                              monikerProvider;
        /**
         * The server provides type hierarchy support.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        Bool                              typeHierarchyProvider;
        /**
         * The server provides inline values.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        Bool                              inlineValueProvider;
        /**
         * The server provides inlay hints.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        Bool                              inlayHintProvider;
        /**
         * The server has support for pull model diagnostics.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        DiagnosticOptions*                diagnosticProvider;
        /**
         * Inline completion options used during static registration.
         *
         * @since 3.18.0
         * @proposed
         * @since 3.18.0
         */
        Bool                              inlineCompletionProvider;
        /**
         * Workspace specific server capabilities.
         */
        struct _Workspace {
            /**
             * The server supports workspace folder.
             *
             * @since 3.6.0
             * @since 3.6.0
             */
            WorkspaceFoldersServerCapabilities*  workspaceFolders;
            /**
             * The server is interested in notifications/requests for operations on files.
             *
             * @since 3.16.0
             * @since 3.16.0
             */
            FileOperationOptions*                fileOperations;
        }* workspace;
        /**
         * Experimental server capabilities.
         */
        String                            experimental;
    };

    template<> struct Schema<ServerCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("positionEncoding",                 &ServerCapabilities::positionEncoding),
            make_field("textDocumentSync",                 &ServerCapabilities::textDocumentSync),
            make_field("notebookDocumentSync",             &ServerCapabilities::notebookDocumentSync),
            make_field("completionProvider",               &ServerCapabilities::completionProvider),
            make_field("hoverProvider",                    &ServerCapabilities::hoverProvider),
            make_field("signatureHelpProvider",            &ServerCapabilities::signatureHelpProvider),
            make_field("declarationProvider",              &ServerCapabilities::declarationProvider),
            make_field("definitionProvider",               &ServerCapabilities::definitionProvider),
            make_field("typeDefinitionProvider",           &ServerCapabilities::typeDefinitionProvider),
            make_field("implementationProvider",           &ServerCapabilities::implementationProvider),
            make_field("referencesProvider",               &ServerCapabilities::referencesProvider),
            make_field("documentHighlightProvider",        &ServerCapabilities::documentHighlightProvider),
            make_field("documentSymbolProvider",           &ServerCapabilities::documentSymbolProvider),
            make_field("codeActionProvider",               &ServerCapabilities::codeActionProvider),
            make_field("codeLensProvider",                 &ServerCapabilities::codeLensProvider),
            make_field("documentLinkProvider",             &ServerCapabilities::documentLinkProvider),
            make_field("colorProvider",                    &ServerCapabilities::colorProvider),
            make_field("workspaceSymbolProvider",          &ServerCapabilities::workspaceSymbolProvider),
            make_field("documentFormattingProvider",       &ServerCapabilities::documentFormattingProvider),
            make_field("documentRangeFormattingProvider",  &ServerCapabilities::documentRangeFormattingProvider),
            make_field("documentOnTypeFormattingProvider", &ServerCapabilities::documentOnTypeFormattingProvider),
            make_field("renameProvider",                   &ServerCapabilities::renameProvider),
            make_field("foldingRangeProvider",             &ServerCapabilities::foldingRangeProvider),
            make_field("selectionRangeProvider",           &ServerCapabilities::selectionRangeProvider),
            make_field("executeCommandProvider",           &ServerCapabilities::executeCommandProvider),
            make_field("callHierarchyProvider",            &ServerCapabilities::callHierarchyProvider),
            make_field("linkedEditingRangeProvider",       &ServerCapabilities::linkedEditingRangeProvider),
            make_field("semanticTokensProvider",           &ServerCapabilities::semanticTokensProvider),
            make_field("monikerProvider",                  &ServerCapabilities::monikerProvider),
            make_field("typeHierarchyProvider",            &ServerCapabilities::typeHierarchyProvider),
            make_field("inlineValueProvider",              &ServerCapabilities::inlineValueProvider),
            make_field("inlayHintProvider",                &ServerCapabilities::inlayHintProvider),
            make_field("diagnosticProvider",               &ServerCapabilities::diagnosticProvider),
            make_field("inlineCompletionProvider",         &ServerCapabilities::inlineCompletionProvider),
            make_field("workspace",                        &ServerCapabilities::workspace),
            make_field("experimental",                     &ServerCapabilities::experimental)
        );
    };

    template<> struct Schema<ServerCapabilities::_Workspace> {
        static constexpr auto fields = std::make_tuple(
            make_field("workspaceFolders", &ServerCapabilities::_Workspace::workspaceFolders),
            make_field("fileOperations",   &ServerCapabilities::_Workspace::fileOperations)
        );
    };

    /**
     * A text document identifier to denote a specific version of a text document.
     */
    struct VersionedTextDocumentIdentifier : public TextDocumentIdentifier {
        /**
         * The version number of this document.
         */
        Int  version;
    };

    template<> struct Schema<VersionedTextDocumentIdentifier> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",     &VersionedTextDocumentIdentifier::uri),
            make_field("version", &VersionedTextDocumentIdentifier::version)
        );
    };

    /**
     * An event describing a file change.
     */
    struct FileEvent {
        /**
         * The file's uri.
         */
        String          uri;
        /**
         * The change type.
         */
        FileChangeType  type;
    };

    template<> struct Schema<FileEvent> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",  &FileEvent::uri),
            make_field("type", &FileEvent::type)
        );
    };

    struct FileSystemWatcher {
        /**
         * The glob pattern to watch. See {@link GlobPattern glob pattern} for more detail.
         *
         * @since 3.17.0 support for relative patterns.
         * @since 3.17.0 support for relative patterns.
         */
        GlobPattern*  globPattern;
        /**
         * The kind of events of interest. If omitted it defaults
         * to WatchKind.Create | WatchKind.Change | WatchKind.Delete
         * which is 7.
         */
        WatchKind     kind;
    };

    template<> struct Schema<FileSystemWatcher> {
        static constexpr auto fields = std::make_tuple(
            make_field("globPattern", &FileSystemWatcher::globPattern),
            make_field("kind",        &FileSystemWatcher::kind)
        );
    };

    /**
     * Represents a diagnostic, such as a compiler error or warning. Diagnostic objects
     * are only valid in the scope of a resource.
     */
    struct Diagnostic {
        /**
         * The range at which the message applies
         */
        Range*                                range;
        /**
         * The diagnostic's severity. Can be omitted. If omitted it is up to the
         * client to interpret diagnostics as error, warning, info or hint.
         */
        DiagnosticSeverity                    severity;
        /**
         * The diagnostic's code, which usually appear in the user interface.
         */
        String                                code;
        /**
         * An optional property to describe the error code.
         * Requires the code field (above) to be present/not null.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        CodeDescription*                      codeDescription;
        /**
         * A human-readable string describing the source of this
         * diagnostic, e.g. 'typescript' or 'super lint'. It usually
         * appears in the user interface.
         */
        String                                source;
        /**
         * The diagnostic's message. It usually appears in the user interface
         */
        String                                message;
        /**
         * Additional metadata about the diagnostic.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Slice<DiagnosticTag>                  tags;
        /**
         * An array of related diagnostic information, e.g. when symbol-names within
         * a scope collide all definitions can be marked via this property.
         */
        Slice<DiagnosticRelatedInformation*>  relatedInformation;
        /**
         * A data entry field that is preserved between a `textDocument/publishDiagnostics`
         * notification and `textDocument/codeAction` request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        String                                data;
    };

    template<> struct Schema<Diagnostic> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",              &Diagnostic::range),
            make_field("severity",           &Diagnostic::severity),
            make_field("code",               &Diagnostic::code),
            make_field("codeDescription",    &Diagnostic::codeDescription),
            make_field("source",             &Diagnostic::source),
            make_field("message",            &Diagnostic::message),
            make_field("tags",               &Diagnostic::tags),
            make_field("relatedInformation", &Diagnostic::relatedInformation),
            make_field("data",               &Diagnostic::data)
        );
    };

    /**
     * Contains additional information about the context in which a completion request is triggered.
     */
    struct CompletionContext {
        /**
         * How the completion was triggered.
         */
        CompletionTriggerKind  triggerKind;
        /**
         * The trigger character (a single character) that has trigger code complete.
         * Is undefined if `triggerKind !== CompletionTriggerKind.TriggerCharacter`
         */
        String                 triggerCharacter;
    };

    template<> struct Schema<CompletionContext> {
        static constexpr auto fields = std::make_tuple(
            make_field("triggerKind",      &CompletionContext::triggerKind),
            make_field("triggerCharacter", &CompletionContext::triggerCharacter)
        );
    };

    /**
     * Additional details for a completion item label.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct CompletionItemLabelDetails {
        /**
         * An optional string which is rendered less prominently directly after {@link CompletionItem.label label},
         * without any spacing. Should be used for function signatures and type annotations.
         */
        String  detail;
        /**
         * An optional string which is rendered less prominently after {@link CompletionItem.detail}. Should be used
         * for fully qualified names and file paths.
         */
        String  description;
    };

    template<> struct Schema<CompletionItemLabelDetails> {
        static constexpr auto fields = std::make_tuple(
            make_field("detail",      &CompletionItemLabelDetails::detail),
            make_field("description", &CompletionItemLabelDetails::description)
        );
    };

    /**
     * A special text edit to provide an insert and a replace operation.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct InsertReplaceEdit {
        /**
         * The string to be inserted.
         */
        String  newText;
        /**
         * The range if the insert is requested
         */
        Range*  insert;
        /**
         * The range if the replace is requested.
         */
        Range*  replace;
    };

    template<> struct Schema<InsertReplaceEdit> {
        static constexpr auto fields = std::make_tuple(
            make_field("newText", &InsertReplaceEdit::newText),
            make_field("insert",  &InsertReplaceEdit::insert),
            make_field("replace", &InsertReplaceEdit::replace)
        );
    };

    /**
     * Additional information about the context in which a signature help request was triggered.
     *
     * @since 3.15.0
     * @since 3.15.0
     */
    struct SignatureHelpContext {
        /**
         * Action that caused signature help to be triggered.
         */
        SignatureHelpTriggerKind  triggerKind;
        /**
         * Character that caused signature help to be triggered.
         *
         * This is undefined when `triggerKind !== SignatureHelpTriggerKind.TriggerCharacter`
         */
        String                    triggerCharacter;
        /**
         * `true` if signature help was already showing when it was triggered.
         *
         * Retriggers occurs when the signature help is already active and can be caused by actions such as
         * typing a trigger character, a cursor move, or document content changes.
         */
        Bool                      isRetrigger;
        /**
         * The currently active `SignatureHelp`.
         *
         * The `activeSignatureHelp` has its `SignatureHelp.activeSignature` field updated based on
         * the user navigating through available signatures.
         */
        SignatureHelp*            activeSignatureHelp;
    };

    template<> struct Schema<SignatureHelpContext> {
        static constexpr auto fields = std::make_tuple(
            make_field("triggerKind",         &SignatureHelpContext::triggerKind),
            make_field("triggerCharacter",    &SignatureHelpContext::triggerCharacter),
            make_field("isRetrigger",         &SignatureHelpContext::isRetrigger),
            make_field("activeSignatureHelp", &SignatureHelpContext::activeSignatureHelp)
        );
    };

    /**
     * Represents the signature of something callable. A signature
     * can have a label, like a function-name, a doc-comment, and
     * a set of parameters.
     */
    struct SignatureInformation {
        /**
         * The label of this signature. Will be shown in
         * the UI.
         */
        String                        label;
        /**
         * The human-readable doc-comment of this signature. Will be shown
         * in the UI but can be omitted.
         */
        String                        documentation;
        /**
         * The parameters of this signature.
         */
        Slice<ParameterInformation*>  parameters;
        /**
         * The index of the active parameter.
         *
         * If provided, this is used in place of `SignatureHelp.activeParameter`.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        UInt                          activeParameter;
    };

    template<> struct Schema<SignatureInformation> {
        static constexpr auto fields = std::make_tuple(
            make_field("label",           &SignatureInformation::label),
            make_field("documentation",   &SignatureInformation::documentation),
            make_field("parameters",      &SignatureInformation::parameters),
            make_field("activeParameter", &SignatureInformation::activeParameter)
        );
    };

    /**
     * Value-object that contains additional information when
     * requesting references.
     */
    struct ReferenceContext {
        /**
         * Include the declaration of the current symbol.
         */
        Bool  includeDeclaration;
    };

    template<> struct Schema<ReferenceContext> {
        static constexpr auto fields = std::make_tuple(
            make_field("includeDeclaration", &ReferenceContext::includeDeclaration)
        );
    };

    /**
     * Contains additional diagnostic information about the context in which
     * a {@link CodeActionProvider.provideCodeActions code action} is run.
     */
    struct CodeActionContext {
        /**
         * An array of diagnostics known on the client side overlapping the range provided to the
         * `textDocument/codeAction` request. They are provided so that the server knows which
         * errors are currently presented to the user for the given range. There is no guarantee
         * that these accurately reflect the error state of the resource. The primary parameter
         * to compute code actions is the provided range.
         */
        Slice<Diagnostic*>     diagnostics;
        /**
         * Requested kind of actions to return.
         *
         * Actions not of this kind are filtered out by the client before being shown. So servers
         * can omit computing them.
         */
        Slice<CodeActionKind>  only;
        /**
         * The reason why code actions were requested.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        CodeActionTriggerKind  triggerKind;
    };

    template<> struct Schema<CodeActionContext> {
        static constexpr auto fields = std::make_tuple(
            make_field("diagnostics", &CodeActionContext::diagnostics),
            make_field("only",        &CodeActionContext::only),
            make_field("triggerKind", &CodeActionContext::triggerKind)
        );
    };

    /**
     * Value-object describing what options formatting should use.
     */
    struct FormattingOptions {
        /**
         * Size of a tab in spaces.
         */
        UInt  tabSize;
        /**
         * Prefer spaces over tabs.
         */
        Bool  insertSpaces;
        /**
         * Trim trailing whitespace on a line.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Bool  trimTrailingWhitespace;
        /**
         * Insert a newline character at the end of the file if one does not exist.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Bool  insertFinalNewline;
        /**
         * Trim all newlines after the final newline at the end of the file.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Bool  trimFinalNewlines;
    };

    template<> struct Schema<FormattingOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("tabSize",                &FormattingOptions::tabSize),
            make_field("insertSpaces",           &FormattingOptions::insertSpaces),
            make_field("trimTrailingWhitespace", &FormattingOptions::trimTrailingWhitespace),
            make_field("insertFinalNewline",     &FormattingOptions::insertFinalNewline),
            make_field("trimFinalNewlines",      &FormattingOptions::trimFinalNewlines)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensLegend {
        /**
         * The token types a server uses.
         */
        Slice<String>  tokenTypes;
        /**
         * The token modifiers a server uses.
         */
        Slice<String>  tokenModifiers;
    };

    template<> struct Schema<SemanticTokensLegend> {
        static constexpr auto fields = std::make_tuple(
            make_field("tokenTypes",     &SemanticTokensLegend::tokenTypes),
            make_field("tokenModifiers", &SemanticTokensLegend::tokenModifiers)
        );
    };

    /**
     * A text document identifier to optionally denote a specific version of a text document.
     */
    struct OptionalVersionedTextDocumentIdentifier : public TextDocumentIdentifier {
        /**
         * The version number of this document. If a versioned text document identifier
         * is sent from the server to the client and the file is not open in the editor
         * (the server has not received an open notification before) the server can send
         * `null` to indicate that the version is unknown and the content on disk is the
         * truth (as specified with document content ownership).
         */
        Int  version;
    };

    template<> struct Schema<OptionalVersionedTextDocumentIdentifier> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",     &OptionalVersionedTextDocumentIdentifier::uri),
            make_field("version", &OptionalVersionedTextDocumentIdentifier::version)
        );
    };

    /**
     * A special text edit with an additional change annotation.
     *
     * @since 3.16.0.
     * @since 3.16.0.
     */
    struct AnnotatedTextEdit : public TextEdit {
        /**
         * The actual identifier of the change annotation
         */
        String  annotationId;
    };

    template<> struct Schema<AnnotatedTextEdit> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",        &AnnotatedTextEdit::range),
            make_field("newText",      &AnnotatedTextEdit::newText),
            make_field("annotationId", &AnnotatedTextEdit::annotationId)
        );
    };

    /**
     * Options to create a file.
     */
    struct CreateFileOptions {
        /**
         * Overwrite existing file. Overwrite wins over `ignoreIfExists`
         */
        Bool  overwrite;
        /**
         * Ignore if exists.
         */
        Bool  ignoreIfExists;
    };

    template<> struct Schema<CreateFileOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("overwrite",      &CreateFileOptions::overwrite),
            make_field("ignoreIfExists", &CreateFileOptions::ignoreIfExists)
        );
    };

    /**
     * Rename file options
     */
    struct RenameFileOptions {
        /**
         * Overwrite target if existing. Overwrite wins over `ignoreIfExists`
         */
        Bool  overwrite;
        /**
         * Ignores if target exists.
         */
        Bool  ignoreIfExists;
    };

    template<> struct Schema<RenameFileOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("overwrite",      &RenameFileOptions::overwrite),
            make_field("ignoreIfExists", &RenameFileOptions::ignoreIfExists)
        );
    };

    /**
     * Delete file options
     */
    struct DeleteFileOptions {
        /**
         * Delete the content recursively if a folder is denoted.
         */
        Bool  recursive;
        /**
         * Ignore the operation if the file doesn't exist.
         */
        Bool  ignoreIfNotExists;
    };

    template<> struct Schema<DeleteFileOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("recursive",         &DeleteFileOptions::recursive),
            make_field("ignoreIfNotExists", &DeleteFileOptions::ignoreIfNotExists)
        );
    };

    /**
     * A pattern to describe in which file operation requests or notifications
     * the server is interested in receiving.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct FileOperationPattern {
        /**
         * The glob pattern to match. Glob patterns can have the following syntax:
         * - `*` to match zero or more characters in a path segment
         * - `?` to match on one character in a path segment
         * - `**` to match any number of path segments, including none
         * - `{}` to group sub patterns into an OR expression. (e.g. `**​/*.{ts,js}` matches all TypeScript and JavaScript files)
         * - `[]` to declare a range of characters to match in a path segment (e.g., `example.[0-9]` to match on `example.0`, `example.1`, …)
         * - `[!...]` to negate a range of characters to match in a path segment (e.g., `example.[!0-9]` to match on `example.a`, `example.b`, but not `example.0`)
         */
        String                        glob;
        /**
         * Whether to match files or folders with this pattern.
         *
         * Matches both if undefined.
         */
        FileOperationPatternKind      matches;
        /**
         * Additional options used during matching.
         */
        FileOperationPatternOptions*  options;
    };

    template<> struct Schema<FileOperationPattern> {
        static constexpr auto fields = std::make_tuple(
            make_field("glob",    &FileOperationPattern::glob),
            make_field("matches", &FileOperationPattern::matches),
            make_field("options", &FileOperationPattern::options)
        );
    };

    /**
     * A full document diagnostic report for a workspace diagnostic result.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct WorkspaceFullDocumentDiagnosticReport : public FullDocumentDiagnosticReport {
        /**
         * The URI for which diagnostic information is reported.
         */
        String  uri;
        /**
         * The version number for which the diagnostics are reported.
         * If the document is not marked as open `null` can be provided.
         */
        Int     version;
    };

    template<> struct Schema<WorkspaceFullDocumentDiagnosticReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",     &WorkspaceFullDocumentDiagnosticReport::kind),
            make_field("resultId", &WorkspaceFullDocumentDiagnosticReport::resultId),
            make_field("items",    &WorkspaceFullDocumentDiagnosticReport::items),
            make_field("uri",      &WorkspaceFullDocumentDiagnosticReport::uri),
            make_field("version",  &WorkspaceFullDocumentDiagnosticReport::version)
        );
    };

    /**
     * An unchanged document diagnostic report for a workspace diagnostic result.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct WorkspaceUnchangedDocumentDiagnosticReport : public UnchangedDocumentDiagnosticReport {
        /**
         * The URI for which diagnostic information is reported.
         */
        String  uri;
        /**
         * The version number for which the diagnostics are reported.
         * If the document is not marked as open `null` can be provided.
         */
        Int     version;
    };

    template<> struct Schema<WorkspaceUnchangedDocumentDiagnosticReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",     &WorkspaceUnchangedDocumentDiagnosticReport::kind),
            make_field("resultId", &WorkspaceUnchangedDocumentDiagnosticReport::resultId),
            make_field("uri",      &WorkspaceUnchangedDocumentDiagnosticReport::uri),
            make_field("version",  &WorkspaceUnchangedDocumentDiagnosticReport::version)
        );
    };

    /**
     * A notebook cell.
     *
     * A cell's document URI must be unique across ALL notebook
     * cells and can therefore be used to uniquely identify a
     * notebook cell or the cell's text document.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookCell {
        /**
         * The cell's kind
         */
        NotebookCellKind   kind;
        /**
         * The URI of the cell's text document
         * content.
         */
        String             document;
        /**
         * Additional metadata stored with the cell.
         *
         * Note: should always be an object literal (e.g. LSPObject)
         */
        LSPAny             metadata;
        /**
         * Additional execution summary information
         * if supported by the client.
         */
        ExecutionSummary*  executionSummary;
    };

    template<> struct Schema<NotebookCell> {
        static constexpr auto fields = std::make_tuple(
            make_field("kind",             &NotebookCell::kind),
            make_field("document",         &NotebookCell::document),
            make_field("metadata",         &NotebookCell::metadata),
            make_field("executionSummary", &NotebookCell::executionSummary)
        );
    };

    /**
     * A change describing how to move a `NotebookCell`
     * array from state S to S'.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookCellArrayChange {
        /**
         * The start oftest of the cell that changed.
         */
        UInt                  start;
        /**
         * The deleted cells
         */
        UInt                  deleteCount;
        /**
         * The new cells, if any
         */
        Slice<NotebookCell*>  cells;
    };

    template<> struct Schema<NotebookCellArrayChange> {
        static constexpr auto fields = std::make_tuple(
            make_field("start",       &NotebookCellArrayChange::start),
            make_field("deleteCount", &NotebookCellArrayChange::deleteCount),
            make_field("cells",       &NotebookCellArrayChange::cells)
        );
    };

    /**
     * Describes the currently selected completion item.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct SelectedCompletionInfo {
        /**
         * The range that will be replaced if this completion item is accepted.
         */
        Range*  range;
        /**
         * The text the range will be replaced with if this completion is accepted.
         */
        String  text;
    };

    template<> struct Schema<SelectedCompletionInfo> {
        static constexpr auto fields = std::make_tuple(
            make_field("range", &SelectedCompletionInfo::range),
            make_field("text",  &SelectedCompletionInfo::text)
        );
    };

    /**
     * Defines the capabilities provided by the client.
     */
    struct ClientCapabilities {
        /**
         * Workspace specific client capabilities.
         */
        WorkspaceClientCapabilities*         workspace;
        /**
         * Text document specific client capabilities.
         */
        TextDocumentClientCapabilities*      textDocument;
        /**
         * Capabilities specific to the notebook document support.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        NotebookDocumentClientCapabilities*  notebookDocument;
        /**
         * Window specific client capabilities.
         */
        WindowClientCapabilities*            window;
        /**
         * General client capabilities.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        GeneralClientCapabilities*           general;
        /**
         * Experimental client capabilities.
         */
        String                               experimental;
    };

    template<> struct Schema<ClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("workspace",        &ClientCapabilities::workspace),
            make_field("textDocument",     &ClientCapabilities::textDocument),
            make_field("notebookDocument", &ClientCapabilities::notebookDocument),
            make_field("window",           &ClientCapabilities::window),
            make_field("general",          &ClientCapabilities::general),
            make_field("experimental",     &ClientCapabilities::experimental)
        );
    };

    struct TextDocumentSyncOptions {
        /**
         * Open and close notifications are sent to the server. If omitted open close notification should not
         * be sent.
         */
        Bool                  openClose;
        /**
         * Change notifications are sent to the server. See TextDocumentSyncKind.None, TextDocumentSyncKind.Full
         * and TextDocumentSyncKind.Incremental. If omitted it defaults to TextDocumentSyncKind.None.
         */
        TextDocumentSyncKind  change;
        /**
         * If present will save notifications are sent to the server. If omitted the notification should not be
         * sent.
         */
        Bool                  willSave;
        /**
         * If present will save wait until requests are sent to the server. If omitted the request should not be
         * sent.
         */
        Bool                  willSaveWaitUntil;
        /**
         * If present save notifications are sent to the server. If omitted the notification should not be
         * sent.
         */
        Bool                  save;
    };

    template<> struct Schema<TextDocumentSyncOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("openClose",         &TextDocumentSyncOptions::openClose),
            make_field("change",            &TextDocumentSyncOptions::change),
            make_field("willSave",          &TextDocumentSyncOptions::willSave),
            make_field("willSaveWaitUntil", &TextDocumentSyncOptions::willSaveWaitUntil),
            make_field("save",              &TextDocumentSyncOptions::save)
        );
    };

    /**
     * Options specific to a notebook plus its cells
     * to be synced to the server.
     *
     * If a selector provides a notebook document
     * filter but no cell selector all cells of a
     * matching notebook document will be synced.
     *
     * If a selector provides no notebook document
     * filter but only a cell selector all notebook
     * document that contain at least one matching
     * cell will be synced.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookDocumentSyncOptions {
        /**
         * The notebooks to be synced
         */
        Slice<LSPAny>  notebookSelector;
        /**
         * Whether save notification should be forwarded to
         * the server. Will only be honored if mode === `notebook`.
         */
        Bool           save;
    };

    template<> struct Schema<NotebookDocumentSyncOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("notebookSelector", &NotebookDocumentSyncOptions::notebookSelector),
            make_field("save",             &NotebookDocumentSyncOptions::save)
        );
    };

    /**
     * Registration options specific to a notebook.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookDocumentSyncRegistrationOptions : public NotebookDocumentSyncOptions {
    };

    template<> struct Schema<NotebookDocumentSyncRegistrationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("notebookSelector", &NotebookDocumentSyncRegistrationOptions::notebookSelector),
            make_field("save",             &NotebookDocumentSyncRegistrationOptions::save)
        );
    };

    struct WorkspaceFoldersServerCapabilities {
        /**
         * The server has support for workspace folders
         */
        Bool    supported;
        /**
         * Whether the server wants to receive workspace folder
         * change notifications.
         *
         * If a string is provided the string is treated as an ID
         * under which the notification is registered on the client
         * side. The ID can be used to unregister for these events
         * using the `client/unregisterCapability` request.
         */
        String  changeNotifications;
    };

    template<> struct Schema<WorkspaceFoldersServerCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("supported",           &WorkspaceFoldersServerCapabilities::supported),
            make_field("changeNotifications", &WorkspaceFoldersServerCapabilities::changeNotifications)
        );
    };

    /**
     * Options for notifications/requests for user operations on files.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct FileOperationOptions {
        /**
         * The server is interested in receiving didCreateFiles notifications.
         */
        FileOperationRegistrationOptions*  didCreate;
        /**
         * The server is interested in receiving willCreateFiles requests.
         */
        FileOperationRegistrationOptions*  willCreate;
        /**
         * The server is interested in receiving didRenameFiles notifications.
         */
        FileOperationRegistrationOptions*  didRename;
        /**
         * The server is interested in receiving willRenameFiles requests.
         */
        FileOperationRegistrationOptions*  willRename;
        /**
         * The server is interested in receiving didDeleteFiles file notifications.
         */
        FileOperationRegistrationOptions*  didDelete;
        /**
         * The server is interested in receiving willDeleteFiles file requests.
         */
        FileOperationRegistrationOptions*  willDelete;
    };

    template<> struct Schema<FileOperationOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("didCreate",  &FileOperationOptions::didCreate),
            make_field("willCreate", &FileOperationOptions::willCreate),
            make_field("didRename",  &FileOperationOptions::didRename),
            make_field("willRename", &FileOperationOptions::willRename),
            make_field("didDelete",  &FileOperationOptions::didDelete),
            make_field("willDelete", &FileOperationOptions::willDelete)
        );
    };

    /**
     * Structure to capture a description for an error code.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CodeDescription {
        /**
         * An URI to open with more information about the diagnostic error.
         */
        String  href;
    };

    template<> struct Schema<CodeDescription> {
        static constexpr auto fields = std::make_tuple(
            make_field("href", &CodeDescription::href)
        );
    };

    /**
     * Represents a related message and source code location for a diagnostic. This should be
     * used to point to code locations that cause or related to a diagnostics, e.g when duplicating
     * a symbol in a scope.
     */
    struct DiagnosticRelatedInformation {
        /**
         * The location of this related diagnostic information.
         */
        Location*  location;
        /**
         * The message of this related diagnostic information.
         */
        String     message;
    };

    template<> struct Schema<DiagnosticRelatedInformation> {
        static constexpr auto fields = std::make_tuple(
            make_field("location", &DiagnosticRelatedInformation::location),
            make_field("message",  &DiagnosticRelatedInformation::message)
        );
    };

    /**
     * Represents a parameter of a callable-signature. A parameter can
     * have a label and a doc-comment.
     */
    struct ParameterInformation {
        /**
         * The label of this parameter information.
         *
         * Either a string or an inclusive start and exclusive end offsets within its containing
         * signature label. (see SignatureInformation.label). The offsets are based on a UTF-16
         * string representation as `Position` and `Range` does.
         *
         * *Note*: a label of type string should be a substring of its containing signature label.
         * Its intended use case is to highlight the parameter label part in the `SignatureInformation.label`.
         */
        String  label;
        /**
         * The human-readable doc-comment of this parameter. Will be shown
         * in the UI but can be omitted.
         */
        String  documentation;
    };

    template<> struct Schema<ParameterInformation> {
        static constexpr auto fields = std::make_tuple(
            make_field("label",         &ParameterInformation::label),
            make_field("documentation", &ParameterInformation::documentation)
        );
    };

    /**
     * A notebook cell text document filter denotes a cell text
     * document by different properties.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookCellTextDocumentFilter {
        /**
         * A filter that matches against the notebook
         * containing the notebook cell. If a string
         * value is provided it matches against the
         * notebook type. '*' matches every notebook.
         */
        String  notebook;
        /**
         * A language id like `python`.
         *
         * Will be matched against the language id of the
         * notebook cell document. '*' matches every language.
         */
        String  language;
    };

    template<> struct Schema<NotebookCellTextDocumentFilter> {
        static constexpr auto fields = std::make_tuple(
            make_field("notebook", &NotebookCellTextDocumentFilter::notebook),
            make_field("language", &NotebookCellTextDocumentFilter::language)
        );
    };

    /**
     * Matching options for the file operation pattern.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct FileOperationPatternOptions {
        /**
         * The pattern should be matched ignoring casing.
         */
        Bool  ignoreCase;
    };

    template<> struct Schema<FileOperationPatternOptions> {
        static constexpr auto fields = std::make_tuple(
            make_field("ignoreCase", &FileOperationPatternOptions::ignoreCase)
        );
    };

    struct ExecutionSummary {
        /**
         * A strict monotonically increasing value
         * indicating the execution order of a cell
         * inside a notebook.
         */
        UInt  executionOrder;
        /**
         * Whether the execution was successful or
         * not if known by the client.
         */
        Bool  success;
    };

    template<> struct Schema<ExecutionSummary> {
        static constexpr auto fields = std::make_tuple(
            make_field("executionOrder", &ExecutionSummary::executionOrder),
            make_field("success",        &ExecutionSummary::success)
        );
    };

    /**
     * Workspace specific client capabilities.
     */
    struct WorkspaceClientCapabilities {
        /**
         * The client supports applying batch edits
         * to the workspace by supporting the request
         * 'workspace/applyEdit'
         */
        Bool                                        applyEdit;
        /**
         * Capabilities specific to `WorkspaceEdit`s.
         */
        WorkspaceEditClientCapabilities*            workspaceEdit;
        /**
         * Capabilities specific to the `workspace/didChangeConfiguration` notification.
         */
        DidChangeConfigurationClientCapabilities*   didChangeConfiguration;
        /**
         * Capabilities specific to the `workspace/didChangeWatchedFiles` notification.
         */
        DidChangeWatchedFilesClientCapabilities*    didChangeWatchedFiles;
        /**
         * Capabilities specific to the `workspace/symbol` request.
         */
        WorkspaceSymbolClientCapabilities*          symbol;
        /**
         * Capabilities specific to the `workspace/executeCommand` request.
         */
        ExecuteCommandClientCapabilities*           executeCommand;
        /**
         * The client has support for workspace folders.
         *
         * @since 3.6.0
         * @since 3.6.0
         */
        Bool                                        workspaceFolders;
        /**
         * The client supports `workspace/configuration` requests.
         *
         * @since 3.6.0
         * @since 3.6.0
         */
        Bool                                        configuration;
        /**
         * Capabilities specific to the semantic token requests scoped to the
         * workspace.
         *
         * @since 3.16.0.
         * @since 3.16.0.
         */
        SemanticTokensWorkspaceClientCapabilities*  semanticTokens;
        /**
         * Capabilities specific to the code lens requests scoped to the
         * workspace.
         *
         * @since 3.16.0.
         * @since 3.16.0.
         */
        CodeLensWorkspaceClientCapabilities*        codeLens;
        /**
         * The client has support for file notifications/requests for user operations on files.
         *
         * Since 3.16.0
         */
        FileOperationClientCapabilities*            fileOperations;
        /**
         * Capabilities specific to the inline values requests scoped to the
         * workspace.
         *
         * @since 3.17.0.
         * @since 3.17.0.
         */
        InlineValueWorkspaceClientCapabilities*     inlineValue;
        /**
         * Capabilities specific to the inlay hint requests scoped to the
         * workspace.
         *
         * @since 3.17.0.
         * @since 3.17.0.
         */
        InlayHintWorkspaceClientCapabilities*       inlayHint;
        /**
         * Capabilities specific to the diagnostic requests scoped to the
         * workspace.
         *
         * @since 3.17.0.
         * @since 3.17.0.
         */
        DiagnosticWorkspaceClientCapabilities*      diagnostics;
        /**
         * Capabilities specific to the folding range requests scoped to the workspace.
         *
         * @since 3.18.0
         * @proposed
         * @since 3.18.0
         */
        FoldingRangeWorkspaceClientCapabilities*    foldingRange;
    };

    template<> struct Schema<WorkspaceClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("applyEdit",              &WorkspaceClientCapabilities::applyEdit),
            make_field("workspaceEdit",          &WorkspaceClientCapabilities::workspaceEdit),
            make_field("didChangeConfiguration", &WorkspaceClientCapabilities::didChangeConfiguration),
            make_field("didChangeWatchedFiles",  &WorkspaceClientCapabilities::didChangeWatchedFiles),
            make_field("symbol",                 &WorkspaceClientCapabilities::symbol),
            make_field("executeCommand",         &WorkspaceClientCapabilities::executeCommand),
            make_field("workspaceFolders",       &WorkspaceClientCapabilities::workspaceFolders),
            make_field("configuration",          &WorkspaceClientCapabilities::configuration),
            make_field("semanticTokens",         &WorkspaceClientCapabilities::semanticTokens),
            make_field("codeLens",               &WorkspaceClientCapabilities::codeLens),
            make_field("fileOperations",         &WorkspaceClientCapabilities::fileOperations),
            make_field("inlineValue",            &WorkspaceClientCapabilities::inlineValue),
            make_field("inlayHint",              &WorkspaceClientCapabilities::inlayHint),
            make_field("diagnostics",            &WorkspaceClientCapabilities::diagnostics),
            make_field("foldingRange",           &WorkspaceClientCapabilities::foldingRange)
        );
    };

    /**
     * Text document specific client capabilities.
     */
    struct TextDocumentClientCapabilities {
        /**
         * Defines which synchronization capabilities the client supports.
         */
        TextDocumentSyncClientCapabilities*          synchronization;
        /**
         * Capabilities specific to the `textDocument/completion` request.
         */
        CompletionClientCapabilities*                completion;
        /**
         * Capabilities specific to the `textDocument/hover` request.
         */
        HoverClientCapabilities*                     hover;
        /**
         * Capabilities specific to the `textDocument/signatureHelp` request.
         */
        SignatureHelpClientCapabilities*             signatureHelp;
        /**
         * Capabilities specific to the `textDocument/declaration` request.
         *
         * @since 3.14.0
         * @since 3.14.0
         */
        DeclarationClientCapabilities*               declaration;
        /**
         * Capabilities specific to the `textDocument/definition` request.
         */
        DefinitionClientCapabilities*                definition;
        /**
         * Capabilities specific to the `textDocument/typeDefinition` request.
         *
         * @since 3.6.0
         * @since 3.6.0
         */
        TypeDefinitionClientCapabilities*            typeDefinition;
        /**
         * Capabilities specific to the `textDocument/implementation` request.
         *
         * @since 3.6.0
         * @since 3.6.0
         */
        ImplementationClientCapabilities*            implementation;
        /**
         * Capabilities specific to the `textDocument/references` request.
         */
        ReferenceClientCapabilities*                 references;
        /**
         * Capabilities specific to the `textDocument/documentHighlight` request.
         */
        DocumentHighlightClientCapabilities*         documentHighlight;
        /**
         * Capabilities specific to the `textDocument/documentSymbol` request.
         */
        DocumentSymbolClientCapabilities*            documentSymbol;
        /**
         * Capabilities specific to the `textDocument/codeAction` request.
         */
        CodeActionClientCapabilities*                codeAction;
        /**
         * Capabilities specific to the `textDocument/codeLens` request.
         */
        CodeLensClientCapabilities*                  codeLens;
        /**
         * Capabilities specific to the `textDocument/documentLink` request.
         */
        DocumentLinkClientCapabilities*              documentLink;
        /**
         * Capabilities specific to the `textDocument/documentColor` and the
         * `textDocument/colorPresentation` request.
         *
         * @since 3.6.0
         * @since 3.6.0
         */
        DocumentColorClientCapabilities*             colorProvider;
        /**
         * Capabilities specific to the `textDocument/formatting` request.
         */
        DocumentFormattingClientCapabilities*        formatting;
        /**
         * Capabilities specific to the `textDocument/rangeFormatting` request.
         */
        DocumentRangeFormattingClientCapabilities*   rangeFormatting;
        /**
         * Capabilities specific to the `textDocument/onTypeFormatting` request.
         */
        DocumentOnTypeFormattingClientCapabilities*  onTypeFormatting;
        /**
         * Capabilities specific to the `textDocument/rename` request.
         */
        RenameClientCapabilities*                    rename;
        /**
         * Capabilities specific to the `textDocument/foldingRange` request.
         *
         * @since 3.10.0
         * @since 3.10.0
         */
        FoldingRangeClientCapabilities*              foldingRange;
        /**
         * Capabilities specific to the `textDocument/selectionRange` request.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        SelectionRangeClientCapabilities*            selectionRange;
        /**
         * Capabilities specific to the `textDocument/publishDiagnostics` notification.
         */
        PublishDiagnosticsClientCapabilities*        publishDiagnostics;
        /**
         * Capabilities specific to the various call hierarchy requests.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        CallHierarchyClientCapabilities*             callHierarchy;
        /**
         * Capabilities specific to the various semantic token request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        SemanticTokensClientCapabilities*            semanticTokens;
        /**
         * Capabilities specific to the `textDocument/linkedEditingRange` request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        LinkedEditingRangeClientCapabilities*        linkedEditingRange;
        /**
         * Client capabilities specific to the `textDocument/moniker` request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        MonikerClientCapabilities*                   moniker;
        /**
         * Capabilities specific to the various type hierarchy requests.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        TypeHierarchyClientCapabilities*             typeHierarchy;
        /**
         * Capabilities specific to the `textDocument/inlineValue` request.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        InlineValueClientCapabilities*               inlineValue;
        /**
         * Capabilities specific to the `textDocument/inlayHint` request.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        InlayHintClientCapabilities*                 inlayHint;
        /**
         * Capabilities specific to the diagnostic pull model.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        DiagnosticClientCapabilities*                diagnostic;
        /**
         * Client capabilities specific to inline completions.
         *
         * @since 3.18.0
         * @proposed
         * @since 3.18.0
         */
        InlineCompletionClientCapabilities*          inlineCompletion;
    };

    template<> struct Schema<TextDocumentClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("synchronization",    &TextDocumentClientCapabilities::synchronization),
            make_field("completion",         &TextDocumentClientCapabilities::completion),
            make_field("hover",              &TextDocumentClientCapabilities::hover),
            make_field("signatureHelp",      &TextDocumentClientCapabilities::signatureHelp),
            make_field("declaration",        &TextDocumentClientCapabilities::declaration),
            make_field("definition",         &TextDocumentClientCapabilities::definition),
            make_field("typeDefinition",     &TextDocumentClientCapabilities::typeDefinition),
            make_field("implementation",     &TextDocumentClientCapabilities::implementation),
            make_field("references",         &TextDocumentClientCapabilities::references),
            make_field("documentHighlight",  &TextDocumentClientCapabilities::documentHighlight),
            make_field("documentSymbol",     &TextDocumentClientCapabilities::documentSymbol),
            make_field("codeAction",         &TextDocumentClientCapabilities::codeAction),
            make_field("codeLens",           &TextDocumentClientCapabilities::codeLens),
            make_field("documentLink",       &TextDocumentClientCapabilities::documentLink),
            make_field("colorProvider",      &TextDocumentClientCapabilities::colorProvider),
            make_field("formatting",         &TextDocumentClientCapabilities::formatting),
            make_field("rangeFormatting",    &TextDocumentClientCapabilities::rangeFormatting),
            make_field("onTypeFormatting",   &TextDocumentClientCapabilities::onTypeFormatting),
            make_field("rename",             &TextDocumentClientCapabilities::rename),
            make_field("foldingRange",       &TextDocumentClientCapabilities::foldingRange),
            make_field("selectionRange",     &TextDocumentClientCapabilities::selectionRange),
            make_field("publishDiagnostics", &TextDocumentClientCapabilities::publishDiagnostics),
            make_field("callHierarchy",      &TextDocumentClientCapabilities::callHierarchy),
            make_field("semanticTokens",     &TextDocumentClientCapabilities::semanticTokens),
            make_field("linkedEditingRange", &TextDocumentClientCapabilities::linkedEditingRange),
            make_field("moniker",            &TextDocumentClientCapabilities::moniker),
            make_field("typeHierarchy",      &TextDocumentClientCapabilities::typeHierarchy),
            make_field("inlineValue",        &TextDocumentClientCapabilities::inlineValue),
            make_field("inlayHint",          &TextDocumentClientCapabilities::inlayHint),
            make_field("diagnostic",         &TextDocumentClientCapabilities::diagnostic),
            make_field("inlineCompletion",   &TextDocumentClientCapabilities::inlineCompletion)
        );
    };

    /**
     * Capabilities specific to the notebook document support.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookDocumentClientCapabilities {
        /**
         * Capabilities specific to notebook document synchronization
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        NotebookDocumentSyncClientCapabilities*  synchronization;
    };

    template<> struct Schema<NotebookDocumentClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("synchronization", &NotebookDocumentClientCapabilities::synchronization)
        );
    };

    struct WindowClientCapabilities {
        /**
         * It indicates whether the client supports server initiated
         * progress using the `window/workDoneProgress/create` request.
         *
         * The capability also controls Whether client supports handling
         * of progress notifications. If set servers are allowed to report a
         * `workDoneProgress` property in the request specific server
         * capabilities.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Bool                                   workDoneProgress;
        /**
         * Capabilities specific to the showMessage request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        ShowMessageRequestClientCapabilities*  showMessage;
        /**
         * Capabilities specific to the showDocument request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        ShowDocumentClientCapabilities*        showDocument;
    };

    template<> struct Schema<WindowClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("workDoneProgress", &WindowClientCapabilities::workDoneProgress),
            make_field("showMessage",      &WindowClientCapabilities::showMessage),
            make_field("showDocument",     &WindowClientCapabilities::showDocument)
        );
    };

    /**
     * General client capabilities.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct GeneralClientCapabilities {
        /**
         * Client capability that signals how the client
         * handles stale requests (e.g. a request
         * for which the client will not process the response
         * anymore since the information is outdated).
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        struct _StaleRequestSupport {
            /**
             * The client will actively cancel the request.
             */
            Bool           cancel;
            /**
             * The list of requests for which the client
             * will retry the request if it receives a
             * response with error code `ContentModified`
             */
            Slice<String>  retryOnContentModified;
        }* staleRequestSupport;
        /**
         * Client capabilities specific to regular expressions.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        RegularExpressionsClientCapabilities*  regularExpressions;
        /**
         * Client capabilities specific to the client's markdown parser.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        MarkdownClientCapabilities*            markdown;
        /**
         * The position encodings supported by the client. Client and server
         * have to agree on the same position encoding to ensure that offsets
         * (e.g. character position in a line) are interpreted the same on both
         * sides.
         *
         * To keep the protocol backwards compatible the following applies: if
         * the value 'utf-16' is missing from the array of position encodings
         * servers can assume that the client supports UTF-16. UTF-16 is
         * therefore a mandatory encoding.
         *
         * If omitted it defaults to ['utf-16'].
         *
         * Implementation considerations: since the conversion from one encoding
         * into another requires the content of the file / line the conversion
         * is best done where the file is read which is usually on the server
         * side.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        Slice<PositionEncodingKind>            positionEncodings;
    };

    template<> struct Schema<GeneralClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("staleRequestSupport", &GeneralClientCapabilities::staleRequestSupport),
            make_field("regularExpressions",  &GeneralClientCapabilities::regularExpressions),
            make_field("markdown",            &GeneralClientCapabilities::markdown),
            make_field("positionEncodings",   &GeneralClientCapabilities::positionEncodings)
        );
    };

    template<> struct Schema<GeneralClientCapabilities::_StaleRequestSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("cancel",                 &GeneralClientCapabilities::_StaleRequestSupport::cancel),
            make_field("retryOnContentModified", &GeneralClientCapabilities::_StaleRequestSupport::retryOnContentModified)
        );
    };

    /**
     * A relative pattern is a helper to construct glob patterns that are matched
     * relatively to a base URI. The common value for a `baseUri` is a workspace
     * folder root, but it can be another absolute URI as well.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct RelativePattern {
        /**
         * A workspace folder or a base URI to which this pattern will be matched
         * against relatively.
         */
        WorkspaceFolder*  baseUri;
        /**
         * The actual glob pattern;
         */
        String            pattern;
    };

    template<> struct Schema<RelativePattern> {
        static constexpr auto fields = std::make_tuple(
            make_field("baseUri", &RelativePattern::baseUri),
            make_field("pattern", &RelativePattern::pattern)
        );
    };

    struct WorkspaceEditClientCapabilities {
        /**
         * The client supports versioned document changes in `WorkspaceEdit`s
         */
        Bool                          documentChanges;
        /**
         * The resource operations the client supports. Clients should at least
         * support 'create', 'rename' and 'delete' files and folders.
         *
         * @since 3.13.0
         * @since 3.13.0
         */
        Slice<ResourceOperationKind>  resourceOperations;
        /**
         * The failure handling strategy of a client if applying the workspace edit
         * fails.
         *
         * @since 3.13.0
         * @since 3.13.0
         */
        FailureHandlingKind           failureHandling;
        /**
         * Whether the client normalizes line endings to the client specific
         * setting.
         * If set to `true` the client will normalize line ending characters
         * in a workspace edit to the client-specified new line
         * character.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool                          normalizesLineEndings;
        /**
         * Whether the client in general supports change annotations on text edits,
         * create file, rename file and delete file changes.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        struct _ChangeAnnotationSupport {
            /**
             * Whether the client groups edits with equal labels into tree nodes,
             * for instance all edits labelled with "Changes in Strings" would
             * be a tree node.
             */
            Bool  groupsOnLabel;
        }* changeAnnotationSupport;
    };

    template<> struct Schema<WorkspaceEditClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentChanges",         &WorkspaceEditClientCapabilities::documentChanges),
            make_field("resourceOperations",      &WorkspaceEditClientCapabilities::resourceOperations),
            make_field("failureHandling",         &WorkspaceEditClientCapabilities::failureHandling),
            make_field("normalizesLineEndings",   &WorkspaceEditClientCapabilities::normalizesLineEndings),
            make_field("changeAnnotationSupport", &WorkspaceEditClientCapabilities::changeAnnotationSupport)
        );
    };

    template<> struct Schema<WorkspaceEditClientCapabilities::_ChangeAnnotationSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("groupsOnLabel", &WorkspaceEditClientCapabilities::_ChangeAnnotationSupport::groupsOnLabel)
        );
    };

    struct DidChangeConfigurationClientCapabilities {
        /**
         * Did change configuration notification supports dynamic registration.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<DidChangeConfigurationClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &DidChangeConfigurationClientCapabilities::dynamicRegistration)
        );
    };

    struct DidChangeWatchedFilesClientCapabilities {
        /**
         * Did change watched files notification supports dynamic registration. Please note
         * that the current protocol doesn't support static configuration for file changes
         * from the server side.
         */
        Bool  dynamicRegistration;
        /**
         * Whether the client has support for {@link  RelativePattern relative pattern}
         * or not.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        Bool  relativePatternSupport;
    };

    template<> struct Schema<DidChangeWatchedFilesClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration",    &DidChangeWatchedFilesClientCapabilities::dynamicRegistration),
            make_field("relativePatternSupport", &DidChangeWatchedFilesClientCapabilities::relativePatternSupport)
        );
    };

    /**
     * Client capabilities for a {@link WorkspaceSymbolRequest}.
     */
    struct WorkspaceSymbolClientCapabilities {
        /**
         * Symbol request supports dynamic registration.
         */
        Bool              dynamicRegistration;
        /**
         * Specific capabilities for the `SymbolKind` in the `workspace/symbol` request.
         */
        struct _SymbolKind {
            /**
             * The symbol kind values the client supports. When this
             * property exists the client also guarantees that it will
             * handle values outside its set gracefully and falls back
             * to a default value when unknown.
             *
             * If this property is not present the client only supports
             * the symbol kinds from `File` to `Array` as defined in
             * the initial version of the protocol.
             */
            Slice<SymbolKind>  valueSet;
        }* symbolKind;
        /**
         * The client supports tags on `SymbolInformation`.
         * Clients supporting tags have to handle unknown tags gracefully.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        struct _TagSupport {
            /**
             * The tags supported by the client.
             */
            Slice<SymbolTag>  valueSet;
        }* tagSupport;
        /**
         * The client support partial workspace symbols. The client will send the
         * request `workspaceSymbol/resolve` to the server to resolve additional
         * properties.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        struct _ResolveSupport {
            /**
             * The properties that a client can resolve lazily. Usually
             * `location.range`
             */
            Slice<String>  properties;
        }* resolveSupport;
    };

    template<> struct Schema<WorkspaceSymbolClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &WorkspaceSymbolClientCapabilities::dynamicRegistration),
            make_field("symbolKind",          &WorkspaceSymbolClientCapabilities::symbolKind),
            make_field("tagSupport",          &WorkspaceSymbolClientCapabilities::tagSupport),
            make_field("resolveSupport",      &WorkspaceSymbolClientCapabilities::resolveSupport)
        );
    };

    template<> struct Schema<WorkspaceSymbolClientCapabilities::_SymbolKind> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &WorkspaceSymbolClientCapabilities::_SymbolKind::valueSet)
        );
    };

    template<> struct Schema<WorkspaceSymbolClientCapabilities::_TagSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &WorkspaceSymbolClientCapabilities::_TagSupport::valueSet)
        );
    };

    template<> struct Schema<WorkspaceSymbolClientCapabilities::_ResolveSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("properties", &WorkspaceSymbolClientCapabilities::_ResolveSupport::properties)
        );
    };

    /**
     * The client capabilities of a {@link ExecuteCommandRequest}.
     */
    struct ExecuteCommandClientCapabilities {
        /**
         * Execute command supports dynamic registration.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<ExecuteCommandClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &ExecuteCommandClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensWorkspaceClientCapabilities {
        /**
         * Whether the client implementation supports a refresh request sent from
         * the server to the client.
         *
         * Note that this event is global and will force the client to refresh all
         * semantic tokens currently shown. It should be used with absolute care
         * and is useful for situation where a server for example detects a project
         * wide change that requires such a calculation.
         */
        Bool  refreshSupport;
    };

    template<> struct Schema<SemanticTokensWorkspaceClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("refreshSupport", &SemanticTokensWorkspaceClientCapabilities::refreshSupport)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CodeLensWorkspaceClientCapabilities {
        /**
         * Whether the client implementation supports a refresh request sent from the
         * server to the client.
         *
         * Note that this event is global and will force the client to refresh all
         * code lenses currently shown. It should be used with absolute care and is
         * useful for situation where a server for example detect a project wide
         * change that requires such a calculation.
         */
        Bool  refreshSupport;
    };

    template<> struct Schema<CodeLensWorkspaceClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("refreshSupport", &CodeLensWorkspaceClientCapabilities::refreshSupport)
        );
    };

    /**
     * Capabilities relating to events from file operations by the user in the client.
     *
     * These events do not come from the file system, they come from user operations
     * like renaming a file in the UI.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct FileOperationClientCapabilities {
        /**
         * Whether the client supports dynamic registration for file requests/notifications.
         */
        Bool  dynamicRegistration;
        /**
         * The client has support for sending didCreateFiles notifications.
         */
        Bool  didCreate;
        /**
         * The client has support for sending willCreateFiles requests.
         */
        Bool  willCreate;
        /**
         * The client has support for sending didRenameFiles notifications.
         */
        Bool  didRename;
        /**
         * The client has support for sending willRenameFiles requests.
         */
        Bool  willRename;
        /**
         * The client has support for sending didDeleteFiles notifications.
         */
        Bool  didDelete;
        /**
         * The client has support for sending willDeleteFiles requests.
         */
        Bool  willDelete;
    };

    template<> struct Schema<FileOperationClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &FileOperationClientCapabilities::dynamicRegistration),
            make_field("didCreate",           &FileOperationClientCapabilities::didCreate),
            make_field("willCreate",          &FileOperationClientCapabilities::willCreate),
            make_field("didRename",           &FileOperationClientCapabilities::didRename),
            make_field("willRename",          &FileOperationClientCapabilities::willRename),
            make_field("didDelete",           &FileOperationClientCapabilities::didDelete),
            make_field("willDelete",          &FileOperationClientCapabilities::willDelete)
        );
    };

    /**
     * Client workspace capabilities specific to inline values.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValueWorkspaceClientCapabilities {
        /**
         * Whether the client implementation supports a refresh request sent from the
         * server to the client.
         *
         * Note that this event is global and will force the client to refresh all
         * inline values currently shown. It should be used with absolute care and is
         * useful for situation where a server for example detects a project wide
         * change that requires such a calculation.
         */
        Bool  refreshSupport;
    };

    template<> struct Schema<InlineValueWorkspaceClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("refreshSupport", &InlineValueWorkspaceClientCapabilities::refreshSupport)
        );
    };

    /**
     * Client workspace capabilities specific to inlay hints.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlayHintWorkspaceClientCapabilities {
        /**
         * Whether the client implementation supports a refresh request sent from
         * the server to the client.
         *
         * Note that this event is global and will force the client to refresh all
         * inlay hints currently shown. It should be used with absolute care and
         * is useful for situation where a server for example detects a project wide
         * change that requires such a calculation.
         */
        Bool  refreshSupport;
    };

    template<> struct Schema<InlayHintWorkspaceClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("refreshSupport", &InlayHintWorkspaceClientCapabilities::refreshSupport)
        );
    };

    /**
     * Workspace client capabilities specific to diagnostic pull requests.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DiagnosticWorkspaceClientCapabilities {
        /**
         * Whether the client implementation supports a refresh request sent from
         * the server to the client.
         *
         * Note that this event is global and will force the client to refresh all
         * pulled diagnostics currently shown. It should be used with absolute care and
         * is useful for situation where a server for example detects a project wide
         * change that requires such a calculation.
         */
        Bool  refreshSupport;
    };

    template<> struct Schema<DiagnosticWorkspaceClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("refreshSupport", &DiagnosticWorkspaceClientCapabilities::refreshSupport)
        );
    };

    /**
     * Client workspace capabilities specific to folding ranges
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct FoldingRangeWorkspaceClientCapabilities {
        /**
         * Whether the client implementation supports a refresh request sent from the
         * server to the client.
         *
         * Note that this event is global and will force the client to refresh all
         * folding ranges currently shown. It should be used with absolute care and is
         * useful for situation where a server for example detects a project wide
         * change that requires such a calculation.
         *
         * @since 3.18.0
         * @proposed
         * @since 3.18.0
         */
        Bool  refreshSupport;
    };

    template<> struct Schema<FoldingRangeWorkspaceClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("refreshSupport", &FoldingRangeWorkspaceClientCapabilities::refreshSupport)
        );
    };

    struct TextDocumentSyncClientCapabilities {
        /**
         * Whether text document synchronization supports dynamic registration.
         */
        Bool  dynamicRegistration;
        /**
         * The client supports sending will save notifications.
         */
        Bool  willSave;
        /**
         * The client supports sending a will save request and
         * waits for a response providing text edits which will
         * be applied to the document before it is saved.
         */
        Bool  willSaveWaitUntil;
        /**
         * The client supports did save notifications.
         */
        Bool  didSave;
    };

    template<> struct Schema<TextDocumentSyncClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &TextDocumentSyncClientCapabilities::dynamicRegistration),
            make_field("willSave",            &TextDocumentSyncClientCapabilities::willSave),
            make_field("willSaveWaitUntil",   &TextDocumentSyncClientCapabilities::willSaveWaitUntil),
            make_field("didSave",             &TextDocumentSyncClientCapabilities::didSave)
        );
    };

    /**
     * Completion client capabilities
     */
    struct CompletionClientCapabilities {
        /**
         * Whether completion supports dynamic registration.
         */
        Bool                  dynamicRegistration;
        /**
         * The client supports the following `CompletionItem` specific
         * capabilities.
         */
        struct _CompletionItem {
            /**
             * Client supports snippets as insert text.
             *
             * A snippet can define tab stops and placeholders with `$1`, `$2`
             * and `${3:foo}`. `$0` defines the final tab stop, it defaults to
             * the end of the snippet. Placeholders with equal identifiers are linked,
             * that is typing in one will update others too.
             */
            Bool                     snippetSupport;
            /**
             * Client supports commit characters on a completion item.
             */
            Bool                     commitCharactersSupport;
            /**
             * Client supports the following content formats for the documentation
             * property. The order describes the preferred format of the client.
             */
            Slice<MarkupKind>        documentationFormat;
            /**
             * Client supports the deprecated property on a completion item.
             */
            Bool                     deprecatedSupport;
            /**
             * Client supports the preselect property on a completion item.
             */
            Bool                     preselectSupport;
            /**
             * Client supports the tag property on a completion item. Clients supporting
             * tags have to handle unknown tags gracefully. Clients especially need to
             * preserve unknown tags when sending a completion item back to the server in
             * a resolve call.
             *
             * @since 3.15.0
             * @since 3.15.0
             */
            struct _TagSupport {
                /**
                 * The tags supported by the client.
                 */
                Slice<CompletionItemTag>  valueSet;
            }* tagSupport;
            /**
             * Client support insert replace edit to control different behavior if a
             * completion item is inserted in the text or should replace text.
             *
             * @since 3.16.0
             * @since 3.16.0
             */
            Bool                     insertReplaceSupport;
            /**
             * Indicates which properties a client can resolve lazily on a completion
             * item. Before version 3.16.0 only the predefined properties `documentation`
             * and `details` could be resolved lazily.
             *
             * @since 3.16.0
             * @since 3.16.0
             */
            struct _ResolveSupport {
                /**
                 * The properties that a client can resolve lazily.
                 */
                Slice<String>  properties;
            }* resolveSupport;
            /**
             * The client supports the `insertTextMode` property on
             * a completion item to override the whitespace handling mode
             * as defined by the client (see `insertTextMode`).
             *
             * @since 3.16.0
             * @since 3.16.0
             */
            struct _InsertTextModeSupport {
                Slice<InsertTextMode>  valueSet;
            }* insertTextModeSupport;
            /**
             * The client has support for completion item label
             * details (see also `CompletionItemLabelDetails`).
             *
             * @since 3.17.0
             * @since 3.17.0
             */
            Bool                     labelDetailsSupport;
        }* completionItem;
        struct _CompletionItemKind {
            /**
             * The completion item kind values the client supports. When this
             * property exists the client also guarantees that it will
             * handle values outside its set gracefully and falls back
             * to a default value when unknown.
             *
             * If this property is not present the client only supports
             * the completion items kinds from `Text` to `Reference` as defined in
             * the initial version of the protocol.
             */
            Slice<CompletionItemKind>  valueSet;
        }* completionItemKind;
        /**
         * Defines how the client handles whitespace and indentation
         * when accepting a completion item that uses multi line
         * text in either `insertText` or `textEdit`.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        InsertTextMode        insertTextMode;
        /**
         * The client supports to send additional context information for a
         * `textDocument/completion` request.
         */
        Bool                  contextSupport;
        /**
         * The client supports the following `CompletionList` specific
         * capabilities.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        struct _CompletionList {
            /**
             * The client supports the following itemDefaults on
             * a completion list.
             *
             * The value lists the supported property names of the
             * `CompletionList.itemDefaults` object. If omitted
             * no properties are supported.
             *
             * @since 3.17.0
             * @since 3.17.0
             */
            Slice<String>  itemDefaults;
        }* completionList;
    };

    template<> struct Schema<CompletionClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &CompletionClientCapabilities::dynamicRegistration),
            make_field("completionItem",      &CompletionClientCapabilities::completionItem),
            make_field("completionItemKind",  &CompletionClientCapabilities::completionItemKind),
            make_field("insertTextMode",      &CompletionClientCapabilities::insertTextMode),
            make_field("contextSupport",      &CompletionClientCapabilities::contextSupport),
            make_field("completionList",      &CompletionClientCapabilities::completionList)
        );
    };

    template<> struct Schema<CompletionClientCapabilities::_CompletionItem::_TagSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &CompletionClientCapabilities::_CompletionItem::_TagSupport::valueSet)
        );
    };

    template<> struct Schema<CompletionClientCapabilities::_CompletionItem::_ResolveSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("properties", &CompletionClientCapabilities::_CompletionItem::_ResolveSupport::properties)
        );
    };

    template<> struct Schema<CompletionClientCapabilities::_CompletionItem::_InsertTextModeSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &CompletionClientCapabilities::_CompletionItem::_InsertTextModeSupport::valueSet)
        );
    };

    template<> struct Schema<CompletionClientCapabilities::_CompletionItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("snippetSupport",          &CompletionClientCapabilities::_CompletionItem::snippetSupport),
            make_field("commitCharactersSupport", &CompletionClientCapabilities::_CompletionItem::commitCharactersSupport),
            make_field("documentationFormat",     &CompletionClientCapabilities::_CompletionItem::documentationFormat),
            make_field("deprecatedSupport",       &CompletionClientCapabilities::_CompletionItem::deprecatedSupport),
            make_field("preselectSupport",        &CompletionClientCapabilities::_CompletionItem::preselectSupport),
            make_field("tagSupport",              &CompletionClientCapabilities::_CompletionItem::tagSupport),
            make_field("insertReplaceSupport",    &CompletionClientCapabilities::_CompletionItem::insertReplaceSupport),
            make_field("resolveSupport",          &CompletionClientCapabilities::_CompletionItem::resolveSupport),
            make_field("insertTextModeSupport",   &CompletionClientCapabilities::_CompletionItem::insertTextModeSupport),
            make_field("labelDetailsSupport",     &CompletionClientCapabilities::_CompletionItem::labelDetailsSupport)
        );
    };

    template<> struct Schema<CompletionClientCapabilities::_CompletionItemKind> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &CompletionClientCapabilities::_CompletionItemKind::valueSet)
        );
    };

    template<> struct Schema<CompletionClientCapabilities::_CompletionList> {
        static constexpr auto fields = std::make_tuple(
            make_field("itemDefaults", &CompletionClientCapabilities::_CompletionList::itemDefaults)
        );
    };

    struct HoverClientCapabilities {
        /**
         * Whether hover supports dynamic registration.
         */
        Bool               dynamicRegistration;
        /**
         * Client supports the following content formats for the content
         * property. The order describes the preferred format of the client.
         */
        Slice<MarkupKind>  contentFormat;
    };

    template<> struct Schema<HoverClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &HoverClientCapabilities::dynamicRegistration),
            make_field("contentFormat",       &HoverClientCapabilities::contentFormat)
        );
    };

    /**
     * Client Capabilities for a {@link SignatureHelpRequest}.
     */
    struct SignatureHelpClientCapabilities {
        /**
         * Whether signature help supports dynamic registration.
         */
        Bool                    dynamicRegistration;
        /**
         * The client supports the following `SignatureInformation`
         * specific properties.
         */
        struct _SignatureInformation {
            /**
             * Client supports the following content formats for the documentation
             * property. The order describes the preferred format of the client.
             */
            Slice<MarkupKind>       documentationFormat;
            /**
             * Client capabilities specific to parameter information.
             */
            struct _ParameterInformation {
                /**
                 * The client supports processing label offsets instead of a
                 * simple label string.
                 *
                 * @since 3.14.0
                 * @since 3.14.0
                 */
                Bool  labelOffsetSupport;
            }* parameterInformation;
            /**
             * The client supports the `activeParameter` property on `SignatureInformation`
             * literal.
             *
             * @since 3.16.0
             * @since 3.16.0
             */
            Bool                    activeParameterSupport;
        }* signatureInformation;
        /**
         * The client supports to send additional context information for a
         * `textDocument/signatureHelp` request. A client that opts into
         * contextSupport will also support the `retriggerCharacters` on
         * `SignatureHelpOptions`.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Bool                    contextSupport;
    };

    template<> struct Schema<SignatureHelpClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration",  &SignatureHelpClientCapabilities::dynamicRegistration),
            make_field("signatureInformation", &SignatureHelpClientCapabilities::signatureInformation),
            make_field("contextSupport",       &SignatureHelpClientCapabilities::contextSupport)
        );
    };

    template<> struct Schema<SignatureHelpClientCapabilities::_SignatureInformation::_ParameterInformation> {
        static constexpr auto fields = std::make_tuple(
            make_field("labelOffsetSupport", &SignatureHelpClientCapabilities::_SignatureInformation::_ParameterInformation::labelOffsetSupport)
        );
    };

    template<> struct Schema<SignatureHelpClientCapabilities::_SignatureInformation> {
        static constexpr auto fields = std::make_tuple(
            make_field("documentationFormat",    &SignatureHelpClientCapabilities::_SignatureInformation::documentationFormat),
            make_field("parameterInformation",   &SignatureHelpClientCapabilities::_SignatureInformation::parameterInformation),
            make_field("activeParameterSupport", &SignatureHelpClientCapabilities::_SignatureInformation::activeParameterSupport)
        );
    };

    /**
     * @since 3.14.0
     * @since 3.14.0
     */
    struct DeclarationClientCapabilities {
        /**
         * Whether declaration supports dynamic registration. If this is set to `true`
         * the client supports the new `DeclarationRegistrationOptions` return value
         * for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
        /**
         * The client supports additional metadata in the form of declaration links.
         */
        Bool  linkSupport;
    };

    template<> struct Schema<DeclarationClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &DeclarationClientCapabilities::dynamicRegistration),
            make_field("linkSupport",         &DeclarationClientCapabilities::linkSupport)
        );
    };

    /**
     * Client Capabilities for a {@link DefinitionRequest}.
     */
    struct DefinitionClientCapabilities {
        /**
         * Whether definition supports dynamic registration.
         */
        Bool  dynamicRegistration;
        /**
         * The client supports additional metadata in the form of definition links.
         *
         * @since 3.14.0
         * @since 3.14.0
         */
        Bool  linkSupport;
    };

    template<> struct Schema<DefinitionClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &DefinitionClientCapabilities::dynamicRegistration),
            make_field("linkSupport",         &DefinitionClientCapabilities::linkSupport)
        );
    };

    /**
     * Since 3.6.0
     */
    struct TypeDefinitionClientCapabilities {
        /**
         * Whether implementation supports dynamic registration. If this is set to `true`
         * the client supports the new `TypeDefinitionRegistrationOptions` return value
         * for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
        /**
         * The client supports additional metadata in the form of definition links.
         *
         * Since 3.14.0
         */
        Bool  linkSupport;
    };

    template<> struct Schema<TypeDefinitionClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &TypeDefinitionClientCapabilities::dynamicRegistration),
            make_field("linkSupport",         &TypeDefinitionClientCapabilities::linkSupport)
        );
    };

    /**
     * @since 3.6.0
     * @since 3.6.0
     */
    struct ImplementationClientCapabilities {
        /**
         * Whether implementation supports dynamic registration. If this is set to `true`
         * the client supports the new `ImplementationRegistrationOptions` return value
         * for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
        /**
         * The client supports additional metadata in the form of definition links.
         *
         * @since 3.14.0
         * @since 3.14.0
         */
        Bool  linkSupport;
    };

    template<> struct Schema<ImplementationClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &ImplementationClientCapabilities::dynamicRegistration),
            make_field("linkSupport",         &ImplementationClientCapabilities::linkSupport)
        );
    };

    /**
     * Client Capabilities for a {@link ReferencesRequest}.
     */
    struct ReferenceClientCapabilities {
        /**
         * Whether references supports dynamic registration.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<ReferenceClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &ReferenceClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * Client Capabilities for a {@link DocumentHighlightRequest}.
     */
    struct DocumentHighlightClientCapabilities {
        /**
         * Whether document highlight supports dynamic registration.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<DocumentHighlightClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &DocumentHighlightClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * Client Capabilities for a {@link DocumentSymbolRequest}.
     */
    struct DocumentSymbolClientCapabilities {
        /**
         * Whether document symbol supports dynamic registration.
         */
        Bool          dynamicRegistration;
        /**
         * Specific capabilities for the `SymbolKind` in the
         * `textDocument/documentSymbol` request.
         */
        struct _SymbolKind {
            /**
             * The symbol kind values the client supports. When this
             * property exists the client also guarantees that it will
             * handle values outside its set gracefully and falls back
             * to a default value when unknown.
             *
             * If this property is not present the client only supports
             * the symbol kinds from `File` to `Array` as defined in
             * the initial version of the protocol.
             */
            Slice<SymbolKind>  valueSet;
        }* symbolKind;
        /**
         * The client supports hierarchical document symbols.
         */
        Bool          hierarchicalDocumentSymbolSupport;
        /**
         * The client supports tags on `SymbolInformation`. Tags are supported on
         * `DocumentSymbol` if `hierarchicalDocumentSymbolSupport` is set to true.
         * Clients supporting tags have to handle unknown tags gracefully.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        struct _TagSupport {
            /**
             * The tags supported by the client.
             */
            Slice<SymbolTag>  valueSet;
        }* tagSupport;
        /**
         * The client supports an additional label presented in the UI when
         * registering a document symbol provider.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool          labelSupport;
    };

    template<> struct Schema<DocumentSymbolClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration",               &DocumentSymbolClientCapabilities::dynamicRegistration),
            make_field("symbolKind",                        &DocumentSymbolClientCapabilities::symbolKind),
            make_field("hierarchicalDocumentSymbolSupport", &DocumentSymbolClientCapabilities::hierarchicalDocumentSymbolSupport),
            make_field("tagSupport",                        &DocumentSymbolClientCapabilities::tagSupport),
            make_field("labelSupport",                      &DocumentSymbolClientCapabilities::labelSupport)
        );
    };

    template<> struct Schema<DocumentSymbolClientCapabilities::_SymbolKind> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &DocumentSymbolClientCapabilities::_SymbolKind::valueSet)
        );
    };

    template<> struct Schema<DocumentSymbolClientCapabilities::_TagSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &DocumentSymbolClientCapabilities::_TagSupport::valueSet)
        );
    };

    /**
     * The Client Capabilities of a {@link CodeActionRequest}.
     */
    struct CodeActionClientCapabilities {
        /**
         * Whether code action supports dynamic registration.
         */
        Bool                        dynamicRegistration;
        /**
         * The client support code action literals of type `CodeAction` as a valid
         * response of the `textDocument/codeAction` request. If the property is not
         * set the request can only return `Command` literals.
         *
         * @since 3.8.0
         * @since 3.8.0
         */
        struct _CodeActionLiteralSupport {
            /**
             * The code action kind is support with the following value
             * set.
             */
            struct _CodeActionKind {
                /**
                 * The code action kind values the client supports. When this
                 * property exists the client also guarantees that it will
                 * handle values outside its set gracefully and falls back
                 * to a default value when unknown.
                 */
                Slice<CodeActionKind>  valueSet;
            }* codeActionKind;
        }* codeActionLiteralSupport;
        /**
         * Whether code action supports the `isPreferred` property.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Bool                        isPreferredSupport;
        /**
         * Whether code action supports the `disabled` property.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool                        disabledSupport;
        /**
         * Whether code action supports the `data` property which is
         * preserved between a `textDocument/codeAction` and a
         * `codeAction/resolve` request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool                        dataSupport;
        /**
         * Whether the client supports resolving additional code action
         * properties via a separate `codeAction/resolve` request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        struct _ResolveSupport {
            /**
             * The properties that a client can resolve lazily.
             */
            Slice<String>  properties;
        }* resolveSupport;
        /**
         * Whether the client honors the change annotations in
         * text edits and resource operations returned via the
         * `CodeAction#edit` property by for example presenting
         * the workspace edit in the user interface and asking
         * for confirmation.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool                        honorsChangeAnnotations;
    };

    template<> struct Schema<CodeActionClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration",      &CodeActionClientCapabilities::dynamicRegistration),
            make_field("codeActionLiteralSupport", &CodeActionClientCapabilities::codeActionLiteralSupport),
            make_field("isPreferredSupport",       &CodeActionClientCapabilities::isPreferredSupport),
            make_field("disabledSupport",          &CodeActionClientCapabilities::disabledSupport),
            make_field("dataSupport",              &CodeActionClientCapabilities::dataSupport),
            make_field("resolveSupport",           &CodeActionClientCapabilities::resolveSupport),
            make_field("honorsChangeAnnotations",  &CodeActionClientCapabilities::honorsChangeAnnotations)
        );
    };

    template<> struct Schema<CodeActionClientCapabilities::_CodeActionLiteralSupport::_CodeActionKind> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &CodeActionClientCapabilities::_CodeActionLiteralSupport::_CodeActionKind::valueSet)
        );
    };

    template<> struct Schema<CodeActionClientCapabilities::_CodeActionLiteralSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("codeActionKind", &CodeActionClientCapabilities::_CodeActionLiteralSupport::codeActionKind)
        );
    };

    template<> struct Schema<CodeActionClientCapabilities::_ResolveSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("properties", &CodeActionClientCapabilities::_ResolveSupport::properties)
        );
    };

    /**
     * The client capabilities  of a {@link CodeLensRequest}.
     */
    struct CodeLensClientCapabilities {
        /**
         * Whether code lens supports dynamic registration.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<CodeLensClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &CodeLensClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * The client capabilities of a {@link DocumentLinkRequest}.
     */
    struct DocumentLinkClientCapabilities {
        /**
         * Whether document link supports dynamic registration.
         */
        Bool  dynamicRegistration;
        /**
         * Whether the client supports the `tooltip` property on `DocumentLink`.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Bool  tooltipSupport;
    };

    template<> struct Schema<DocumentLinkClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &DocumentLinkClientCapabilities::dynamicRegistration),
            make_field("tooltipSupport",      &DocumentLinkClientCapabilities::tooltipSupport)
        );
    };

    struct DocumentColorClientCapabilities {
        /**
         * Whether implementation supports dynamic registration. If this is set to `true`
         * the client supports the new `DocumentColorRegistrationOptions` return value
         * for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<DocumentColorClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &DocumentColorClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * Client capabilities of a {@link DocumentFormattingRequest}.
     */
    struct DocumentFormattingClientCapabilities {
        /**
         * Whether formatting supports dynamic registration.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<DocumentFormattingClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &DocumentFormattingClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * Client capabilities of a {@link DocumentRangeFormattingRequest}.
     */
    struct DocumentRangeFormattingClientCapabilities {
        /**
         * Whether range formatting supports dynamic registration.
         */
        Bool  dynamicRegistration;
        /**
         * Whether the client supports formatting multiple ranges at once.
         *
         * @since 3.18.0
         * @proposed
         * @since 3.18.0
         */
        Bool  rangesSupport;
    };

    template<> struct Schema<DocumentRangeFormattingClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &DocumentRangeFormattingClientCapabilities::dynamicRegistration),
            make_field("rangesSupport",       &DocumentRangeFormattingClientCapabilities::rangesSupport)
        );
    };

    /**
     * Client capabilities of a {@link DocumentOnTypeFormattingRequest}.
     */
    struct DocumentOnTypeFormattingClientCapabilities {
        /**
         * Whether on type formatting supports dynamic registration.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<DocumentOnTypeFormattingClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &DocumentOnTypeFormattingClientCapabilities::dynamicRegistration)
        );
    };

    struct RenameClientCapabilities {
        /**
         * Whether rename supports dynamic registration.
         */
        Bool                           dynamicRegistration;
        /**
         * Client supports testing for validity of rename operations
         * before execution.
         *
         * @since 3.12.0
         * @since 3.12.0
         */
        Bool                           prepareSupport;
        /**
         * Client supports the default behavior result.
         *
         * The value indicates the default behavior used by the
         * client.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        PrepareSupportDefaultBehavior  prepareSupportDefaultBehavior;
        /**
         * Whether the client honors the change annotations in
         * text edits and resource operations returned via the
         * rename request's workspace edit by for example presenting
         * the workspace edit in the user interface and asking
         * for confirmation.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool                           honorsChangeAnnotations;
    };

    template<> struct Schema<RenameClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration",           &RenameClientCapabilities::dynamicRegistration),
            make_field("prepareSupport",                &RenameClientCapabilities::prepareSupport),
            make_field("prepareSupportDefaultBehavior", &RenameClientCapabilities::prepareSupportDefaultBehavior),
            make_field("honorsChangeAnnotations",       &RenameClientCapabilities::honorsChangeAnnotations)
        );
    };

    struct FoldingRangeClientCapabilities {
        /**
         * Whether implementation supports dynamic registration for folding range
         * providers. If this is set to `true` the client supports the new
         * `FoldingRangeRegistrationOptions` return value for the corresponding
         * server capability as well.
         */
        Bool                dynamicRegistration;
        /**
         * The maximum number of folding ranges that the client prefers to receive
         * per document. The value serves as a hint, servers are free to follow the
         * limit.
         */
        UInt                rangeLimit;
        /**
         * If set, the client signals that it only supports folding complete lines.
         * If set, client will ignore specified `startCharacter` and `endCharacter`
         * properties in a FoldingRange.
         */
        Bool                lineFoldingOnly;
        /**
         * Specific options for the folding range kind.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        struct _FoldingRangeKind {
            /**
             * The folding range kind values the client supports. When this
             * property exists the client also guarantees that it will
             * handle values outside its set gracefully and falls back
             * to a default value when unknown.
             */
            Slice<FoldingRangeKind>  valueSet;
        }* foldingRangeKind;
        /**
         * Specific options for the folding range.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        struct _FoldingRange {
            /**
             * If set, the client signals that it supports setting collapsedText on
             * folding ranges to display custom labels instead of the default text.
             *
             * @since 3.17.0
             * @since 3.17.0
             */
            Bool  collapsedText;
        }* foldingRange;
    };

    template<> struct Schema<FoldingRangeClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &FoldingRangeClientCapabilities::dynamicRegistration),
            make_field("rangeLimit",          &FoldingRangeClientCapabilities::rangeLimit),
            make_field("lineFoldingOnly",     &FoldingRangeClientCapabilities::lineFoldingOnly),
            make_field("foldingRangeKind",    &FoldingRangeClientCapabilities::foldingRangeKind),
            make_field("foldingRange",        &FoldingRangeClientCapabilities::foldingRange)
        );
    };

    template<> struct Schema<FoldingRangeClientCapabilities::_FoldingRangeKind> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &FoldingRangeClientCapabilities::_FoldingRangeKind::valueSet)
        );
    };

    template<> struct Schema<FoldingRangeClientCapabilities::_FoldingRange> {
        static constexpr auto fields = std::make_tuple(
            make_field("collapsedText", &FoldingRangeClientCapabilities::_FoldingRange::collapsedText)
        );
    };

    struct SelectionRangeClientCapabilities {
        /**
         * Whether implementation supports dynamic registration for selection range providers. If this is set to `true`
         * the client supports the new `SelectionRangeRegistrationOptions` return value for the corresponding server
         * capability as well.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<SelectionRangeClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &SelectionRangeClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * The publish diagnostic client capabilities.
     */
    struct PublishDiagnosticsClientCapabilities {
        /**
         * Whether the clients accepts diagnostics with related information.
         */
        Bool          relatedInformation;
        /**
         * Client supports the tag property to provide meta data about a diagnostic.
         * Clients supporting tags have to handle unknown tags gracefully.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        struct _TagSupport {
            /**
             * The tags supported by the client.
             */
            Slice<DiagnosticTag>  valueSet;
        }* tagSupport;
        /**
         * Whether the client interprets the version property of the
         * `textDocument/publishDiagnostics` notification's parameter.
         *
         * @since 3.15.0
         * @since 3.15.0
         */
        Bool          versionSupport;
        /**
         * Client supports a codeDescription property
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool          codeDescriptionSupport;
        /**
         * Whether code action supports the `data` property which is
         * preserved between a `textDocument/publishDiagnostics` and
         * `textDocument/codeAction` request.
         *
         * @since 3.16.0
         * @since 3.16.0
         */
        Bool          dataSupport;
    };

    template<> struct Schema<PublishDiagnosticsClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("relatedInformation",     &PublishDiagnosticsClientCapabilities::relatedInformation),
            make_field("tagSupport",             &PublishDiagnosticsClientCapabilities::tagSupport),
            make_field("versionSupport",         &PublishDiagnosticsClientCapabilities::versionSupport),
            make_field("codeDescriptionSupport", &PublishDiagnosticsClientCapabilities::codeDescriptionSupport),
            make_field("dataSupport",            &PublishDiagnosticsClientCapabilities::dataSupport)
        );
    };

    template<> struct Schema<PublishDiagnosticsClientCapabilities::_TagSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("valueSet", &PublishDiagnosticsClientCapabilities::_TagSupport::valueSet)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct CallHierarchyClientCapabilities {
        /**
         * Whether implementation supports dynamic registration. If this is set to `true`
         * the client supports the new `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
         * return value for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<CallHierarchyClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &CallHierarchyClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * @since 3.16.0
     * @since 3.16.0
     */
    struct SemanticTokensClientCapabilities {
        /**
         * Whether implementation supports dynamic registration. If this is set to `true`
         * the client supports the new `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
         * return value for the corresponding server capability as well.
         */
        Bool                dynamicRegistration;
        /**
         * Which requests the client supports and might send to the server
         * depending on the server's capability. Please note that clients might not
         * show semantic tokens or degrade some of the user experience if a range
         * or full request is advertised by the client but not provided by the
         * server. If for example the client capability `requests.full` and
         * `request.range` are both set to true but the server only provides a
         * range provider the client might not render a minimap correctly or might
         * even decide to not show any semantic tokens at all.
         */
        struct _Requests {
            /**
             * The client will send the `textDocument/semanticTokens/range` request if
             * the server provides a corresponding handler.
             */
            Bool  range;
            /**
             * The client will send the `textDocument/semanticTokens/full` request if
             * the server provides a corresponding handler.
             */
            Bool  full;
        }* requests;
        /**
         * The token types that the client supports.
         */
        Slice<String>       tokenTypes;
        /**
         * The token modifiers that the client supports.
         */
        Slice<String>       tokenModifiers;
        /**
         * The token formats the clients supports.
         */
        Slice<TokenFormat>  formats;
        /**
         * Whether the client supports tokens that can overlap each other.
         */
        Bool                overlappingTokenSupport;
        /**
         * Whether the client supports tokens that can span multiple lines.
         */
        Bool                multilineTokenSupport;
        /**
         * Whether the client allows the server to actively cancel a
         * semantic token request, e.g. supports returning
         * LSPErrorCodes.ServerCancelled. If a server does the client
         * needs to retrigger the request.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        Bool                serverCancelSupport;
        /**
         * Whether the client uses semantic tokens to augment existing
         * syntax tokens. If set to `true` client side created syntax
         * tokens and semantic tokens are both used for colorization. If
         * set to `false` the client only uses the returned semantic tokens
         * for colorization.
         *
         * If the value is `undefined` then the client behavior is not
         * specified.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        Bool                augmentsSyntaxTokens;
    };

    template<> struct Schema<SemanticTokensClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration",     &SemanticTokensClientCapabilities::dynamicRegistration),
            make_field("requests",                &SemanticTokensClientCapabilities::requests),
            make_field("tokenTypes",              &SemanticTokensClientCapabilities::tokenTypes),
            make_field("tokenModifiers",          &SemanticTokensClientCapabilities::tokenModifiers),
            make_field("formats",                 &SemanticTokensClientCapabilities::formats),
            make_field("overlappingTokenSupport", &SemanticTokensClientCapabilities::overlappingTokenSupport),
            make_field("multilineTokenSupport",   &SemanticTokensClientCapabilities::multilineTokenSupport),
            make_field("serverCancelSupport",     &SemanticTokensClientCapabilities::serverCancelSupport),
            make_field("augmentsSyntaxTokens",    &SemanticTokensClientCapabilities::augmentsSyntaxTokens)
        );
    };

    template<> struct Schema<SemanticTokensClientCapabilities::_Requests> {
        static constexpr auto fields = std::make_tuple(
            make_field("range", &SemanticTokensClientCapabilities::_Requests::range),
            make_field("full",  &SemanticTokensClientCapabilities::_Requests::full)
        );
    };

    /**
     * Client capabilities for the linked editing range request.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct LinkedEditingRangeClientCapabilities {
        /**
         * Whether implementation supports dynamic registration. If this is set to `true`
         * the client supports the new `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
         * return value for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<LinkedEditingRangeClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &LinkedEditingRangeClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * Client capabilities specific to the moniker request.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct MonikerClientCapabilities {
        /**
         * Whether moniker supports dynamic registration. If this is set to `true`
         * the client supports the new `MonikerRegistrationOptions` return value
         * for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<MonikerClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &MonikerClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * @since 3.17.0
     * @since 3.17.0
     */
    struct TypeHierarchyClientCapabilities {
        /**
         * Whether implementation supports dynamic registration. If this is set to `true`
         * the client supports the new `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
         * return value for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<TypeHierarchyClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &TypeHierarchyClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * Client capabilities specific to inline values.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValueClientCapabilities {
        /**
         * Whether implementation supports dynamic registration for inline value providers.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<InlineValueClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &InlineValueClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * Inlay hint client capabilities.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlayHintClientCapabilities {
        /**
         * Whether inlay hints support dynamic registration.
         */
        Bool              dynamicRegistration;
        /**
         * Indicates which properties a client can resolve lazily on an inlay
         * hint.
         */
        struct _ResolveSupport {
            /**
             * The properties that a client can resolve lazily.
             */
            Slice<String>  properties;
        }* resolveSupport;
    };

    template<> struct Schema<InlayHintClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &InlayHintClientCapabilities::dynamicRegistration),
            make_field("resolveSupport",      &InlayHintClientCapabilities::resolveSupport)
        );
    };

    template<> struct Schema<InlayHintClientCapabilities::_ResolveSupport> {
        static constexpr auto fields = std::make_tuple(
            make_field("properties", &InlayHintClientCapabilities::_ResolveSupport::properties)
        );
    };

    /**
     * Client capabilities specific to diagnostic pull requests.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DiagnosticClientCapabilities {
        /**
         * Whether implementation supports dynamic registration. If this is set to `true`
         * the client supports the new `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
         * return value for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
        /**
         * Whether the clients supports related documents for document diagnostic pulls.
         */
        Bool  relatedDocumentSupport;
    };

    template<> struct Schema<DiagnosticClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration",    &DiagnosticClientCapabilities::dynamicRegistration),
            make_field("relatedDocumentSupport", &DiagnosticClientCapabilities::relatedDocumentSupport)
        );
    };

    /**
     * Client capabilities specific to inline completions.
     *
     * @since 3.18.0
     * @proposed
     * @since 3.18.0
     */
    struct InlineCompletionClientCapabilities {
        /**
         * Whether implementation supports dynamic registration for inline completion providers.
         */
        Bool  dynamicRegistration;
    };

    template<> struct Schema<InlineCompletionClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration", &InlineCompletionClientCapabilities::dynamicRegistration)
        );
    };

    /**
     * Notebook specific client capabilities.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookDocumentSyncClientCapabilities {
        /**
         * Whether implementation supports dynamic registration. If this is
         * set to `true` the client supports the new
         * `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
         * return value for the corresponding server capability as well.
         */
        Bool  dynamicRegistration;
        /**
         * The client supports sending execution summary data per cell.
         */
        Bool  executionSummarySupport;
    };

    template<> struct Schema<NotebookDocumentSyncClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("dynamicRegistration",     &NotebookDocumentSyncClientCapabilities::dynamicRegistration),
            make_field("executionSummarySupport", &NotebookDocumentSyncClientCapabilities::executionSummarySupport)
        );
    };

    /**
     * Show message request client capabilities
     */
    struct ShowMessageRequestClientCapabilities {
        /**
         * Capabilities specific to the `MessageActionItem` type.
         */
        struct _MessageActionItem {
            /**
             * Whether the client supports additional attributes which
             * are preserved and send back to the server in the
             * request's response.
             */
            Bool  additionalPropertiesSupport;
        }* messageActionItem;
    };

    template<> struct Schema<ShowMessageRequestClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("messageActionItem", &ShowMessageRequestClientCapabilities::messageActionItem)
        );
    };

    template<> struct Schema<ShowMessageRequestClientCapabilities::_MessageActionItem> {
        static constexpr auto fields = std::make_tuple(
            make_field("additionalPropertiesSupport", &ShowMessageRequestClientCapabilities::_MessageActionItem::additionalPropertiesSupport)
        );
    };

    /**
     * Client capabilities for the showDocument request.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct ShowDocumentClientCapabilities {
        /**
         * The client has support for the showDocument
         * request.
         */
        Bool  support;
    };

    template<> struct Schema<ShowDocumentClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("support", &ShowDocumentClientCapabilities::support)
        );
    };

    /**
     * Client capabilities specific to regular expressions.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct RegularExpressionsClientCapabilities {
        /**
         * The engine's name.
         */
        String  engine;
        /**
         * The engine's version.
         */
        String  version;
    };

    template<> struct Schema<RegularExpressionsClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("engine",  &RegularExpressionsClientCapabilities::engine),
            make_field("version", &RegularExpressionsClientCapabilities::version)
        );
    };

    /**
     * Client capabilities specific to the used markdown parser.
     *
     * @since 3.16.0
     * @since 3.16.0
     */
    struct MarkdownClientCapabilities {
        /**
         * The name of the parser.
         */
        String         parser;
        /**
         * The version of the parser.
         */
        String         version;
        /**
         * A list of HTML tags that the client allows / supports in
         * Markdown.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        Slice<String>  allowedTags;
    };

    template<> struct Schema<MarkdownClientCapabilities> {
        static constexpr auto fields = std::make_tuple(
            make_field("parser",      &MarkdownClientCapabilities::parser),
            make_field("version",     &MarkdownClientCapabilities::version),
            make_field("allowedTags", &MarkdownClientCapabilities::allowedTags)
        );
    };

    /**
     * The definition of a symbol represented as one or many {@link Location locations}.
     * For most programming languages there is only one location at which a symbol is
     * defined.
     *
     * Servers should prefer returning `DefinitionLink` over `Definition` if supported
     * by the client.
     */
    struct Definition {
        String  uri;
        Range*  range;
    };

    template<> struct Schema<Definition> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",   &Definition::uri),
            make_field("range", &Definition::range)
        );
    };

    /**
     * The declaration of a symbol representation as one or many {@link Location locations}.
     */
    struct Declaration {
        String  uri;
        Range*  range;
    };

    template<> struct Schema<Declaration> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",   &Declaration::uri),
            make_field("range", &Declaration::range)
        );
    };

    /**
     * Inline value information can be provided by different means:
     * - directly as a text value (class InlineValueText).
     * - as a name to use for a variable lookup (class InlineValueVariableLookup)
     * - as an evaluatable expression (class InlineValueEvaluatableExpression)
     * The InlineValue types combines all inline value types into one type.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct InlineValue {
        /**
         * The document range for which the inline value applies.
         */
        Range*  range;
        /**
         * The text of the inline value.
         */
        String  text;
        /**
         * If specified the name of the variable to look up.
         */
        String  variableName;
        /**
         * How to perform the lookup.
         */
        Bool    caseSensitiveLookup;
        /**
         * If specified the expression overrides the extracted expression.
         */
        String  expression;
    };

    template<> struct Schema<InlineValue> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",               &InlineValue::range),
            make_field("text",                &InlineValue::text),
            make_field("variableName",        &InlineValue::variableName),
            make_field("caseSensitiveLookup", &InlineValue::caseSensitiveLookup),
            make_field("expression",          &InlineValue::expression)
        );
    };

    /**
     * The result of a document diagnostic pull request. A report can
     * either be a full report containing all diagnostics for the
     * requested document or an unchanged report indicating that nothing
     * has changed in terms of diagnostics in comparison to the last
     * pull request.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct DocumentDiagnosticReport {
        /**
         * Diagnostics of related documents. This information is useful
         * in programming languages where code in a file A can generate
         * diagnostics in a file B which A depends on. An example of
         * such a language is C/C++ where marco definitions in a file
         * a.cpp and result in errors in a header file b.hpp.
         *
         * @since 3.17.0
         * @since 3.17.0
         */
        LSPAny  relatedDocuments;
    };

    template<> struct Schema<DocumentDiagnosticReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("relatedDocuments", &DocumentDiagnosticReport::relatedDocuments)
        );
    };

    struct PrepareRenameResult {
        /**
         * The range's start position.
         */
        Position*  start;
        /**
         * The range's end position.
         */
        Position*  end;
        Range*     range;
        String     placeholder;
        Bool       defaultBehavior;
    };

    template<> struct Schema<PrepareRenameResult> {
        static constexpr auto fields = std::make_tuple(
            make_field("start",           &PrepareRenameResult::start),
            make_field("end",             &PrepareRenameResult::end),
            make_field("range",           &PrepareRenameResult::range),
            make_field("placeholder",     &PrepareRenameResult::placeholder),
            make_field("defaultBehavior", &PrepareRenameResult::defaultBehavior)
        );
    };

    /**
     * A workspace diagnostic document report.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct WorkspaceDocumentDiagnosticReport {
        /**
         * The URI for which diagnostic information is reported.
         */
        String  uri;
        /**
         * The version number for which the diagnostics are reported.
         * If the document is not marked as open `null` can be provided.
         */
        Int     version;
    };

    template<> struct Schema<WorkspaceDocumentDiagnosticReport> {
        static constexpr auto fields = std::make_tuple(
            make_field("uri",     &WorkspaceDocumentDiagnosticReport::uri),
            make_field("version", &WorkspaceDocumentDiagnosticReport::version)
        );
    };

    /**
     * An event describing a change to a text document. If only a text is provided
     * it is considered to be the full content of the document.
     */
    struct TextDocumentContentChangeEvent {
        /**
         * The range of the document that changed.
         */
        Range*  range;
        /**
         * The optional length of the range that got replaced.
         *
         * @deprecated use range instead.
         */
        UInt    rangeLength;
        /**
         * The new text for the provided range.
         */
        String  text;
    };

    template<> struct Schema<TextDocumentContentChangeEvent> {
        static constexpr auto fields = std::make_tuple(
            make_field("range",       &TextDocumentContentChangeEvent::range),
            make_field("rangeLength", &TextDocumentContentChangeEvent::rangeLength),
            make_field("text",        &TextDocumentContentChangeEvent::text)
        );
    };

    /**
     * MarkedString can be used to render human readable text. It is either a markdown string
     * or a code-block that provides a language and a code snippet. The language identifier
     * is semantically equal to the optional language identifier in fenced code blocks in GitHub
     * issues. See https://help.github.com/articles/creating-and-highlighting-code-blocks/#syntax-highlighting
     *
     * The pair of a language and a value is an equivalent to markdown:
     * ```${language}
     * ${value}
     * ```
     *
     * Note that markdown strings will be sanitized - that means html will be escaped.
     * @deprecated use MarkupContent instead.
     */
    struct MarkedString {
        String  language;
        String  value;
    };

    template<> struct Schema<MarkedString> {
        static constexpr auto fields = std::make_tuple(
            make_field("language", &MarkedString::language),
            make_field("value",    &MarkedString::value)
        );
    };

    /**
     * A document filter describes a top level text document or
     * a notebook cell document.
     *
     * @since 3.17.0 - proposed support for NotebookCellTextDocumentFilter.
     * @since 3.17.0 - proposed support for NotebookCellTextDocumentFilter.
     */
    struct DocumentFilter {
        /**
         * A language id, like `typescript`.
         */
        String  language;
        /**
         * A Uri {@link Uri.scheme scheme}, like `file` or `untitled`.
         */
        String  scheme;
        /**
         * A glob pattern, like **​/*.{ts,js}. See TextDocumentFilter for examples.
         */
        String  pattern;
        /**
         * A filter that matches against the notebook
         * containing the notebook cell. If a string
         * value is provided it matches against the
         * notebook type. '*' matches every notebook.
         */
        String  notebook;
    };

    template<> struct Schema<DocumentFilter> {
        static constexpr auto fields = std::make_tuple(
            make_field("language", &DocumentFilter::language),
            make_field("scheme",   &DocumentFilter::scheme),
            make_field("pattern",  &DocumentFilter::pattern),
            make_field("notebook", &DocumentFilter::notebook)
        );
    };

    /**
     * The glob pattern. Either a string pattern or a relative pattern.
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct GlobPattern {
        /**
         * A workspace folder or a base URI to which this pattern will be matched
         * against relatively.
         */
        WorkspaceFolder*  baseUri;
        /**
         * The actual glob pattern;
         */
        String            pattern;
    };

    template<> struct Schema<GlobPattern> {
        static constexpr auto fields = std::make_tuple(
            make_field("baseUri", &GlobPattern::baseUri),
            make_field("pattern", &GlobPattern::pattern)
        );
    };

    /**
     * A document filter denotes a document by different properties like
     * the {@link TextDocument.languageId language}, the {@link Uri.scheme scheme} of
     * its resource, or a glob-pattern that is applied to the {@link TextDocument.fileName path}.
     *
     * Glob patterns can have the following syntax:
     * - `*` to match zero or more characters in a path segment
     * - `?` to match on one character in a path segment
     * - `**` to match any number of path segments, including none
     * - `{}` to group sub patterns into an OR expression. (e.g. `**​/*.{ts,js}` matches all TypeScript and JavaScript files)
     * - `[]` to declare a range of characters to match in a path segment (e.g., `example.[0-9]` to match on `example.0`, `example.1`, …)
     * - `[!...]` to negate a range of characters to match in a path segment (e.g., `example.[!0-9]` to match on `example.a`, `example.b`, but not `example.0`)
     *
     * @sample A language filter that applies to typescript files on disk: `{ language: 'typescript', scheme: 'file' }`
     * @sample A language filter that applies to all package.json paths: `{ language: 'json', pattern: '**package.json' }`
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct TextDocumentFilter {
        /**
         * A language id, like `typescript`.
         */
        String  language;
        /**
         * A Uri {@link Uri.scheme scheme}, like `file` or `untitled`.
         */
        String  scheme;
        /**
         * A glob pattern, like **​/*.{ts,js}. See TextDocumentFilter for examples.
         */
        String  pattern;
    };

    template<> struct Schema<TextDocumentFilter> {
        static constexpr auto fields = std::make_tuple(
            make_field("language", &TextDocumentFilter::language),
            make_field("scheme",   &TextDocumentFilter::scheme),
            make_field("pattern",  &TextDocumentFilter::pattern)
        );
    };

    /**
     * A notebook document filter denotes a notebook document by
     * different properties. The properties will be match
     * against the notebook's URI (same as with documents)
     *
     * @since 3.17.0
     * @since 3.17.0
     */
    struct NotebookDocumentFilter {
        /**
         * The type of the enclosing notebook.
         */
        String  notebookType;
        /**
         * A Uri {@link Uri.scheme scheme}, like `file` or `untitled`.
         */
        String  scheme;
        /**
         * A glob pattern.
         */
        String  pattern;
    };

    template<> struct Schema<NotebookDocumentFilter> {
        static constexpr auto fields = std::make_tuple(
            make_field("notebookType", &NotebookDocumentFilter::notebookType),
            make_field("scheme",       &NotebookDocumentFilter::scheme),
            make_field("pattern",      &NotebookDocumentFilter::pattern)
        );
    };

}

namespace Lsp {

    typedef Lsp::T::InitializeParams               Initialize;
    typedef Lsp::T::InitializedParams              Initialized;

    namespace TextDocument {
        typedef Lsp::T::ImplementationParams           Implementation;
        typedef Lsp::T::TypeDefinitionParams           TypeDefinition;
        typedef Lsp::T::DocumentColorParams            DocumentColor;
        typedef Lsp::T::ColorPresentationParams        ColorPresentation;
        typedef Lsp::T::FoldingRangeParams             FoldingRange;
        typedef Lsp::T::DeclarationParams              Declaration;
        typedef Lsp::T::SelectionRangeParams           SelectionRange;
        typedef Lsp::T::CallHierarchyPrepareParams     PrepareCallHierarchy;
        typedef Lsp::T::SemanticTokensParams           SemanticTokensfull;
        typedef Lsp::T::SemanticTokensDeltaParams      SemanticTokensfulldelta;
        typedef Lsp::T::SemanticTokensRangeParams      SemanticTokensrange;
        typedef Lsp::T::LinkedEditingRangeParams       LinkedEditingRange;
        typedef Lsp::T::MonikerParams                  Moniker;
        typedef Lsp::T::TypeHierarchyPrepareParams     PrepareTypeHierarchy;
        typedef Lsp::T::InlineValueParams              InlineValue;
        typedef Lsp::T::InlayHintParams                InlayHint;
        typedef Lsp::T::DocumentDiagnosticParams       Diagnostic;
        typedef Lsp::T::InlineCompletionParams         InlineCompletion;
        typedef Lsp::T::WillSaveTextDocumentParams     WillSaveWaitUntil;
        typedef Lsp::T::CompletionParams               Completion;
        typedef Lsp::T::HoverParams                    Hover;
        typedef Lsp::T::SignatureHelpParams            SignatureHelp;
        typedef Lsp::T::DefinitionParams               Definition;
        typedef Lsp::T::ReferenceParams                References;
        typedef Lsp::T::DocumentHighlightParams        DocumentHighlight;
        typedef Lsp::T::DocumentSymbolParams           DocumentSymbol;
        typedef Lsp::T::CodeActionParams               CodeAction;
        typedef Lsp::T::CodeLensParams                 CodeLens;
        typedef Lsp::T::DocumentLinkParams             DocumentLink;
        typedef Lsp::T::DocumentFormattingParams       Formatting;
        typedef Lsp::T::DocumentRangeFormattingParams  RangeFormatting;
        typedef Lsp::T::DocumentRangesFormattingParams RangesFormatting;
        typedef Lsp::T::DocumentOnTypeFormattingParams OnTypeFormatting;
        typedef Lsp::T::RenameParams                   Rename;
        typedef Lsp::T::PrepareRenameParams            PrepareRename;
        typedef Lsp::T::DidOpenTextDocumentParams      DidOpen;
        typedef Lsp::T::DidChangeTextDocumentParams    DidChange;
        typedef Lsp::T::DidCloseTextDocumentParams     DidClose;
        typedef Lsp::T::DidSaveTextDocumentParams      DidSave;
        typedef Lsp::T::WillSaveTextDocumentParams     WillSave;
        typedef Lsp::T::PublishDiagnosticsParams       PublishDiagnostics;
    }

    namespace Workspace {
        typedef Lsp::T::ConfigurationParams            Configuration;
        typedef Lsp::T::CreateFilesParams              WillCreateFiles;
        typedef Lsp::T::RenameFilesParams              WillRenameFiles;
        typedef Lsp::T::DeleteFilesParams              WillDeleteFiles;
        typedef Lsp::T::WorkspaceDiagnosticParams      Diagnostic;
        typedef Lsp::T::WorkspaceSymbolParams          Symbol;
        typedef Lsp::T::ExecuteCommandParams           ExecuteCommand;
        typedef Lsp::T::ApplyWorkspaceEditParams       ApplyEdit;
        typedef Lsp::T::DidChangeWorkspaceFoldersParams DidChangeWorkspaceFolders;
        typedef Lsp::T::CreateFilesParams              DidCreateFiles;
        typedef Lsp::T::RenameFilesParams              DidRenameFiles;
        typedef Lsp::T::DeleteFilesParams              DidDeleteFiles;
        typedef Lsp::T::DidChangeConfigurationParams   DidChangeConfiguration;
        typedef Lsp::T::DidChangeWatchedFilesParams    DidChangeWatchedFiles;
    }

    namespace Window {
        typedef Lsp::T::WorkDoneProgressCreateParams   WorkDoneProgresscreate;
        typedef Lsp::T::ShowDocumentParams             ShowDocument;
        typedef Lsp::T::ShowMessageRequestParams       ShowMessageRequest;
        typedef Lsp::T::WorkDoneProgressCancelParams   WorkDoneProgresscancel;
        typedef Lsp::T::ShowMessageParams              ShowMessage;
        typedef Lsp::T::LogMessageParams               LogMessage;
    }

    namespace CallHierarchy {
        typedef Lsp::T::CallHierarchyIncomingCallsParams IncomingCalls;
        typedef Lsp::T::CallHierarchyOutgoingCallsParams OutgoingCalls;
    }

    namespace TypeHierarchy {
        typedef Lsp::T::TypeHierarchySupertypesParams  Supertypes;
        typedef Lsp::T::TypeHierarchySubtypesParams    Subtypes;
    }

    namespace InlayHint {
        typedef Lsp::T::InlayHint                      Resolve;
    }

    namespace Client {
        typedef Lsp::T::RegistrationParams             RegisterCapability;
        typedef Lsp::T::UnregistrationParams           UnregisterCapability;
    }

    namespace CompletionItem {
        typedef Lsp::T::CompletionItem                 Resolve;
    }

    namespace CodeAction {
        typedef Lsp::T::CodeAction                     Resolve;
    }

    namespace WorkspaceSymbol {
        typedef Lsp::T::WorkspaceSymbol                Resolve;
    }

    namespace CodeLens {
        typedef Lsp::T::CodeLens                       Resolve;
    }

    namespace DocumentLink {
        typedef Lsp::T::DocumentLink                   Resolve;
    }

    namespace NotebookDocument {
        typedef Lsp::T::DidOpenNotebookDocumentParams  DidOpen;
        typedef Lsp::T::DidChangeNotebookDocumentParams DidChange;
        typedef Lsp::T::DidSaveNotebookDocumentParams  DidSave;
        typedef Lsp::T::DidCloseNotebookDocumentParams DidClose;
    }

    namespace Telemetry {
        typedef Lsp::T::LSPAny                         Event;
    }

    namespace Root {
        typedef Lsp::T::SetTraceParams                 SetTrace;
        typedef Lsp::T::LogTraceParams                 LogTrace;
        typedef Lsp::T::CancelParams                   CancelRequest;
        typedef Lsp::T::ProgressParams                 Progress;
    }

}

// ===
// END OF AUTO-GENERATED LSP PROTOCOL








// Logger
// ===

namespace Lsp::Err {

    enum Kind {
        OK = 0,
        ALLOC,
        INVALID_PARAMS,
        LOAD_FILE,
        RESOLVE_FILE,
        SYNC,
    };

    struct Info {
        Kind   kind;
        String file = { 0 };
    };

    inline const char* str(Kind err) {
        switch (err) {
        case ALLOC:
            return "Malloc! Malloc! Maaaaalloc!";
        case INVALID_PARAMS:
            return "The request parameters are invalid or missing required fields.";
        case LOAD_FILE:
            return "Was unable to load requested file!";
        case RESOLVE_FILE:
            return "Was unable to resolve given file!";
        case SYNC:
            return "Desynchronized. Ezio haven't handled it this way.";
        default:
            return "No error";
        }
    }

}

namespace Lsp::Inf {

    enum Kind {
        INFO  = 0,
        DEBUG
    };

    struct Info {
        Kind   kind;
        String file = { 0 };
    };

}

namespace Lsp {
    Inf::Kind report(Inf::Info inf, const char* format, ...);
    Err::Kind report(Err::Info err, ...);

    void panic(Err::Info err);
}



// Implementation
// ===

namespace Lsp {

    // TODO: allocations are awkward
    struct Allocator {
        void* (*alloc) (void* context, size_t size);
        void* context;
    };

    // Hierarchical
    enum Permission : uint8_t {
        P_NONE     = 1 << 0,
        P_PARSE    = 1 << 1,
        P_VALIDATE = 1 << 2,
        P_COMPILE_TIME_EXECUTION = 1 << 3,
        P_FOREIGN_CODE_EXECUTION = 1 << 4
    };

    namespace State {
        inline bool   initialized = false;
        inline String rootUri;
        inline void*  clientCapabilities;

        // Allocator to build the request and following response.
        inline Allocator allocator;

        // Tracking the client
        inline String clientName;
        inline String clientVersion;

        inline CommProvider::Info* comm;
        inline uint32_t compilerThreadCount;

        inline Permission permission;
    }

    struct AstSnapshot {
        uint64_t version;
        Reg::Unit* unit;
    };

    typedef uint32_t LineOffset;

    struct SemanticToken {
        uint32_t ln;
        uint32_t ch;
        uint32_t len;
        uint16_t type;
        uint16_t mod; // as bit flags
    };

    struct FileString : String {
        size_t capacity;
    };

    struct FileData {
        FileString    data;
        uint32_t      version;

        Array<LineOffset>    lineOffsets;
        Array<SemanticToken> semanticTokens;
        Array<SemanticToken> semanticTokensOld;

        Reg::Unit        unit[2];
        Arena::Container arenas[2];
        std::atomic<int> committedIdx { 0 };
        // Number of active queries reading each arena
        std::atomic<int> readerCount[2] { 0, 0 };
    };



    void init();
    void release();
    void setAndClearCompilerAllocator(Arena::Container* arena);
    void updateSemanticTokens(FileData* data);



    // TODO : move somewhere
    static bool match(String str, const char* lit) {
        return (str.len == (int) strlen(lit) && strncmp(str.buff, lit, str.len) == 0);
    }

    static T::RequestMethod toRequestMethod(JsonString val) {
        T::RequestMethod method = T::toRequestMethod(val);
        if (match({ val.data, val.len }, T::toString(method))) return method;
        return T::RM_NONE;
    }


    template <typename T>
    static T* alloc(Allocator* alc, size_t count = 1) {
        return (T*) alc->alloc(alc->context, sizeof(T) * count);
    }

    inline thread_local DArray::Container stack;

    typedef uint32_t StackMark;

    inline StackMark markStack(DArray::Container* stack) {
        return stack->size;
    }

    template <typename T>
    static T* commitStack(Allocator* alc, StackMark mark, size_t* len) {
        T* ptr = alloc<T>(alc, stack.size);
        memcpy(ptr, stack.buffer, sizeof(T) * stack.size);
        *len = stack.size;

        stack.size = mark;

        return ptr;
    }



    template <typename U>
    using Slice = T::Slice<U>;

    // Forward declaration
    template<typename Method>
    void str(JsonWriter* js, const Method* obj);

    // Helper trait to detect Slice<T>
    template<typename T> struct is_slice : std::false_type {};
    template<typename T> struct is_slice<Slice<T>> : std::true_type { using type = T; };

    // Helper trait to detect if enum is 'string' enum
    template<typename T, typename = void>
    struct has_to_string : std::false_type {};

    template<typename T>
    struct has_to_string<T, std::void_t<decltype(toString(std::declval<T>()))>> : std::true_type {};

    // Helper trait to detect Schema
    template <typename T, typename = void>
    struct is_schema : std::false_type {};

    template <typename ST>
    struct is_schema<ST, std::void_t<decltype(T::Schema<ST>::fields)>> : std::true_type {};

    // Helper trait to detect UnionSchema
    template <typename UT, typename = void>
    struct is_union_schema : std::false_type {};

    template <typename UT>
    struct is_union_schema<UT, std::void_t<decltype(T::UnionSchema<UT>::variants)>> : std::true_type {};

    template <typename FieldType>
    constexpr bool matchesJsonToken(JsonType token) {
        using Clean = std::decay_t<FieldType>;

        if constexpr (std::is_same_v<Clean, T::Bool>) {
            return token == JSON_BOOL;
        } else if constexpr (std::is_same_v<Clean, T::Int> || std::is_same_v<Clean, T::UInt>) {
            return token == JSON_NUMBER;
        } else if constexpr (std::is_same_v<Clean, String>) {
            return token == JSON_STRING;
        } else if constexpr (is_schema<Clean>::value || std::is_pointer_v<Clean>) {
            return token == JSON_OBJECT_OPEN;
        } else if constexpr (is_slice<Clean>::value) {
            return token == JSON_ARRAY_OPEN;
        }
        return false;
    }

    template<typename VType>
    void parseElement(JsonLex* js, Allocator* alc, VType& val) {
        using MType = std::decay_t<VType>;

        if constexpr (std::is_same_v<MType, T::Int>) {
            jsonNext(js);
            val = (T::Int) js->value.n;
        }
        else if constexpr (std::is_same_v<MType, T::UInt>) {
            jsonNext(js);
            val = (T::UInt) js->value.n;
        }
        else if constexpr (std::is_same_v<MType, T::Bool>) {
            jsonNext(js);
            val = js->value.b;
        }
        else if constexpr (std::is_same_v<MType, String>) {
            jsonNext(js);
            val = { js->value.s.data, js->value.s.len };
        }
        else if constexpr (std::is_enum_v<MType>) {
            jsonNext(js);
            val = (MType) js->value.n;
        }
        else if constexpr (std::is_pointer_v<MType>) {
            using Inner = std::remove_pointer_t<MType>;
            if constexpr (!std::is_void_v<Inner>) {
                if (jsonNext(js) == JSON_OBJECT_OPEN) {
                    val = parse<Inner>(js, alc);
                }
            }
        }
        else if constexpr (is_union_schema<MType>::value) {
            JsonType token = jsonPeek(js);
            bool matched = false;

            std::apply([&](auto&&... variants) {
                (([&] {
                    if (matched) return;
                    using TargetType = std::decay_t<decltype(val.*(variants.ptr))>;

                    if (matchesJsonToken<TargetType>(token)) {
                        val.*(T::UnionSchema<MType>::tag) = variants.kind;
                        parseElement(js, alc, val.*(variants.ptr));
                        matched = true;
                    }
                })(), ...);
            }, T::UnionSchema<MType>::variants);

            if (!matched) jsonSkipValue(js, jsonNext(js));
        }
        else if constexpr (is_schema<MType>::value) {
            if (jsonNext(js) == JSON_OBJECT_OPEN) {
                val = *parse<MType>(js, alc);
            }
        }
        else if constexpr (is_slice<MType>::value) {
            if (jsonNext(js) == JSON_ARRAY_OPEN) {
                StackMark mark = markStack(&stack);
                using ItemType = typename is_slice<MType>::type;

                while (true) {
                    JsonType arrToken = jsonNext(js);
                    if (arrToken == JSON_ARRAY_CLOSE) break;

                    if constexpr (std::is_pointer_v<ItemType>) {
                        if (arrToken == JSON_OBJECT_OPEN) {
                            using Inner = std::remove_pointer_t<ItemType>;
                            void* ptr = (void*) parse<Inner>(js, alc);
                            DArray::push(&stack, &ptr);
                        }
                    } else {
                        ItemType item = {};
                        parseElement(js, alc, item);
                        DArray::push(&stack, (void*) &item);
                    }
                }
                val.data = (ItemType*) commitStack<ItemType>(alc, mark, &val.size);
            }
        }
    }

    // Note: Opening '{' assumed consumed by dispatcher or parent
    template<typename Method>
    Method* parse(JsonLex* js, Allocator* alc) {
        auto* obj = alloc<Method>(alc);

        while (true) {
            JsonType token = jsonNext(js);
            if (token == JSON_OBJECT_CLOSE) break;
            if (token != JSON_KEY) break;
            JsonString key = js->value.s;

            bool found = false;
            std::apply([&](auto&&... fields) {(([&] {
                if (!found && match(key, fields.name)) {
                    found = true;
                    auto& val = obj->*(fields.ptr);
                    parseElement(js, alc, val);
                }
                })(), ...);
            }, T::Schema<Method>::fields);

            if (!found) jsonSkipValue(js, jsonNext(js));
            if (js->errCode != 0) break;
        }
        return obj;
    }

    static inline JsonString toJS(const char* str) {
        const size_t len = strlen(str);
        return { .data = (char*) str, .len = len };
    }

    static inline JsonString toJS(String str) {
        return { .data = str.buff, .len = str.len };
    }

    template<typename VType>
    void strValue(JsonWriter* js, const VType& val) {
        using MType = std::decay_t<VType>;

        // Handle LSPAny aka void* specifically to prevent recursion into 'void'
        if constexpr (std::is_same_v<MType, void*> ||
                      std::is_same_v<MType, T::LSPAny>) {
            if (val != nullptr) {
                // TODO
                jsonWriteNull(js);
            }
        }
        else if constexpr (std::is_enum_v<MType>) {
            if constexpr (has_to_string<MType>::value) {
                jsonWriteStr(js, toJS(toString(val)));
            } else {
                jsonWriteInt(js, static_cast<long long>(val));
            }
        }
        else if constexpr (std::is_same_v<MType, T::Int>) {
            jsonWriteInt(js, (long long) val);
        }
        else if constexpr (std::is_same_v<MType, T::UInt>) {
            jsonWriteInt(js, (long long) val);
        }
        else if constexpr (std::is_same_v<MType, T::Bool>) {
            jsonWriteBool(js, val);
        }
        else if constexpr (std::is_same_v<MType, T::String>) {
            if (val.buff) {
                jsonWriteStr(js, toJS(val));
            }
        }
        else if constexpr (std::is_pointer_v<MType>) {
            if (val != nullptr) {
                jsonWriteObjectStart(js);
                str<std::remove_pointer_t<MType>>(js, val);
                jsonWriteObjectEnd(js);
            }
        }
        else if constexpr (is_union_schema<MType>::value) {
            auto currentKind = val.*(T::UnionSchema<MType>::tag);

            std::apply([&](auto&&... variants) {
                (([&] {
                    if (variants.kind == currentKind) {\
                        auto& memberVal = val.*(variants.ptr);
                        using MemberType = std::decay_t<decltype(memberVal)>;

                        strValue<MemberType>(js, memberVal);
                    }
                })(), ...);
            }, T::UnionSchema<MType>::variants);
        }
        else if constexpr (is_schema<MType>::value) {
            jsonWriteObjectStart(js);
            str<MType>(js, &val);
            jsonWriteObjectEnd(js);
        }
        else if constexpr (is_slice<MType>::value) {
            if (val.data != nullptr) {
                jsonWriteArrayStart(js);
                using ItemType = typename is_slice<MType>::type;

                for (size_t i = 0; i < val.size; i++) {
                    if constexpr (std::is_pointer_v<ItemType>) {
                        jsonWriteObjectStart(js);
                        str<std::remove_pointer_t<ItemType>>(js, val.data[i]);
                        jsonWriteObjectEnd(js);
                    } else {
                        strValue<ItemType>(js, val.data[i]);
                    }
                }
                jsonWriteArrayEnd(js);
            }
        }
    }

    template<typename Method>
    void str(JsonWriter* js, const Method* obj) {
        #if defined(_MSC_VER)
            // printf("DEBUG TYPE: %s\n", __FUNCSIG__);
        #else
            // printf("DEBUG TYPE: %s\n", __PRETTY_FUNCTION__);
        #endif
        std::apply([&](auto&&... fields) {(([&] {
            // We need to skip empty/null fields
            auto& val = obj->*(fields.ptr);
            using MType = std::decay_t<decltype(val)>;

            bool skip = false;
            if constexpr (
                !std::is_same_v<MType, T::Int>  &&
                !std::is_same_v<MType, T::UInt> &&
                !std::is_same_v<MType, T::Bool>
            ) {
                if constexpr (std::is_pointer_v<MType>) {
                    skip = (val == nullptr);
                }
                else if constexpr (is_slice<MType>::value) {
                    skip = (val.data == nullptr);
                }
                else if constexpr (std::is_same_v<MType, String>) {
                    skip = (val.buff == nullptr);
                }
                else if constexpr (std::is_enum_v<MType>) {
                    skip = ((int)val == 0);
                }
            }

            if (!skip) {
                jsonWriteKey(js, toJS(fields.name));
                strValue(js, val);
            }
        })(), ...);
        }, T::Schema<Method>::fields);
    }

    template<typename MethodResult>
    JsonString serialize(JsonWriter* js, MethodResult* result) {

        if (!result) {
            jsonWriteNull(js);
        } else {
            jsonWriteObjectStart(js);
            str(js, result);
            jsonWriteObjectEnd(js);
        }

        return jsonWriterCommit(js);

    }

    Err::Kind handle(TextDocument::DidOpen*   method);
    Err::Kind handle(TextDocument::DidClose*  method);
    Err::Kind handle(TextDocument::DidChange* method);

    T::SemanticTokens*      handle(TextDocument::SemanticTokensfull* method);
    T::SemanticTokensDelta* handle(TextDocument::SemanticTokensfulldelta* method);

    T::Hover* handle(TextDocument::Hover* method);

}
