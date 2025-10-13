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

bool lsp_document_analyze(LSPDocument *doc, LSPServer *server,
                          BuildConfig *config) {
  if (!doc || !doc->needs_reanalysis)
    return true;

  // NEW: Save import scopes BEFORE destroying arena (they live in server arena after successful typecheck)
  Scope **saved_scopes = NULL;
  size_t saved_import_count = doc->import_count;
  if (saved_import_count > 0 && doc->imports) {
    // Allocate in server arena (persistent)
    saved_scopes = arena_alloc(server->arena, saved_import_count * sizeof(Scope *), alignof(Scope *));
    for (size_t i = 0; i < saved_import_count; i++) {
      saved_scopes[i] = doc->imports[i].scope;
    }
  }

  arena_destroy(doc->arena);
  arena_allocator_init(doc->arena, 1024 * 1024);

  error_clear();

  const char *file_path = lsp_uri_to_path(doc->uri, doc->arena);
  if (!file_path) {
    file_path = doc->uri;
  }

  fprintf(stderr, "[LSP] Analyzing document: %s\n", file_path);

  extract_imports(doc, doc->arena);

  // NEW: Restore saved scopes to the newly created imports
  if (saved_scopes && doc->import_count == saved_import_count) {
    for (size_t i = 0; i < doc->import_count; i++) {
      doc->imports[i].scope = saved_scopes[i];
      fprintf(stderr, "[LSP] Restored scope for import '%s'\n", doc->imports[i].module_path);
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
    doc->diagnostics =
        convert_errors_to_diagnostics(&doc->diagnostic_count, doc->arena);
    doc->needs_reanalysis = false;
    return false;
  }

  GrowableArray all_modules;
  growable_array_init(&all_modules, doc->arena, 8, sizeof(AstNode *));

  // Resolve and collect imported modules
  for (size_t i = 0; i < doc->import_count; i++) {
    ImportedModule *import = &doc->imports[i];
    const char *resolved_uri = lookup_module(server, import->module_path);

    if (!resolved_uri) {
      fprintf(stderr, "[LSP] Module '%s' not found in registry\n",
              import->module_path);
      continue;
    }

    fprintf(stderr, "[LSP] Resolved '%s' -> %s\n", import->module_path,
            resolved_uri);

    AstNode *module_ast =
        parse_imported_module_ast(server, resolved_uri, config, doc->arena);

    if (module_ast) {
      // Add to all_modules
      AstNode **slot = (AstNode **)growable_array_push(&all_modules);
      if (slot) {
        *slot = module_ast;
      }
    }
  }

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

  // Use SERVER arena for global scope so it persists across document analyses
  Scope global_scope;
  init_scope(&global_scope, NULL, "global", server->arena);
  global_scope.config = config;
  doc->scope = &global_scope;

  tc_error_init(doc->tokens, doc->token_count, file_path, doc->arena);

  fprintf(stderr, "[LSP] Starting typecheck with %zu modules...\n",
          all_modules.count);

  bool success = false;

  if (combined_program && all_modules.count > 0) {
    success = typecheck(combined_program, &global_scope, server->arena, config);
  }

  fprintf(stderr, "[LSP] Typecheck result: %s, errors: %d\n",
          success ? "success" : "failed", error_get_count());

  // Link module scopes EVEN IF typecheck failed
  // The imported modules may have been successfully typechecked even if main module has errors
  for (size_t i = 0; i < doc->import_count; i++) {
    ImportedModule *import = &doc->imports[i];

    // Find the corresponding module AST node from imported_modules
    for (size_t j = 0; j < all_modules.count - 1; j++) { // -1 to skip main module
      AstNode *module_ast = ((AstNode **)all_modules.data)[j];

      if (module_ast->type == AST_PREPROCESSOR_MODULE) {
        const char *module_file_name = module_ast->preprocessor.module.name;

        if (strcmp(module_file_name, import->module_path) == 0) {
          // Found the matching module - extract its scope
          Scope *module_scope = (Scope *)module_ast->preprocessor.module.scope;
          
          // Only update if we got a valid scope
          if (module_scope) {
            import->scope = module_scope;
          }
          // If module_scope is NULL, keep the saved scope

          fprintf(stderr,
                  "[LSP] Linked import '%s' (alias: %s) to scope with %zu symbols\n",
                  import->module_path, 
                  import->alias ? import->alias : "none",
                  import->scope ? import->scope->symbols.count : 0);
          break;
        }
      }
    }
  }

  doc->diagnostics =
      convert_errors_to_diagnostics(&doc->diagnostic_count, doc->arena);

  fprintf(stderr, "[LSP] Generated %zu diagnostics\n", doc->diagnostic_count);

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