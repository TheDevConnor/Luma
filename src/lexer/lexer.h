/**
 * @file lexer.h
 * @brief Tokenizer (lexer) definitions and API for lexical analysis of source
 * code.
 *
 * Provides token types, lexer state, token structure, and functions
 * to initialize a lexer, retrieve tokens, and report lexer errors.
 */

#pragma once

#include "../c_libs/memory/memory.h"
#include <stdint.h>

/**
 * @enum LumaTokenType
 * @brief Enumeration of all possible token types recognized by the lexer.
 */
typedef enum {
  TOK_EOF,          /**< End of file/input */
  TOK_ERROR,        /**< Error token */
  TOK_IDENTIFIER,   /**< Identifier (variable/function names) */
  TOK_KEYWORD,      /**< Reserved keyword */
  TOK_NUMBER,       /**< Numeric literal */
  TOK_NUM_FLOAT,    /**< Floating point numeric literal */
  TOK_STRING,       /**< String literal */
  TOK_CHAR_LITERAL, /**< Character literal */

  // Primitive Types
  TOK_INT,     /**< int */
  TOK_DOUBLE,  /**< double */
  TOK_UINT,    /**< unsigned int */
  TOK_FLOAT,   /**< float */
  TOK_BOOL,    /**< bool */
  TOK_STRINGT, /**< str (string type) */
  TOK_VOID,    /**< void */
  TOK_CHAR,    /**< char */

  // Keywords
  TOK_IF,       /**< if keyword */
  TOK_ELIF,     /**< elif keyword */
  TOK_ELSE,     /**< else keyword */
  TOK_LOOP,     /**< loop keyword */
  TOK_RETURN,   /**< return keyword */
  TOK_BREAK,    /**< break keyword */
  TOK_CONTINUE, /**< continue keyword */
  TOK_STRUCT,   /**< struct keyword */
  TOK_ENUM,     /**< enum keyword */
  TOK_MOD,      /**< mod keyword */
  TOK_IMPORT,   /**< import keyword */
  TOK_TRUE,     /**< true keyword */
  TOK_FALSE,    /**< false keyword */
  TOK_PUBLIC,   /**< pub keyword */
  TOK_PRIVATE,  /**< private keyword */
  TOK_VAR,      /**< let keyword */
  TOK_CONST,    /**< const keyword */
  TOK_FN,       /**< fn keyword */
  TOK_PRINT,    /**< output keyword */
  TOK_PRINTLN,  /**< println keyword */
  TOK_ALLOC,    /**< alloc(size_t size) */
  TOK_FREE,     /**< free(void *ptr, size_t size) */
  TOK_CAST,     /**< cast<Type>(value you want to cast too) */
  TOK_SIZE_OF,  /**< size_of<TYPE> */
  TOK_MEMCPY,   /**< memcpy(void *to, void *from, int size) */
  TOK_AS,       /**< as keyword (for use in modules) */
  TOK_DEFER,    /**< defer keyword */
  TOK_IN,       /**< in keyword */
  TOK_SWITCH,   /**< switch keyword */

  // prepocessor directives
  TOK_MODULE, /**< @module */
  TOK_USE,    /**< @use */

  // Symbols
  TOK_SYMBOL,      /**< Fallback symbol */
  TOK_LPAREN,      /**< ( */
  TOK_RPAREN,      /**< ) */
  TOK_LBRACE,      /**< { */
  TOK_RBRACE,      /**< } */
  TOK_LBRACKET,    /**< [ */
  TOK_RBRACKET,    /**< ] */
  TOK_SEMICOLON,   /**< ; */
  TOK_COMMA,       /**< , */
  TOK_DOT,         /**< . */
  TOK_AT,          /**< @ */
  TOK_EQUAL,       /**< = */
  TOK_PLUS,        /**< + */
  TOK_MINUS,       /**< - */
  TOK_STAR,        /**< * */
  TOK_SLASH,       /**< / */
  TOK_LT,          /**< < */
  TOK_GT,          /**< > */
  TOK_LE,          /**< <= */
  TOK_GE,          /**< >= */
  TOK_EQEQ,        /**< == */
  TOK_NEQ,         /**< != */
  TOK_AMP,         /**< & */
  TOK_PIPE,        /**< | */
  TOK_CARET,       /**< ^ */
  TOK_TILDE,       /**< ~ */
  TOK_AND,         /**< && */
  TOK_OR,          /**< || */
  TOK_RESOLVE,     /**< :: */
  TOK_COLON,       /**< : */
  TOK_BANG,        /**< ! */
  TOK_QUESTION,    /**< ? */
  TOK_PLUSPLUS,    /**< ++ */
  TOK_MINUSMINUS,  /**< -- */
  TOK_SHIFT_LEFT,  /**< << */
  TOK_SHIFT_RIGHT, /**< >> */
  TOK_RANGE,       /**< .. */
  TOK_RIGHT_ARROW, /**< => */
  TOK_MODL,        /**< % */
  TOK_WHITESPACE,  /**< whitespace */
  TOK_COMMENT      /**< comment */
} LumaTokenType;

/**
 * @struct Lexer
 * @brief Lexer state object for scanning source code.
 */
typedef struct {
  ArenaAllocator *arena; /**< Arena allocator for token storage */
  const char *src;       /**< Pointer to the source code string */
  const char *current;   /**< Current scanning position in source */
  int line;              /**< Current line number */
  int col;               /**< Current column number */
} Lexer;

/**
 * @struct Token
 * @brief Represents a single token extracted by the lexer.
 */
typedef struct {
  LumaTokenType type_; /**< Token type */
  const char *value;   /**< Pointer to token text start */
  int line;            /**< Line number of token */
  int col;             /**< Column number of token */
  int length;          /**< Length of the token text */
  int whitespace_len;  /**< Leading whitespace length before token */
} Token;

/**
 * @struct SymbolEntry
 * @brief Maps symbol text to token type for quick lookup.
 */
typedef struct {
  const char *text;   /**< Symbol text */
  LumaTokenType type; /**< Corresponding token type */
} SymbolEntry;

/**
 * @struct KeywordEntry
 * @brief Maps keyword text to token type for quick lookup.
 */
typedef struct {
  const char *text;   /**< Keyword text */
  LumaTokenType type; /**< Corresponding token type */
} KeywordEntry;

/**
 * @brief Reports a lexer error by adding an error to the global error list.
 *
 * @param lx Pointer to Lexer
 * @param error_type String describing the type of error
 * @param file File path of the source file
 * @param msg Error message string
 * @param line_text Source code line text where error occurred
 * @param line Line number of error
 * @param col Column number of error
 * @param tk_length Length of the erroneous token
 */
void report_lexer_error(Lexer *lx, const char *error_type, const char *file,
                        const char *msg, const char *line_text, int line,
                        int col, int tk_length);

/**
 * @brief Retrieves the text of a specific line from the source code.
 *
 * @param source Full source code string
 * @param target_line The 1-based line number to extract
 * @return Pointer to static buffer containing the line text
 */
const char *get_line_text_from_source(const char *source, int target_line);

/**
 * @brief Initializes the lexer with source code and memory arena.
 *
 * @param lexer Pointer to Lexer to initialize
 * @param source Source code string
 * @param arena Arena allocator to use for memory allocations
 */
void init_lexer(Lexer *lexer, const char *source, ArenaAllocator *arena);

/**
 * @brief Returns the next token parsed from the source code.
 *
 * @param lexer Pointer to initialized Lexer
 * @return The next Token found in the input stream
 */
Token next_token(Lexer *lexer);
