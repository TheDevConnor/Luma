/**
 * @file parser.h
 * @brief Recursive descent and Pratt parser for the Zura language.
 *
 * This module implements parsing of tokens into an Abstract Syntax Tree (AST),
 * handling expressions, statements, types, and error reporting.
 * The parser uses binding power (precedence) to correctly parse expressions.
 *
 * The parser supports:
 * - Parsing expressions with Pratt parsing techniques (nud/led functions).
 * - Parsing statements including variable declarations, functions, loops, etc.
 * - Parsing types including pointers and arrays.
 * - Error reporting with detailed location info.
 *
 * @author Connor Harris
 * @date 2025
 */

#pragma once

#include <stddef.h>

#include "../ast/ast.h"
#include "../c_libs/memory/memory.h"
#include "../helper/help.h"
#include "../lexer/lexer.h"

/**
 * @def CURRENT_TOKEN_LENGTH(parser)
 * @brief Get the length of the current token's lexeme.
 */
#define CURRENT_TOKEN_LENGTH(parser) ((int)p_current(parser).length)

/**
 * @def CURRENT_TOKEN_VALUE(parser)
 * @brief Get the value string of the current token.
 */
#define CURRENT_TOKEN_VALUE(parser) (p_current(parser).value)

/**
 * @brief Maximum allowed size for statements, expressions, and types.
 */
#define MAX_STMT 1024
#define MAX_EXPR 1024
#define MAX_TYPE 1024

/**
 * @enum BindingPower
 * @brief Binding power (precedence) levels for expression parsing.
 *
 * Used to control operator precedence and associativity in Pratt parsing.
 */
typedef enum {
  BP_NONE = 0,    /**< No binding power */
  BP_LOWEST,      /**< Lowest binding power */
  BP_ASSIGN,      /**< Assignment operators (=, +=, etc.) */
  BP_TERNARY,     /**< Ternary conditional operator (? :) */
  BP_LOGICAL_OR,  /**< Logical OR operator (||) */
  BP_LOGICAL_AND, /**< Logical AND operator (&&) */
  BP_BITWISE_OR,  /**< Bitwise OR operator (|) */
  BP_BITWISE_XOR, /**< Bitwise XOR operator (^) */
  BP_BITWISE_AND, /**< Bitwise AND operator (&) */
  BP_EQUALITY,    /**< Equality operators (==, !=) */
  BP_RELATIONAL,  /**< Relational operators (<, >, <=, >=) */
  BP_RANGE,       /**< Range operations (..) */
  BP_SHIFT,       /**< Shift operators (<<, >>) */
  BP_SUM,         /**< Addition and subtraction (+, -) */
  BP_PRODUCT,     /**< Multiplication, division, modulo (*, /, %) */
  BP_EXPONENT,    /**< Exponentiation operator (**) */
  BP_UNARY,       /**< Unary operators (!, ~, +, -, prefix ++/--) */
  BP_POSTFIX,     /**< Postfix operators (++/-- postfix) */
  BP_CALL,        /**< Function call or indexing */
  BP_PRIMARY      /**< Primary expressions (literals, variables) */
} BindingPower;

/**
 * @brief Maps token types to their corresponding literal types for primary
 * expressions.
 */
static const LiteralType PRIMARY_LITERAL_TYPE_MAP[] = {
    [TOK_NUMBER] = LITERAL_INT,       [TOK_NUM_FLOAT] = LITERAL_FLOAT,
    [TOK_STRING] = LITERAL_STRING,    [TOK_CHAR_LITERAL] = LITERAL_CHAR,
    [TOK_TRUE] = LITERAL_BOOL,        [TOK_FALSE] = LITERAL_BOOL,
    [TOK_IDENTIFIER] = LITERAL_IDENT,
};

/**
 * @brief Maps token types to their corresponding binary operators.
 */
static const BinaryOp TOKEN_TO_BINOP_MAP[] = {
    [TOK_PLUS] = BINOP_ADD,      [TOK_MINUS] = BINOP_SUB,
    [TOK_STAR] = BINOP_MUL,      [TOK_SLASH] = BINOP_DIV,
    [TOK_EQEQ] = BINOP_EQ,       [TOK_NEQ] = BINOP_NE,
    [TOK_LT] = BINOP_LT,         [TOK_LE] = BINOP_LE,
    [TOK_GT] = BINOP_GT,         [TOK_GE] = BINOP_GE,
    [TOK_AND] = BINOP_AND,       [TOK_OR] = BINOP_OR,
    [TOK_AMP] = BINOP_BIT_AND,   [TOK_PIPE] = BINOP_BIT_OR,
    [TOK_CARET] = BINOP_BIT_XOR, [TOK_RANGE] = BINOP_RANGE,
    [TOK_MODL] = BINOP_MOD,
};

/**
 * @brief Maps token types to their corresponding unary operators.
 */
static const UnaryOp TOKEN_TO_UNOP_MAP[] = {
    [TOK_BANG] = UNOP_NOT,         [TOK_TILDE] = UNOP_BIT_NOT,
    [TOK_PLUS] = UNOP_POS,         [TOK_MINUS] = UNOP_NEG,
    [TOK_PLUSPLUS] = UNOP_PRE_INC, [TOK_MINUSMINUS] = UNOP_PRE_DEC,
};

/**
 * @struct Parser
 * @brief Parser state holding token stream and current position.
 */
typedef struct {
  const char *file_path;
  ArenaAllocator *arena; /**< Memory arena for AST node allocations */
  Token *tks;            /**< Array of tokens to parse */
  size_t tk_count;       /**< Number of tokens in the array */
  size_t capacity;       /**< Capacity for statements and expressions */
  size_t pos;            /**< Current token position */
} Parser;

/**
 * @brief Report a parser error with detailed location info.
 *
 * @param psr Pointer to the parser.
 * @param error_type Type/category of the error.
 * @param file Source file where error occurred.
 * @param msg Error message.
 * @param line Line number of error.
 * @param col Column number of error.
 * @param tk_length Length of the token causing the error.
 */
void parser_error(Parser *psr, const char *error_type, const char *file,
                  const char *msg, int line, int col, int tk_length);

bool p_has_tokens(Parser *psr);
Token p_peek(Parser *psr, size_t offset);
Token p_current(Parser *psr);
Token p_advance(Parser *psr);
Token p_consume(Parser *psr, LumaTokenType type, const char *error_msg);
char *get_name(Parser *psr);

/**
 * @brief Parses a full program from tokens into an AST of statements.
 *
 * @param tks GrowableArray containing tokens.
 * @param arena Memory arena for allocations.
 * @return Pointer to the root AST statement node (program).
 */
Stmt *parse(GrowableArray *tks, ArenaAllocator *arena, BuildConfig *config);
Expr *parse_expr(Parser *parser, BindingPower bp);
Stmt *parse_stmt(Parser *parser);
Type *parse_type(Parser *parser);

// Helper functions for the parser
bool init_parser_arrays(Parser *parser, GrowableArray *stmts,
                        GrowableArray *modules);
const char *parse_module_declaration(Parser *parser);

Expr *nud(Parser *parser);

/**
 * @brief Pratt parser function for left denotation (infix/postfix parsing).
 *
 * @param parser Parser pointer.
 * @param left Left expression.
 * @param bp Binding power.
 * @return Parsed expression node.
 */
Expr *led(Parser *parser, Expr *left, BindingPower bp);
BindingPower get_bp(LumaTokenType kind);

Expr *primary(Parser *parser);
Expr *unary(Parser *parser);
Expr *grouping(Parser *parser);
Expr *binary(Parser *parser, Expr *left, BindingPower bp);
Expr *call_expr(Parser *parser, Expr *left, BindingPower bp);
Expr *assign_expr(Parser *parser, Expr *left, BindingPower bp);
Expr *prefix_expr(Parser *parser, Expr *left, BindingPower bp);
Expr *array_expr(Parser *parser);
Expr *index_expr(Parser *parser);
// pointer and memory related
Expr *deref_expr(Parser *parser);
Expr *addr_expr(Parser *parser);
Expr *alloc_expr(Parser *parser);
Expr *free_expr(Parser *parser);
Expr *cast_expr(Parser *parser);
Expr *input_expr(Parser *parser);
Expr *system_expr(Parser *parser);
Expr *sizeof_expr(Parser *parser);

Type *tnud(Parser *parser);
Type *tled(Parser *parser, Type *left, BindingPower bp);
BindingPower tget_bp(Parser *parser, LumaTokenType kind);

Type *pointer(Parser *parser);
Type *array_type(Parser *parser);

Stmt *use_stmt(Parser *parser);
Stmt *expr_stmt(Parser *parser);
Stmt *var_stmt(Parser *parser, bool is_public);
Stmt *const_stmt(Parser *parser, bool is_public, bool returns_ownership,
                 bool takes_ownership);
Stmt *fn_stmt(Parser *parser, const char *name, bool is_public,
              bool returns_ownership, bool takes_ownership);
Stmt *enum_stmt(Parser *parser, const char *name, bool is_public);
Stmt *struct_stmt(Parser *parser, const char *name, bool is_public);
Stmt *print_stmt(Parser *parser, bool ln);
Stmt *return_stmt(Parser *parser);
Stmt *block_stmt(Parser *parser);
Stmt *infinite_loop_stmt(Parser *parser, int line, int col);
Stmt *for_loop_stmt(Parser *parser, int line, int col);
Stmt *loop_stmt(Parser *parser);
Stmt *if_stmt(Parser *parser);
Stmt *break_continue_stmt(Parser *parser, bool is_continue);
Stmt *defer_stmt(Parser *parser);
Stmt *switch_stmt(Parser *parser);
