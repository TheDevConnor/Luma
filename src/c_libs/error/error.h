/**
 * @file error.h
 * @brief Error reporting utilities for lexer and parser diagnostics.
 *
 * This module defines an error information struct and functions
 * to add, report, and clear errors, with support for detailed source
 * location and optional notes and help messages.
 */

#pragma once

#include <stdbool.h>

#include "../../lexer/lexer.h"
#include "../memory/memory.h"

/**
 * @struct ErrorInformation
 * @brief Stores detailed information about an error.
 *
 * Fields:
 * - @c error_type: A string representing the error category/type.
 * - @c file_path: Source file path where the error occurred.
 * - @c message: Primary error message describing the issue.
 * - @c line: Line number in the source file.
 * - @c col: Column number in the source line.
 * - @c line_text: (Optional) The full text of the source line.
 * - @c token_length: Length of the token related to the error.
 * - @c label: (Optional) A label to highlight part of the error context.
 * - @c note: (Optional) Additional notes or explanations.
 * - @c help: (Optional) Suggestions or help messages for fixing the error.
 */
typedef struct {
  const char *error_type;
  const char *file_path;
  const char *message;

  int line;
  int col;

  const char *line_text; // Optional: source line text
  int token_length;

  const char *label; // optional
  const char *note;  // optional
  const char *help;  // optional
} ErrorInformation;

/**
 * @brief Generates the source code line text for a given line number.
 *
 * Uses the provided tokens and copies the line text into memory
 * allocated from the given arena.
 *
 * @param arena Arena allocator used for string allocation.
 * @param tokens Array of tokens representing the source.
 * @param token_count Number of tokens in the array.
 * @param target_line The line number for which to generate the line text.
 * @return Pointer to a null-terminated string containing the line text,
 *         allocated in the arena. Returns NULL if the line is not found.
 */
const char *generate_line(ArenaAllocator *arena, Token *tokens, int token_count,
                          int target_line);

/**
 * @brief Gets the current error count.
 *
 * @return Number of errors currently accumulated.
 */
int error_get_count(void);

/**
 * @brief Gets the error at the specified index.
 *
 * @param index Index of the error to retrieve (0-based).
 * @return Pointer to the ErrorInformation struct, or NULL if index is invalid.
 */
ErrorInformation *error_get_at_index(int index);

/**
 * @brief Checks if there are any errors.
 *
 * @return true if there are errors, false otherwise.
 */
bool error_has_errors(void);

/**
 * @brief Adds a new error to the internal error list.
 *
 * The error information is copied or referenced for later reporting.
 *
 * @param err The error information struct to add.
 */
void error_add(ErrorInformation err);

/**
 * @brief Reports all accumulated errors to stderr or appropriate output.
 *
 * @return true if there were errors to report, false if none.
 */
bool error_report(void);

/**
 * @brief Clears all accumulated errors.
 *
 * Frees any internal storage and resets the error list.
 */
void error_clear(void);
