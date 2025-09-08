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
#include <string.h>

#include "../../lexer/lexer.h"
#include "../memory/memory.h"
#include "../color/color.h"
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
const char *generate_line(ArenaAllocator *arena, Token *tokens, int token_count, int target_line) {
    if (target_line < 0 || !tokens) return "";
    
    // Calculate total length needed
    size_t total_len = 1; // For newline
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].line == target_line) {
            total_len += tokens[i].whitespace_len + tokens[i].length;
        }
    }
    
    // Allocate result buffer
    char *result = arena_alloc(arena, total_len + 1, alignof(char));
    if (!result) return "";
    
    char *pos = result;
    
    // Build the line
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].line != target_line) continue;
        
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
void error_clear(void) {
    error_count = 0;
}

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
 * @brief Prints a caret (^) indicator under the error position in the source line.
 *
 * @param col Column number where the error starts.
 * @param len Length of the token or range to highlight.
 * @param line The source line number.
 */
static void print_indicator(int col, int len, int line) {
    printf(GRAY(" %s | "), convert_line_to_string(line));
    for (int i = 1; i < col; i++) printf(" ");
    for (int i = 0; i < len; i++) printf(RED("^"));
    printf(STYLE_RESET "\n");
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
        printf(BOLD_WHITE("%s\n"), text);
    } else {
        printf(GRAY(" %d |\n"), line);
    }
}

/**
 * @brief Prints all accumulated errors with detailed formatting and color.
 *
 * Displays error type, message, file and line info, source snippet, caret
 * indicator, and optional label, note, and help messages.
 *
 * @return true if errors were reported, false if no errors exist.
 */
bool error_report(void) {
    if (error_count == 0) return false;

    printf("%s: %d\n", BOLD_WHITE("Total errors"), error_count);

    for (int i = 0; i < error_count; i++) {
        ErrorInformation *e = &error_list[i];
        printf(RED("%s: "), e->error_type);
        printf(BOLD_WHITE("%s\n"), e->message);
        printf("  -->");
        printf(BOLD_YELLOW("%s"), e->file_path);
        printf(":%d::%d\n", e->line, e->col);

        printf(GRAY(" %s |\n"), convert_line_to_string(e->line));
        print_source_line(e->line, e->line_text);
        print_indicator(e->col, e->token_length > 0 ? e->token_length : 1, e->line);

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
