#include "lsp.h"

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

LSPCompletionItem *lsp_completion(LSPDocument *doc, LSPPosition position,
                                  size_t *completion_count,
                                  ArenaAllocator *arena) {
  fprintf(stderr,
          "[LSP] lsp_completion called: doc=%p, position=(%d,%d), arena=%p\n",
          (void *)doc, position.line, position.character, (void *)arena);

  if (!doc || !completion_count) {
    fprintf(stderr,
            "[LSP] lsp_completion early return: doc=%p, completion_count=%p\n",
            (void *)doc, (void *)completion_count);
    return NULL;
  }

  Token *token = lsp_token_at_position(doc, position);
  fprintf(stderr, "[LSP] Token at position: %p\n", (void *)token);

  GrowableArray completions;
  growable_array_init(&completions, arena, 32, sizeof(LSPCompletionItem));
  fprintf(stderr, "[LSP] Initialized completions array\n");

  // Add keyword snippets based on Luma syntax
  const struct {
    const char *label;
    const char *snippet;
    const char *detail;
  } keywords[] = {
      // Top-level declarations
      {"const fn", "const ${1:name} = fn (${2:params}) ${3:Type} {\n\t$0\n}",
       "Function declaration (Private by default)"},
      {"pub const fn",
       "pub const ${1:name} = fn (${2:params}) ${3:Type} {\n\t$0\n}",
       "Public Function declaration"},
      {"priv const fn",
       "priv const ${1:name} = fn (${2:params}) ${3:Type} {\n\t$0\n}",
       "Private Function declaration"},

      {"const struct",
       "const ${1:Name} = struct {\n\t${2:field}: ${3:Type}$0,\n};",
       "Struct definition"},
      {"const enum", "const ${1:Name} = enum {\n\t${2:Variant}$0,\n};",
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

  fprintf(stderr, "[LSP] Added %zu keyword completions\n", completions.count);

  // Add symbols from scope (variables, functions, etc.)
  fprintf(stderr, "[LSP] Checking document scope: %p\n", (void *)doc->scope);
  if (doc->scope) {
    Scope *current_scope = doc->scope;
    int scope_depth = 0;
    while (current_scope) {
      fprintf(stderr, "[LSP] Scope depth %d: symbols.data=%p, count=%zu\n",
              scope_depth, (void *)current_scope->symbols.data,
              current_scope->symbols.count);

      // SAFETY CHECK: Validate scope has valid data
      if (current_scope->symbols.data && current_scope->symbols.count > 0) {
        for (size_t i = 0; i < current_scope->symbols.count; i++) {
          Symbol *sym = (Symbol *)((char *)current_scope->symbols.data +
                                   i * sizeof(Symbol));

          // SAFETY CHECK: Validate symbol has valid name and type
          if (!sym || !sym->name || !sym->type) {
            continue;
          }

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
      }
      scope_depth++;
      current_scope = current_scope->parent;
    }
    fprintf(stderr, "[LSP] Finished adding scope symbols, total depth: %d\n",
            scope_depth);
  }

  fprintf(stderr, "[LSP] Checking %zu imports for completions\n",
          doc->import_count);

  // Add imported module symbols
  if (doc->imports && doc->import_count > 0) {
    for (size_t i = 0; i < doc->import_count; i++) {
      ImportedModule *import = &doc->imports[i];

      fprintf(stderr,
              "[LSP] Import %zu: module_path='%s', alias='%s', scope=%p\n", i,
              import->module_path ? import->module_path : "NULL",
              import->alias ? import->alias : "NULL", (void *)import->scope);

      // CRITICAL SAFETY CHECKS
      if (!import->scope) {
        fprintf(stderr, "[LSP] Skipping import - no scope\n");
        continue;
      }

      // Validate scope structure
      if (!import->scope->symbols.data) {
        fprintf(stderr, "[LSP] Skipping import - scope has no symbol data\n");
        continue;
      }

      fprintf(stderr, "[LSP] Import scope has %zu symbols\n",
              import->scope->symbols.count);

      // Add symbols with prefix (e.g., "string::strlen")
      const char *prefix = import->alias ? import->alias : "module";

      for (size_t j = 0; j < import->scope->symbols.count; j++) {
        Symbol *sym = (Symbol *)((char *)import->scope->symbols.data +
                                 j * sizeof(Symbol));

        // SAFETY CHECK: Validate symbol
        if (!sym || !sym->name || !sym->type) {
          fprintf(stderr, "[LSP]   Symbol %zu: INVALID (skipping)\n", j);
          continue;
        }

        fprintf(stderr, "[LSP]   Symbol %zu: '%s', is_public=%d\n", j,
                sym->name, sym->is_public);

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
  }

  *completion_count = completions.count;
  fprintf(stderr, "[LSP] Returning %zu completion items\n", *completion_count);
  fflush(stderr); // CRITICAL: Ensure logs are written before return
  return (LSPCompletionItem *)completions.data;
}
