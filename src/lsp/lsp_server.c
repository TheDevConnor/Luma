/**
 * @file lsp_server.c
 * @brief Language Server Protocol implementation - UPDATED VERSION
 */

#include "lsp.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// JSON Parsing Utilities
// ============================================================================

// Simple JSON string extraction (handles escaped quotes)
static char *extract_string(const char *json, const char *key,
                            ArenaAllocator *arena) {
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *found = strstr(json, search);
  if (!found)
    return NULL;

  // Find the value after the colon
  const char *colon = strchr(found + strlen(search), ':');
  if (!colon)
    return NULL;

  // Skip whitespace and find opening quote
  const char *start = colon + 1;
  while (*start && isspace(*start))
    start++;
  if (*start != '"')
    return NULL;
  start++;

  // Find closing quote (handle escapes)
  const char *end = start;
  while (*end && (*end != '"' || *(end - 1) == '\\'))
    end++;

  size_t len = end - start;
  char *result = arena_alloc(arena, len + 1, 1);
  memcpy(result, start, len);
  result[len] = '\0';

  return result;
}

// Extract integer from JSON
static int extract_int(const char *json, const char *key) {
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *found = strstr(json, search);
  if (!found)
    return -1;

  const char *colon = strchr(found + strlen(search), ':');
  if (!colon)
    return -1;

  return atoi(colon + 1);
}

// Extract position from JSON
static LSPPosition extract_position(const char *json) {
  LSPPosition pos = {0, 0};

  // Find the position object
  const char *position_obj = strstr(json, "\"position\"");
  if (!position_obj)
    return pos;

  pos.line = extract_int(position_obj, "line");
  pos.character = extract_int(position_obj, "character");
  return pos;
}

// ============================================================================
// JSON-RPC Helpers (FIXED)
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

// FIXED: Calculate exact Content-Length
void lsp_send_response(int id, const char *result) {
  char json_msg[8192];
  int msg_len =
      snprintf(json_msg, sizeof(json_msg),
               "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result);

  printf("Content-Length: %d\r\n\r\n%s", msg_len, json_msg);
  fflush(stdout);
}

// FIXED: Calculate exact Content-Length
void lsp_send_notification(const char *method, const char *params) {
  char json_msg[8192];
  int msg_len = snprintf(
      json_msg, sizeof(json_msg),
      "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}", method, params);

  printf("Content-Length: %d\r\n\r\n%s", msg_len, json_msg);
  fflush(stdout);
}

void lsp_send_error(int id, int code, const char *message) {
  char json_msg[8192];
  int msg_len = snprintf(json_msg, sizeof(json_msg),
                         "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%"
                         "d,\"message\":\"%s\"}}",
                         id, code, message);

  printf("Content-Length: %d\r\n\r\n%s", msg_len, json_msg);
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
#ifdef _WIN32
    // On Windows, remove leading slash from /C:/path
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
  arena_allocator_init(doc->arena,
                       1024 * 1024); // FIXED: Actually initialize it!

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
        arena_destroy(server->documents[i]->arena);
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

  snprintf(hover, len, "```\\n%s: %s\\n```\\n%s%s", symbol->name, type_str,
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
// Main Message Handler (COMPLETELY REWRITTEN)
// ============================================================================

void lsp_handle_message(LSPServer *server, const char *message) {
  if (!server || !message)
    return;

  fprintf(stderr, "[LSP] Received message: %.100s...\n", message);

  LSPMethod method = lsp_parse_method(message);
  int request_id = extract_int(message, "id");

  // Create temporary arena for parsing
  ArenaAllocator temp_arena;
  arena_allocator_init(&temp_arena, 64 * 1024);

  switch (method) {
  case LSP_METHOD_INITIALIZE: {
    fprintf(stderr, "[LSP] Handling initialize\n");
    server->initialized = true;
    const char *capabilities =
        "{"
        "\"capabilities\":{"
        "\"textDocumentSync\":1,"
        "\"hoverProvider\":true,"
        "\"definitionProvider\":true,"
        "\"completionProvider\":{\"triggerCharacters\":[\".\",\":\"]},"
        "\"documentSymbolProvider\":true"
        "},"
        "\"serverInfo\":{\"name\":\"Luma LSP\",\"version\":\"0.1.0\"}"
        "}";
    lsp_send_response(request_id, capabilities);
    break;
  }

  case LSP_METHOD_INITIALIZED:
    fprintf(stderr, "[LSP] Client initialized\n");
    break;

  case LSP_METHOD_TEXT_DOCUMENT_DID_OPEN: {
    fprintf(stderr, "[LSP] Handling didOpen\n");

    // Extract document info
    const char *uri = extract_string(message, "uri", &temp_arena);
    const char *text = extract_string(message, "text", &temp_arena);
    int version = extract_int(message, "version");

    if (uri && text) {
      fprintf(stderr, "[LSP] Opening document: %s (version %d)\n", uri,
              version);

      LSPDocument *doc = lsp_document_open(server, uri, text, version);
      if (doc) {
        // Analyze and send diagnostics
        BuildConfig config = {0}; // Initialize your config properly
        lsp_document_analyze(doc, &config);

        size_t diag_count;
        lsp_diagnostics(doc, &diag_count, &temp_arena);

        // Send diagnostics notification
        char params[4096];
        snprintf(params, sizeof(params), "{\"uri\":\"%s\",\"diagnostics\":[]}",
                 uri);
        lsp_send_notification("textDocument/publishDiagnostics", params);
      }
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE: {
    fprintf(stderr, "[LSP] Handling didChange\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    const char *text = extract_string(message, "text", &temp_arena);
    int version = extract_int(message, "version");

    if (uri && text) {
      lsp_document_update(server, uri, text, version);

      // Re-analyze
      LSPDocument *doc = lsp_document_find(server, uri);
      if (doc) {
        BuildConfig config = {0};
        lsp_document_analyze(doc, &config);

        // Send updated diagnostics
        char params[4096];
        snprintf(params, sizeof(params), "{\"uri\":\"%s\",\"diagnostics\":[]}",
                 uri);
        lsp_send_notification("textDocument/publishDiagnostics", params);
      }
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE: {
    fprintf(stderr, "[LSP] Handling didClose\n");
    const char *uri = extract_string(message, "uri", &temp_arena);
    if (uri) {
      lsp_document_close(server, uri);
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_HOVER: {
    fprintf(stderr, "[LSP] Handling hover\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    LSPPosition position = extract_position(message);

    if (uri) {
      LSPDocument *doc = lsp_document_find(server, uri);

      if (doc) {
        const char *hover_text = lsp_hover(doc, position, &temp_arena);
        if (hover_text) {
          char result[2048];
          snprintf(result, sizeof(result),
                   "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}",
                   hover_text);
          lsp_send_response(request_id, result);
        } else {
          lsp_send_response(request_id, "null");
        }
      } else {
        lsp_send_response(request_id, "null");
      }
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_DEFINITION: {
    fprintf(stderr, "[LSP] Handling definition\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    LSPPosition position = extract_position(message);

    if (uri) {
      LSPDocument *doc = lsp_document_find(server, uri);

      if (doc) {
        LSPLocation *loc = lsp_definition(doc, position, &temp_arena);
        if (loc) {
          char result[1024];
          snprintf(result, sizeof(result),
                   "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,"
                   "\"character\":%d},"
                   "\"end\":{\"line\":%d,\"character\":%d}}}",
                   loc->uri, loc->range.start.line, loc->range.start.character,
                   loc->range.end.line, loc->range.end.character);
          lsp_send_response(request_id, result);
        } else {
          lsp_send_response(request_id, "null");
        }
      } else {
        lsp_send_response(request_id, "null");
      }
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_COMPLETION: {
    fprintf(stderr, "[LSP] Handling completion\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    LSPPosition position = extract_position(message);

    if (uri) {
      LSPDocument *doc = lsp_document_find(server, uri);

      if (doc) {
        size_t count;
        LSPCompletionItem *items =
            lsp_completion(doc, position, &count, &temp_arena);

        if (items && count > 0) {
          char result[16384];
          serialize_completion_items(items, count, result, sizeof(result));
          lsp_send_response(request_id, result);
        } else {
          lsp_send_response(request_id, "{\"items\":[]}");
        }
      } else {
        lsp_send_response(request_id, "{\"items\":[]}");
      }
    }
    break;
  }

  case LSP_METHOD_SHUTDOWN:
    fprintf(stderr, "[LSP] Handling shutdown\n");
    lsp_send_response(request_id, "null");
    break;

  case LSP_METHOD_EXIT:
    fprintf(stderr, "[LSP] Exiting\n");
    exit(0);
    break;

  default:
    fprintf(stderr, "[LSP] Unhandled method\n");
    break;
  }

  arena_destroy(&temp_arena);
}

// ============================================================================
// LSP Features - Completion with Snippets
// ============================================================================

LSPCompletionItem *lsp_completion(LSPDocument *doc, LSPPosition position,
                                  size_t *completion_count,
                                  ArenaAllocator *arena) {
  if (!doc || !completion_count)
    return NULL;

  // Determine context (what triggered completion)
  Token *token = lsp_token_at_position(doc, position);

  GrowableArray completions;
  growable_array_init(&completions, arena, 32, sizeof(LSPCompletionItem));

  const struct {
    const char *label;
    const char *snippet;
    const char *detail;
  } keywords[] = {
      // Top-level declarations
      {"const fn", "const ${1:name} = fn (${2:params}) ${3:Type} {\n\t$0\n}",
       "Function declaration"},
      {"const struct",
       "const ${1:Name} = struct {\n\t${2:field}: ${3:Type}$0\n};",
       "Struct definition"},
      {"const enum", "const ${1:Name} = enum {\n\t${2:Variant}$0\n};",
       "Enum definition"},
      {"const var", "const ${1:name}: ${2:Type} = ${3:value};$0",
       "Top-level constant"},

      // Function attributes for memory management
      {"#returns_ownership",
       "#returns_ownership\nconst ${1:name} = fn (${2:params}) *${3:Type} "
       "{\n\tlet ${4:ptr}: *${3:Type} = "
       "cast<*${3:Type}>(alloc(sizeof<${3:Type}>));\n\t$0\n\treturn "
       "${4:ptr};\n};",
       "Function that returns owned pointer"},
      {"#takes_ownership",
       "#takes_ownership\nconst ${1:name} = fn (${2:ptr}: *${3:Type}) "
       "${4:void} {\n\t$0\n\tfree(${2:ptr});\n};",
       "Function that takes ownership and frees"},

      // Control flow
      {"if", "if ${1:condition} {\n\t$0\n}", "If statement"},
      {"if else", "if ${1:condition} {\n\t${2}\n} else {\n\t$0\n}",
       "If-else statement"},
      {"if elif",
       "if ${1:condition} {\n\t${2}\n} elif ${3:condition} {\n\t${4}\n} else "
       "{\n\t$0\n}",
       "If-elif-else statement"},

      // Loop constructs
      {"loop", "loop {\n\t$0\n}", "Infinite loop"},
      {"loop for",
       "loop [${1:i}: int = 0](${1:i} < ${2:10}) : (++${1:i}) {\n\t$0\n}",
       "For-style loop"},
      {"loop while", "loop (${1:condition}) {\n\t$0\n}", "While-style loop"},

      // Switch statement
      {"switch", "switch (${1:value}) {\n\t${2:case} => ${3:result};$0\n}",
       "Switch statement"},

      // Variables
      {"let", "let ${1:name}: ${2:Type} = ${3:value};$0",
       "Variable declaration"},

      // Memory management
      {"alloc", "cast<*${1:Type}>(alloc(sizeof<${1:Type}>))",
       "Allocate memory"},
      {"defer", "defer free(${1:ptr});$0", "Defer statement"},
      {"defer block", "defer {\n\t${1:cleanup()};\n\t$0\n}", "Defer block"},

      // Module system
      {"@module", "@module \"${1:name}\"$0", "Module declaration"},
      {"@use", "@use \"${1:module}\" as ${2:alias}$0", "Import module"},

      // Other statements
      {"return", "return ${1:value};$0", "Return statement"},
      {"break", "break;$0", "Break statement"},
      {"continue", "continue;$0", "Continue statement"},

      // Struct with access modifiers
      {"struct pub/priv",
       "const ${1:Name} = struct {\npub:\n\t${2:public_field}: "
       "${3:Type},\npriv:\n\t${4:private_field}: ${5:Type}$0\n};",
       "Struct with access modifiers"},

      // Common patterns
      {"main", "const main = fn () int {\n\t$0\n\treturn 0;\n};",
       "Main function"},
      {"outputln", "outputln(${1:message});$0", "Output with newline"},
      {"cast", "cast<${1:Type}>(${2:value})$0", "Type cast"},
      {"sizeof", "sizeof<${1:Type}>$0", "Size of type"},
  };
  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    LSPCompletionItem *item =
        (LSPCompletionItem *)growable_array_push(&completions);
    if (item) {
      item->label = arena_strdup(arena, keywords[i].label);
      item->kind = LSP_COMPLETION_SNIPPET;
      item->insert_text = arena_strdup(arena, keywords[i].snippet);
      item->format = LSP_INSERT_FORMAT_SNIPPET;
      item->detail = arena_strdup(arena, keywords[i].detail);
      item->documentation = NULL;
      item->sort_text = NULL;
      item->filter_text = NULL;
    }
  }

  // Add symbols from scope (variables, functions, etc.)
  if (doc->scope) {
    Scope *current_scope = doc->scope;
    while (current_scope) {
      for (size_t i = 0; i < current_scope->depth; i++) {
        Symbol *sym = &current_scope->symbols.data[i];

        LSPCompletionItem *item =
            (LSPCompletionItem *)growable_array_push(&completions);
        if (item) {
          item->label = arena_strdup(arena, sym->name);

          // Determine kind based on symbol type
          if (sym->type->type == AST_STMT_FUNCTION) {
            item->kind = LSP_COMPLETION_FUNCTION;
            // Create function call snippet
            char snippet[512];
            snprintf(snippet, sizeof(snippet), "%s($0)", sym->name);
            item->insert_text = arena_strdup(arena, snippet);
            item->format = LSP_INSERT_FORMAT_SNIPPET;
          } else if (sym->type->type == AST_STMT_STRUCT) {
            item->kind = LSP_COMPLETION_STRUCT;
            item->insert_text = arena_strdup(arena, sym->name);
            item->format = LSP_INSERT_FORMAT_PLAIN_TEXT;
          } else {
            item->kind = LSP_COMPLETION_VARIABLE;
            item->insert_text = arena_strdup(arena, sym->name);
            item->format = LSP_INSERT_FORMAT_PLAIN_TEXT;
          }

          item->detail = type_to_string(sym->type, arena);
          item->documentation = NULL;
          item->sort_text = NULL;
          item->filter_text = NULL;
        }
      }
      current_scope = current_scope->parent;
    }
  }

  *completion_count = completions.count;
  return (LSPCompletionItem *)completions.data;
}

// Helper to serialize completion items to JSON
void serialize_completion_items(LSPCompletionItem *items, size_t count,
                                char *output, size_t output_size) {
  size_t offset = 0;
  offset += snprintf(output + offset, output_size - offset, "{\"items\":[");

  for (size_t i = 0; i < count; i++) {
    LSPCompletionItem *item = &items[i];

    if (i > 0) {
      offset += snprintf(output + offset, output_size - offset, ",");
    }

    offset +=
        snprintf(output + offset, output_size - offset,
                 "{\"label\":\"%s\",\"kind\":%d", item->label, item->kind);

    if (item->insert_text) {
      // Escape special characters in snippet
      char escaped[1024];
      const char *src = item->insert_text;
      char *dst = escaped;
      while (*src && (dst - escaped) < (int)sizeof(escaped) - 1) {
        if (*src == '"' || *src == '\\') {
          *dst++ = '\\';
        } else if (*src == '\n') {
          *dst++ = '\\';
          *dst++ = 'n';
          src++;
          continue;
        } else if (*src == '\t') {
          *dst++ = '\\';
          *dst++ = 't';
          src++;
          continue;
        }
        *dst++ = *src++;
      }
      *dst = '\0';

      offset += snprintf(output + offset, output_size - offset,
                         ",\"insertText\":\"%s\",\"insertTextFormat\":%d",
                         escaped, item->format);
    }

    if (item->detail) {
      offset += snprintf(output + offset, output_size - offset,
                         ",\"detail\":\"%s\"", item->detail);
    }

    offset += snprintf(output + offset, output_size - offset, "}");
  }

  offset += snprintf(output + offset, output_size - offset, "]}");
}

// ============================================================================
// Main Loop
// ============================================================================

void lsp_server_run(LSPServer *server) {
  if (!server)
    return;

  fprintf(stderr, "[LSP] Server started, waiting for messages...\n");

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
    fprintf(stderr, "[LSP] Content-Length: %d\n", content_length);

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
