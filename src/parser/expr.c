/**
 * @file expr.c
 * @brief Expression parsing implementation for the programming language
 * compiler
 *
 * This file contains implementations for parsing all types of expressions in
 * the programming language. It works in conjunction with the Pratt parser
 * implementation in parser.c to handle operator precedence and associativity
 * correctly.
 *
 * The expression parser handles:
 * - Primary expressions: literals, identifiers
 * - Unary expressions: prefix and postfix operators
 * - Binary expressions: arithmetic, logical, comparison operators
 * - Function call expressions with argument lists
 * - Assignment expressions
 * - Member access and array indexing expressions
 * - Grouping expressions with parentheses
 * - Array literal expressions
 * - Adder, Deref, Alloc, Free, Cast, Sizeof
 *
 * All parsing functions follow the Pratt parser pattern, where expressions are
 * built recursively based on operator precedence (binding power). The functions
 * correspond to null denotation (nud) and left denotation (led) operations.
 *
 * @author Connor Harris
 * @date 2025
 * @version 1.0
 */

#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

Expr *primary(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  Token current = p_current(parser);
  LiteralType lit_type = PRIMARY_LITERAL_TYPE_MAP[current.type_];

  if (lit_type != LITERAL_NULL) {
    void *value = NULL;
    switch (lit_type) {
    case LITERAL_INT:
      value = arena_alloc(parser->arena, sizeof(long long), alignof(long long));
      *(long long *)value = strtoll(current.value, NULL, 10);
      break;
    case LITERAL_FLOAT:
      value = arena_alloc(parser->arena, sizeof(double), alignof(double));
      *(double *)value = strtod(current.value, NULL);
      break;
    case LITERAL_STRING:
      value = arena_alloc(parser->arena, strlen(get_name(parser)) + 1,
                          alignof(char));
      strcpy((char *)value, get_name(parser));
      break;
    case LITERAL_CHAR:
      value = arena_alloc(parser->arena, sizeof(char), alignof(char));
      *(char *)value = current.value[0]; // Assume single character
      break;
    case LITERAL_BOOL:
      value = arena_alloc(parser->arena, sizeof(bool), alignof(bool));
      *(bool *)value = (strcmp(current.value, "true") == 0);
      break;
    case LITERAL_IDENT:
      value = get_name(parser); // Get the identifier name
      break;
    default:
      value = NULL; // Handle null or unsupported literal types
      break;
    }
    p_advance(parser); // Consume the token

    if (lit_type == LITERAL_IDENT) {
      return create_identifier_expr(parser->arena, (char *)value, line, col);
    }
    return create_literal_expr(parser->arena, lit_type, value, line, col);
  }

  return NULL; // Handle error or unsupported literal type
}

Expr *unary(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  Token current = p_current(parser);
  UnaryOp op = TOKEN_TO_UNOP_MAP[current.type_];

  if (op) {
    p_advance(parser); // Consume the token
    Expr *operand = parse_expr(parser, BP_UNARY);
    return create_unary_expr(parser->arena, op, operand, line, col);
  }

  return NULL;
}

Expr *grouping(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_LPAREN, "Expected '(' for grouping");
  Expr *expr = parse_expr(parser, BP_LOWEST);
  p_consume(parser, TOK_RPAREN, "Expected ')' to close grouping");
  return create_grouping_expr(parser->arena, expr, line, col);
}

Expr *binary(Parser *parser, Expr *left, BindingPower bp) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  Token current = p_current(parser);
  BinaryOp op = TOKEN_TO_BINOP_MAP[current.type_];
  p_advance(parser); // Consume the token
  Expr *right = parse_expr(parser, bp);

  return create_binary_expr(parser->arena, op, left, right, line, col);
}

Expr *call_expr(Parser *parser, Expr *left, BindingPower bp) {
  (void)bp; // Unused parameter, can be removed if not needed
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  GrowableArray args;
  if (!growable_array_init(&args, parser->arena, 4, sizeof(Expr *))) {
    fprintf(stderr, "Failed to initialize call arguments\n");
  }

  p_consume(parser, TOK_LPAREN, "Expected '(' for function call");
  while (p_current(parser).type_ != TOK_RPAREN) {
    Expr *arg = parse_expr(parser, BP_LOWEST);
    if (!arg) {
      fprintf(stderr, "Expected expression inside function call\n");
      return NULL;
    }
    Expr **slot = (Expr **)growable_array_push(&args);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing call arguments\n");
      return NULL;
    }
    *slot = arg;
    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser); // Consume the comma
    }
  }
  p_consume(parser, TOK_RPAREN, "Expected ')' to close function call");

  return create_call_expr(parser->arena, left, (Expr **)args.data, args.count,
                          line, col);
}

Expr *assign_expr(Parser *parser, Expr *left, BindingPower bp) {
  (void)bp; // Unused parameter, can be removed if not needed
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  if (p_current(parser).type_ != TOK_EQUAL) {
    fprintf(stderr, "Expected '=' for assignment\n");
    return NULL;
  }
  p_advance(parser); // Consume the '=' token

  Expr *value = parse_expr(parser, BP_ASSIGN);
  if (!value) {
    parser_error(parser, "Assignment Error", "parser.c",
                 "Failed to parse assignment value", line, col,
                 p_current(parser).length);
    return NULL;
  }

  return create_assignment_expr(parser->arena, left, value, line, col);
}

Expr *prefix_expr(Parser *parser, Expr *left, BindingPower bp) {
  (void)bp; // Unused parameter, can be removed if not needed
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  Token current = p_current(parser);
  switch (current.type_) {
  case TOK_LBRACKET:
    p_advance(parser); // Consume the '[' token
    Expr *index = parse_expr(parser, BP_LOWEST);
    if (!index) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Expected expression inside index", p_current(parser).line,
                   p_current(parser).col, p_current(parser).length);
      return NULL;
    }
    p_consume(parser, TOK_RBRACKET, "Expected ']' to close index expression");
    return create_index_expr(parser->arena, left, index, line, col);

  case TOK_DOT:
  case TOK_RESOLVE: {
    bool is_compiletime =
        (p_current(parser).type_ == TOK_RESOLVE) ? true : false;
    Token op_token = p_current(parser);
    p_advance(parser); // Consume the '.' or '::' token

    if (p_current(parser).type_ != TOK_IDENTIFIER) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Expected identifier after '::' or '.' for member access",
                   p_current(parser).line, p_current(parser).col,
                   p_current(parser).length);
      return NULL;
    }

    char *member = get_name(parser);
    p_advance(parser); // Consume the identifier token
    return create_member_expr(parser->arena, left, is_compiletime, member,
                              op_token.line, op_token.col);
  }

  case TOK_PLUSPLUS:
  case TOK_MINUSMINUS: {
    UnaryOp op =
        (current.type_ == TOK_PLUSPLUS) ? UNOP_POST_INC : UNOP_POST_DEC;
    p_advance(parser); // Consume the token
    return create_unary_expr(parser->arena, op, left, line, col);
  }

  default: {
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg),
             "Unexpected token for prefix expression: %s", current.value);

    parser_error(parser, "SyntaxError", parser->file_path, error_msg,
                 current.line, current.col, current.length);
    return NULL;
  }
  }
}

Expr *array_expr(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  GrowableArray elements;
  if (!growable_array_init(&elements, parser->arena, 4, sizeof(Expr *))) {
    fprintf(stderr, "Failed to initialize array elements\n");
    return NULL;
  }

  p_consume(parser, TOK_LBRACKET, "Expected '[' for array expression");
  while (p_current(parser).type_ != TOK_RBRACKET) {
    Expr *element = parse_expr(parser, BP_LOWEST);
    if (!element) {
      fprintf(stderr, "Expected expression inside array\n");
      return NULL;
    }

    Expr **slot = (Expr **)growable_array_push(&elements);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing array elements\n");
      return NULL;
    }

    *slot = element;

    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser); // Consume the comma
    }
  }
  p_consume(parser, TOK_RBRACKET, "Expected ']' to close array expression");

  return create_array_expr(parser->arena, (Expr **)elements.data,
                           elements.count, line, col);
}

// Anonymous struct initialization: { x: 20, y: 50 }
Expr *struct_expr(Parser *parser) {
  // When we get here from nud(), we're AT the '{' token
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_LBRACE, "Expected '{' for struct expression");

  GrowableArray field_names, field_values;
  if (!growable_array_init(&field_names, parser->arena, 4, sizeof(char *)) ||
      !growable_array_init(&field_values, parser->arena, 4, sizeof(Expr *))) {
    fprintf(stderr, "Failed to initialize struct field arrays\n");
    return NULL;
  }

  // Parse field initializers: field_name: value, ...
  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RBRACE) {
    // Parse field name
    if (p_current(parser).type_ != TOK_IDENTIFIER) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Expected field name in struct expression",
                   p_current(parser).line, p_current(parser).col,
                   p_current(parser).length);
      return NULL;
    }

    char *field_name = get_name(parser);
    p_advance(parser); // Consume the field name

    p_consume(parser, TOK_COLON, "Expected ':' after field name");

    // Parse field value
    Expr *field_value = parse_expr(parser, BP_LOWEST);
    if (!field_value) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Expected expression for field value",
                   p_current(parser).line, p_current(parser).col,
                   p_current(parser).length);
      return NULL;
    }

    // Store field name and value
    char **name_slot = (char **)growable_array_push(&field_names);
    Expr **value_slot = (Expr **)growable_array_push(&field_values);

    if (!name_slot || !value_slot) {
      fprintf(stderr, "Out of memory while growing struct field arrays\n");
      return NULL;
    }

    *name_slot = field_name;
    *value_slot = field_value;

    // Handle comma separator
    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser); // Consume the comma
    } else if (p_current(parser).type_ != TOK_RBRACE) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Expected ',' or '}' after field value",
                   p_current(parser).line, p_current(parser).col,
                   p_current(parser).length);
      return NULL;
    }
  }

  p_consume(parser, TOK_RBRACE, "Expected '}' to close struct expression");

  // Anonymous struct (name is NULL)
  return create_struct_expr(parser->arena, NULL, (char **)field_names.data,
                            (AstNode **)field_values.data, field_names.count,
                            line, col);
}

// Named struct initialization: Point { x: 20, y: 50 }
// This is called from led() when we see '{' after an identifier
Expr *named_struct_expr(Parser *parser, Expr *left, BindingPower bp) {
  (void)bp; // Unused parameter

  // left should be an identifier expression containing the struct name
  if (left->type != AST_EXPR_IDENTIFIER) {
    parser_error(parser, "SyntaxError", parser->file_path,
                 "Expected identifier before '{' for named struct",
                 p_current(parser).line, p_current(parser).col,
                 p_current(parser).length);
    return NULL;
  }

  char *struct_name = left->expr.identifier.name;
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_LBRACE, "Expected '{' for struct expression");

  GrowableArray field_names, field_values;
  if (!growable_array_init(&field_names, parser->arena, 4, sizeof(char *)) ||
      !growable_array_init(&field_values, parser->arena, 4, sizeof(Expr *))) {
    fprintf(stderr, "Failed to initialize struct field arrays\n");
    return NULL;
  }

  // Parse field initializers: field_name: value, ...
  while (p_has_tokens(parser) && p_current(parser).type_ != TOK_RBRACE) {
    // Parse field name
    if (p_current(parser).type_ != TOK_IDENTIFIER) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Expected field name in struct expression",
                   p_current(parser).line, p_current(parser).col,
                   p_current(parser).length);
      return NULL;
    }

    char *field_name = get_name(parser);
    p_advance(parser); // Consume the field name

    p_consume(parser, TOK_COLON, "Expected ':' after field name");

    // Parse field value
    Expr *field_value = parse_expr(parser, BP_LOWEST);
    if (!field_value) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Expected expression for field value",
                   p_current(parser).line, p_current(parser).col,
                   p_current(parser).length);
      return NULL;
    }

    // Store field name and value
    char **name_slot = (char **)growable_array_push(&field_names);
    Expr **value_slot = (Expr **)growable_array_push(&field_values);

    if (!name_slot || !value_slot) {
      fprintf(stderr, "Out of memory while growing struct field arrays\n");
      return NULL;
    }

    *name_slot = field_name;
    *value_slot = field_value;

    // Handle comma separator
    if (p_current(parser).type_ == TOK_COMMA) {
      p_advance(parser); // Consume the comma
    } else if (p_current(parser).type_ != TOK_RBRACE) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Expected ',' or '}' after field value",
                   p_current(parser).line, p_current(parser).col,
                   p_current(parser).length);
      return NULL;
    }
  }

  p_consume(parser, TOK_RBRACE, "Expected '}' to close struct expression");

  // Named struct
  return create_struct_expr(
      parser->arena, struct_name, (char **)field_names.data,
      (AstNode **)field_values.data, field_names.count, line, col);
}

Expr *deref_expr(Parser *parser) {
  p_advance(parser); // Advance past the *
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  Expr *object = parse_expr(parser, BP_UNARY);

  return create_deref_expr(parser->arena, object, line, col);
}

Expr *addr_expr(Parser *parser) {
  p_advance(parser); // Advance past the &
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  Expr *object = parse_expr(parser, BP_NONE);

  return create_addr_expr(parser->arena, object, line, col);
}

// void *alloc(size_t size);
Expr *alloc_expr(Parser *parser) {
  p_advance(parser); // Advance past the alloc
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_LPAREN,
            "Expected an '(' before you define your size for alloc.");
  Expr *size = parse_expr(parser, BP_NONE);
  p_consume(parser, TOK_RPAREN,
            "Expected an ')' after you define your size for alloc.");

  return create_alloc_expr(parser->arena, size, line, col);
}

// void free(void *ptr)
Expr *free_expr(Parser *parser) {
  p_advance(parser); // Advance past the memcpy
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_LPAREN,
            "Expected an '(' before you pass your variable to free.");
  Expr *ptr = parse_expr(parser, BP_NONE);
  p_consume(parser, TOK_RPAREN,
            "Expected an ')' after you pass your variable to free.");

  return create_free_expr(parser->arena, ptr, line, col);
}

// cast<TYPE>(value);
Expr *cast_expr(Parser *parser) {
  p_advance(parser); // Advance past the cast
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_LT,
            "Expected a '<' before you declare the type you want to cast too.");
  Type *cast_type = parse_type(parser);
  p_advance(parser);
  p_consume(parser, TOK_GT,
            "Expected a '>' after defining the type you want to cast too, but "
            "before defining what you are casting");
  p_consume(parser, TOK_LPAREN,
            "Expected a '(' before defining what you are casting");
  Expr *castee = parse_expr(parser, BP_NONE);
  p_consume(parser, TOK_RPAREN,
            "Expected a ')' after defining what you are casting");

  return create_cast_expr(parser->arena, cast_type, castee, line, col);
}

// input<TYPE>(msg);
Expr *input_expr(Parser *parser) {
  p_advance(parser); // Advance past the cast
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_LT,
            "Expected a '<' before you declare the type you want to cast too.");
  Type *type = parse_type(parser);
  p_advance(parser);
  p_consume(parser, TOK_GT,
            "Expected a '>' after defining the type you want to cast too, but "
            "before defining what you are casting");
  p_consume(parser, TOK_LPAREN,
            "Expected a '(' before defining what you are casting");
  Expr *msg = parse_expr(parser, BP_NONE);
  p_consume(parser, TOK_RPAREN,
            "Expected a ')' after defining what you are casting");

  return create_input_expr(parser->arena, type, msg, line, col);
}

Expr *system_expr(Parser *parser) {
  p_advance(parser);
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(parser, TOK_LPAREN,
            "Expected a '(' before you give your system command.");
  Expr *command = parse_expr(parser, BP_NONE);
  p_consume(parser, TOK_RPAREN,
            "Expected a ')' after you give your system command.");

  return create_system_expr(parser->arena, command, line, col);
}

Expr *syscall_expr(Parser *parser) {
  p_advance(parser);
  int line = p_current(parser).line;
  int col = p_current(parser).col;

  p_consume(
      parser, TOK_LPAREN,
      "Expected a '(' before you give the params for the syscall command");

  GrowableArray args;
  if (!growable_array_init(&args, parser->arena, 2, sizeof(Expr *))) {
    fprintf(stderr, "Failed to initialize array elements\n");
    return NULL;
  }

  while (p_current(parser).type_ != TOK_RPAREN) {
    Expr *arg = parse_expr(parser, BP_NONE);
    if (!arg) {
      fprintf(stderr, "Expected expression inside array\n");
      return NULL;
    }

    Expr **slot = (Expr **)growable_array_push(&args);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing array elements\n");
      return NULL;
    }

    *slot = arg;

    if (p_current(parser).type_ == TOK_COMMA)
      p_advance(parser);
  }
  p_consume(parser, TOK_RPAREN,
            "Expected a ')' after you give your arguments for syscall.");

  return create_syscall_expr(parser->arena, (Expr **)args.data, args.count,
                             line, col);
}

// size_t sizeof(TYPE);
// sizeof<int>         Compile-time constant
// sizeof<[10]int>     Compile-time constant
// sizeof<[n]int>      Runtime when n is variable
// sizeof<MyStruct>    Compile-time constant
Expr *sizeof_expr(Parser *parser) {
  p_advance(parser); // Advance past the sizeof
  int line = p_current(parser).line;
  int col = p_current(parser).col;
  AstNode *object = NULL;
  bool is_type = false;

  p_consume(parser, TOK_LT,
            "Expected a '<' before defining the var or type you want to get "
            "the size of.");
  if (parse_type(parser) != NULL) {
    ;
    object = parse_type(parser);
    p_advance(parser);
    is_type = true;
  } else {
    object = parse_expr(parser, BP_NONE);
  }
  p_consume(parser, TOK_GT,
            "Expected a '>' after defining the var or type you want to get the "
            "size of.");

  return create_sizeof_expr(parser->arena, object, is_type, line, col);
}
