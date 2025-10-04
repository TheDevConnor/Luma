#include "../c_libs/error/error.h"
#include "type.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

Token *g_tokens = NULL;
int g_token_count = 0;
const char *g_file_path = NULL;
ArenaAllocator *g_arena = NULL;

/**
 * @brief Set global context for error reporting
 * Call this once at the start of typechecking
 */
void tc_error_init(Token *tokens, int token_count, const char *file_path,
                   ArenaAllocator *arena) {
  g_tokens = tokens;
  g_token_count = token_count;
  g_file_path = file_path;
  g_arena = arena;
}

/**
 * @brief Simple error reporting - replaces fprintf(stderr, ...)
 * Usage: tc_error(node, "Type Error", "Variable '%s' not found", var_name);
 */
void tc_error(AstNode *node, const char *error_type, const char *format, ...) {
  char *message = arena_alloc(g_arena, 512, alignof(char));

  va_list args;
  va_start(args, format);
  vsnprintf(message, 512, format, args);
  va_end(args);

  ErrorInformation error = {0};
  error.error_type = error_type;
  error.file_path = g_file_path;
  error.message = message;
  error.line = (int)node->line;
  error.col = (int)node->column;
  error.token_length = 1;

  if (g_tokens && g_token_count > 0) {
    error.line_text =
        generate_line(g_arena, g_tokens, g_token_count, error.line);
  }

  error_add(error);
}

/**
 * @brief Error with help message
 */
void tc_error_help(AstNode *node, const char *error_type, const char *help,
                   const char *format, ...) {
  char *message = arena_alloc(g_arena, 512, alignof(char));

  va_list args;
  va_start(args, format);
  vsnprintf(message, 512, format, args);
  va_end(args);

  ErrorInformation error = {0};
  error.error_type = error_type;
  error.file_path = g_file_path;
  error.message = message;
  error.line = (int)node->line;
  error.col = (int)node->column;
  error.token_length = 1;
  error.help = help;

  if (g_tokens && g_token_count > 0) {
    error.line_text =
        generate_line(g_arena, g_tokens, g_token_count, error.line);
  }

  error_add(error);
}

/**
 * @brief Error with identifier highlighting
 */
void tc_error_id(AstNode *node, const char *identifier, const char *error_type,
                 const char *format, ...) {
  char *message = arena_alloc(g_arena, 512, alignof(char));

  va_list args;
  va_start(args, format);
  vsnprintf(message, 512, format, args);
  va_end(args);

  ErrorInformation error = {0};
  error.error_type = error_type;
  error.file_path = g_file_path;
  error.message = message;
  error.line = (int)node->line;
  error.col = (int)node->column;
  error.token_length = identifier ? (int)strlen(identifier) : 1;

  if (g_tokens && g_token_count > 0) {
    error.line_text =
        generate_line(g_arena, g_tokens, g_token_count, error.line);
  }

  error_add(error);
}
