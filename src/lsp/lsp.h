/**
 * @file lsp_server.h
 * @brief Language Server Protocol implementation for your language
 *
 * This LSP server provides IDE features like:
 * - Diagnostics (errors, warnings)
 * - Hover information
 * - Go to definition
 * - Code completion
 * - Document symbols
 */

#pragma once

#include "../c_libs/memory/memory.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../typechecker/type.h"
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// LSP Message Types
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
// Completion Types
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
  const char *label;          // What user sees
  LSPCompletionItemKind kind; // Icon/type
  const char *insert_text;    // Text to insert
  LSPInsertTextFormat format; // Plain or snippet
  const char *detail;         // Type signature
  const char *documentation;  // Description
  const char *sort_text;      // For ordering
  const char *filter_text;    // For filtering
} LSPCompletionItem;

// ============================================================================
// Document Management
// ============================================================================

typedef struct {
  const char *module_path; // Resolved file path
  const char *alias;       // Import alias (e.g., "string", "mem")
  Scope *scope;            // Parsed scope from that module
} ImportedModule;

typedef struct {
  const char *module_name; // e.g., "math", "string", "std/memory"
  const char *file_uri;    // Full file URI where this module is defined
} ModuleRegistryEntry;

typedef struct {
  ModuleRegistryEntry *entries;
  size_t count;
  size_t capacity;
} ModuleRegistry;

typedef struct {
  const char *uri;
  const char *content;
  int version;

  // Cached analysis results
  Token *tokens;
  size_t token_count;
  AstNode *ast;
  Scope *scope;
  LSPDiagnostic *diagnostics;
  size_t diagnostic_count;

  ArenaAllocator *arena;
  bool needs_reanalysis;

  ImportedModule *imports;
  size_t import_count;
} LSPDocument;

typedef struct {
  LSPDocument **documents;
  size_t document_count;
  size_t document_capacity;

  ModuleRegistry module_registry;
  ArenaAllocator *arena;
  bool initialized;
  int client_process_id;
} LSPServer;

// ============================================================================
// Core LSP Server Functions
// ============================================================================

/**
 * @brief Initialize the LSP server
 */
bool lsp_server_init(LSPServer *server, ArenaAllocator *arena);

/**
 * @brief Main message processing loop
 */
void lsp_server_run(LSPServer *server);

/**
 * @brief Handle incoming LSP message
 */
void lsp_handle_message(LSPServer *server, const char *message);

/**
 * @brief Shutdown the server
 */
void lsp_server_shutdown(LSPServer *server);

// ============================================================================
// Document Management Functions
// ============================================================================

/**
 * @brief Open a document and add it to the server
 */
LSPDocument *lsp_document_open(LSPServer *server, const char *uri,
                               const char *content, int version);

/**
 * @brief Update document content
 */
bool lsp_document_update(LSPServer *server, const char *uri,
                         const char *content, int version);

/**
 * @brief Close and remove a document
 */
bool lsp_document_close(LSPServer *server, const char *uri);

/**
 * @brief Find document by URI
 */
LSPDocument *lsp_document_find(LSPServer *server, const char *uri);

/**
 * @brief Analyze document (lex, parse, typecheck)
 */
bool lsp_document_analyze(LSPDocument *doc, LSPServer *server,
                          BuildConfig *config);

// ============================================================================
// LSP Feature Implementations
// ============================================================================

/**
 * @brief Get hover information at position
 */
const char *lsp_hover(LSPDocument *doc, LSPPosition position,
                      ArenaAllocator *arena);

/**
 * @brief Get definition location for symbol at position
 */
LSPLocation *lsp_definition(LSPDocument *doc, LSPPosition position,
                            ArenaAllocator *arena);

/**
 * @brief Get completions at position
 */
LSPCompletionItem *lsp_completion(LSPDocument *doc, LSPPosition position,
                                  size_t *completion_count,
                                  ArenaAllocator *arena);

/**
 * @brief Get document symbols (outline)
 */
LSPDocumentSymbol **lsp_document_symbols(LSPDocument *doc, size_t *symbol_count,
                                         ArenaAllocator *arena);

/**
 * @brief Get diagnostics (errors, warnings)
 */
LSPDiagnostic *lsp_diagnostics(LSPDocument *doc, size_t *diagnostic_count,
                               ArenaAllocator *arena);

/**
 * @brief Convert error system errors to LSP diagnostics
 */
LSPDiagnostic *convert_errors_to_diagnostics(size_t *diagnostic_count,
                                             ArenaAllocator *arena);

// ============================================================================
// JSON-RPC Helpers
// ============================================================================

/**
 * @brief Parse incoming JSON-RPC message
 */
LSPMethod lsp_parse_method(const char *json);

/**
 * @brief Send JSON-RPC response
 */
void lsp_send_response(int id, const char *result);

/**
 * @brief Send JSON-RPC notification
 */
void lsp_send_notification(const char *method, const char *params);

/**
 * @brief Send error response
 */
void lsp_send_error(int id, int code, const char *message);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Convert file URI to path
 */
const char *lsp_uri_to_path(const char *uri, ArenaAllocator *arena);

/**
 * @brief Convert path to file URI
 */
const char *lsp_path_to_uri(const char *path, ArenaAllocator *arena);

/**
 * @brief Get token at position
 */
Token *lsp_token_at_position(LSPDocument *doc, LSPPosition position);

/**
 * @brief Get AST node at position
 */
AstNode *lsp_node_at_position(LSPDocument *doc, LSPPosition position);

/**
 * @brief Get symbol at position
 */
Symbol *lsp_symbol_at_position(LSPDocument *doc, LSPPosition position);

void serialize_completion_items(LSPCompletionItem *items, size_t count,
                                char *output, size_t output_size);
