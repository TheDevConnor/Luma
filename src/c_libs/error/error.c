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
 *
 * It reconstructs the line by concatenating token values and whitespace
 * for all tokens on the target line, allocating the string in the arena.
 *
 * @param arena Arena allocator for allocating the returned string.
 * @param tokens Array of tokens from the source.
 * @param token_count Number of tokens.
 * @param target_line The line number to generate.
 * @return Pointer to a null-terminated string containing the source line,
 *         or empty string if line not found or error occurs.
 */
const char *generate_line(ArenaAllocator *arena, Token *tokens, int token_count,
                          int target_line) {
  if (target_line < 0 || !tokens)
    return "";

  // Calculate total length needed
  size_t total_len = 1; // For newline
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

  *pos++ = '\n';
  *pos = '\0';

  return result;
}

/**
 * @brief Adds an error to the internal error list.
 *
 * If the list is full (>= MAX_ERRORS), the error is ignored.
 *
 * @param err The ErrorInformation to add.
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
 * @brief Helper function to convert a line number to a zero-padded string.
 *
 * Used for formatting line number indicators.
 *
 * @param line The line number.
 * @return Pointer to a static buffer containing the zero-padded string.
 */
const char *convert_line_to_string(int line) {
  static char buffer[16];

  // First get the width by formatting the actual number
  char temp[16];
  int width = snprintf(temp, sizeof(temp), "%d", line);

  // Then create a string of zeros with that width
  for (int i = 0; i < width; i++) {
    buffer[i] = ' ';
  }
  buffer[width] = '\0';

  return buffer;
}

/**
 * @brief Prints the source line with line number and color formatting.
 *
 * @param line Line number.
 * @param text The line text to print.
 */
static void print_source_line(int line, const char *text) {
  if (text) {
    printf(GRAY(" %d | "), line);
    printf(BOLD_WHITE("%s"), text);
  } else {
    printf(GRAY(" %d |\n"), line);
  }
}

/**
 * @brief Finds the position of a token in the reconstructed line text.
 *
 * @param line_text The reconstructed line text.
 * @param token_text The token text to find.
 * @param token_length Length of the token.
 * @param original_col Original column position as fallback.
 * @return The actual position in the line where the token appears.
 */
static int find_token_position(const char *line_text, const char *token_text,
                               int token_length, int original_col) {
  if (!line_text || !token_text || token_length <= 0) {
    return original_col - 1; // Fallback to original position (0-based)
  }

  // Create a null-terminated copy of the token for searching
  char *token_copy = malloc(token_length + 1);
  if (!token_copy) {
    return original_col - 1;
  }

  strncpy(token_copy, token_text, token_length);
  token_copy[token_length] = '\0';

  // Find the token in the line
  const char *found = strstr(line_text, token_copy);
  free(token_copy);

  if (found) {
    return (int)(found - line_text);
  }

  // If not found, fallback to original position
  return original_col - 1;
}

/**
 * @brief Prints a caret (^) indicator under the error position in the source
 * line.
 *
 * @param col Column number where the error starts.
 * @param len Length of the token or range to highlight.
 * @param line The source line number.
 * @param line_text The reconstructed line text.
 * @param token_text The actual token text.
 * @param token_length Length of the token text.
 */
static void print_indicator(int col, int len, int line, const char *line_text,
                            const char *token_text, int token_length) {
  printf(GRAY(" %s | "), convert_line_to_string(line));

  // Find the actual position of the token in the line
  int actual_pos =
      find_token_position(line_text, token_text, token_length, col);

  // Print spaces up to the indicator position
  for (int i = 0; i < actual_pos; i++) {
    printf(" ");
  }

  // Print the carets
  for (int i = 0; i < len; i++) {
    printf(RED("^"));
  }
  printf(STYLE_RESET "\n");
}

// We need to modify the ErrorInformation struct to include token_text
// But since we can't modify the struct, let's extract it from existing data

/**
 * @brief Extracts token text from error information.
 * For memory errors, we can extract the variable name from the message.
 */
static const char *extract_token_from_error(ErrorInformation *e, char *buffer,
                                            size_t buffer_size) {
  if (strstr(e->error_type, "Memory Leak") ||
      strstr(e->error_type, "Double Free")) {
    // For memory errors, extract variable name from message like "Variable
    // 'ptr1' allocated but never freed"
    const char *start = strchr(e->message, '\'');
    if (start) {
      start++; // Move past the opening quote
      const char *end = strchr(start, '\'');
      if (end && (end - start) < buffer_size - 1) {
        strncpy(buffer, start, end - start);
        buffer[end - start] = '\0';
        return buffer;
      }
    }
  } else if (strstr(e->error_type, "Main Visibility")) {
    // For main visibility, the token is "main"
    strncpy(buffer, "main", buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return buffer;
  }

  return NULL; // Couldn't extract token
}

// Update error_report to use the new print_indicator
bool error_report(void) {
  if (error_count == 0)
    return false;

  printf("\n%s: %d\n", BOLD_WHITE("Total Errors/Warnings"), error_count);

  for (int i = 0; i < error_count; i++) {
    ErrorInformation *e = &error_list[i];
    printf(RED("%s: "), e->error_type);
    printf(BOLD_WHITE("%s\n"), e->message);
    printf("  --> ");
    printf(BOLD_YELLOW("%s"), e->file_path);
    printf(":%d::%d\n", e->line, e->col);

    print_source_line(e->line, e->line_text);

    // Extract token text from error message
    char token_buffer[64];
    const char *token_text =
        extract_token_from_error(e, token_buffer, sizeof(token_buffer));

    print_indicator(e->col, e->token_length > 0 ? e->token_length : 1, e->line,
                    e->line_text, token_text, e->token_length);

    if (e->label) {
      printf("  %s: %s\n", CYAN("label"), e->label);
    }
    if (e->note) {
      printf("  %s: %s\n", CYAN("note"), e->note);
    }
    if (e->help) {
      printf("  %s: %s\n", CYAN("help"), e->help);
    }
    printf("\n");
  }
  return true;
}
