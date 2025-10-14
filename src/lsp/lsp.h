/**
 * @file lsp.h
 * @brief Language Server Protocol implementation for Luma language
 */

#pragma once

#include "../c_libs/memory/memory.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../typechecker/type.h"
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// CORE LSP TYPES
// ============================================================================

typedef enum { LSP_REQUEST, LSP_RESPONSE, LSP_NOTIFICATION } LSPMessageType;

typedef enum {
  LSP_METHOD_INITIALIZE,
  LSP_METHOD_INITIALIZED,
  LSP_METHOD_SHUTDOWN,
  LSP_METHOD_EXIT,
  LSP_METHOD_TEXT_DOCUMENT_DID_OPEN,
  LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE,
  LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE,
  LSP_METHOD_TEXT_DOCUMENT_HOVER,
  LSP_METHOD_TEXT_DOCUMENT_DEFINITION,
  LSP_METHOD_TEXT_DOCUMENT_COMPLETION,
  LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL,
  LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS,
  LSP_METHOD_UNKNOWN
} LSPMethod;

// ============================================================================
// LSP PROTOCOL STRUCTURES (Position, Range, Location)
// ============================================================================

typedef struct {
  int line;
  int character;
} LSPPosition;

typedef struct {
  LSPPosition start;
  LSPPosition end;
} LSPRange;

typedef struct {
  const char *uri;
  LSPRange range;
} LSPLocation;

// ============================================================================
// DIAGNOSTICS
// ============================================================================

typedef enum {
  LSP_DIAGNOSTIC_ERROR = 1,
  LSP_DIAGNOSTIC_WARNING = 2,
  LSP_DIAGNOSTIC_INFORMATION = 3,
  LSP_DIAGNOSTIC_HINT = 4
} LSPDiagnosticSeverity;

typedef struct {
  LSPRange range;
  LSPDiagnosticSeverity severity;
  const char *message;
  const char *source;
} LSPDiagnostic;

// ============================================================================
// DOCUMENT SYMBOLS
// ============================================================================

typedef enum {
  LSP_SYMBOL_FILE = 1,
  LSP_SYMBOL_MODULE = 2,
  LSP_SYMBOL_NAMESPACE = 3,
  LSP_SYMBOL_PACKAGE = 4,
  LSP_SYMBOL_CLASS = 5,
  LSP_SYMBOL_METHOD = 6,
  LSP_SYMBOL_PROPERTY = 7,
  LSP_SYMBOL_FIELD = 8,
  LSP_SYMBOL_CONSTRUCTOR = 9,
  LSP_SYMBOL_ENUM = 10,
  LSP_SYMBOL_INTERFACE = 11,
  LSP_SYMBOL_FUNCTION = 12,
  LSP_SYMBOL_VARIABLE = 13,
  LSP_SYMBOL_CONSTANT = 14,
  LSP_SYMBOL_STRING = 15,
  LSP_SYMBOL_NUMBER = 16,
  LSP_SYMBOL_BOOLEAN = 17,
  LSP_SYMBOL_ARRAY = 18,
  LSP_SYMBOL_STRUCT = 23
} LSPSymbolKind;

typedef struct LSPDocumentSymbol {
  const char *name;
  LSPSymbolKind kind;
  LSPRange range;
  LSPRange selection_range;
  struct LSPDocumentSymbol **children;
  size_t child_count;
} LSPDocumentSymbol;

// ============================================================================
// COMPLETION
// ============================================================================

typedef enum {
  LSP_COMPLETION_TEXT = 1,
  LSP_COMPLETION_METHOD = 2,
  LSP_COMPLETION_FUNCTION = 3,
  LSP_COMPLETION_CONSTRUCTOR = 4,
  LSP_COMPLETION_FIELD = 5,
  LSP_COMPLETION_VARIABLE = 6,
  LSP_COMPLETION_CLASS = 7,
  LSP_COMPLETION_INTERFACE = 8,
  LSP_COMPLETION_MODULE = 9,
  LSP_COMPLETION_PROPERTY = 10,
  LSP_COMPLETION_KEYWORD = 14,
  LSP_COMPLETION_SNIPPET = 15,
  LSP_COMPLETION_STRUCT = 22
} LSPCompletionItemKind;

typedef enum {
  LSP_INSERT_FORMAT_PLAIN_TEXT = 1,
  LSP_INSERT_FORMAT_SNIPPET = 2
} LSPInsertTextFormat;

typedef struct {
  const char *label;
  LSPCompletionItemKind kind;
  const char *insert_text;
  LSPInsertTextFormat format;
  const char *detail;
  const char *documentation;
  const char *sort_text;
  const char *filter_text;
} LSPCompletionItem;

// ============================================================================
// MODULE SYSTEM
// ============================================================================

typedef struct {
  const char *module_path; // e.g., "string", "std/memory"
  const char *alias;       // Import alias (e.g., "str")
  Scope *scope;            // Parsed scope from that module
} ImportedModule;

typedef struct {
  const char *module_name; // e.g., "math", "string"
  const char *file_uri;    // Full file URI where module is defined
} ModuleRegistryEntry;

typedef struct {
  ModuleRegistryEntry *entries;
  size_t count;
  size_t capacity;
} ModuleRegistry;

typedef struct {
  AstNode *ast;           // The module AST node
  Scope *scope;           // The scope created during typecheck
  const char *module_uri; // URI of the module file
} ModuleInfo;

// ============================================================================
// DOCUMENT & SERVER STATE
// ============================================================================

typedef struct {
  // Document identity
  const char *uri;
  const char *content;
  int version;

  // Analysis results (cached)
  Token *tokens;
  size_t token_count;
  AstNode *ast;
  Scope *scope;
  LSPDiagnostic *diagnostics;
  size_t diagnostic_count;

  // Module imports
  ImportedModule *imports;
  size_t import_count;

  // Memory & state
  ArenaAllocator *arena;
  bool needs_reanalysis;
} LSPDocument;

typedef struct {
  // Document tracking
  LSPDocument **documents;
  size_t document_count;
  size_t document_capacity;

  // Module registry for workspace
  ModuleRegistry module_registry;

  // Server state
  ArenaAllocator *arena;
  bool initialized;
  int client_process_id;
} LSPServer;

// ============================================================================
// SERVER LIFECYCLE
// ============================================================================

bool lsp_server_init(LSPServer *server, ArenaAllocator *arena);
void lsp_server_run(LSPServer *server);
void lsp_server_shutdown(LSPServer *server);
void lsp_handle_message(LSPServer *server, const char *message);

// ============================================================================
// DOCUMENT MANAGEMENT
// ============================================================================

LSPDocument *lsp_document_open(LSPServer *server, const char *uri,
                               const char *content, int version);
bool lsp_document_update(LSPServer *server, const char *uri,
                         const char *content, int version);
bool lsp_document_close(LSPServer *server, const char *uri);
LSPDocument *lsp_document_find(LSPServer *server, const char *uri);
bool lsp_document_analyze(LSPDocument *doc, LSPServer *server,
                          BuildConfig *config);

// ============================================================================
// MODULE & IMPORT RESOLUTION
// ============================================================================

void extract_imports(LSPDocument *doc, ArenaAllocator *arena);
void resolve_imports(LSPServer *server, LSPDocument *doc, BuildConfig *config,
                     GrowableArray *imported_modules);
void build_module_registry(LSPServer *server, const char *workspace_uri);
const char *lookup_module(LSPServer *server, const char *module_name);
AstNode *parse_imported_module_ast(LSPServer *server, const char *module_uri,
                                   BuildConfig *config, ArenaAllocator *arena);

// ============================================================================
// LSP FEATURES (Hover, Definition, Completion, etc.)
// ============================================================================

const char *lsp_hover(LSPDocument *doc, LSPPosition position,
                      ArenaAllocator *arena);
LSPLocation *lsp_definition(LSPDocument *doc, LSPPosition position,
                            ArenaAllocator *arena);
LSPCompletionItem *lsp_completion(LSPDocument *doc, LSPPosition position,
                                  size_t *completion_count,
                                  ArenaAllocator *arena);
LSPDocumentSymbol **lsp_document_symbols(LSPDocument *doc, size_t *symbol_count,
                                         ArenaAllocator *arena);
LSPDiagnostic *lsp_diagnostics(LSPDocument *doc, size_t *diagnostic_count,
                               ArenaAllocator *arena);
LSPDiagnostic *convert_errors_to_diagnostics(size_t *diagnostic_count,
                                             ArenaAllocator *arena);

// ============================================================================
// JSON-RPC PROTOCOL
// ============================================================================

LSPMethod lsp_parse_method(const char *json);
void lsp_send_response(int id, const char *result);
void lsp_send_notification(const char *method, const char *params);
void lsp_send_error(int id, int code, const char *message);

// JSON extraction helpers
char *extract_string(const char *json, const char *key, ArenaAllocator *arena);
int extract_int(const char *json, const char *key);
LSPPosition extract_position(const char *json);

// JSON serialization helpers
void serialize_diagnostics_to_json(const char *uri, LSPDiagnostic *diagnostics,
                                   size_t diag_count, char *output,
                                   size_t output_size);
void serialize_completion_items(LSPCompletionItem *items, size_t count,
                                char *output, size_t output_size);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// URI conversion
const char *lsp_uri_to_path(const char *uri, ArenaAllocator *arena);
const char *lsp_path_to_uri(const char *path, ArenaAllocator *arena);

// Position-based queries
Token *lsp_token_at_position(LSPDocument *doc, LSPPosition position);
AstNode *lsp_node_at_position(LSPDocument *doc, LSPPosition position);
Symbol *lsp_symbol_at_position(LSPDocument *doc, LSPPosition position);
