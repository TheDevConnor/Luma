#include "../c_libs/error/error.h"
#include "lsp.h"

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

// NEW: Helper to recursively collect all module dependencies
static void collect_all_module_deps(LSPServer *server, const char *module_uri,
                                    BuildConfig *config, ArenaAllocator *arena,
                                    GrowableArray *all_modules,
                                    GrowableArray *visited_uris) {
  // Check if already visited (prevent cycles)
  for (size_t i = 0; i < visited_uris->count; i++) {
    const char *visited = ((const char **)visited_uris->data)[i];
    if (strcmp(visited, module_uri) == 0) {
      fprintf(stderr, "[LSP] Skipping already visited module: %s\n",
              module_uri);
      return; // Already processed
    }
  }

  // Mark as visited
  const char **visited_slot = (const char **)growable_array_push(visited_uris);
  if (visited_slot) {
    *visited_slot = arena_strdup(arena, module_uri);
  }

  fprintf(stderr, "[LSP] Collecting dependencies for: %s\n", module_uri);

  // Parse the module
  AstNode *module_ast =
      parse_imported_module_ast(server, module_uri, config, arena);

  if (!module_ast) {
    fprintf(stderr, "[LSP] Failed to parse module: %s\n", module_uri);
    return;
  }

  // Extract imports from this module's content
  const char *file_path = lsp_uri_to_path(module_uri, arena);
  if (!file_path) {
    fprintf(stderr, "[LSP] Failed to convert URI to path: %s\n", module_uri);
    return;
  }

  FILE *f = fopen(file_path, "r");
  if (!f) {
    fprintf(stderr, "[LSP] Failed to open file: %s\n", file_path);
    return;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *content = arena_alloc(arena, size + 1, 1);
  fread(content, 1, size, f);
  content[size] = '\0';
  fclose(f);

  // Create temporary document to extract imports
  LSPDocument temp_doc = {0};
  temp_doc.content = content;
  temp_doc.arena = arena;

  extract_imports(&temp_doc, arena);

  fprintf(stderr, "[LSP] Module %s has %zu imports\n", module_uri,
          temp_doc.import_count);

  // Recursively process each import
  for (size_t i = 0; i < temp_doc.import_count; i++) {
    const char *imported_module_path = temp_doc.imports[i].module_path;
    const char *resolved_uri = lookup_module(server, imported_module_path);

    if (resolved_uri) {
      fprintf(stderr, "[LSP] Recursively processing import: %s -> %s\n",
              imported_module_path, resolved_uri);
      // Recurse!
      collect_all_module_deps(server, resolved_uri, config, arena, all_modules,
                              visited_uris);
    } else {
      fprintf(stderr, "[LSP] Warning: Could not resolve import '%s' in %s\n",
              imported_module_path, module_uri);
    }
  }

  // Add this module to the list (after processing its dependencies)
  AstNode **slot = (AstNode **)growable_array_push(all_modules);
  if (slot) {
    *slot = module_ast;
    fprintf(stderr, "[LSP] Added module to list: %s (total: %zu)\n", module_uri,
            all_modules->count);
  }
}

bool lsp_document_analyze(LSPDocument *doc, LSPServer *server,
                          BuildConfig *config) {
  if (!doc || !doc->needs_reanalysis)
    return true;

  // Save import scopes BEFORE destroying arena
  Scope **saved_scopes = NULL;
  size_t saved_import_count = doc->import_count;
  if (saved_import_count > 0 && doc->imports) {
    saved_scopes = arena_alloc(
        server->arena, saved_import_count * sizeof(Scope *), alignof(Scope *));
    for (size_t i = 0; i < saved_import_count; i++) {
      saved_scopes[i] = doc->imports[i].scope;
    }
  }

  // ALSO save the last successful scope
  Scope *last_successful_scope = doc->scope;

  arena_destroy(doc->arena);
  arena_allocator_init(doc->arena, 1024 * 1024);

  error_clear();

  const char *file_path = lsp_uri_to_path(doc->uri, doc->arena);
  if (!file_path) {
    file_path = doc->uri;
  }

  fprintf(stderr, "[LSP] Analyzing document: %s\n", file_path);

  extract_imports(doc, doc->arena);

  // Restore saved scopes
  if (saved_scopes && doc->import_count == saved_import_count) {
    for (size_t i = 0; i < doc->import_count; i++) {
      doc->imports[i].scope = saved_scopes[i];
    }
  }

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

  if (!doc->ast || error_get_count() > 0) {
    fprintf(stderr, "[LSP] Parse has %d errors, skipping typecheck\n",
            error_get_count());

    // PRESERVE the last successful scope for completions
    if (last_successful_scope) {
      fprintf(stderr,
              "[LSP] Preserving last successful scope for completions\n");
      doc->scope = last_successful_scope;
    } else {
      doc->scope = NULL;
    }

    doc->diagnostics =
        convert_errors_to_diagnostics(&doc->diagnostic_count, doc->arena);
    doc->needs_reanalysis = false;
    return false;
  }

  // NEW: Recursively collect ALL module dependencies (transitive closure)
  GrowableArray all_modules;
  growable_array_init(&all_modules, doc->arena, 16, sizeof(AstNode *));

  GrowableArray visited_uris;
  growable_array_init(&visited_uris, doc->arena, 16, sizeof(const char *));

  // Collect all transitive dependencies
  for (size_t i = 0; i < doc->import_count; i++) {
    ImportedModule *import = &doc->imports[i];
    const char *resolved_uri = lookup_module(server, import->module_path);

    if (resolved_uri) {
      collect_all_module_deps(server, resolved_uri, config, doc->arena,
                              &all_modules, &visited_uris);
    }
  }

  // Add the main module last (so it can see all dependencies)
  AstNode *main_module = doc->ast;
  if (doc->ast->type == AST_PROGRAM &&
      doc->ast->stmt.program.module_count > 0) {
    main_module = doc->ast->stmt.program.modules[0];
  }

  AstNode **main_slot = (AstNode **)growable_array_push(&all_modules);
  if (main_slot) {
    *main_slot = main_module;
  }

  // Create combined program with all modules
  AstNode *combined_program = create_program_node(
      doc->arena, (AstNode **)all_modules.data, all_modules.count, 0, 0);

  if (!combined_program) {
    // Preserve last scope on error
    if (last_successful_scope) {
      doc->scope = last_successful_scope;
    } else {
      doc->scope = NULL;
    }
    doc->needs_reanalysis = false;
    return false;
  }

  // Use SERVER arena for global scope (persists across analyses)
  Scope *global_scope =
      arena_alloc(server->arena, sizeof(Scope), alignof(Scope));
  if (!global_scope) {
    // Preserve last scope on error
    if (last_successful_scope) {
      doc->scope = last_successful_scope;
    } else {
      doc->scope = NULL;
    }
    doc->needs_reanalysis = false;
    return false;
  }

  init_scope(global_scope, NULL, "global", server->arena);
  global_scope->config = config;
  doc->scope = global_scope;

  tc_error_init(doc->tokens, doc->token_count, file_path, doc->arena);

  fprintf(stderr, "[LSP] Starting typecheck with %zu modules...\n",
          all_modules.count);

  bool success = false;

  if (combined_program && all_modules.count > 0) {
    success = typecheck(combined_program, global_scope, server->arena, config);
  }

  fprintf(stderr, "[LSP] Typecheck result: %s, errors: %d\n",
          success ? "success" : "failed", error_get_count());

  if (!success) {
    fprintf(stderr,
            "[LSP] Typecheck failed, preserving last successful scope\n");
    // On typecheck failure, preserve the last successful scope if we have one
    if (last_successful_scope) {
      doc->scope = last_successful_scope;
    } else {
      doc->scope = NULL;
    }
  }

  // Link module scopes ONLY IF typecheck succeeded
  if (success) {
    for (size_t i = 0; i < doc->import_count; i++) {
      ImportedModule *import = &doc->imports[i];

      // Find the corresponding module AST node
      for (size_t j = 0; j < all_modules.count - 1; j++) { // -1 to skip main
        AstNode *module_ast = ((AstNode **)all_modules.data)[j];

        if (module_ast->type == AST_PREPROCESSOR_MODULE) {
          const char *module_file_name = module_ast->preprocessor.module.name;

          if (strcmp(module_file_name, import->module_path) == 0) {
            Scope *module_scope =
                (Scope *)module_ast->preprocessor.module.scope;

            if (module_scope) {
              import->scope = module_scope;
            }
            break;
          }
        }
      }
    }
  }

  doc->diagnostics =
      convert_errors_to_diagnostics(&doc->diagnostic_count, doc->arena);

  doc->needs_reanalysis = false;

  return success;
}

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
