/**
 * @file lexer.c
 * @brief Implements the lexical analysis functions for tokenizing source code.
 *
 * Contains the logic to identify tokens, skip whitespace and comments,
 * recognize keywords and symbols, and report errors.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../c_libs/error/error.h"
#include "../c_libs/memory/memory.h"
#include "lexer.h"

/** @internal Macro to compare string to a key of known length */
#define STR_EQUALS_LEN(str, key, len)                                          \
  (strncmp(str, key, len) == 0 && key[len] == '\0')

/** @internal Macro to check if next two chars match given pair */
#define MATCH_NEXT(lx, a, b) (peek(lx, 0) == (a) && peek(lx, 1) == (b))

/** @internal Macro to construct a token */
#define MAKE_TOKEN(type, start, lx, length, whitespace_len)                    \
  make_token(type, start, lx->line, lx->col - 1, length, whitespace_len)

/** @internal Symbol to token type mapping */
static const SymbolEntry symbols[] = {
    {"%", TOK_MODL},        {"(", TOK_LPAREN},       {")", TOK_RPAREN},
    {"{", TOK_LBRACE},      {"}", TOK_RBRACE},       {"[", TOK_LBRACKET},
    {"]", TOK_RBRACKET},    {"..", TOK_RANGE},       {";", TOK_SEMICOLON},
    {",", TOK_COMMA},       {".", TOK_DOT},          {"=>", TOK_RIGHT_ARROW},
    {"==", TOK_EQEQ},       {"!=", TOK_NEQ},         {"<=", TOK_LE},
    {">=", TOK_GE},         {"&&", TOK_AND},         {"||", TOK_OR},
    {"=", TOK_EQUAL},       {"+", TOK_PLUS},         {"-", TOK_MINUS},
    {"*", TOK_STAR},        {"/", TOK_SLASH},        {"<", TOK_LT},
    {">", TOK_GT},          {"&", TOK_AMP},          {"|", TOK_PIPE},
    {"^", TOK_CARET},       {"~", TOK_TILDE},        {"!", TOK_BANG},
    {"?", TOK_QUESTION},    {"::", TOK_RESOLVE},     {":", TOK_COLON},
    {"_", TOK_SYMBOL},      {"++", TOK_PLUSPLUS},    {"--", TOK_MINUSMINUS},
    {"<<", TOK_SHIFT_LEFT}, {">>", TOK_SHIFT_RIGHT}, {"@", TOK_AT},
};

/** @internal Keyword text to token type mapping */
static const KeywordEntry keywords[] = {
    {"if", TOK_IF},
    {"else", TOK_ELSE},
    {"elif", TOK_ELIF},
    {"loop", TOK_LOOP},
    {"return", TOK_RETURN},
    {"break", TOK_BREAK},
    {"continue", TOK_CONTINUE},
    {"struct", TOK_STRUCT},
    {"enum", TOK_ENUM},
    {"mod", TOK_MOD},
    {"import", TOK_IMPORT},
    {"true", TOK_TRUE},
    {"false", TOK_FALSE},
    {"pub", TOK_PUBLIC},
    {"priv", TOK_PRIVATE},
    {"void", TOK_VOID},
    {"char", TOK_CHAR},
    {"str", TOK_STRINGT},
    {"int", TOK_INT},
    {"float", TOK_FLOAT},
    {"double", TOK_DOUBLE},
    {"bool", TOK_BOOL},
    {"let", TOK_VAR},
    {"fn", TOK_FN},
    {"output", TOK_PRINT},
    {"outputln", TOK_PRINTLN},
    {"const", TOK_CONST},
    {"alloc", TOK_ALLOC},
    {"free", TOK_FREE},
    {"cast", TOK_CAST},
    {"sizeof", TOK_SIZE_OF},
    {"as", TOK_AS},
    {"defer", TOK_DEFER},
    {"in", TOK_IN},
    {"switch", TOK_SWITCH},
};

static const KeywordEntry preprocessor_directives[] = {
    {"@module", TOK_MODULE},
    {"@use", TOK_USE},
};

static const KeywordEntry builtin_macros[] = {};

/**
 * @brief Adds a lexer error to the global error list.
 *
 * @param lx Lexer instance pointer
 * @param error_type Description of the error type
 * @param file Source file path
 * @param msg Error message
 * @param line_text Source code line text where error occurred
 * @param line Line number of the error
 * @param col Column number of the error
 * @param tk_length Length of the token causing the error
 */
void report_lexer_error(Lexer *lx, const char *error_type, const char *file,
                        const char *msg, const char *line_text, int line,
                        int col, int tk_length) {
  ErrorInformation err = {
      .error_type = error_type,
      .file_path = file,
      .message = msg,
      .line = line,
      .col = col,
      .line_text = arena_strdup(lx->arena, line_text),
      .token_length = tk_length,
      .label = "Undefined Token",
      .note = NULL,
      .help = NULL,
  };
  error_add(err);
}

/**
 * @brief Retrieves the text of a specific line from the full source.
 *
 * @param source Full source code string
 * @param target_line Line number to extract (1-based)
 * @return Pointer to static buffer containing the line text
 */
const char *get_line_text_from_source(const char *source, int target_line) {
  static char line_buffer[1024];
  const char *start = source;
  int current_line = 1;

  while (current_line < target_line && *start != '\0') {
    if (*start == '\n') {
      current_line++;
    }
    start++;
  }

  if (current_line != target_line) {
    line_buffer[0] = '\0';
    return line_buffer;
  }

  int i = 0;
  while (*start != '\0' && *start != '\n' && i < (int)sizeof(line_buffer) - 1) {
    line_buffer[i++] = *start++;
  }
  line_buffer[i] = '\0';

  return line_buffer;
}

/**
 * @internal
 * @brief Looks up if a string matches a keyword token.
 *
 * @param str Pointer to string to match
 * @param length Length of the string
 * @return LumaTokenType keyword token if found, else TOK_IDENTIFIER
 */
static LumaTokenType lookup_keyword(const char *str, int length) {
  for (int i = 0; i < (int)(sizeof(keywords) / sizeof(*keywords)); ++i) {
    if (STR_EQUALS_LEN(str, keywords[i].text, length)) {
      return keywords[i].type;
    }
  }
  return TOK_IDENTIFIER;
}

/**
 * @internal
 * @brief Looks up if a string matches a preprocessor directive.
 *
 * @param str Pointer to string to match
 * @param length Length of the string
 * @return LumaTokenType preprocessor token if found, else TOK_SYMBOL
 */
static LumaTokenType lookup_preprocessor(const char *str, int length) {
  for (int i = 0; i < (int)(sizeof(preprocessor_directives) /
                            sizeof(*preprocessor_directives));
       ++i) {
    if (STR_EQUALS_LEN(str, preprocessor_directives[i].text, length)) {
      return preprocessor_directives[i].type;
    }
  }
  return TOK_SYMBOL;
}

/**
 * @internal
 * @brief Looks up if a string matches a symbol token.
 *
 * @param str Pointer to string to match
 * @param length Length of the string
 * @return LumaTokenType symbol token if found, else TOK_SYMBOL
 */
static LumaTokenType lookup_symbol(const char *str, int length) {
  for (int i = 0; i < (int)(sizeof(symbols) / sizeof(*symbols)); ++i) {
    if (STR_EQUALS_LEN(str, symbols[i].text, length)) {
      return symbols[i].type;
    }
  }
  return TOK_SYMBOL;
}

/**
 * @brief Initializes a Lexer struct for scanning the given source code.
 *
 * @param lexer Pointer to Lexer struct to initialize
 * @param source Source code string
 * @param arena Arena allocator to allocate tokens and strings
 */
void init_lexer(Lexer *lexer, const char *source, ArenaAllocator *arena) {
  lexer->arena = arena;
  lexer->src = source;
  lexer->current = source;
  lexer->line = 1;
  lexer->col = 0;
}

/**
 * @internal
 * @brief Peeks ahead in the input stream by a given offset.
 *
 * @param lx Pointer to Lexer
 * @param offset Number of characters to peek forward
 * @return The character at the peek position
 */
char peek(Lexer *lx, int offset) { return lx->current[offset]; }

/**
 * @internal
 * @brief Checks if lexer reached end of input.
 *
 * @param lx Pointer to Lexer
 * @return true if at end, false otherwise
 */
bool is_at_end(Lexer *lx) { return *lx->current == '\0'; }

/**
 * @internal
 * @brief Advances the lexer by one character, updating line and column
 * counters.
 *
 * @param lx Pointer to Lexer
 * @return The character that was advanced past
 */
char advance(Lexer *lx) {
  char c = *lx->current++;
  if (c == '\n') {
    lx->line++;
    lx->col = 0;
  } else if (c != '\0') {
    lx->col++;
  }
  return c;
}

/**
 * @brief Constructs a Token object.
 *
 * @param type Token type
 * @param start Pointer to start of token text
 * @param line Line number
 * @param col Column number
 * @param length Length of token text
 * @param whitespace_len Length of leading whitespace
 * @return Token struct initialized with given values
 */
Token make_token(LumaTokenType type, const char *start, int line, int col,
                 int length, int whitespace_len) {
  return (Token){type, start, line, col, length, whitespace_len};
}

/**
 * @internal
 * @brief Skips over a multiline comment block.
 *
 * @param lx Pointer to Lexer
 * @return Number of characters skipped
 */
int skip_multiline_comment(Lexer *lx) {
  int count = 0;
  advance(lx); // skip '/'
  advance(lx); // skip '*'
  count += 2;
  while (!is_at_end(lx) && !MATCH_NEXT(lx, '*', '/')) {
    advance(lx);
    count++;
  }
  if (!is_at_end(lx)) {
    advance(lx); // skip '*'
    advance(lx); // skip '/'
  }
  return count + 2;
}

/**
 * @internal
 * @brief Skips over whitespace and single-line or multiline comments.
 *
 * @param lx Pointer to Lexer
 * @return Total count of skipped characters
 */
int skip_whitespace(Lexer *lx) {
  int whitespace_count = 0;

  while (!is_at_end(lx)) {
    char c = peek(lx, 0);

    if (c == ' ' || c == '\t') {
      // Count spaces and tabs as whitespace
      advance(lx);
      whitespace_count++;
    } else if (c == '\n' || c == '\r') {
      // Newline resets whitespace count - we only want leading whitespace on
      // the current line
      advance(lx);
      whitespace_count = 0; // Reset because we're on a new line
    } else if (c == '/' && peek(lx, 1) == '/') {
      // Skip single-line comment
      while (!is_at_end(lx) && peek(lx, 0) != '\n') {
        advance(lx);
      }
      // Don't count comment characters as whitespace
    } else if (c == '/' && peek(lx, 1) == '*') {
      // Skip multiline comment
      skip_multiline_comment(lx);
      // Don't count comment characters as whitespace
    } else {
      // Not whitespace or comment, stop skipping
      break;
    }
  }

  return whitespace_count;
}

/**
 * @brief Retrieves the next token from the input stream.
 *
 * @param lx Pointer to Lexer instance
 * @return Next token found in the input
 */
Token next_token(Lexer *lx) {
  int wh_count = skip_whitespace(lx);
  if (is_at_end(lx)) {
    return MAKE_TOKEN(TOK_EOF, lx->current, lx, 0, wh_count);
  }

  const char *start = lx->current;
  char c = advance(lx);

  // Preprocessor directives (starting with @)
  if (c == '@') {
    if (isalpha(peek(lx, 0))) {
      // Read the rest of the directive
      while (isalnum(peek(lx, 0)) || peek(lx, 0) == '_') {
        advance(lx);
      }
      int len = (int)(lx->current - start);
      LumaTokenType type = lookup_preprocessor(start, len);
      if (type != TOK_SYMBOL) {
        return MAKE_TOKEN(type, start, lx, len, wh_count);
      }
      // If not a known preprocessor directive, treat as error or symbol
      static char error_msg[64];
      snprintf(error_msg, sizeof(error_msg),
               "Unknown preprocessor directive: '%.*s'", len, start);
      report_lexer_error(lx, "LexerError", "unknown_file", error_msg,
                         get_line_text_from_source(lx->src, lx->line), lx->line,
                         lx->col - len, len);
      return MAKE_TOKEN(TOK_ERROR, start, lx, len, wh_count);
    }
    // Just @ by itself - treat as symbol
    LumaTokenType single_type = lookup_symbol(start, 1);
    return MAKE_TOKEN(single_type, start, lx, 1, wh_count);
  }

  // Identifiers and keywords
  if (isalpha(c) || c == '_') {
    while (isalnum(peek(lx, 0)) || peek(lx, 0) == '_') {
      advance(lx);
    }
    int len = (int)(lx->current - start);
    LumaTokenType type = lookup_keyword(start, len);
    return MAKE_TOKEN(type, start, lx, len, wh_count);
  }

  // Numbers
  if (isdigit(c)) {
    // Read the integer part
    while (isdigit(peek(lx, 0))) {
      advance(lx);
    }

    // Check for decimal point
    if (peek(lx, 0) == '.' && isdigit(peek(lx, 1))) {
      advance(lx); // consume the '.'

      // Read the fractional part
      while (isdigit(peek(lx, 0))) {
        advance(lx);
      }

      int len = (int)(lx->current - start);
      return MAKE_TOKEN(TOK_NUM_FLOAT, start, lx, len,
                        wh_count); // Make sure this is TOK_FLOAT_LITERAL
    }

    int len = (int)(lx->current - start);
    return MAKE_TOKEN(TOK_NUMBER, start, lx, len,
                      wh_count); // Make sure this is TOK_NUMBER
  }

  if (c == '\'') {
    const char *char_start = lx->current; // Start after opening quote

    if (is_at_end(lx)) {
      // Unclosed character literal
      report_lexer_error(lx, "LexerError", "unknown_file",
                         "Unclosed character literal",
                         get_line_text_from_source(lx->src, lx->line), lx->line,
                         lx->col - 1, 1);
      return MAKE_TOKEN(TOK_ERROR, start, lx, 1, wh_count);
    }

    char ch = peek(lx, 0);

    // Handle escape sequences
    if (ch == '\\') {
      advance(lx); // consume backslash
      if (is_at_end(lx)) {
        report_lexer_error(lx, "LexerError", "unknown_file",
                           "Incomplete escape sequence in character literal",
                           get_line_text_from_source(lx->src, lx->line),
                           lx->line, lx->col - 2, 2);
        return MAKE_TOKEN(TOK_ERROR, start, lx, 2, wh_count);
      }

      char escaped = peek(lx, 0);
      // Validate common escape sequences
      if (escaped != 'n' && escaped != 't' && escaped != 'r' &&
          escaped != '\\' && escaped != '\'' && escaped != '\"' &&
          escaped != '0') {
        static char error_msg[64];
        snprintf(error_msg, sizeof(error_msg),
                 "Invalid escape sequence '\\%c' in character literal",
                 escaped);
        report_lexer_error(lx, "LexerError", "unknown_file", error_msg,
                           get_line_text_from_source(lx->src, lx->line),
                           lx->line, lx->col - 2, 3);
        return MAKE_TOKEN(TOK_ERROR, start, lx, 3, wh_count);
      }
      advance(lx); // consume escaped character
    } else if (ch == '\'') {
      // Empty character literal
      report_lexer_error(lx, "LexerError", "unknown_file",
                         "Empty character literal",
                         get_line_text_from_source(lx->src, lx->line), lx->line,
                         lx->col - 1, 2);
      advance(lx); // consume closing quote
      return MAKE_TOKEN(TOK_ERROR, start, lx, 2, wh_count);
    } else if (ch == '\n' || ch == '\r') {
      // Newline in character literal
      report_lexer_error(lx, "LexerError", "unknown_file",
                         "Newline in character literal",
                         get_line_text_from_source(lx->src, lx->line), lx->line,
                         lx->col - 1, 1);
      return MAKE_TOKEN(TOK_ERROR, start, lx, 1, wh_count);
    } else {
      // Regular character
      advance(lx); // consume the character
    }

    // Check for closing quote
    if (is_at_end(lx) || peek(lx, 0) != '\'') {
      report_lexer_error(lx, "LexerError", "unknown_file",
                         "Unclosed character literal",
                         get_line_text_from_source(lx->src, lx->line), lx->line,
                         lx->col - 1, (int)(lx->current - start));
      return MAKE_TOKEN(TOK_ERROR, start, lx, (int)(lx->current - start),
                        wh_count);
    }

    advance(lx); // consume closing quote
    int total_len = (int)(lx->current - start);
    int content_len =
        (int)(lx->current - char_start - 1); // Length of content between quotes
    return make_token(TOK_CHAR_LITERAL, char_start, lx->line,
                      lx->col - total_len, content_len, wh_count);
  }

  // Strings
  if (c == '"') {
    while (!is_at_end(lx) && peek(lx, 0) != '"') {
      advance(lx);
    }
    if (!is_at_end(lx)) {
      advance(lx); // Skip closing quote
    }
    int len = (int)(lx->current - start - 2);
    return MAKE_TOKEN(TOK_STRING, start + 1, lx, len, wh_count);
  }

  // Try to match two-character symbol
  if (!is_at_end(lx)) {
    char two[3] = {start[0], peek(lx, 0), '\0'};
    LumaTokenType ttype = lookup_symbol(two, 2);
    if (ttype != TOK_SYMBOL) {
      advance(lx);
      return MAKE_TOKEN(ttype, start, lx, 2, wh_count);
    }
  }

  // Single-character symbol fallback
  LumaTokenType single_type = lookup_symbol(start, 1);
  if (single_type != TOK_SYMBOL)
    return MAKE_TOKEN(single_type, start, lx, 1, wh_count);

  // Error token if none matched
  static char error_msg[64];
  snprintf(error_msg, sizeof(error_msg), "Token not found: '%c'", c);
  report_lexer_error(lx, "LexerError", "unknown_file", error_msg,
                     get_line_text_from_source(lx->src, lx->line), lx->line,
                     lx->col, 1);
  return MAKE_TOKEN(TOK_ERROR, start, lx, 1, wh_count);
}
