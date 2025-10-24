/**
 * @file stmt.c
 * @brief Statement parsing implementation for the programming language compiler
 *
 * This file contains implementations for parsing all types of statements in the
 * programming language, including declarations, control flow statements, and
 * compound statements. Each parsing function is responsible for consuming the
 * appropriate tokens and constructing the corresponding AST nodes.
 *
 * Supported statement types:
 * - Expression statements
 * - Variable and constant declarations (with optional type annotations)
 * - Function declarations with parameters and return types
 * - Struct and enum declarations with member visibility
 * - Control flow: if/elif/else, loops (infinite, while, for), return
 * - Block statements and compound statements
 * - Print statements for output
 * - Break and continue statements for loop control
 *
 * @author Connor Harris
 * @date 2025
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>

// #include "../ast/ast_utils.h"
#include "../ast/ast.h"
#include "parser.h"

/**
 * @brief Parses an expression statement
 *
 * An expression statement consists of any expression followed by a semicolon.
 * This is used for statements that evaluate an expression for its side effects,
 * such as function calls or assignment expressions.
 *
 * @param parser Pointer to the parser instance
 *
 * @return Pointer to the created expression statement AST node, or NULL on
 * failure
 *
 * @note Captures line and column information at the start of parsing
 * @note Requires a semicolon terminator
 *
 * @see parse_expr(), create_expr_stmt()
 */
Stmt *expr_stmt(Parser *parser) {
  // Capture line/col info at the beginning
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  Expr *expr = parse_expr(parser, BP_LOWEST);
  p_consume(parser, TOK_SEMICOLON,
            "Expected semicolon after expression statement");
  return create_expr_stmt(parser->arena, expr, line, col);
}

// @use "module_name" as module_alias
Stmt *use_stmt(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_USE, "Expected '@use' keyword");
  const char *module_name = get_name(parser);
  p_advance(parser); // Advance past the identifier token
  const char *module_alias = NULL;
  if (p_current(parser).type_ == TOK_AS) {
    p_consume(parser, TOK_AS, "Expected 'as' keyword for module alias");
    module_alias = get_name(parser);
    p_advance(parser); // Advance past the alias identifier token
  }

  return create_use_node(parser->arena, module_name, module_alias, line, col);
}

/**
 * @brief Parses a constant declaration statement
 *
 * Handles multiple forms of constant declarations:
 * - `const name: Type = value;` - Explicit type annotation
 * - `const name = fn ...` - Function declaration
 * - `const name = struct ...` - Struct declaration
 * - `const name = enum ...` - Enum declaration
 *
 * @param parser Pointer to the parser instance
 * @param is_public Whether this declaration has public visibility
 *
 * @return Pointer to the appropriate declaration statement AST node, or NULL on
 * failure
 *
 * @note For simple constants, creates a variable declaration with immutable
 * flag
 * @note For complex types (functions, structs, enums), delegates to specialized
 * parsers
 * @note If no type is specified, it defaults to the type of the value
 *
 * @see fn_stmt(), struct_stmt(), enum_stmt(), create_var_decl_stmt()
 */
Stmt *const_stmt(Parser *parser, bool is_public, bool returns_ownership,
                 bool takes_ownership) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_CONST, "Expected 'const' keyword");
  const char *name = get_name(parser);
  p_advance(parser); // Advance past the identifier token

  // Handle explicit type annotation: const name: Type = value
  if (p_current(parser).type_ == TOK_COLON) {
    p_consume(parser, TOK_COLON, "Expected ':' after const name");

    Type *type = parse_type(parser);
    p_advance(parser); // Advance past the type token

    p_consume(parser, TOK_EQUAL, "Expected '=' after const type");
    Expr *value = parse_expr(parser, BP_LOWEST);

    p_consume(parser, TOK_SEMICOLON,
              "Expected semicolon after const declaration");
    return create_var_decl_stmt(parser->arena, name, type, value, false,
                                is_public, line, col);
  }

  p_consume(parser, TOK_RIGHT_ARROW, "Expected '->' after const name");

  // Handle complex constant types (functions, structs, enums)
  switch (p_current(parser).type_) {
  case TOK_FN:
    return fn_stmt(parser, name, is_public, returns_ownership, takes_ownership);
  case TOK_STRUCT:
    return struct_stmt(parser, name, is_public);
  case TOK_ENUM:
    return enum_stmt(parser, name, is_public);
  default: {
    fprintf(stderr, "Expected function, struct, or enum after const '%s'\n",
            name);
    return NULL;
  }
  }
}

/**
 * @brief Parses a function declaration statement
 *
 * Handles function declarations with the syntax:
 * `fn(param1: Type1, param2: Type2, ...) ReturnType { body }`
 *
 * @param parser Pointer to the parser instance
 * @param name Function name (already parsed by caller)
 * @param is_public Whether this function has public visibility
 *
 * @return Pointer to the function declaration AST node, or NULL on failure
 *
 * @note Function parameters are stored as parallel arrays of names and types
 * @note Return type is required and parsed after the parameter list
 * @note Function body must be a block statement
 * @note Memory for parameter arrays is allocated using the arena allocator
 *
 * @see parse_type(), block_stmt(), create_func_decl_stmt()
 */
Stmt *fn_stmt(Parser *parser, const char *name, bool is_public,
              bool returns_ownership, bool takes_ownership) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  GrowableArray param_names, param_types;
  if (!growable_array_init(&param_names, parser->arena, 4, sizeof(char *)) ||
      !growable_array_init(&param_types, parser->arena, 4, sizeof(Type *))) {
    fprintf(stderr, "Failed to initialize parameter arrays.\n");
    return NULL;
  }

  p_consume(parser, TOK_FN, "Expected 'fn' keyword");
  p_consume(parser, TOK_LPAREN, "Expected '(' after function name");

  // Parse parameter list: param_name: param_type, ...
  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RPAREN) {
    if (p_current(parser).type_ != TOK_IDENTIFIER) {
      fprintf(stderr, "Expected identifier for function parameter\n");
      return NULL;
    }

    char *param_name = get_name(parser);
    p_advance(parser); // Advance past the identifier token

    p_consume(parser, TOK_COLON, "Expected ':' after parameter name");

    Type *param_type = parse_type(parser);
    if (!param_type) {
      fprintf(stderr, "Failed to parse type for parameter '%s'\n", param_name);
      return NULL;
    }
    p_advance(parser); // Advance past the type token

    // Store parameter name and type
    char **name_slot = (char **)growable_array_push(&param_names);
    Type **type_slot = (Type **)growable_array_push(&param_types);
    if (!name_slot || !type_slot) {
      fprintf(stderr, "Out of memory while growing parameter arrays\n");
      return NULL;
    }

    *name_slot = param_name;
    *type_slot = param_type;

    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser); // Advance past the comma
    }
  }

  p_consume(parser, TOK_RPAREN, "Expected ')' after function parameters");

  Type *return_type = parse_type(parser);
  p_advance(parser); // Advance past the return type token

  Stmt *body = block_stmt(parser);

  return create_func_decl_stmt(parser->arena, name, (char **)param_names.data,
                               (AstNode **)param_types.data, param_names.count,
                               return_type, is_public, returns_ownership,
                               takes_ownership, body, line, col);
}

/**
 * @brief Parses an enumeration declaration statement
 *
 * Handles enum declarations with the syntax:
 * `enum { member1, member2, member3, ... };`
 *
 * @param parser Pointer to the parser instance
 * @param name Enum name (already parsed by caller)
 * @param is_public Whether this enum has public visibility
 *
 * @return Pointer to the enum declaration AST node, or NULL on failure
 *
 * @note Enum members are identifiers separated by commas
 * @note Trailing commas are allowed but not required
 * @note Requires a semicolon terminator after the closing brace
 *
 * @see create_enum_decl_stmt()
 */
Stmt *enum_stmt(Parser *parser, const char *name, bool is_public) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  GrowableArray members;
  if (!growable_array_init(&members, parser->arena, 4, sizeof(char *))) {
    fprintf(stderr, "Failed to initialize enum members array.\n");
    return NULL;
  }

  p_consume(parser, TOK_ENUM, "Expected 'enum' keyword");
  p_consume(parser, TOK_LBRACE, "Expected '{' after enum name");

  // Parse enum members: member1, member2, ...
  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RBRACE) {
    if (p_current(parser).type_ != TOK_IDENTIFIER) {
      fprintf(stderr, "Expected identifier for enum member\n");
      return NULL;
    }

    char *member_name = get_name(parser);
    p_advance(parser); // Advance past the identifier token

    char **slot = (char **)growable_array_push(&members);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing enum members array\n");
      return NULL;
    }
    *slot = member_name;

    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser); // Advance past the comma
    }
  }

  p_consume(parser, TOK_RBRACE, "Expected '}' to end enum declaration");
  p_consume(parser, TOK_SEMICOLON, "Expected semicolon after enum declaration");

  return create_enum_decl_stmt(parser->arena, name, (char **)members.data,
                               members.count, is_public, line, col);
}

/**
 * @brief Parses a structure declaration statement
 *
 * Handles struct declarations with public/private member visibility:
 * ```
 * struct {
 *   public:
 *     field1: Type1,
 *     method1 = fn() Type { ... }
 *   private:
 *     field2: Type2,
 * };
 * ```
 *
 * @param parser Pointer to the parser instance
 * @param name Struct name (already parsed by caller)
 * @param is_public Whether this struct has public visibility
 *
 * @return Pointer to the struct declaration AST node, or NULL on failure
 *
 * @note Members are separated into public and private arrays
 * @note Supports both data fields (name: Type) and methods (name = fn ...)
 * @note Visibility defaults to public unless explicitly changed
 * @note Visibility changes affect all subsequent members until changed again
 *
 * @see fn_stmt(), create_field_decl_stmt(), create_struct_decl_stmt()
 */
Stmt *struct_stmt(Parser *parser, const char *name, bool is_public) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_STRUCT, "Expected 'struct' keyword");
  p_consume(parser, TOK_LBRACE, "Expected '{' after struct name");

  GrowableArray public_fields;
  GrowableArray private_fields;
  if (!growable_array_init(&public_fields, parser->arena, 4, sizeof(Stmt *)) ||
      !growable_array_init(&private_fields, parser->arena, 4, sizeof(Stmt *))) {
    fprintf(stderr, "Failed to initialize field arrays.\n");
    return NULL;
  }

  bool public_member = true; // default: everything is public

  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RBRACE) {
    LumaTokenType tok = p_current(parser).type_;

    // Handle visibility modifiers
    if (tok == TOK_PUBLIC || tok == TOK_PRIVATE) {
      public_member = (tok == TOK_PUBLIC);
      p_advance(parser);
      p_consume(parser, TOK_COLON, "Expected ':' after visibility keyword");
      continue;
    }

    // Parse a field or method
    int field_line = p_current(parser).line;
    int field_col = p_current(parser).col;

    // CRITICAL FIX: Check for ownership modifiers BEFORE parsing the field name
    bool takes_ownership = false;
    bool returns_ownership = false;

    if (p_current(parser).type_ == TOK_RETURNES_OWNERSHIP) {
      returns_ownership = true;
      p_advance(parser);
    } else if (p_current(parser).type_ == TOK_TAKES_OWNERSHIP) {
      takes_ownership = true;
      p_advance(parser);
    }

    // Now parse the field name (after consuming any ownership modifiers)
    char *field_name = get_name(parser);
    if (!field_name) {
      parser_error(parser, "Parse Error", __FILE__,
                   "Expected field or method name", field_line, field_col, 1);
      return NULL;
    }

    p_advance(parser);

    Stmt *field_function = NULL;
    Type *field_type = NULL;

    // Method: field_name -> fn(...)
    if (p_current(parser).type_ == TOK_RIGHT_ARROW) {
      p_consume(parser, TOK_RIGHT_ARROW, "Expected '->' after field name");
      field_function = fn_stmt(parser, field_name, public_member,
                               returns_ownership, takes_ownership);
    } else {
      // Data field: field_name: Type
      p_consume(parser, TOK_COLON, "Expected ':' after field name");
      field_type = parse_type(parser);

      // Data fields shouldn't have ownership modifiers
      if (takes_ownership || returns_ownership) {
        parser_error(
            parser, "Invalid Modifier", __FILE__,
            "Ownership modifiers (#takes_ownership, #returns_ownership) "
            "are only valid for methods, not data fields",
            field_line, field_col, 1);
        return NULL;
      }

      p_advance(parser);
    }

    // Handle field separators
    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser);
    } else if (p_current(parser).type_ != TOK_RBRACE) {
      parser_error(parser, "Unexpected token", __FILE__,
                   "Expected ',' to separate struct fields", field_line,
                   field_col, 1);
      return NULL;
    }

    // Create field declaration and add to appropriate visibility list
    Stmt *field_decl = create_field_decl_stmt(
        parser->arena, field_name, field_type, field_function, public_member,
        field_line, field_col);

    Stmt **slot = public_member ? (Stmt **)growable_array_push(&public_fields)
                                : (Stmt **)growable_array_push(&private_fields);

    if (!slot) {
      fprintf(stderr, "Failed to add field to struct.\n");
      return NULL;
    }

    *slot = field_decl;
  }

  p_consume(parser, TOK_RBRACE, "Expected '}' to end struct declaration");
  p_consume(parser, TOK_SEMICOLON,
            "Expected semicolon after struct declaration");

  return create_struct_decl_stmt(
      parser->arena, name, (Stmt **)public_fields.data, public_fields.count,
      (Stmt **)private_fields.data, private_fields.count, is_public, line, col);
}

/**
 * @brief Parses a variable declaration statement
 *
 * Handles variable declarations with the syntax:
 * `var name: Type = value;`
 *
 * @param parser Pointer to the parser instance
 * @param is_public Whether this variable has public visibility
 *
 * @return Pointer to the variable declaration AST node, or NULL on failure
 *
 * @note Variables are mutable by default (unlike constants)
 * @note Type annotation is required
 * @note Initial value assignment is required
 * @note Creates a variable declaration with is_mutable set to true
 *
 * @see parse_type(), parse_expr(), create_var_decl_stmt()
 */
Stmt *var_stmt(Parser *parser, bool is_public) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_VAR, "Expected 'let' keyword");
  const char *name = get_name(parser);
  p_advance(parser); // Advance past the identifier token

  p_consume(parser, TOK_COLON, "Expected ':' after variable name");
  Type *type = parse_type(parser);
  p_advance(parser); // Advance past the type token

  if (p_current(parser).type_ != TOK_EQUAL) {
    p_consume(parser, TOK_SEMICOLON,
              "Expected semicolon after variable declaration");
    return create_var_decl_stmt(parser->arena, name, type, NULL, true,
                                is_public, line, col);
  }

  p_consume(parser, TOK_EQUAL, "Expected '=' after variable declaration");

  Expr *value = parse_expr(parser, BP_LOWEST);
  p_consume(parser, TOK_SEMICOLON,
            "Expected semicolon after variable declaration");

  // const are not changable aka immutable and vars are mutable so we set
  // is_mutable to true
  return create_var_decl_stmt(parser->arena, name, type, value, true, is_public,
                              line, col);
}

/**
 * @brief Parses a return statement
 *
 * Handles return statements with optional return values:
 * - `return;` - Return with no value (void return)
 * - `return expression;` - Return with a value
 *
 * @param parser Pointer to the parser instance
 *
 * @return Pointer to the return statement AST node, or NULL on failure
 *
 * @note The return value is optional; if not present, creates a void return
 * @note Requires a semicolon terminator
 *
 * @see parse_expr(), create_return_stmt()
 */
Stmt *return_stmt(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_RETURN, "Expected 'return' keyword");
  Expr *value = NULL;
  if (p_current(parser).type_ != TOK_SEMICOLON) {
    value = parse_expr(parser, BP_LOWEST);
  }
  p_consume(parser, TOK_SEMICOLON, "Expected semicolon after return statement");

  return create_return_stmt(parser->arena, value, line, col);
}

/**
 * @brief Parses a block statement
 *
 * Handles block statements with the syntax:
 * `{ statement1; statement2; ... }`
 *
 * @param parser Pointer to the parser instance
 *
 * @return Pointer to the block statement AST node, or NULL on failure
 *
 * @note Empty blocks are allowed and create a valid block statement with 0
 * statements
 * @note Each statement in the block is parsed recursively using parse_stmt()
 * @note Handles memory allocation for the statement array using growable arrays
 * @note Continues parsing on individual statement failures (for error recovery)
 *
 * @see parse_stmt(), create_block_stmt()
 */
Stmt *block_stmt(Parser *parser) {
  p_consume(parser, TOK_LBRACE, "Expected '{' to start block statement");

  GrowableArray block;
  if (!growable_array_init(&block, parser->arena, 4, sizeof(Stmt *))) {
    fprintf(stderr, "Failed to initialize block statement array.\n");
    return NULL;
  }

  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RBRACE) {
    Stmt *stmt = parse_stmt(parser);
    if (!stmt) {
      fprintf(stderr, "parse_stmt returned NULL inside block\n");
      continue; // or return NULL to fail the entire block
    }

    Stmt **slot = (Stmt **)growable_array_push(&block);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing block statement array\n");
      return NULL;
    }

    *slot = stmt;
  }

  p_consume(parser, TOK_RBRACE, "Expected '}' to end block statement");

  Stmt **stmts = (Stmt **)block.data;
  if (block.count == 0) {
    return create_block_stmt(parser->arena, NULL, 0, p_current(parser).line,
                             p_current(parser).col);
  }

  return create_block_stmt(parser->arena, stmts, block.count,
                           p_current(parser).line, p_current(parser).col);
}

/**
 * @brief Parses if/elif/else conditional statements
 *
 * Handles complex conditional statements with multiple branches:
 * ```
 * if (condition1) { ... }
 * elif (condition2) { ... }
 * elif (condition3) { ... }
 * else { ... }
 * ```
 *
 * @param parser Pointer to the parser instance
 *
 * @return Pointer to the if statement AST node, or NULL on failure
 *
 * @note Supports multiple elif clauses
 * @note Else clause is optional
 * @note Each condition must be parenthesized
 * @note Each branch must be a block statement
 * @note Elif statements are collected in an array rather than nested
 * recursively
 *
 * @see parse_expr(), block_stmt(), create_if_stmt()
 */
Stmt *if_stmt(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  if (p_current(parser).type_ != TOK_IF &&
      p_current(parser).type_ != TOK_ELIF) {
    fprintf(stderr, "Expected 'if' or 'elif' keyword\n");
    return NULL;
  }
  p_consume(parser, p_current(parser).type_, "Expected 'if' or 'elif' keyword");

  p_consume(parser, TOK_LPAREN, "Expected '(' after 'if' keyword");
  Expr *condition = parse_expr(parser, BP_LOWEST);
  p_consume(parser, TOK_RPAREN, "Expected ')' after if condition");

  Stmt *then_stmt = block_stmt(parser);

  // Collect all elif statements in a list/array instead of recursing
  Stmt **elif_stmts =
      (Stmt **)arena_alloc(parser->arena, sizeof(Stmt *) * 4, alignof(Stmt *));
  int elif_count = 0;

  while (p_has_tokens(parser) && p_current(parser).type_ == TOK_ELIF) {
    int elif_line = p_current(parser).line;
    int elif_col = p_current(parser).col;

    p_consume(parser, TOK_ELIF, "Expected 'elif' keyword");
    p_consume(parser, TOK_LPAREN, "Expected '(' after 'elif' keyword");

    Expr *elif_condition = parse_expr(parser, BP_LOWEST);
    p_consume(parser, TOK_RPAREN, "Expected ')' after elif condition");

    Stmt *elif_stmt = block_stmt(parser);

    // Store the elif condition and statement
    elif_stmts[elif_count] =
        create_if_stmt(parser->arena, elif_condition, elif_stmt, NULL,
                       elif_count, NULL, elif_line, elif_col);
    elif_count++;
  }

  Stmt *else_stmt = NULL;
  if (p_current(parser).type_ == TOK_ELSE) {
    p_consume(parser, TOK_ELSE, "Expected 'else' keyword");
    else_stmt = block_stmt(parser);
  }

  return create_if_stmt(parser->arena, condition, then_stmt, elif_stmts,
                        elif_count, else_stmt, line, col);
}

/**
 * @brief Parses an infinite loop statement
 *
 * Handles infinite loops with the syntax:
 * `loop { ... }`
 *
 * @param parser Pointer to the parser instance
 * @param line Line number where the loop statement starts
 * @param col Column number where the loop statement starts
 *
 * @return Pointer to the infinite loop statement AST node, or NULL on failure
 *
 * @note The loop body must be a block statement
 * @note This creates a loop that runs indefinitely unless broken by
 * break/return
 *
 * @see block_stmt(), create_infinite_loop_stmt()
 */
Stmt *infinite_loop_stmt(Parser *parser, int line, int col) {
  Stmt *body = block_stmt(parser);
  if (!body) {
    parser_error(parser, "Syntax Error", __FILE__, "Expected block statement",
                 line, col, 1);
    return NULL;
  }
  return create_infinite_loop_stmt(parser->arena, body, line, col);
}

/**
 * @brief Parses a loop initializer declaration
 *
 * Helper function for parsing variable declarations within for-loop
 * initializers. Handles the syntax: `name: Type = expression`
 *
 * @param parser Pointer to the parser instance
 * @param line Line number for the declaration
 * @param col Column number for the declaration
 *
 * @return Pointer to the variable declaration statement, or NULL on failure
 *
 * @note Creates a mutable, non-public variable declaration
 * @note Used exclusively within for-loop initializer lists
 *
 * @see create_var_decl_stmt()
 */
Stmt *loop_init(Parser *parser, int line, int col) {
  const char *name = get_name(parser);
  p_advance(parser); // Advance past the identifier token

  p_consume(parser, TOK_COLON, "Expected ':' after loop initializer");
  Type *type = parse_type(parser);
  p_advance(parser); // Advance past the type token

  p_consume(parser, TOK_EQUAL, "Expected '=' after loop initializer");
  Expr *initializer = parse_expr(parser, BP_LOWEST);
  return create_var_decl_stmt(parser->arena, name, type, initializer, true,
                              false, line, col);
}

/**
 * @brief Parses a for loop statement
 *
 * Handles for loops with the syntax:
 * ```
 * loop [i: int = 0, j: int = 1](condition) { ... }
 * loop [i: int = 0, j: int = 1](condition) : (optional_condition) { ... }
 * ```
 *
 * @param parser Pointer to the parser instance
 * @param line Line number where the loop statement starts
 * @param col Column number where the loop statement starts
 *
 * @return Pointer to the for loop statement AST node, or NULL on failure
 *
 * @note Supports multiple initializer variables separated by commas
 * @note Main condition is required and parenthesized
 * @note Optional secondary condition can be provided after a colon
 * @note Loop body must be a block statement
 *
 * @see loop_init(), parse_expr(), block_stmt(), create_for_loop_stmt()
 */
Stmt *for_loop_stmt(Parser *parser, int line, int col) {
  GrowableArray intializers;
  if (!growable_array_init(&intializers, parser->arena, 4, sizeof(Expr *))) {
    fprintf(stderr, "Failed to initialize loop initializers array.\n");
    return NULL;
  }

  p_consume(parser, TOK_LBRACKET, "Expected '[' after 'loop' keyword");
  // Parse initializers: i: int = 0, j: int = 1
  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RBRACKET) {
    Expr *init = loop_init(parser, line, col);
    if (!init) {
      fprintf(stderr, "Failed to parse loop initializer\n");
      return NULL;
    }

    Expr **slot = (Expr **)growable_array_push(&intializers);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing loop initializers array\n");
      return NULL;
    }

    *slot = init;

    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser); // Advance past the comma
    }
  }
  p_consume(parser, TOK_RBRACKET, "Expected ']' after loop initializer");

  p_consume(parser, TOK_LPAREN, "Expected '(' after loop initializer");
  Expr *condition = parse_expr(parser, BP_LOWEST);
  p_consume(parser, TOK_RPAREN, "Expected ')' after loop initializer");

  // Check for the optional condition
  Expr *optional_condition = NULL;
  if (p_current(parser).type_ == TOK_COLON) {
    p_consume(parser, TOK_COLON, "Expected ':' after loop condition");
    p_consume(parser, TOK_LPAREN, "Expected '(' after ':' in loop statement");
    optional_condition = parse_expr(parser, BP_LOWEST);
    p_consume(parser, TOK_RPAREN,
              "Expected ')' after optional condition in loop statement");
  }

  Stmt *body = block_stmt(parser);
  return create_for_loop_stmt(parser->arena, (AstNode **)intializers.data,
                              intializers.count, condition, optional_condition,
                              body, line, col);
}

/**
 * @brief Parses loop statements (infinite, while, or for loops)
 *
 * Dispatcher function that determines the type of loop based on the following
 * tokens and delegates to the appropriate specialized parser:
 *
 * - `loop { ... }` → infinite loop
 * - `loop [initializers](...) { ... }` → for loop
 * - `loop (condition) { ... }` → while loop
 * - `loop (condition) : (optional_condition) { ... }` → while loop with
 * secondary condition
 *
 * @param parser Pointer to the parser instance
 *
 * @return Pointer to the appropriate loop statement AST node, or NULL on
 * failure
 *
 * @note The specific loop type is determined by the token following 'loop'
 * @note All loop bodies must be block statements
 *
 * @see infinite_loop_stmt(), for_loop_stmt(), parse_expr(), block_stmt(),
 * create_loop_stmt()
 */
Stmt *loop_stmt(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_LOOP, "Expected 'loop' keyword");

  if (p_current(parser).type_ ==
      TOK_LBRACE) { // Aka we have a infinite loop 'loop { ... }'
    return infinite_loop_stmt(parser, line, col);
  }

  if (p_current(parser).type_ ==
      TOK_LBRACKET) { // Aka we have a for loop 'loop [ ... ] { ... }'
    return for_loop_stmt(parser, line, col);
  }

  // else we have a standard while loop 'loop (condition) { ... }'
  p_consume(parser, TOK_LPAREN, "Expected '(' after 'loop' keyword");
  Expr *condition = parse_expr(parser, BP_LOWEST);
  p_consume(parser, TOK_RPAREN, "Expected ')' after loop condition");

  // check if there is an optional condition 'loop (condition) :
  // (optional_condition) { ... }'
  Expr *optional_condition = NULL;
  if (p_current(parser).type_ == TOK_COLON) {
    p_advance(parser); // Advance past the colon
    p_consume(parser, TOK_LPAREN, "Expected '(' after ':' in loop statement");
    optional_condition = parse_expr(parser, BP_LOWEST);
    p_consume(parser, TOK_RPAREN,
              "Expected ')' after optional condition in loop statement");
  }

  Stmt *body = block_stmt(parser);
  return create_loop_stmt(parser->arena, condition, optional_condition, body,
                          line, col);
}

/**
 * @brief Parses print/println statements
 *
 * Handles output statements with the syntax:
 * - `print(expr1, expr2, ...);`
 * - `println(expr1, expr2, ...);`
 *
 * @param parser Pointer to the parser instance
 * @param ln Whether this is a println (true) or print (false) statement
 *
 * @return Pointer to the print statement AST node, or NULL on failure
 *
 * @note Supports multiple expressions separated by commas
 * @note println automatically adds a newline after output
 * @note Empty argument lists are allowed: `print();`
 * @note All expressions are evaluated and their values are printed
 *
 * @see parse_expr(), create_print_stmt()
 */
Stmt *print_stmt(Parser *parser, bool ln) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, ln ? TOK_PRINTLN : TOK_PRINT,
            "Expected 'output' or 'outputln' keyword");
  p_consume(parser, TOK_LPAREN, "Expected '(' after print statement");

  GrowableArray expressions;
  if (!growable_array_init(&expressions, parser->arena, 4, sizeof(Expr *))) {
    fprintf(stderr, "Failed to initialize print expressions array.\n");
    return NULL;
  }

  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RPAREN) {
    Expr *expr = parse_expr(parser, BP_LOWEST);
    if (!expr) {
      fprintf(stderr, "Failed to parse expression in print statement\n");
      return NULL;
    }

    Expr **slot = (Expr **)growable_array_push(&expressions);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing print expressions array\n");
      return NULL;
    }

    *slot = expr;

    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser); // Advance past the comma
    }
  }
  p_consume(parser, TOK_RPAREN, "Expected ')' to end print statement");
  p_consume(parser, TOK_SEMICOLON, "Expected semicolon after print statement");

  return create_print_stmt(parser->arena, (Expr **)expressions.data,
                           expressions.count, ln, line, col);
}

/**
 * @brief Parses break and continue statements
 *
 * Handles loop control statements with the syntax:
 * - `break;` - Exit the current loop
 * - `continue;` - Skip to the next iteration of the current loop
 *
 * @param parser Pointer to the parser instance
 * @param is_continue Whether this is a continue (true) or break (false)
 * statement
 *
 * @return Pointer to the break/continue statement AST node, or NULL on failure
 *
 * @note Both statements require semicolon terminators
 * @note These statements are only valid within loop contexts (enforced at
 * semantic analysis)
 * @note Break exits the innermost enclosing loop
 * @note Continue skips to the next iteration of the innermost enclosing loop
 *
 * @see create_break_continue_stmt()
 */
Stmt *break_continue_stmt(Parser *parser, bool is_continue) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, is_continue ? TOK_CONTINUE : TOK_BREAK,
            is_continue ? "Expected 'continue' keyword"
                        : "Expected 'break' keyword");
  p_consume(parser, TOK_SEMICOLON,
            "Expected semicolon after break/continue statement");

  return create_break_continue_stmt(parser->arena, is_continue, line, col);
}

Stmt *defer_stmt(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_DEFER, "Expected 'defer' keyword");
  Stmt *stmt = parse_stmt(parser);
  if (!stmt) {
    parser_error(parser, "Syntax Error", __FILE__,
                 "Expected statement after 'defer'", line, col, 1);
    return NULL;
  }

  return create_defer_stmt(parser->arena, stmt, line, col);
}

Stmt *switch_stmt(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_SWITCH, "Expected 'switch' keyword");
  p_consume(parser, TOK_LPAREN, "Expected '(' after 'switch' keyword");
  Expr *condition = parse_expr(parser, BP_LOWEST);
  p_consume(parser, TOK_RPAREN, "Expected ')' after switch condition");
  p_consume(parser, TOK_LBRACE, "Expected '{' to start switch body");

  GrowableArray cases;
  if (!growable_array_init(&cases, parser->arena, 4, sizeof(Stmt *))) {
    fprintf(stderr, "Failed to initialize switch cases array.\n");
    return NULL;
  }

  Stmt *default_case = NULL;

  // Parse all cases and default
  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RBRACE) {
    int case_line = p_current(parser).line;
    int case_col = p_current(parser).col;

    // Check if this is the default case
    if (p_current(parser).type_ == TOK_IDENTIFIER &&
        strcmp(get_name(parser), "_") == 0) {
      // Handle default case: _: { ... }
      p_advance(parser); // consume '_'
      p_consume(parser, TOK_RIGHT_ARROW,
                "Expected '=>' after default case '_'");

      Stmt *default_body;
      if (p_current(parser).type_ == TOK_LBRACE) {
        default_body = block_stmt(parser);
      } else {
        default_body = parse_stmt(parser);
      }

      default_case =
          create_default_stmt(parser->arena, default_body, case_line, case_col);
      continue;
    }

    // Parse case values: can be single value or comma-separated list
    GrowableArray case_values;
    if (!growable_array_init(&case_values, parser->arena, 4, sizeof(Expr *))) {
      fprintf(stderr, "Failed to initialize case values array.\n");
      return NULL;
    }

    // Parse first case value
    Expr *case_expr = parse_expr(parser, BP_LOWEST);
    if (!case_expr) {
      fprintf(stderr, "Failed to parse case expression\n");
      return NULL;
    }

    Expr **value_slot = (Expr **)growable_array_push(&case_values);
    if (!value_slot) {
      fprintf(stderr, "Out of memory while growing case values array\n");
      return NULL;
    }
    *value_slot = case_expr;

    // Parse additional case values if comma-separated
    while (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser); // consume comma

      Expr *additional_case = parse_expr(parser, BP_LOWEST);
      if (!additional_case) {
        fprintf(stderr, "Failed to parse additional case expression\n");
        return NULL;
      }

      Expr **additional_slot = (Expr **)growable_array_push(&case_values);
      if (!additional_slot) {
        fprintf(stderr, "Out of memory while growing case values array\n");
        return NULL;
      }
      *additional_slot = additional_case;
    }

    p_consume(parser, TOK_RIGHT_ARROW, "Expected '=>' after case value(s)");

    // Parse case body (either block or single statement)
    Stmt *case_body;
    if (p_current(parser).type_ == TOK_LBRACE) {
      case_body = block_stmt(parser);
    } else {
      // Single statement
      case_body = parse_stmt(parser);
    }

    if (!case_body) {
      fprintf(stderr, "Failed to parse case body\n");
      return NULL;
    }

    // Create case statement and add to cases array
    Stmt *case_stmt =
        create_case_stmt(parser->arena, (AstNode **)case_values.data,
                         case_values.count, case_body, case_line, case_col);

    Stmt **case_slot = (Stmt **)growable_array_push(&cases);
    if (!case_slot) {
      fprintf(stderr, "Out of memory while growing cases array\n");
      return NULL;
    }
    *case_slot = case_stmt;
  }

  p_consume(parser, TOK_RBRACE, "Expected '}' to end switch statement");

  return create_switch_stmt(parser->arena, condition, (AstNode **)cases.data,
                            cases.count, (AstNode *)default_case, line, col);
}
// Impl [fun1: void, fun2: void, ...] -> [struct1, struct2, ...]
Stmt *impl_stmt(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  GrowableArray function_name_list, function_name_types;
  GrowableArray struct_name_list;

  p_consume(parser, TOK_IMPL, "Expected 'impl' keyword");
  p_consume(parser, TOK_LBRACKET, "Expected '[' after 'impl' keyword");

  if (!growable_array_init(&function_name_list, parser->arena, 4,
                           sizeof(Stmt *)) ||
      !growable_array_init(&function_name_types, parser->arena, 4,
                           sizeof(Stmt *))) {
    fprintf(stderr, "Failed to initialize impl function array list\n");
    return NULL;
  }

  if (!growable_array_init(&struct_name_list, parser->arena, 4,
                           sizeof(Stmt *))) {
    fprintf(stderr, "Failed to initialize impl struct array list");
    return NULL;
  }

  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RBRACKET) {
    int list_line = p_current(parser).line;
    int list_col = p_current(parser).col;

    if (p_current(parser).type_ != TOK_IDENTIFIER) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Required identifier", list_line, list_col,
                   p_current(parser).length);
      return NULL;
    }

    char *function_list_name = get_name(parser);
    p_advance(parser);
    p_consume(parser, TOK_COLON, "Expected ':' after parameter name");

    Type *function_list_type = parse_type(parser);
    if (!function_list_type) {
      fprintf(stderr, "Failed to parse type for parameter '%s'\n",
              function_list_name);
      return NULL;
    }
    p_advance(parser);

    char **name_identifier = (char **)growable_array_push(&function_name_list);
    Type **type_specifier = (Type **)growable_array_push(&function_name_types);

    if (!name_identifier || !type_specifier) {
      fprintf(stderr, "Out of memory while growing function list for impl\n");
      return NULL;
    }
    *name_identifier = function_list_name;
    *type_specifier = function_list_type;

    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser);
    }
  }
  p_consume(parser, TOK_RBRACKET, "Expected ']' after function parameters");
  p_consume(parser, TOK_RIGHT_ARROW, "Expected '->' after function list");
  p_consume(parser, TOK_LBRACKET, "Expected '[' after '->'");

  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RBRACKET) {
    int list_line = p_current(parser).line;
    int list_col = p_current(parser).col;

    if (p_current(parser).type_ != TOK_IDENTIFIER) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Required identifier", list_line, list_col,
                   p_current(parser).length);
      return NULL;
    }
    char *struct_list_name = get_name(parser);
    p_advance(parser);
    char **struct_identifier = (char **)growable_array_push(&struct_name_list);

    if (!struct_identifier) {
      fprintf(stderr, "Out of memory while growing struct list for impl\n");
      return NULL;
    }
    *struct_identifier = struct_list_name;
    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser);
    }
  }
  p_consume(parser, TOK_RBRACKET, "Expected ']' after struct list");
  Stmt *body = block_stmt(parser);
  return create_impl_stmt(parser->arena, (char **)function_name_list.data,
                          (AstNode **)function_name_types.data, body,
                          (char **)struct_name_list.data,
                          function_name_list.count, struct_name_list.count,
                          line, col);
}
