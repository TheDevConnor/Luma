#include "../c_libs/error/error.h"
#include "type.h"

#include <stdio.h>
#include <string.h>

void static_memory_analyzer_init(StaticMemoryAnalyzer *analyzer,
                                 ArenaAllocator *arena) {
  analyzer->arena = arena;
  growable_array_init(&analyzer->allocations, arena, 32,
                      sizeof(StaticAllocation));
}

void static_memory_track_alloc(StaticMemoryAnalyzer *analyzer, size_t line,
                               size_t column, const char *var_name) {
  // Don't track anonymous allocations - let variable declarations handle it
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
    alloc->free_count = 0; // Track number of times freed
    // printf("STATIC: Tracked alloc() for variable '%s' at line %zu:%zu\n",
    // var_name, line, column);
  }
}

void static_memory_track_free(StaticMemoryAnalyzer *analyzer,
                              const char *var_name) {
  if (!var_name || strcmp(var_name, "unknown") == 0) {
    printf("Warning: free() called on unknown variable\n");
    return;
  }

  bool found = false;
  StaticAllocation *target_alloc = NULL;

  // Find the allocation for this variable
  for (size_t i = 0; i < analyzer->allocations.count; i++) {
    StaticAllocation *alloc =
        (StaticAllocation *)((char *)analyzer->allocations.data +
                             i * sizeof(StaticAllocation));
    if (alloc->variable_name && strcmp(alloc->variable_name, var_name) == 0) {
      target_alloc = alloc;
      found = true;
      break;
    }
  }

  if (!found) {
    printf("Warning: free() called on variable '%s' without matching alloc()\n",
           var_name);
    return;
  }

  // Check for double free
  if (target_alloc->has_matching_free) {
    target_alloc->free_count++;
    // printf("ERROR: Double free detected! Variable '%s' freed %d times
    // (originally allocated at line %zu:%zu)\n",
    //        var_name, target_alloc->free_count + 1, target_alloc->line,
    //        target_alloc->column);
    return;
  }

  // Mark as freed for the first time
  target_alloc->has_matching_free = true;
  target_alloc->free_count = 1;
  // printf("STATIC: Tracked free(%s) - matches allocation at line %zu:%zu\n",
  //        var_name, target_alloc->line, target_alloc->column);
}

/**
 * @brief Reports memory leaks and double frees using the error system
 *
 * @param analyzer The static memory analyzer containing allocation data
 * @param arena Arena allocator for error message allocation
 * @param tokens Token array for generating source line context
 * @param token_count Number of tokens
 * @param file_path Source file path
 */
void static_memory_report_leaks(StaticMemoryAnalyzer *analyzer,
                                ArenaAllocator *arena, Token *tokens,
                                int token_count, const char *file_path) {
  size_t leak_count = 0;
  size_t double_free_count = 0;

  for (size_t i = 0; i < analyzer->allocations.count; i++) {
    StaticAllocation *alloc =
        (StaticAllocation *)((char *)analyzer->allocations.data +
                             i * sizeof(StaticAllocation));

    if (alloc->free_count > 1) {
      // Create error for double free
      ErrorInformation error = {0};
      error.error_type = "Double Free";
      error.file_path = file_path;

      // Allocate formatted message in arena
      char *message = arena_alloc(arena, 256, alignof(char));
      snprintf(message, 256, "Variable '%s' was freed %d times",
               alloc->variable_name, alloc->free_count);
      error.message = message;

      error.line = (int)alloc->line;
      error.col = (int)alloc->column;
      error.token_length = (int)strlen(alloc->variable_name);

      // Generate source line context
      error.line_text = generate_line(arena, tokens, token_count, error.line);

      // Add helpful information
      error.label = "Double free detected here";
      error.note = "Memory was already freed previously";
      error.help =
          "Remove the duplicate free() call or check your control flow";

      error_add(error);
      double_free_count++;

    } else if (!alloc->has_matching_free) {
      // Create error for memory leak
      ErrorInformation error = {0};
      error.error_type = "Memory Leak";
      error.file_path = file_path;

      // Allocate formatted message in arena
      char *message = arena_alloc(arena, 256, alignof(char));
      snprintf(message, 256, "Variable '%s' allocated but never freed",
               alloc->variable_name);
      error.message = message;

      error.line = (int)alloc->line;
      error.col = (int)alloc->column;
      error.token_length = (int)strlen(alloc->variable_name);

      // Generate source line context
      error.line_text = generate_line(arena, tokens, token_count, error.line);

      // Add helpful information
      error.label = "Memory allocated here";
      error.note = "This allocation has no corresponding free()";
      error.help = "Add a free() call before the variable goes out of scope";

      error_add(error);
      leak_count++;
    }
  }

  // Optional: Print summary if you want to keep it
  if (leak_count == 0 && double_free_count == 0) {
    // Silence is golden - no issues found
  } else {
    // The detailed errors will be shown when error_report() is called
    // You could add a summary here if desired
    printf("\nMemory Analysis Summary:\n");
    if (leak_count > 0) {
      printf("- Found %zu potential memory leak(s)\n", leak_count);
    }
    if (double_free_count > 0) {
      printf("- Found %zu double free error(s)\n", double_free_count);
    }
    printf("See detailed error report above.\n");
  }
}

/**
 * @brief Alternative version that only reports errors without summary
 *
 * @param analyzer The static memory analyzer containing allocation data
 * @param arena Arena allocator for error message allocation
 * @param tokens Token array for generating source line context
 * @param token_count Number of tokens
 * @param file_path Source file path
 * @return Number of memory issues found
 */
int static_memory_check_and_report(StaticMemoryAnalyzer *analyzer,
                                   ArenaAllocator *arena, Token *tokens,
                                   int token_count, const char *file_path) {
  int issues_found = 0;

  for (size_t i = 0; i < analyzer->allocations.count; i++) {
    StaticAllocation *alloc =
        (StaticAllocation *)((char *)analyzer->allocations.data +
                             i * sizeof(StaticAllocation));

    if (alloc->free_count > 1) {
      ErrorInformation error = {
          .error_type = "Double Free",
          .file_path = file_path,
          .line = (int)alloc->line,
          .col = (int)alloc->column,
          .token_length = (int)strlen(alloc->variable_name),
          // .label = "Double free detected here",
          .note = "Memory was already freed previously",
          .help =
              "Remove the duplicate free() call or check your control flow"};

      // Allocate and format message
      char *message = arena_alloc(arena, 256, alignof(char));
      snprintf(message, 256, "Variable '%s' was freed %d times",
               alloc->variable_name, alloc->free_count);
      error.message = message;

      // Generate source line context
      error.line_text = generate_line(arena, tokens, token_count, error.line);

      error_add(error);
      issues_found++;

    } else if (!alloc->has_matching_free) {
      ErrorInformation error = {
          .error_type = "Memory Leak",
          .file_path = file_path,
          .line = (int)alloc->line,
          .col = (int)alloc->column,
          .token_length = (int)strlen(alloc->variable_name),
          // .label = "Memory allocated here",
          .note = "This allocation has no corresponding free()",
          .help = "Add a free() call before the variable goes out of scope"};

      // Allocate and format message
      char *message = arena_alloc(arena, 256, alignof(char));
      snprintf(message, 256, "Variable '%s' allocated but never freed",
               alloc->variable_name);
      error.message = message;

      // Generate source line context
      error.line_text = generate_line(arena, tokens, token_count, error.line);

      error_add(error);
      issues_found++;
    }
  }

  return issues_found;
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
