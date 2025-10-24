#include "../c_libs/error/error.h"
#include "type.h"
#include <stdio.h>
#include <string.h>

void static_memory_analyzer_init(StaticMemoryAnalyzer *analyzer,
                                 ArenaAllocator *arena) {
  analyzer->arena = arena;
  analyzer->skip_memory_tracking = false;
  growable_array_init(&analyzer->allocations, arena, 32,
                      sizeof(StaticAllocation));
}

void static_memory_track_alloc(StaticMemoryAnalyzer *analyzer, size_t line,
                               size_t column, const char *var_name,
                               const char *function_name, Token *tokens,
                               size_t token_count, const char *file_path) {
  if (!var_name || strcmp(var_name, "anonymous") == 0) {
    return;
  }

  StaticAllocation *alloc =
      (StaticAllocation *)growable_array_push(&analyzer->allocations);
  if (alloc) {
    alloc->line = line;
    alloc->column = column;
    alloc->variable_name = arena_strdup(analyzer->arena, var_name);
    alloc->has_matching_free = false;
    alloc->free_count = 0;
    alloc->use_after_free_count = 0;
    alloc->reported = false;
    alloc->function_name =
        function_name ? arena_strdup(analyzer->arena, function_name) : NULL;

    alloc->file_path =
        file_path ? arena_strdup(analyzer->arena, file_path) : NULL;

    growable_array_init(&alloc->aliases, analyzer->arena, 4, sizeof(char *));
  }
}

static StaticAllocation *find_allocation_by_name(StaticMemoryAnalyzer *analyzer,
                                                 const char *var_name,
                                                 const char *current_function) {
  if (!var_name)
    return NULL;

  // Search in reverse order to find the most recent allocation
  for (size_t i = analyzer->allocations.count; i > 0; i--) {
    StaticAllocation *alloc =
        (StaticAllocation *)((char *)analyzer->allocations.data +
                             (i - 1) * sizeof(StaticAllocation));

    // Match both variable name AND function scope
    if (alloc->variable_name && strcmp(alloc->variable_name, var_name) == 0) {
      // If we're tracking function names, ensure they match
      if (alloc->function_name && current_function) {
        if (strcmp(alloc->function_name, current_function) == 0) {
          return alloc;
        }
      } else {
        // Legacy behavior: match by name only
        return alloc;
      }
    }

    // Also check aliases
    if (alloc->aliases.data) {
      for (size_t j = 0; j < alloc->aliases.count; j++) {
        char **alias =
            (char **)((char *)alloc->aliases.data + j * sizeof(char *));
        if (*alias && strcmp(*alias, var_name) == 0) {
          if (alloc->function_name && current_function) {
            if (strcmp(alloc->function_name, current_function) == 0) {
              return alloc;
            }
          } else {
            return alloc;
          }
        }
      }
    }
  }

  return NULL;
}

void static_memory_track_free(StaticMemoryAnalyzer *analyzer,
                              const char *var_name, const char *function_name) {
  if (analyzer->skip_memory_tracking) {
    return;
  }

  StaticAllocation *alloc =
      find_allocation_by_name(analyzer, var_name, function_name);

  if (!alloc) {
    return;
  }

  if (alloc->has_matching_free) {
    alloc->free_count++;
    return;
  }

  alloc->has_matching_free = true;
  alloc->free_count = 1;
}

// NEW: Check if a variable is accessing freed memory
bool static_memory_check_use_after_free(StaticMemoryAnalyzer *analyzer,
                                        const char *var_name, size_t line,
                                        size_t column, ArenaAllocator *arena,
                                        Token *tokens, int token_count,
                                        const char *file_path,
                                        const char *function_name) {
  if (!var_name)
    return true;

  StaticAllocation *alloc =
      find_allocation_by_name(analyzer, var_name, function_name);

  if (alloc && alloc->has_matching_free) {
    // Use after free detected!
    ErrorInformation error = {0};
    error.error_type = "Use After Free";
    error.file_path = file_path;
    error.line = (int)line;
    error.col = (int)column;
    error.token_length = (int)strlen(var_name);

    char *message = arena_alloc(arena, 256, alignof(char));
    snprintf(message, 256,
             "Variable '%s' used after free (originally allocated at line %zu)",
             var_name, alloc->line);
    error.message = message;

    error.line_text = generate_line(arena, g_tokens, g_token_count, error.line);
    error.note = "Memory was freed earlier in this scope";
    error.help = "Remove the use after free or restructure your code";

    error_add(error);
    return false;
  }

  return true;
}

void static_memory_track_alias(StaticMemoryAnalyzer *analyzer,
                               const char *new_var, const char *source_var) {
  // fprintf(stderr, "DEBUG: track_alias called: new='%s', source='%s'\n",
  // new_var ? new_var : "NULL",
  // source_var ? source_var : "NULL");

  if (!new_var || !source_var || strcmp(new_var, source_var) == 0) {
    // fprintf(stderr, "DEBUG: Skipping alias (invalid params or same var)\n");
    return;
  }

  StaticAllocation *source_alloc =
      find_allocation_by_name(analyzer, source_var, NULL);

  if (!source_alloc) {
    // fprintf(stderr, "DEBUG: Source allocation not found for '%s'\n",
    // source_var);
    return;
  }

  // fprintf(stderr, "DEBUG: Found source allocation, adding alias '%s'\n",
  // new_var);

  // Add new_var as an alias to the same allocation
  char **alias_slot = (char **)growable_array_push(&source_alloc->aliases);
  if (alias_slot) {
    *alias_slot = arena_strdup(analyzer->arena, new_var);
    // fprintf(stderr, "DEBUG: Successfully added alias. Total aliases: %zu\n",
    // source_alloc->aliases.count);
  } else {
    // fprintf(stderr, "DEBUG: FAILED to add alias (growable_array_push
    // failed)\n");
  }
}

StaticMemoryAnalyzer *get_static_analyzer(Scope *scope) {
  Scope *current = scope;
  while (current) {
    if (current->memory_analyzer) {
      return current->memory_analyzer;
    }
    current = current->parent;
  }
  return NULL;
}

const char *extract_variable_name_from_free(AstNode *free_expr) {
  AstNode *ptr_expr = free_expr->expr.free.ptr;

  if (ptr_expr && ptr_expr->type == AST_EXPR_IDENTIFIER) {
    return ptr_expr->expr.identifier.name;
  }

  return "unknown";
}

int static_memory_check_and_report(StaticMemoryAnalyzer *analyzer,
                                   ArenaAllocator *arena) {
  int issues_found = 0;

  for (size_t i = 0; i < analyzer->allocations.count; i++) {
    StaticAllocation *alloc =
        (StaticAllocation *)((char *)analyzer->allocations.data +
                             i * sizeof(StaticAllocation));

    if (alloc->reported) {
      continue;
    }

    if (alloc->free_count > 1) {
      // Double free - need to re-read the file
      const char *source = read_file(alloc->file_path);
      if (!source)
        continue;

      Lexer temp_lexer;
      init_lexer(&temp_lexer, source, arena);

      GrowableArray temp_tokens;
      growable_array_init(&temp_tokens, arena, 100, sizeof(Token));

      Token tk;
      while ((tk = next_token(&temp_lexer)).type_ != TOK_EOF) {
        Token *slot = (Token *)growable_array_push(&temp_tokens);
        if (slot)
          *slot = tk;
      }

      ErrorInformation error = {0};
      error.error_type = "Double Free";
      error.file_path = alloc->file_path;
      error.line = (int)alloc->line;
      error.col = (int)alloc->column;
      error.token_length = (int)strlen(alloc->variable_name);
      error.note = "This memory has been freed multiple times";
      error.help = "Remove duplicate free() calls";

      char *message = arena_alloc(arena, 512, alignof(char));
      snprintf(message, 512,
               "Variable '%s' freed %d times (should only be freed once)",
               alloc->variable_name, alloc->free_count);
      error.message = message;
      error.line_text = generate_line(arena, (Token *)temp_tokens.data,
                                      temp_tokens.count, error.line);

      free((void *)source);
      error_add(error);
      issues_found++;

    } else if (!alloc->has_matching_free) {
      // Memory leak - re-read the file
      const char *source = read_file(alloc->file_path);
      if (!source)
        continue;

      Lexer temp_lexer;
      init_lexer(&temp_lexer, source, arena);

      GrowableArray temp_tokens;
      growable_array_init(&temp_tokens, arena, 100, sizeof(Token));

      Token tk;
      while ((tk = next_token(&temp_lexer)).type_ != TOK_EOF) {
        Token *slot = (Token *)growable_array_push(&temp_tokens);
        if (slot)
          *slot = tk;
      }

      ErrorInformation error = {0};
      error.error_type = "Memory Leak";
      error.file_path = alloc->file_path;
      error.line = (int)alloc->line;
      error.col = (int)alloc->column;
      error.token_length = (int)strlen(alloc->variable_name);
      error.note = "This allocation has no corresponding free()";
      error.help = "Add a free() call before all variables go out of scope";

      char *message = arena_alloc(arena, 512, alignof(char));

      if (alloc->aliases.count > 0) {
        char alias_list[256] = {0};
        size_t offset = 0;
        for (size_t j = 0; j < alloc->aliases.count && offset < 250; j++) {
          char **alias =
              (char **)((char *)alloc->aliases.data + j * sizeof(char *));
          if (*alias) {
            offset += snprintf(alias_list + offset, 256 - offset, "%s%s",
                               j > 0 ? ", " : "", *alias);
          }
        }
        snprintf(message, 512,
                 "Variable '%s' (aliased by: %s) allocated but never freed",
                 alloc->variable_name, alias_list);
      } else {
        snprintf(message, 512, "Variable '%s' allocated but never freed",
                 alloc->variable_name);
      }

      error.message = message;
      error.line_text = generate_line(arena, (Token *)temp_tokens.data,
                                      temp_tokens.count, error.line);

      free((void *)source);
      error_add(error);
      issues_found++;
    }

    alloc->reported = true;
  }

  return issues_found;
}
