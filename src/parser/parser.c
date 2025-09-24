/**
 * @file parser.c
 * @brief Implementation of the parser module for the programming language
 * compiler
 *
 * This file contains the core parsing functionality that converts a stream of
 * tokens into an Abstract Syntax Tree (AST). The parser uses a Pratt parser
 * approach for handling operator precedence and associativity in expressions.
 *
 * The parser supports:
 * - Statement parsing (variables, functions, control flow, etc.)
 * - Expression parsing with proper operator precedence
 * - Type parsing for type annotations
 * - Error reporting with source location information
 *
 * @author Connor Harris
 * @date 2025
 * @version 1.0
 */

#include <stdalign.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../c_libs/error/error.h"
#include "../c_libs/memory/memory.h"
#include "../helper/help.h"
#include "parser.h"

/**
 * @brief Reports a parser error with detailed location information
 *
 * Creates and adds an error to the global error system with information about
 * where the error occurred in the source code, including line and column
 * information.
 *
 * @param psr Pointer to the parser instance
 * @param error_type String describing the type of error (e.g., "SyntaxError")
 * @param file Path to the source file where the error occurred
 * @param msg Detailed error message describing what went wrong
 * @param line Line number where the error occurred (1-based)
 * @param col Column number where the error occurred (1-based)
 * @param tk_length Length of the token that caused the error
 *
 * @note This function uses the arena allocator to duplicate the line text
 * @see ErrorInformation, error_add()
 */
void parser_error(Parser *psr, const char *error_type, const char *file,
                  const char *msg, int line, int col, int tk_length) {
  (void)file;
  ErrorInformation err = {
      .error_type = error_type,
      .file_path = psr->file_path,
      .message = msg,
      .line = line,
      .col = col,
      .line_text = generate_line(psr->arena, psr->tks, psr->tk_count, err.line),
      .token_length = tk_length,
      .label = "Parser Error",
      .note = NULL,
      .help = NULL,
  };
  error_add(err);
}

/**
 * @brief Main parsing function that converts tokens into an AST
 *
 * This is the entry point for the parser. It takes a growable array of tokens
 * and converts them into a complete program AST node containing all parsed
 * statements.
 *
 * @param tks Growable array containing all tokens from the lexer
 * @param arena Arena allocator for memory management during parsing
 *
 * @return Pointer to the root AST node (Program node) containing all parsed
 * statements, or NULL if parsing fails
 *
 * @note The function estimates the initial capacity for statements based on
 * token count
 * @note All memory allocations use the provided arena allocator
 *
 * @see Parser, parse_stmt(), create_program_node()
 */

Stmt *parse(GrowableArray *tks, ArenaAllocator *arena, BuildConfig *config) {
  // Initialize parser
  Parser parser = {
      .file_path = config->filepath,
      .arena = arena,
      .tks = (Token *)tks->data,
      .tk_count = tks->count,
      .capacity = (tks->count / 4) + 10,
      .pos = 0,
  };

  if (!parser.tks) {
    fprintf(stderr, "Failed to get tokens from GrowableArray\n");
    return NULL;
  }

  // Initialize arrays
  GrowableArray stmts, modules;
  if (!init_parser_arrays(&parser, &stmts, &modules)) {
    return NULL;
  }

  // Parse module declaration
  Token module_tok = p_current(&parser);
  const char *module_name = parse_module_declaration(&parser);
  if (!module_name) {
    return NULL;
  }

  // Create initial module node and add to modules array
  Stmt *module_stmt = create_module_node(parser.arena, module_name, 0, NULL, 0,
                                         module_tok.line, module_tok.col);

  Stmt **module_slot = (Stmt **)growable_array_push(&modules);
  if (!module_slot) {
    fprintf(stderr, "Out of memory while growing modules array\n");
    return NULL;
  }
  *module_slot = module_stmt;

  // Parse all statements
  while (p_current(&parser).type_ != TOK_EOF) {
    Stmt *stmt = parse_stmt(&parser);
    if (!stmt) {
      // Error already reported in parse_stmt
      return NULL;
    }

    Stmt **slot = (Stmt **)growable_array_push(&stmts);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing statements array\n");
      return NULL;
    }
    *slot = stmt;
  }

  // Update module with parsed statements
  *module_slot =
      create_module_node(parser.arena, module_name, 0, (Stmt **)stmts.data,
                         stmts.count, module_tok.line, module_tok.col);

  // Create and return program node
  return create_program_node(parser.arena, (AstNode **)modules.data,
                             modules.count, 0, 0);
}
/**
 * @brief Gets the binding power (precedence) for a given token type
 *
 * This function is crucial for the Pratt parser implementation. It returns
 * the binding power (precedence level) for different operators, which
 * determines the order of operations during expression parsing.
 *
 * Higher binding power values indicate higher precedence operators.
 *
 * @param kind The token type to get binding power for
 *
 * @return BindingPower enumeration value representing the precedence level
 *         Returns BP_NONE for tokens that don't have binding power
 *
 * @note Precedence levels (highest to lowest):
 *       - BP_CALL: Function calls, member access, indexing
 *       - BP_POSTFIX: Postfix increment/decrement
 *       - BP_PRODUCT: Multiplication, division
 *       - BP_SUM: Addition, subtraction
 *       - BP_RELATIONAL: Comparison operators
 *       - BP_EQUALITY: Equality and inequality
 *       - BP_BITWISE_AND, BP_BITWISE_XOR, BP_BITWISE_OR: Bitwise operations
 *       - BP_LOGICAL_AND, BP_LOGICAL_OR: Logical operations
 *       - BP_TERNARY: Ternary conditional operator
 *       - BP_ASSIGN: Assignment operators
 *
 * @see BindingPower, LumaTokenType
 */
BindingPower get_bp(LumaTokenType kind) {
  switch (kind) {
  // Assignment
  case TOK_EQUAL:
    return BP_ASSIGN;

  // Ternary
  case TOK_QUESTION:
    return BP_TERNARY;

  // Logical
  case TOK_OR:
    return BP_LOGICAL_OR;
  case TOK_AND:
    return BP_LOGICAL_AND;

  // Bitwise
  case TOK_PIPE:
    return BP_BITWISE_OR;
  case TOK_CARET:
    return BP_BITWISE_XOR;
  case TOK_AMP:
    return BP_BITWISE_AND;

  // Equality
  case TOK_EQEQ:
  case TOK_NEQ:
    return BP_EQUALITY;

  // Relational
  case TOK_LT:
  case TOK_LE:
  case TOK_GT:
  case TOK_GE:
    return BP_RELATIONAL;

  // Arithmetic
  case TOK_PLUS:
  case TOK_MINUS:
    return BP_SUM;
  case TOK_STAR:
  case TOK_SLASH:
  case TOK_MODL:
    return BP_PRODUCT;

  // Postfix
  case TOK_PLUSPLUS:
  case TOK_MINUSMINUS:
    return BP_POSTFIX;

  // Call/indexing/member access
  case TOK_LPAREN:
  case TOK_LBRACKET:
  case TOK_DOT:
  case TOK_RESOLVE:
    return BP_CALL;

  case TOK_RANGE:
    return BP_RANGE;

  default:
    return BP_NONE;
  }
}

/**
 * @brief Null Denotation - handles prefix expressions and primary expressions
 *
 * This is part of the Pratt parser implementation. The "nud" function handles
 * tokens that can appear at the beginning of an expression (prefix operators
 * and primary expressions like literals and identifiers).
 *
 * @param parser Pointer to the parser instance
 *
 * @return Pointer to the parsed expression AST node, or NULL if parsing fails
 *
 * @note Handles:
 *       - Primary expressions: numbers, strings, identifiers
 *       - Prefix unary operators: -, +, !, ++, --
 *       - Grouped expressions: (expression)
 *       - Array literals: [expression, ...]
 *
 * @see led(), parse_expr(), primary(), unary(), grouping(), array_expr()
 */
Expr *nud(Parser *parser) {
  switch (p_current(parser).type_) {
  case TOK_NUMBER:
  case TOK_NUM_FLOAT:
  case TOK_STRING:
  case TOK_IDENTIFIER:
    return primary(parser);
  case TOK_MINUS:
  case TOK_PLUS:
  case TOK_BANG:
  case TOK_PLUSPLUS:
  case TOK_MINUSMINUS:
    return unary(parser);
  case TOK_LPAREN:
    return grouping(parser);
  case TOK_LBRACKET:
    return array_expr(parser);
  case TOK_STAR:
    return deref_expr(parser);
  case TOK_AMP:
    return addr_expr(parser);
  case TOK_ALLOC:
    return alloc_expr(parser);
  case TOK_FREE:
    return free_expr(parser);
  case TOK_CAST:
    return cast_expr(parser);

  // Compile time
  case TOK_SIZE_OF:
    return sizeof_expr(parser);
  default:
    p_advance(parser);
    return NULL;
  }
}

/**
 * @brief Left Denotation - handles binary and postfix expressions
 *
 * This is part of the Pratt parser implementation. The "led" function handles
 * tokens that can appear after an expression has been parsed (binary operators
 * and postfix operators).
 *
 * @param parser Pointer to the parser instance
 * @param left The left operand expression (already parsed)
 * @param bp The current binding power context
 *
 * @return Pointer to the parsed expression AST node incorporating the left
 * operand, or the original left expression if no valid LED is found
 *
 * @note Handles:
 *       - Binary arithmetic and logical operators: +, -, *, /, ==, !=, etc.
 *       - Function calls: function(args)
 *       - Assignment: variable = value
 *       - Member access: object.member
 *       - Postfix operators: variable++, variable--
 *       - Array indexing: array[index]
 *
 * @see nud(), parse_expr(), binary(), call_expr(), assign_expr(), prefix_expr()
 */
Expr *led(Parser *parser, Expr *left, BindingPower bp) {
  switch (p_current(parser).type_) {
  case TOK_PLUS:
  case TOK_MINUS:
  case TOK_STAR:
  case TOK_SLASH:
  case TOK_MODL:
  case TOK_EQEQ:
  case TOK_NEQ:
  case TOK_LT:
  case TOK_LE:
  case TOK_GT:
  case TOK_GE:
  case TOK_AMP:
  case TOK_PIPE:
  case TOK_CARET:
  case TOK_AND: // Add logical AND
  case TOK_OR:  // Add logical OR
  case TOK_RANGE:
    return binary(parser, left, bp);
  case TOK_LPAREN:
    return call_expr(parser, left, bp);
  case TOK_EQUAL:
    return assign_expr(parser, left, bp);
  case TOK_DOT:
  case TOK_RESOLVE:
  case TOK_PLUSPLUS:
  case TOK_MINUSMINUS:
  case TOK_LBRACKET:
    // Handle member access, postfix increment/decrement, and indexing
    return prefix_expr(parser, left, bp);
  default:
    p_advance(parser);
    return left; // No valid LED found, return left expression
  }
}

/**
 * @brief Parses an expression using the Pratt parsing algorithm
 *
 * This is the core expression parsing function that implements the Pratt parser
 * algorithm. It handles operator precedence and associativity automatically
 * through the binding power mechanism.
 *
 * @param parser Pointer to the parser instance
 * @param bp Minimum binding power - only operators with higher binding power
 *           will be consumed by this call
 *
 * @return Pointer to the parsed expression AST node, or NULL if parsing fails
 *
 * @note The algorithm works by:
 *       1. Getting the left expression using nud()
 *       2. While the next operator has higher binding power than bp:
 *          - Use led() to extend the expression with the operator
 *       3. Return the final expression
 *
 * @see nud(), led(), get_bp(), BindingPower
 */
Expr *parse_expr(Parser *parser, BindingPower bp) {
  Expr *left = nud(parser);

  while (p_has_tokens(parser) && get_bp(p_current(parser).type_) > bp) {
    left = led(parser, left, get_bp(p_current(parser).type_));
  }

  return left;
}

/**
 * @brief Parses a single statement
 *
 * This function dispatches to the appropriate statement parsing function based
 * on the current token. It also handles visibility modifiers (public/private)
 * that can appear before certain statement types.
 *
 * @param parser Pointer to the parser instance
 *
 * @return Pointer to the parsed statement AST node, or NULL if parsing fails
 *
 * @note Handles:
 *       - Variable declarations: const, var
 *       - Control flow: return, if, loop, break, continue
 *       - Block statements: { ... }
 *       - Print statements: print, println
 *       - Expression statements: any expression followed by semicolon
 *       - Visibility modifiers: public, private (applied to declarations)
 *
 * @see const_stmt(), var_stmt(), return_stmt(), block_stmt(), if_stmt(),
 *      loop_stmt(), print_stmt(), break_continue_stmt(), expr_stmt()
 */
Stmt *parse_stmt(Parser *parser) {
  bool is_public = false;

  // Handle visibility modifiers
  if (p_current(parser).type_ == TOK_PUBLIC) {
    is_public = true;
    p_advance(parser);
  } else if (p_current(parser).type_ == TOK_PRIVATE) {
    is_public = false;
    p_advance(parser);
  }

  switch (p_current(parser).type_) {
  case TOK_USE:
    return use_stmt(parser);
  case TOK_CONST:
    return const_stmt(parser, is_public);
  case TOK_VAR:
    return var_stmt(parser, is_public);
  case TOK_RETURN:
    return return_stmt(parser);
  case TOK_LBRACE:
    return block_stmt(parser);
  case TOK_IF:
    return if_stmt(parser);
  case TOK_LOOP:
    return loop_stmt(parser);
  case TOK_PRINT:
    return print_stmt(parser, false);
  case TOK_PRINTLN:
    return print_stmt(parser, true);
  case TOK_CONTINUE:
  case TOK_BREAK:
    return break_continue_stmt(parser, p_current(parser).type_ == TOK_CONTINUE);
  case TOK_DEFER:
    return defer_stmt(parser);
  case TOK_SWITCH:
    return switch_stmt(parser);
  default:
    return expr_stmt(
        parser); // expression statements handle their own semicolon
  }
}

/**
 * @brief Parses a type annotation
 *
 * This function parses type expressions used in variable declarations,
 * function parameters, return types, etc. It handles primitive types,
 * pointer types, array types, and user-defined types.
 *
 * @param parser Pointer to the parser instance
 *
 * @return Pointer to the parsed Type AST node, or NULL if parsing fails
 *
 * @note Handles:
 *       - Primitive types: int, uint, float, bool, string, void, char
 *       - Pointer types: *type
 *       - Array types: [size]type or []type
 *       - User-defined types: identified by TOK_IDENTIFIER
 *
 * @warning Prints error message to stderr for unexpected tokens
 *
 * @see tnud(), tled(), LumaTokenType
 */
Type *parse_type(Parser *parser) {
  LumaTokenType tok = p_current(parser).type_;

  switch (tok) {
  case TOK_INT:
  case TOK_UINT:
  case TOK_DOUBLE:
  case TOK_FLOAT:
  case TOK_BOOL:
  case TOK_STRINGT:
  case TOK_VOID:
  case TOK_CHAR:
  case TOK_STAR:     // Pointer type
  case TOK_LBRACKET: // Array type
    return tnud(parser);

  // Optionally: handle identifiers like 'MyStruct' or user-defined types
  case TOK_IDENTIFIER:
    return create_basic_type(parser->arena, get_name(parser), parser->tks->line,
                             parser->tks->col);

  default:
    fprintf(stderr, "[parse_type] Unexpected token for type: %d\n", tok);
    return NULL;
  }
}
