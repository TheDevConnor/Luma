#ifdef _WIN32
#include <windows.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#include "lsp.h"

// Extract @module declaration from file content
const char *extract_module_name(const char *content, ArenaAllocator *arena) {
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
void scan_file_for_module(LSPServer *server, const char *file_uri,
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
}

// Recursively scan directory for .lx files
void scan_directory_recursive(LSPServer *server, const char *dir_path,
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
void build_module_registry(LSPServer *server, const char *workspace_uri) {
  ArenaAllocator temp_arena;
  arena_allocator_init(&temp_arena, 64 * 1024);

  const char *workspace_path = lsp_uri_to_path(workspace_uri, &temp_arena);
  if (!workspace_path) {
    arena_destroy(&temp_arena);
    return;
  }

  scan_directory_recursive(server, workspace_path, &temp_arena);
  arena_destroy(&temp_arena);
}

// Look up module in registry
const char *lookup_module(LSPServer *server, const char *module_name) {
  for (size_t i = 0; i < server->module_registry.count; i++) {
    if (strcmp(server->module_registry.entries[i].module_name, module_name) ==
        0) {
      return server->module_registry.entries[i].file_uri;
    }
  }

  return NULL;
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
AstNode *parse_imported_module_ast(LSPServer *server, const char *module_uri,
                                   BuildConfig *config, ArenaAllocator *arena) {
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
    return NULL;
  }

  // Extract the first module from the program if it's a program node
  if (module_ast->type == AST_PROGRAM &&
      module_ast->stmt.program.module_count > 0) {
    return module_ast->stmt.program.modules[0];
  }

  return module_ast;
}

// Update resolve_imports to store module scopes from the typechecker
void resolve_imports(LSPServer *server, LSPDocument *doc, BuildConfig *config,
                     GrowableArray *imported_modules) {
  if (!doc || !doc->imports || doc->import_count == 0)
    return;

  for (size_t i = 0; i < doc->import_count; i++) {
    ImportedModule *import = &doc->imports[i];

    // Look up module in registry by name
    const char *resolved_uri = lookup_module(server, import->module_path);

    if (!resolved_uri) {
      continue;
    }

    // Parse module and get its AST
    AstNode *module_ast =
        parse_imported_module_ast(server, resolved_uri, config, doc->arena);

    if (module_ast) {
      // Add to the list of modules
      AstNode **slot = (AstNode **)growable_array_push(imported_modules);
      if (slot) {
        *slot = module_ast;
      }
    }
  }
}
