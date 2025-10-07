/**
 * @file error.c
 * @brief Implementation of error reporting and diagnostics functions.
 *
 * This module manages an internal list of errors, supports generating
 * source line context from tokens, and prints detailed error reports
 * with color highlighting.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../lexer/lexer.h"
#include "../color/color.h"
#include "../memory/memory.h"
#include "error.h"

#define MAX_ERRORS 256
static ErrorInformation error_list[MAX_ERRORS];
static int error_count = 0;

/**
 * @brief Generates the full source line text for a given line number.
 */
const char *generate_line(ArenaAllocator *arena, Token *tokens, int token_count,
                          int target_line) {
  if (target_line < 0 || !tokens)
    return "";

  // Calculate total length needed
  size_t total_len = 1; // For null terminator
  for (int i = 0; i < token_count; i++) {
    if (tokens[i].line == target_line) {
      total_len += tokens[i].whitespace_len + tokens[i].length;
    }
  }

  // Allocate result buffer
  char *result = arena_alloc(arena, total_len + 1, alignof(char));
  if (!result)
    return "";

  char *pos = result;

  // Build the line
  for (int i = 0; i < token_count; i++) {
    if (tokens[i].line != target_line)
      continue;

    // Add whitespace
    memset(pos, ' ', tokens[i].whitespace_len);
    pos += tokens[i].whitespace_len;

    // Add token text
    memcpy(pos, tokens[i].value, tokens[i].length);
    pos += tokens[i].length;
  }

  *pos = '\0';

  return result;
}

/**
 * @brief Adds an error to the internal error list.
 */
void error_add(ErrorInformation err) {
  if (error_count < MAX_ERRORS) {
    error_list[error_count++] = err;
  }
}

/**
 * @brief Clears all accumulated errors from the error list.
 */
void error_clear(void) { error_count = 0; }

/**
 * @brief Calculates the number of digits in a line number.
 */
static int get_line_width(int line) {
  int width = 1;
  int temp = line;
  while (temp >= 10) {
    width++;
    temp /= 10;
  }
  return width;
}

/**
 * @brief Finds the maximum line number width for padding.
 */
static int get_max_line_width(void) {
  int max_width = 1;
  for (int i = 0; i < error_count; i++) {
    int width = get_line_width(error_list[i].line);
    if (width > max_width) {
      max_width = width;
    }
  }
  return max_width;
}

/**
 * @brief Prints padding spaces for line number alignment.
 */
static void print_line_padding(int current_line, int max_width) {
  int current_width = get_line_width(current_line);
  int padding = max_width - current_width;
  for (int i = 0; i < padding; i++) {
    printf(" ");
  }
}

/**
 * @brief Finds the actual column position of the token in the line.
 *
 * This searches for the token text within the line to get accurate positioning.
 */
static int find_token_column(const char *line_text, const char *token_text,
                             int token_length, int hint_col) {
  if (!line_text || !token_text || token_length <= 0) {
    return hint_col - 1; // Convert to 0-based
  }

  // Search for the token in the line
  const char *found = strstr(line_text, token_text);
  if (found) {
    return (int)(found - line_text);
  }

  // Fallback to hint column
  return hint_col - 1;
}

/**
 * @brief Extracts the token text from various error types.
 */
static const char *extract_token_text(ErrorInformation *e, char *buffer,
                                      size_t buffer_size) {
  // For memory errors, extract variable name from message
  if (strstr(e->error_type, "Memory Leak") ||
      strstr(e->error_type, "Double Free") ||
      strstr(e->error_type, "Use After Free")) {

    // Pattern: "Variable 'name' ..." or "'name' ..."
    const char *start = strchr(e->message, '\'');
    if (start) {
      start++;
      const char *end = strchr(start, '\'');
      if (end && (size_t)(end - start) < buffer_size - 1) {
        strncpy(buffer, start, end - start);
        buffer[end - start] = '\0';
        return buffer;
      }
    }
  }

  // For identifier errors, look for patterns like "identifier 'name'"
  const char *id_pattern = "identifier '";
  const char *id_pos = strstr(e->message, id_pattern);
  if (id_pos) {
    const char *start = id_pos + strlen(id_pattern);
    const char *end = strchr(start, '\'');
    if (end && (size_t)(end - start) < buffer_size - 1) {
      strncpy(buffer, start, end - start);
      buffer[end - start] = '\0';
      return buffer;
    }
  }

  return NULL;
}

/**
 * @brief Prints the gutter (empty space with vertical bar).
 */
static void print_gutter(int max_width) {
  printf(" ");
  for (int i = 0; i < max_width; i++) {
    printf(" ");
  }
  printf(BLUE(" |")); // Changed to blue for softer look
  printf(" ");
}

/**
 * @brief Prints the source line with line number and color formatting.
 */
static void print_source_line(int line, const char *text, int max_width) {
  if (text && strlen(text) > 0) {
    printf(" ");
    print_line_padding(line, max_width);
    printf(BOLD_BLUE("%d |"), line);   // Changed to blue
    printf(BOLD_WHITE(" %s\n"), text); // Bright white
  } else {
    printf(" ");
    print_line_padding(line, max_width);
    printf(BOLD_BLUE("%d |\n"), line); // Changed to blue
  }
}

/**
 * @brief Prints the error indicator (carets) under the problematic token.
 */
static void print_indicator(int col, int length, int line,
                            const char *line_text, const char *token_text,
                            int token_length, int max_width,
                            const char *label) {
  print_gutter(max_width);

  // Calculate actual column position
  int actual_col = col - 1;
  if (token_text && line_text) {
    actual_col = find_token_column(line_text, token_text, token_length, col);
  }

  // Ensure we have a valid length
  int indicator_length = length > 0 ? length : 1;
  if (token_length > 0 && token_length < indicator_length) {
    indicator_length = token_length;
  }

  // Print spaces up to the error position
  for (int i = 0; i < actual_col; i++) {
    printf(" ");
  }

  // Print the carets in yellow for better visibility
  printf(BOLD_YELLOW(""));
  for (int i = 0; i < indicator_length; i++) {
    printf("^");
  }

  // Print label inline if provided
  if (label) {
    printf(" %s", label);
  }

  printf(STYLE_RESET "\n");
}

/**
 * @brief Reports all accumulated errors with formatting.
 */
bool error_report(void) {
  if (error_count == 0)
    return false;

  int max_width = get_max_line_width();

  // Summary header with better color hierarchy
  printf("\n");
  if (error_count == 1) {
    printf(BOLD_RED("error"));
    printf(": could not compile due to previous error\n");
  } else {
    printf(BOLD_RED("error"));
    printf(": could not compile due to %d previous errors\n", error_count);
  }
  printf("\n");

  // Print each error
  for (int i = 0; i < error_count; i++) {
    ErrorInformation *e = &error_list[i];

    // Error header - bold red for "error", white for type, normal for message
    printf(BOLD_RED("error"));
    printf(BOLD_WHITE("[%s]"), e->error_type);
    printf(": %s\n", e->message); // Normal white text for message

    // File location with arrow - use blue for better contrast
    printf("  ");
    printf(BOLD_BLUE("-->"));
    printf(GRAY(" %s:%d:%d\n"), e->file_path, e->line, e->col);

    // Empty gutter line
    print_gutter(max_width);
    printf("\n");

    // Source line
    if (e->line_text) {
      print_source_line(e->line, e->line_text, max_width);

      // Extract token for accurate positioning
      char token_buffer[128];
      const char *token_text =
          extract_token_text(e, token_buffer, sizeof(token_buffer));

      // Error indicator with label - use yellow for carets to stand out
      print_indicator(e->col, e->token_length, e->line, e->line_text,
                      token_text, token_text ? strlen(token_text) : 0,
                      max_width, e->label);
    }

    // Additional context with distinct colors
    if (e->note) {
      print_gutter(max_width);
      printf(BOLD_CYAN("note"));
      printf(": %s\n", e->note);
    }

    if (e->help) {
      print_gutter(max_width);
      printf(BOLD_GREEN("help")); // Green for helpful suggestions
      printf(": %s\n", e->help);
    }

    // Final empty gutter line
    print_gutter(max_width);
    printf("\n");

    // Spacing between errors
    if (i < error_count - 1) {
      printf("\n");
    }
  }

  return true;
}

/**
 * @brief Gets the current error count.
 */
int error_get_count(void) { return error_count; }

/**
 * @brief Checks if there are any errors.
 */
bool error_has_errors(void) { return error_count > 0; }
