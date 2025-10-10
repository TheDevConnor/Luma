/**
 * @file lsp_server.c
 * @brief Language Server Protocol implementation
 */

#include "lsp.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// JSON-RPC Helpers
// ============================================================================

LSPMethod lsp_parse_method(const char *json) {
  if (!json)
    return LSP_METHOD_UNKNOWN;

  if (strstr(json, "initialize"))
    return LSP_METHOD_INITIALIZE;
  if (strstr(json, "initialized"))
    return LSP_METHOD_INITIALIZED;
  if (strstr(json, "shutdown"))
    return LSP_METHOD_SHUTDOWN;
  if (strstr(json, "exit"))
    return LSP_METHOD_EXIT;
  if (strstr(json, "textDocument/didOpen"))
    return LSP_METHOD_TEXT_DOCUMENT_DID_OPEN;
  if (strstr(json, "textDocument/didChange"))
    return LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE;
  if (strstr(json, "textDocument/didClose"))
    return LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE;
  if (strstr(json, "textDocument/hover"))
    return LSP_METHOD_TEXT_DOCUMENT_HOVER;
  if (strstr(json, "textDocument/definition"))
    return LSP_METHOD_TEXT_DOCUMENT_DEFINITION;
  if (strstr(json, "textDocument/completion"))
    return LSP_METHOD_TEXT_DOCUMENT_COMPLETION;
  if (strstr(json, "textDocument/documentSymbol"))
    return LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL;
  if (strstr(json, "textDocument/semanticTokens"))
    return LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS;

  return LSP_METHOD_UNKNOWN;
}

void lsp_send_response(int id, const char *result) {
  printf("Content-Length: %zu\r\n\r\n", strlen(result) + 50);
  printf("{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}\n", id, result);
  fflush(stdout);
}

void lsp_send_notification(const char *method, const char *params) {
  printf("Content-Length: %zu\r\n\r\n", strlen(method) + strlen(params) + 50);
  printf("{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}\n", method,
         params);
  fflush(stdout);
}

void lsp_send_error(int id, int code, const char *message) {
  printf("Content-Length: %zu\r\n\r\n", strlen(message) + 100);
  printf("{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%d,\"message\":\"%"
         "s\"}}\n",
         id, code, message);
  fflush(stdout);
}

// ============================================================================
// URI Utilities
// ============================================================================

const char *lsp_uri_to_path(const char *uri, ArenaAllocator *arena) {
  if (!uri)
    return NULL;

  // Handle file:// URIs
  if (strncmp(uri, "file://", 7) == 0) {
    const char *path = uri + 7;
// On Windows, remove leading slash from /C:/path
#ifdef _WIN32
    if (path[0] == '/' && path[2] == ':') {
      path++;
    }
#endif
    return arena_strdup(arena, path);
  }

  return arena_strdup(arena, uri);
}

const char *lsp_path_to_uri(const char *path, ArenaAllocator *arena) {
  if (!path)
    return NULL;

  size_t len = strlen(path) + 8; // "file://" + path + null
  char *uri = arena_alloc(arena, len, 1);

#ifdef _WIN32
  snprintf(uri, len, "file:///%s", path);
#else
  snprintf(uri, len, "file://%s", path);
#endif

  return uri;
}

// ============================================================================
// Document Management
// ============================================================================

bool lsp_server_init(LSPServer *server, ArenaAllocator *arena) {
  if (!server || !arena)
    return false;

  server->arena = arena;
  server->initialized = false;
  server->client_process_id = -1;
  server->document_count = 0;
  server->document_capacity = 16;

  server->documents =
      arena_alloc(arena, server->document_capacity * sizeof(LSPDocument *),
                  alignof(LSPDocument *));

  return server->documents != NULL;
}

LSPDocument *lsp_document_open(LSPServer *server, const char *uri,
                               const char *content, int version) {
  if (!server || !uri || !content)
    return NULL;

  // Check if document already exists
  LSPDocument *existing = lsp_document_find(server, uri);
  if (existing) {
    return existing;
  }

  // Check capacity
  if (server->document_count >= server->document_capacity) {
    return NULL; // Could implement resize here
  }

  // Create new document
  LSPDocument *doc =
      arena_alloc(server->arena, sizeof(LSPDocument), alignof(LSPDocument));
  if (!doc)
    return NULL;

  doc->uri = arena_strdup(server->arena, uri);
  doc->content = arena_strdup(server->arena, content);
  doc->version = version;
  doc->tokens = NULL;
  doc->token_count = 0;
  doc->ast = NULL;
  doc->scope = NULL;
  doc->diagnostics = NULL;
  doc->diagnostic_count = 0;
  doc->needs_reanalysis = true;

  // Create arena for document-specific allocations
  doc->arena = arena_alloc(server->arena, sizeof(ArenaAllocator),
                           alignof(ArenaAllocator));
  // init_arena(doc->arena, 1024 * 1024); // 1MB for document analysis
  arena_alloc(doc->arena, 1024 * 1024, sizeof(LSPDocument));

  server->documents[server->document_count++] = doc;

  return doc;
}

bool lsp_document_update(LSPServer *server, const char *uri,
                         const char *content, int version) {
  if (!server || !uri || !content)
    return false;

  LSPDocument *doc = lsp_document_find(server, uri);
  if (!doc)
    return false;

  // Update content
  doc->content = arena_strdup(server->arena, content);
  doc->version = version;
  doc->needs_reanalysis = true;

  return true;
}

bool lsp_document_close(LSPServer *server, const char *uri) {
  if (!server || !uri)
    return false;

  for (size_t i = 0; i < server->document_count; i++) {
    if (strcmp(server->documents[i]->uri, uri) == 0) {
      // Clean up document arena
      if (server->documents[i]->arena) {
        // cleanup_arena(server->documents[i]->arena);
      }

      // Remove from array by shifting
      for (size_t j = i; j < server->document_count - 1; j++) {
        server->documents[j] = server->documents[j + 1];
      }
      server->document_count--;
      return true;
    }
  }

  return false;
}

LSPDocument *lsp_document_find(LSPServer *server, const char *uri) {
  if (!server || !uri)
    return NULL;

  for (size_t i = 0; i < server->document_count; i++) {
    if (strcmp(server->documents[i]->uri, uri) == 0) {
      return server->documents[i];
    }
  }

  return NULL;
}

bool lsp_document_analyze(LSPDocument *doc, BuildConfig *config) {
  if (!doc || !doc->needs_reanalysis)
    return true;

  // Reset document arena for fresh analysis
  arena_destroy(doc->arena);
  arena_allocator_init(doc->arena, 1024 * 1024);

  // Lex the content
  Lexer lexer;
  init_lexer(&lexer, doc->content, doc->arena);

  GrowableArray tokens;
  growable_array_init(&tokens, doc->arena, 256, sizeof(Token));

  Token token;
  while ((token = next_token(&lexer)).type_ != TOK_EOF) {
    Token *slot = (Token *)growable_array_push(&tokens);
    if (slot)
      *slot = token;
  }

  doc->tokens = (Token *)tokens.data;
  doc->token_count = tokens.count;

  // Parse the tokens
  doc->ast = parse(&tokens, doc->arena, config);
  if (!doc->ast) {
    return false;
  }

  // Type check
  Scope global_scope;
  init_scope(&global_scope, NULL, "global", doc->arena);
  doc->scope = &global_scope;

  bool success = typecheck(doc->ast, &global_scope, doc->arena, config);

  doc->needs_reanalysis = false;
  return success;
}

// ============================================================================
// LSP Features - Hover
// ============================================================================

Token *lsp_token_at_position(LSPDocument *doc, LSPPosition position) {
  if (!doc || !doc->tokens)
    return NULL;

  for (size_t i = 0; i < doc->token_count; i++) {
    Token *tok = &doc->tokens[i];
    if ((int)tok->line == position.line) {
      size_t token_end_col = tok->col + tok->length;
      if ((int)tok->col <= position.character &&
          position.character < (int)token_end_col) {
        return tok;
      }
    }
  }

  return NULL;
}

Symbol *lsp_symbol_at_position(LSPDocument *doc, LSPPosition position) {
  if (!doc || !doc->scope)
    return NULL;

  Token *token = lsp_token_at_position(doc, position);
  if (!token || token->type_ != TOK_IDENTIFIER)
    return NULL;

  // Extract identifier name
  char name[256];
  size_t len = token->length < 255 ? token->length : 255;
  memcpy(name, token->value, len);
  name[len] = '\0';

  return scope_lookup(doc->scope, name);
}

const char *lsp_hover(LSPDocument *doc, LSPPosition position,
                      ArenaAllocator *arena) {
  if (!doc)
    return NULL;

  Symbol *symbol = lsp_symbol_at_position(doc, position);
  if (!symbol)
    return NULL;

  // Build hover text
  const char *type_str = type_to_string(symbol->type, arena);
  size_t len = strlen(symbol->name) + strlen(type_str) + 50;
  char *hover = arena_alloc(arena, len, 1);

  snprintf(hover, len, "```\n%s: %s\n```\n%s%s", symbol->name, type_str,
           symbol->is_public ? "public " : "",
           symbol->is_mutable ? "mutable" : "immutable");

  return hover;
}

// ============================================================================
// LSP Features - Definition
// ============================================================================

LSPLocation *lsp_definition(LSPDocument *doc, LSPPosition position,
                            ArenaAllocator *arena) {
  if (!doc)
    return NULL;

  Symbol *symbol = lsp_symbol_at_position(doc, position);
  if (!symbol)
    return NULL;

  // For now, return current location (would need definition tracking)
  LSPLocation *loc =
      arena_alloc(arena, sizeof(LSPLocation), alignof(LSPLocation));
  loc->uri = doc->uri;
  loc->range.start.line = position.line;
  loc->range.start.character = 0;
  loc->range.end.line = position.line;
  loc->range.end.character = 100;

  return loc;
}

// ============================================================================
// LSP Features - Diagnostics
// ============================================================================

LSPDiagnostic *lsp_diagnostics(LSPDocument *doc, size_t *diagnostic_count,
                               ArenaAllocator *arena) {
  if (!doc || !diagnostic_count)
    return NULL;

  // This would integrate with your error system
  // For now, return empty diagnostics
  *diagnostic_count = 0;
  return NULL;
}

// ============================================================================
// Main Message Handler
// ============================================================================

void lsp_handle_message(LSPServer *server, const char *message) {
  if (!server || !message)
    return;

  LSPMethod method = lsp_parse_method(message);

  switch (method) {
  case LSP_METHOD_INITIALIZE: {
    server->initialized = true;
    const char *capabilities =
        "{\"capabilities\":{"
        "\"textDocumentSync\":1,"
        "\"hoverProvider\":true,"
        "\"definitionProvider\":true,"
        "\"completionProvider\":{\"triggerCharacters\":[\".\",\":\"]},"
        "\"documentSymbolProvider\":true"
        "}}";
    lsp_send_response(1, capabilities);
    break;
  }

  case LSP_METHOD_INITIALIZED:
    // Nothing to do
    break;

  case LSP_METHOD_SHUTDOWN:
    lsp_send_response(2, "null");
    break;

  case LSP_METHOD_EXIT:
    exit(0);
    break;

  case LSP_METHOD_TEXT_DOCUMENT_DID_OPEN:
    // Parse document from message
    // lsp_document_open(server, uri, content, version);
    break;

  case LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE:
    // Parse updates from message
    // lsp_document_update(server, uri, content, version);
    break;

  case LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE:
    // Parse URI from message
    // lsp_document_close(server, uri);
    break;

  default:
    fprintf(stderr, "Unhandled method\n");
    break;
  }
}

void lsp_server_run(LSPServer *server) {
  if (!server)
    return;

  char buffer[65536];

  while (1) {
    // Read Content-Length header
    if (!fgets(buffer, sizeof(buffer), stdin)) {
      break;
    }

    if (strncmp(buffer, "Content-Length:", 15) != 0) {
      continue;
    }

    int content_length = atoi(buffer + 15);

    // Skip empty line
    fgets(buffer, sizeof(buffer), stdin);

    // Read message
    if (content_length > 0 && content_length < (int)sizeof(buffer)) {
      size_t read = fread(buffer, 1, content_length, stdin);
      buffer[read] = '\0';

      lsp_handle_message(server, buffer);
    }
  }
}

void lsp_server_shutdown(LSPServer *server) {
  if (!server)
    return;

  // Clean up all documents
  for (size_t i = 0; i < server->document_count; i++) {
    if (server->documents[i]->arena) {
      arena_destroy(server->documents[i]->arena);
    }
  }

  server->initialized = false;
  server->document_count = 0;
}
