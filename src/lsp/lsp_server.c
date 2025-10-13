/**
 * @file lsp_server.c
 * @brief Language Server Protocol implementation with type error diagnostics
 */

#include "../c_libs/error/error.h"
#include "lsp.h"
#include <ctype.h>
#include <dirent.h> // For Unix
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Add these functions to lsp_server.c:

// Extract @module declaration from file content
static const char *extract_module_name(const char *content,
                                       ArenaAllocator *arena) {
  if (!content)
    return NULL;

  const char *src = content;

  // Look for @module directive (usually at the top)
  while (*src) {
    // Skip whitespace and comments
    while (*src && (isspace(*src) || *src == '/' || *src == '#')) {
      if (*src == '/' && *(src + 1) == '/') {
        // Skip line comment
        while (*src && *src != '\n')
          src++;
      } else if (*src == '/' && *(src + 1) == '*') {
        // Skip block comment
        src += 2;
        while (*src && !(*src == '*' && *(src + 1) == '/'))
          src++;
        if (*src)
          src += 2;
      } else {
        src++;
      }
    }

    // Check for @module
    if (strncmp(src, "@module", 7) == 0) {
      src += 7;

      // Skip whitespace
      while (*src && isspace(*src))
        src++;

      // Extract module name in quotes
      if (*src == '"') {
        src++;
        const char *name_start = src;
        while (*src && *src != '"')
          src++;

        size_t name_len = src - name_start;
        char *module_name = arena_alloc(arena, name_len + 1, 1);
        memcpy(module_name, name_start, name_len);
        module_name[name_len] = '\0';

        fprintf(stderr, "[LSP] Found @module \"%s\"\n", module_name);
        return module_name;
      }
    }

    // If we hit a non-whitespace, non-comment, non-@module token, stop looking
    if (*src && !isspace(*src) && *src != '@') {
      break;
    }

    if (*src)
      src++;
  }

  return NULL;
}

// Scan a single file and register its module
static void scan_file_for_module(LSPServer *server, const char *file_uri,
                                 ArenaAllocator *temp_arena) {
  const char *file_path = lsp_uri_to_path(file_uri, temp_arena);
  if (!file_path)
    return;

  FILE *f = fopen(file_path, "r");
  if (!f)
    return;

  // Read first 1KB to check for @module (should be at the top)
  char buffer[1024];
  size_t read = fread(buffer, 1, sizeof(buffer) - 1, f);
  buffer[read] = '\0';
  fclose(f);

  const char *module_name = extract_module_name(buffer, temp_arena);
  if (!module_name)
    return;

  // Check if already registered
  for (size_t i = 0; i < server->module_registry.count; i++) {
    if (strcmp(server->module_registry.entries[i].module_name, module_name) ==
        0) {
      // Already registered, update URI
      server->module_registry.entries[i].file_uri =
          arena_strdup(server->arena, file_uri);
      fprintf(stderr, "[LSP] Updated module '%s' -> %s\n", module_name,
              file_uri);
      return;
    }
  }

  // Add new entry
  if (server->module_registry.count >= server->module_registry.capacity) {
    // Grow registry
    size_t new_capacity = server->module_registry.capacity * 2;
    if (new_capacity == 0)
      new_capacity = 32;

    ModuleRegistryEntry *new_entries =
        arena_alloc(server->arena, new_capacity * sizeof(ModuleRegistryEntry),
                    alignof(ModuleRegistryEntry));

    if (server->module_registry.entries) {
      memcpy(new_entries, server->module_registry.entries,
             server->module_registry.count * sizeof(ModuleRegistryEntry));
    }

    server->module_registry.entries = new_entries;
    server->module_registry.capacity = new_capacity;
  }

  ModuleRegistryEntry *entry =
      &server->module_registry.entries[server->module_registry.count++];
  entry->module_name = arena_strdup(server->arena, module_name);
  entry->file_uri = arena_strdup(server->arena, file_uri);

  fprintf(stderr, "[LSP] Registered module '%s' -> %s\n", module_name,
          file_uri);
}

// Recursively scan directory for .lx files
static void scan_directory_recursive(LSPServer *server, const char *dir_path,
                                     ArenaAllocator *temp_arena) {
#ifdef _WIN32
  WIN32_FIND_DATA find_data;
  char search_path[512];
  snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

  HANDLE hFind = FindFirstFile(search_path, &find_data);
  if (hFind == INVALID_HANDLE_VALUE)
    return;

  do {
    if (strcmp(find_data.cFileName, ".") == 0 ||
        strcmp(find_data.cFileName, "..") == 0) {
      continue;
    }

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path,
             find_data.cFileName);

    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      scan_directory_recursive(server, full_path, temp_arena);
    } else {
      // Check if .lx file
      const char *ext = strrchr(find_data.cFileName, '.');
      if (ext && strcmp(ext, ".lx") == 0) {
        const char *uri = lsp_path_to_uri(full_path, temp_arena);
        if (uri) {
          scan_file_for_module(server, uri, temp_arena);
        }
      }
    }
  } while (FindNextFile(hFind, &find_data));

  FindClose(hFind);
#else
  // Unix/Linux implementation
  DIR *dir = opendir(dir_path);
  if (!dir)
    return;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        scan_directory_recursive(server, full_path, temp_arena);
      } else if (S_ISREG(st.st_mode)) {
        // Check if .lx file
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".lx") == 0) {
          const char *uri = lsp_path_to_uri(full_path, temp_arena);
          if (uri) {
            scan_file_for_module(server, uri, temp_arena);
          }
        }
      }
    }
  }

  closedir(dir);
#endif
}

// Build module registry by scanning workspace
static void build_module_registry(LSPServer *server,
                                  const char *workspace_uri) {
  fprintf(stderr, "[LSP] Building module registry for workspace: %s\n",
          workspace_uri);

  ArenaAllocator temp_arena;
  arena_allocator_init(&temp_arena, 64 * 1024);

  const char *workspace_path = lsp_uri_to_path(workspace_uri, &temp_arena);
  if (!workspace_path) {
    arena_destroy(&temp_arena);
    return;
  }

  scan_directory_recursive(server, workspace_path, &temp_arena);

  fprintf(stderr, "[LSP] Module registry built: %zu modules\n",
          server->module_registry.count);

  arena_destroy(&temp_arena);
}

// Look up module in registry
static const char *lookup_module(LSPServer *server, const char *module_name) {
  for (size_t i = 0; i < server->module_registry.count; i++) {
    if (strcmp(server->module_registry.entries[i].module_name, module_name) ==
        0) {
      fprintf(stderr, "[LSP] Lookup: '%s' -> %s\n", module_name,
              server->module_registry.entries[i].file_uri);
      return server->module_registry.entries[i].file_uri;
    }
  }

  fprintf(stderr, "[LSP] Lookup: '%s' not found\n", module_name);
  return NULL;
}

// ============================================================================
// JSON Parsing Utilities
// ============================================================================

static char *extract_string(const char *json, const char *key,
                            ArenaAllocator *arena) {
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *found = strstr(json, search);
  if (!found)
    return NULL;

  const char *colon = strchr(found + strlen(search), ':');
  if (!colon)
    return NULL;

  const char *start = colon + 1;
  while (*start && isspace(*start))
    start++;
  if (*start != '"')
    return NULL;
  start++;

  // Allocate a buffer for the unescaped string (worst case: same size)
  size_t max_len = strlen(start);
  char *result = arena_alloc(arena, max_len + 1, 1);
  char *dst = result;

  // Parse and unescape the JSON string
  const char *src = start;
  while (*src && *src != '"') {
    if (*src == '\\') {
      src++; // Skip the backslash
      if (*src == 'n') {
        *dst++ = '\n';
      } else if (*src == 't') {
        *dst++ = '\t';
      } else if (*src == 'r') {
        *dst++ = '\r';
      } else if (*src == '"') {
        *dst++ = '"';
      } else if (*src == '\\') {
        *dst++ = '\\';
      } else {
        // Unknown escape, keep the character
        *dst++ = *src;
      }
      src++;
    } else {
      *dst++ = *src++;
    }
  }

  *dst = '\0';
  return result;
}

static int extract_int(const char *json, const char *key) {
  // Build search patterns for both "key": value and "key" : value (with spaces)
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *found = strstr(json, search);
  if (!found) {
    return -1;
  }

  // Find the colon after the key
  const char *colon = found + strlen(search);

  // Skip whitespace and colon
  while (*colon && (isspace(*colon) || *colon == ':')) {
    colon++;
  }

  // Now we should be at the number
  if (!*colon || !isdigit(*colon)) {
    return -1;
  }

  return atoi(colon);
}

static LSPPosition extract_position(const char *json) {
  LSPPosition pos = {0, 0};

  const char *position_obj = strstr(json, "\"position\"");
  if (!position_obj)
    return pos;

  pos.line = extract_int(position_obj, "line");
  pos.character = extract_int(position_obj, "character");
  return pos;
}

// ============================================================================
// JSON-RPC Helpers
// ============================================================================

LSPMethod lsp_parse_method(const char *json) {
  if (!json)
    return LSP_METHOD_UNKNOWN;

  // CHECK LONGER STRINGS FIRST!
  if (strstr(json, "textDocument/documentSymbol"))
    return LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL;
  if (strstr(json, "textDocument/semanticTokens"))
    return LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS;
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

  // Check "initialized" BEFORE "initialize"!
  if (strstr(json, "initialized"))
    return LSP_METHOD_INITIALIZED;
  if (strstr(json, "initialize"))
    return LSP_METHOD_INITIALIZE;

  if (strstr(json, "shutdown"))
    return LSP_METHOD_SHUTDOWN;
  if (strstr(json, "exit"))
    return LSP_METHOD_EXIT;

  return LSP_METHOD_UNKNOWN;
}

void lsp_send_response(int id, const char *result) {
  char json_msg[8192];
  int msg_len =
      snprintf(json_msg, sizeof(json_msg),
               "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result);

  printf("Content-Length: %d\r\n\r\n%s", msg_len, json_msg);
  fflush(stdout);
}

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

  if (strncmp(uri, "file://", 7) == 0) {
    const char *path = uri + 7;
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

  size_t len = strlen(path) + 8;
  char *uri = arena_alloc(arena, len, 1);

#ifdef _WIN32
  snprintf(uri, len, "file:///%s", path);
#else
  snprintf(uri, len, "file://%s", path);
#endif

  return uri;
}

// ============================================================================
// Helper for Diagnostics Serialization
// ============================================================================

static void serialize_diagnostics_to_json(const char *uri,
                                          LSPDiagnostic *diagnostics,
                                          size_t diag_count, char *output,
                                          size_t output_size) {
  size_t offset = 0;
  offset += snprintf(output + offset, output_size - offset,
                     "{\"uri\":\"%s\",\"diagnostics\":[", uri);

  for (size_t i = 0; i < diag_count; i++) {
    LSPDiagnostic *diag = &diagnostics[i];
    if (i > 0) {
      offset += snprintf(output + offset, output_size - offset, ",");
    }

    // Escape message text
    char escaped_msg[2048] = {0};
    const char *src = diag->message;
    char *dst = escaped_msg;
    while (*src && (dst - escaped_msg) < (int)sizeof(escaped_msg) - 1) {
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
                       "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                       "\"end\":{\"line\":%d,\"character\":%d}},"
                       "\"severity\":%d,\"message\":\"%s\",\"source\":\"%s\"}",
                       diag->range.start.line, diag->range.start.character,
                       diag->range.end.line, diag->range.end.character,
                       diag->severity, escaped_msg,
                       diag->source ? diag->source : "luma");
  }

  offset += snprintf(output + offset, output_size - offset, "]}");
}

// Extract @use declarations from source code
void extract_imports(LSPDocument *doc, ArenaAllocator *arena) {
  if (!doc || !doc->content)
    return;

  GrowableArray imports;
  growable_array_init(&imports, arena, 4, sizeof(ImportedModule));

  const char *src = doc->content;
  while (*src) {
    // Find @use directive
    if (strncmp(src, "@use", 4) == 0) {
      src += 4;

      // Skip whitespace
      while (*src && isspace(*src))
        src++;

      // Extract module path in quotes
      if (*src == '"') {
        src++;
        const char *path_start = src;
        while (*src && *src != '"')
          src++;

        size_t path_len = src - path_start;
        char *module_path = arena_alloc(arena, path_len + 1, 1);
        memcpy(module_path, path_start, path_len);
        module_path[path_len] = '\0';

        if (*src == '"')
          src++;

        // Skip whitespace and look for "as"
        while (*src && isspace(*src))
          src++;

        const char *alias = NULL;
        if (strncmp(src, "as", 2) == 0) {
          src += 2;
          while (*src && isspace(*src))
            src++;

          // Extract alias identifier
          const char *alias_start = src;
          while (*src && (isalnum(*src) || *src == '_'))
            src++;

          size_t alias_len = src - alias_start;
          char *alias_buf = arena_alloc(arena, alias_len + 1, 1);
          memcpy(alias_buf, alias_start, alias_len);
          alias_buf[alias_len] = '\0';
          alias = alias_buf;
        }

        ImportedModule *import =
            (ImportedModule *)growable_array_push(&imports);
        if (import) {
          import->module_path = module_path;
          import->alias = alias;
          import->scope = NULL; // Will be resolved later
        }
      }
    }
    src++;
  }

  doc->imports = (ImportedModule *)imports.data;
  doc->import_count = imports.count;
}

// Resolve module path relative to current document
const char *resolve_module_path(const char *current_uri,
                                const char *module_path,
                                ArenaAllocator *arena) {
  // Convert URI to path
  const char *current_path = lsp_uri_to_path(current_uri, arena);
  if (!current_path)
    return NULL;

  // Find directory of current file
  const char *last_slash = strrchr(current_path, '/');
  if (!last_slash)
    last_slash = strrchr(current_path, '\\');

  size_t dir_len = last_slash ? (last_slash - current_path + 1) : 0;

  // Build full path
  size_t total_len = dir_len + strlen(module_path) + 4; // +4 for ".lx"
  char *full_path = arena_alloc(arena, total_len, 1);

  if (dir_len > 0) {
    memcpy(full_path, current_path, dir_len);
  }
  strcpy(full_path + dir_len, module_path);

  // Add .lx extension if not present
  if (!strstr(module_path, ".lx")) {
    strcat(full_path, ".lx");
  }

  return lsp_path_to_uri(full_path, arena);
}

// Update parse_imported_module to return the parsed module AST, not just scope
static AstNode *parse_imported_module_ast(LSPServer *server,
                                          const char *module_uri,
                                          BuildConfig *config,
                                          ArenaAllocator *arena) {
  // Check if already opened
  LSPDocument *module_doc = lsp_document_find(server, module_uri);
  if (module_doc && module_doc->ast) {
    return module_doc->ast;
  }

  // Try to read file from disk
  const char *file_path = lsp_uri_to_path(module_uri, arena);
  if (!file_path)
    return NULL;

  FILE *f = fopen(file_path, "r");
  if (!f) {
    fprintf(stderr, "[LSP] Failed to open module: %s\n", file_path);
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *content = arena_alloc(arena, size + 1, 1);
  fread(content, 1, size, f);
  content[size] = '\0';
  fclose(f);

  // Lex the module
  Lexer lexer;
  init_lexer(&lexer, content, arena);

  GrowableArray tokens;
  growable_array_init(&tokens, arena, 256, sizeof(Token));

  Token token;
  while ((token = next_token(&lexer)).type_ != TOK_EOF) {
    Token *slot = (Token *)growable_array_push(&tokens);
    if (slot)
      *slot = token;
  }

  // Parse the module
  AstNode *module_ast = parse(&tokens, arena, config);

  if (!module_ast) {
    fprintf(stderr, "[LSP] Failed to parse imported module: %s\n", file_path);
    return NULL;
  }

  // Extract the first module from the program if it's a program node
  if (module_ast->type == AST_PROGRAM &&
      module_ast->stmt.program.module_count > 0) {
    return module_ast->stmt.program.modules[0];
  }

  return module_ast;
}

// Update resolve_imports to collect module ASTs instead of just scopes
static void resolve_imports(LSPServer *server, LSPDocument *doc,
                            BuildConfig *config,
                            GrowableArray *imported_modules) {
  if (!doc || !doc->imports || doc->import_count == 0)
    return;

  fprintf(stderr, "[LSP] Resolving %zu imports for %s\n", doc->import_count,
          doc->uri);

  for (size_t i = 0; i < doc->import_count; i++) {
    ImportedModule *import = &doc->imports[i];

    // Look up module in registry by name
    const char *resolved_uri = lookup_module(server, import->module_path);

    if (!resolved_uri) {
      fprintf(stderr, "[LSP] Module '%s' not found in registry\n",
              import->module_path);
      continue;
    }

    fprintf(stderr, "[LSP] Resolved '%s' -> %s\n", import->module_path,
            resolved_uri);

    // Parse module and get its AST
    AstNode *module_ast =
        parse_imported_module_ast(server, resolved_uri, config, doc->arena);

    if (module_ast) {
      // Add to the list of modules
      AstNode **slot = (AstNode **)growable_array_push(imported_modules);
      if (slot) {
        *slot = module_ast;
        fprintf(stderr, "[LSP] Successfully loaded module: %s (alias: %s)\n",
                import->module_path, import->alias ? import->alias : "none");
      }
    }
  }
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

  // Initialize module registry
  server->module_registry.entries = NULL;
  server->module_registry.count = 0;
  server->module_registry.capacity = 0;

  return server->documents != NULL;
}

LSPDocument *lsp_document_open(LSPServer *server, const char *uri,
                               const char *content, int version) {
  if (!server || !uri || !content)
    return NULL;

  LSPDocument *existing = lsp_document_find(server, uri);
  if (existing) {
    return existing;
  }

  if (server->document_count >= server->document_capacity) {
    return NULL;
  }

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

  doc->arena = arena_alloc(server->arena, sizeof(ArenaAllocator),
                           alignof(ArenaAllocator));
  arena_allocator_init(doc->arena, 1024 * 1024);

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
      if (server->documents[i]->arena) {
        arena_destroy(server->documents[i]->arena);
      }

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

bool lsp_document_analyze(LSPDocument *doc, LSPServer *server,
                          BuildConfig *config) {
  if (!doc || !doc->needs_reanalysis)
    return true;

  arena_destroy(doc->arena);
  arena_allocator_init(doc->arena, 1024 * 1024);

  error_clear();

  const char *file_path = lsp_uri_to_path(doc->uri, doc->arena);
  if (!file_path) {
    file_path = doc->uri;
  }

  fprintf(stderr, "[LSP] Analyzing document: %s\n", file_path);

  extract_imports(doc, doc->arena);

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

  fprintf(stderr, "[LSP] Lexed %zu tokens\n", doc->token_count);

  doc->ast = parse(&tokens, doc->arena, config);

  fprintf(stderr, "[LSP] Parse result: %s\n", doc->ast ? "success" : "failed");

  // ADD THIS CHECK: If parse failed or has errors, return early with
  // diagnostics
  if (!doc->ast || error_get_count() > 0) {
    fprintf(stderr, "[LSP] Parse has %d errors, skipping typecheck\n",
            error_get_count());
    doc->diagnostics =
        convert_errors_to_diagnostics(&doc->diagnostic_count, doc->arena);
    doc->needs_reanalysis = false;
    return false;
  }

  GrowableArray all_modules;
  growable_array_init(&all_modules, doc->arena, 8, sizeof(AstNode *));

  resolve_imports(server, doc, config, &all_modules);

  AstNode *main_module = doc->ast;
  if (doc->ast->type == AST_PROGRAM &&
      doc->ast->stmt.program.module_count > 0) {
    main_module = doc->ast->stmt.program.modules[0];
  }

  AstNode **main_slot = (AstNode **)growable_array_push(&all_modules);
  if (main_slot) {
    *main_slot = main_module;
  }

  fprintf(stderr, "[LSP] Combined %zu modules for typechecking\n",
          all_modules.count);

  AstNode *combined_program = create_program_node(
      doc->arena, (AstNode **)all_modules.data, all_modules.count, 0, 0);

  if (!combined_program) {
    fprintf(stderr, "[LSP] Failed to create combined program\n");
    doc->needs_reanalysis = false;
    return false;
  }

  Scope global_scope;
  init_scope(&global_scope, NULL, "global", doc->arena);
  global_scope.config = config;
  doc->scope = &global_scope;

  tc_error_init(doc->tokens, doc->token_count, file_path, doc->arena);

  fprintf(stderr, "[LSP] Starting typecheck with %zu modules...\n",
          all_modules.count);

  // ADD ERROR HANDLER: Wrap typecheck in error checking
  bool success = false;

  // Try to typecheck, but catch if it fails catastrophically
  if (combined_program && all_modules.count > 0) {
    success = typecheck(combined_program, &global_scope, doc->arena, config);
  }

  fprintf(stderr, "[LSP] Typecheck result: %s, errors: %d\n",
          success ? "success" : "failed", error_get_count());

  doc->diagnostics =
      convert_errors_to_diagnostics(&doc->diagnostic_count, doc->arena);

  fprintf(stderr, "[LSP] Generated %zu diagnostics\n", doc->diagnostic_count);

  doc->needs_reanalysis = false;

  return success;
}

// ============================================================================
// LSP Features - Diagnostics
// ============================================================================

LSPDiagnostic *convert_errors_to_diagnostics(size_t *diagnostic_count,
                                             ArenaAllocator *arena) {
  if (!diagnostic_count)
    return NULL;

  size_t error_count = error_get_count();

  fprintf(stderr, "[LSP] Converting %zu errors to diagnostics\n", error_count);

  if (error_count == 0) {
    *diagnostic_count = 0;
    return NULL;
  }

  LSPDiagnostic *diagnostics = arena_alloc(
      arena, error_count * sizeof(LSPDiagnostic), alignof(LSPDiagnostic));
  if (!diagnostics) {
    fprintf(stderr, "[LSP] Failed to allocate diagnostics array\n");
    *diagnostic_count = 0;
    return NULL;
  }

  for (size_t i = 0; i < error_count; i++) {
    ErrorInformation *error = error_get_at_index(i);
    if (!error) {
      fprintf(stderr, "[LSP] Warning: error at index %zu is NULL\n", i);
      continue;
    }

    fprintf(stderr, "[LSP] Error %zu: %s at line %d, col %d: %s\n", i,
            error->error_type, error->line, error->col, error->message);

    LSPDiagnostic *diag = &diagnostics[i];

    diag->range.start.line = error->line > 0 ? error->line - 1 : 0;
    diag->range.start.character = error->col > 0 ? error->col - 1 : 0;
    diag->range.end.line = diag->range.start.line;
    diag->range.end.character =
        diag->range.start.character +
        (error->token_length > 0 ? error->token_length : 1);

    if (strstr(error->error_type, "Error")) {
      diag->severity = LSP_DIAGNOSTIC_ERROR;
    } else if (strstr(error->error_type, "Warning")) {
      diag->severity = LSP_DIAGNOSTIC_WARNING;
    } else {
      diag->severity = LSP_DIAGNOSTIC_INFORMATION;
    }

    if (error->help) {
      size_t msg_len = strlen(error->message) + strlen(error->help) + 20;
      char *full_msg = arena_alloc(arena, msg_len, 1);
      snprintf(full_msg, msg_len, "%s\n\nHelp: %s", error->message,
               error->help);
      diag->message = full_msg;
    } else {
      diag->message = arena_strdup(arena, error->message);
    }

    diag->source = "luma-lsp";

    fprintf(stderr, "[LSP] Diagnostic %zu: severity=%d, line=%d, col=%d\n", i,
            diag->severity, diag->range.start.line,
            diag->range.start.character);
  }

  *diagnostic_count = error_count;
  return diagnostics;
}

LSPDiagnostic *lsp_diagnostics(LSPDocument *doc, size_t *diagnostic_count,
                               ArenaAllocator *arena) {
  if (!doc || !diagnostic_count)
    return NULL;

  // Return cached diagnostics if available
  if (doc->diagnostics && doc->diagnostic_count > 0) {
    *diagnostic_count = doc->diagnostic_count;
    return doc->diagnostics;
  }

  // Convert current errors to diagnostics
  return convert_errors_to_diagnostics(diagnostic_count, arena);
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
// LSP Features - Completion
// ============================================================================

LSPCompletionItem *lsp_completion(LSPDocument *doc, LSPPosition position,
                                  size_t *completion_count,
                                  ArenaAllocator *arena) {
  if (!doc || !completion_count)
    return NULL;

  Token *token = lsp_token_at_position(doc, position);

  GrowableArray completions;
  growable_array_init(&completions, arena, 32, sizeof(LSPCompletionItem));

  // Add keyword snippets based on Luma syntax
  const struct {
    const char *label;
    const char *snippet;
    const char *detail;
  } keywords[] = {
      // Top-level declarations
      {"const fn", "const ${1:name} = fn (${2:params}) ${3:Type} {\n\t$0\n};",
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
      for (size_t i = 0; i < current_scope->symbols.count; i++) {
        Symbol *sym = (Symbol *)((char *)current_scope->symbols.data +
                                 i * sizeof(Symbol));

        LSPCompletionItem *item =
            (LSPCompletionItem *)growable_array_push(&completions);
        if (item) {
          item->label = arena_strdup(arena, sym->name);

          // Determine kind based on symbol type
          if (sym->type->type == AST_TYPE_FUNCTION) {
            item->kind = LSP_COMPLETION_FUNCTION;
            // Create function call snippet
            char snippet[512];
            snprintf(snippet, sizeof(snippet), "%s($0)", sym->name);
            item->insert_text = arena_strdup(arena, snippet);
            item->format = LSP_INSERT_FORMAT_SNIPPET;
          } else if (sym->type->type == AST_TYPE_STRUCT) {
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

  // NEW: Add symbols from imported modules
  for (size_t i = 0; i < doc->import_count; i++) {
    ImportedModule *import = &doc->imports[i];
    if (!import->scope)
      continue;

    // Add symbols with prefix (e.g., "string::strlen")
    const char *prefix = import->alias ? import->alias : "module";

    for (size_t j = 0; j < import->scope->symbols.count; j++) {
      Symbol *sym =
          (Symbol *)((char *)import->scope->symbols.data + j * sizeof(Symbol));

      // Only include public symbols
      if (!sym->is_public)
        continue;

      LSPCompletionItem *item =
          (LSPCompletionItem *)growable_array_push(&completions);
      if (item) {
        // Format: "alias::name"
        size_t label_len = strlen(prefix) + strlen(sym->name) + 3;
        char *label = arena_alloc(arena, label_len, 1);
        snprintf(label, label_len, "%s::%s", prefix, sym->name);

        item->label = label;
        item->kind = (sym->type->type == AST_TYPE_FUNCTION)
                         ? LSP_COMPLETION_FUNCTION
                         : LSP_COMPLETION_VARIABLE;
        item->insert_text = arena_strdup(arena, label);
        item->format = LSP_INSERT_FORMAT_PLAIN_TEXT;
        item->detail = type_to_string(sym->type, arena);
        item->documentation = NULL;
        item->sort_text = NULL;
        item->filter_text = NULL;
      }
    }
  }

  *completion_count = completions.count;
  return (LSPCompletionItem *)completions.data;
}

// ============================================================================
// Main Message Handler
// ============================================================================

void lsp_handle_message(LSPServer *server, const char *message) {
  if (!server || !message)
    return;

  // DON'T truncate the log - show more of the message to debug
  fprintf(stderr, "[LSP] Received message: %.500s...\n",
          message); // Changed from 100 to 500

  LSPMethod method = lsp_parse_method(message);
  int request_id = extract_int(message, "id");

  fprintf(stderr, "[LSP] Extracted request_id: %d\n", request_id); // ADD THIS

  ArenaAllocator temp_arena;
  arena_allocator_init(&temp_arena, 64 * 1024);

  switch (method) {
  case LSP_METHOD_INITIALIZE: {
    fprintf(stderr, "[LSP] Handling initialize\n");

    // Only respond if we have a valid ID
    if (request_id >= 0) {
      const char *workspace_uri = extract_string(message, "uri", &temp_arena);
      if (workspace_uri) {
        build_module_registry(server, workspace_uri);
      }

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
    } else {
      fprintf(stderr, "[LSP] ERROR: No valid request_id for initialize!\n");
    }
    break;
  }

  case LSP_METHOD_INITIALIZED:
    fprintf(stderr, "[LSP] Client initialized\n");
    // This is a notification, no response needed
    break;

  case LSP_METHOD_TEXT_DOCUMENT_DID_OPEN: {
    fprintf(stderr, "[LSP] Handling didOpen\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    const char *text = extract_string(message, "text", &temp_arena);
    int version = extract_int(message, "version");

    if (uri && text) {
      fprintf(stderr, "[LSP] Opening document: %s (version %d)\n", uri,
              version);
      fprintf(stderr, "[LSP] Document content length: %zu\n", strlen(text));

      LSPDocument *doc = lsp_document_open(server, uri, text, version);
      if (doc) {
        BuildConfig config = {0};
        config.check_mem = true;
        lsp_document_analyze(doc, server, &config); // ADD server parameter

        size_t diag_count;
        LSPDiagnostic *diagnostics =
            lsp_diagnostics(doc, &diag_count, &temp_arena);

        fprintf(stderr, "[LSP] Sending %zu diagnostics\n", diag_count);

        char params[16384];
        serialize_diagnostics_to_json(uri, diagnostics, diag_count, params,
                                      sizeof(params));

        fprintf(stderr, "[LSP] Diagnostic JSON: %s\n", params);

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

      LSPDocument *doc = lsp_document_find(server, uri);
      if (doc) {
        BuildConfig config = {0};
        config.check_mem = true;
        lsp_document_analyze(doc, server, &config); // ADD server parameter

        size_t diag_count;
        LSPDiagnostic *diagnostics =
            lsp_diagnostics(doc, &diag_count, &temp_arena);

        char params[16384];
        serialize_diagnostics_to_json(uri, diagnostics, diag_count, params,
                                      sizeof(params));
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
