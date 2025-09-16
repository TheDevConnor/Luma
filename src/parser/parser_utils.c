/**
 * @file parser_utils.c
 * @brief Utility functions for the parser module
 *
 * This file contains essential utility functions that provide the core
 * infrastructure for token navigation and manipulation during parsing.
 * These functions abstract the low-level details of token stream management
 * and provide a clean interface for parser operations.
 *
 * The utilities include:
 * - Token stream navigation (current, peek, advance)
 * - Token consumption with error reporting
 * - Token validation and bounds checking
 * - String extraction and memory management for identifiers
 *
 * All functions are designed to be safe and handle edge cases like
 * end-of-stream conditions gracefully by returning EOF tokens.
 *
 * @author [Your Name]
 * @date [Current Date]
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>

#include "parser.h"

/**
 * @brief Checks if there are more tokens available for parsing
 *
 * This function determines whether the parser has reached the end of the token
 * stream. It's used throughout the parser to control parsing loops and prevent
 * buffer overruns when accessing tokens.
 *
 * @param psr Pointer to the parser instance
 *
 * @return true if there are more tokens to process (and current token is not
 * EOF), false if at end of stream or current token is EOF
 *
 * @note This function performs two checks:
 *       1. Position is within the token array bounds
 *       2. Current token is not TOK_EOF (which marks end of input)
 *
 * @see p_current(), p_advance()
 */
bool p_has_tokens(Parser *psr) {
  return (psr->pos < psr->tk_count && psr->tks[psr->pos].type_ != TOK_EOF);
}

/**
 * @brief Peeks at a token at the specified offset from current position
 *
 * This function allows looking ahead in the token stream without advancing
 * the current position. It's useful for making parsing decisions based on
 * upcoming tokens (lookahead parsing).
 *
 * @param psr Pointer to the parser instance
 * @param offset Number of positions ahead to look (0 = current, 1 = next, etc.)
 *
 * @return Token at the specified offset, or an EOF token if the offset
 *         goes beyond the end of the token stream
 *
 * @note Common usage patterns:
 *       - `p_peek(parser, 0)` is equivalent to `p_current(parser)`
 *       - `p_peek(parser, 1)` looks at the next token
 *       - Safe to use with any offset; returns EOF token for out-of-bounds
 * access
 *
 * @see p_current(), Token, TOK_EOF
 */
Token p_peek(Parser *psr, size_t offset) {
  return (psr->pos + offset < psr->tk_count)
             ? psr->tks[psr->pos + offset]
             : (Token){.type_ = TOK_EOF}; // Return EOF token if out of bounds
}

/**
 * @brief Gets the current token without advancing the parser position
 *
 * This function returns the token at the current parser position without
 * modifying the parser state. It's the most frequently used function for
 * examining the current token during parsing.
 *
 * @param psr Pointer to the parser instance
 *
 * @return Current token, or an EOF token if at the end of the token stream
 *
 * @note This function is safe to call even when at the end of the token stream
 * @note The returned token contains all token information: type, value,
 * position, etc.
 * @note Does not modify parser state - can be called multiple times safely
 *
 * @see p_advance(), p_peek(), Token
 */
Token p_current(Parser *psr) {
  return psr->pos < psr->tk_count
             ? psr->tks[psr->pos]
             : (Token){.type_ = TOK_EOF}; // Return EOF token if no tokens left
}

/**
 * @brief Advances to the next token and returns the current token
 *
 * This function moves the parser position forward by one token and returns
 * the token that was current before advancing. This is the primary mechanism
 * for consuming tokens during parsing.
 *
 * @param psr Pointer to the parser instance
 *
 * @return The token that was current before advancing, or EOF token if
 *         already at the end of the token stream
 *
 * @note The parser position is incremented only if there are tokens available
 * @note Safe to call at end of stream - returns EOF token and doesn't advance
 * @note This is a mutating operation that changes parser state
 *
 * @warning After calling this function, subsequent calls to p_current() will
 *          return the next token in the stream
 *
 * @see p_current(), p_has_tokens(), p_consume()
 */
Token p_advance(Parser *psr) {
  if (p_has_tokens(psr)) {
    return psr->tks[psr->pos++];
  }
  return (Token){.type_ = TOK_EOF}; // Return EOF token if no tokens left
}

/**
 * @brief Consumes a token of the expected type or reports an error
 *
 * This function is used when the parser expects a specific token type at the
 * current position. If the current token matches the expected type, it advances
 * and returns the token. If not, it reports a syntax error with the provided
 * error message.
 *
 * @param psr Pointer to the parser instance
 * @param type The expected token type that should be at the current position
 * @param error_msg Error message to display if the token doesn't match
 *
 * @return The consumed token if it matches the expected type,
 *         or an EOF token if there's a mismatch (indicating an error)
 *
 * @note This function combines token validation and consumption in one
 * operation
 * @note Error reporting includes current line, column, and token length
 * information
 * @note The parser position advances only on successful token match
 * @note On error, the parser position remains unchanged
 *
 * @warning Always check the return value's type if you need to handle parsing
 * errors
 *
 * @see p_current(), p_advance(), parser_error(), LumaTokenType
 */
Token p_consume(Parser *psr, LumaTokenType type, const char *error_msg) {
  int line = p_current(psr).line;
  int col = p_current(psr).col;

  if (p_current(psr).type_ == type)
    return p_advance(psr);
  else {
    parser_error(psr, "SyntaxError", "unknown_file", error_msg, line, col,
                 CURRENT_TOKEN_LENGTH(psr));
    return (Token){.type_ = TOK_EOF}; // Return an error token
  }
}

/**
 * @brief Extracts and duplicates the current token's string value
 *
 * This function creates a null-terminated string copy of the current token's
 * value using the arena allocator. It's primarily used for extracting
 * identifier names, string literals, and other textual token content that
 * needs to be preserved in the AST.
 *
 * @param psr Pointer to the parser instance
 *
 * @return Pointer to a null-terminated string containing the current token's
 * value, allocated using the parser's arena allocator
 *
 * @note The returned string is allocated using arena_alloc() for automatic
 * memory management
 * @note The string is properly null-terminated for safe use with standard
 * string functions
 * @note The original token value is copied, so the returned string is
 * independent
 * @note Memory is automatically managed by the arena - no manual deallocation
 * needed
 *
 * @warning This function assumes the current token has a valid value field
 * @warning Should only be called when you know the current token contains
 * string data
 *
 * @see CURRENT_TOKEN_VALUE(), CURRENT_TOKEN_LENGTH(), arena_alloc()
 */
char *get_name(Parser *psr) {
  char *name = (char *)arena_alloc(psr->arena, CURRENT_TOKEN_LENGTH(psr) + 1,
                                   alignof(char));
  memcpy((void *)name, CURRENT_TOKEN_VALUE(psr), CURRENT_TOKEN_LENGTH(psr));
  ((char *)name)[CURRENT_TOKEN_LENGTH(psr)] = '\0';
  return name;
}

// Helper function to handle parser initialization errors
bool init_parser_arrays(Parser *parser, GrowableArray *stmts,
                        GrowableArray *modules) {
  if (!growable_array_init(stmts, parser->arena, 1024, sizeof(Stmt *)) ||
      !growable_array_init(modules, parser->arena, 16, sizeof(Stmt *))) {
    fprintf(stderr, "Failed to initialize parser arrays\n");
    return false;
  }
  return true;
}

// Helper function to validate and consume module declaratio
const char *parse_module_declaration(Parser *parser) {
  if (!p_has_tokens(parser) || p_current(parser).type_ != TOK_MODULE) {
    fprintf(stderr, "Error: file must begin with @module declaration\n");
    return NULL;
  }
  p_advance(parser); // consume @module

  if (!p_has_tokens(parser) || p_current(parser).type_ != TOK_STRING) {
    fprintf(stderr, "Error: expected module name string after @module\n");
    return NULL;
  }

  const char *module_name = get_name(parser);
  p_advance(parser); // consume module name
  return module_name;
}
