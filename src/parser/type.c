#include <stdio.h>

#include "../ast/ast.h"
#include "parser.h"

// *Type
Type *pointer(Parser *parser) {
  // We already advanced past the '*' token
  Type *pointee_type = parse_type(parser);
  if (!pointee_type) {
    fprintf(stderr, "Expected type after '*'\n");
    return NULL;
  }

  return create_pointer_type(parser->arena, pointee_type,
                             p_current(parser).line, p_current(parser).col);
}

// [Type; Size]
Type *array_type(Parser *parser) {
  Type *element_type = parse_type(parser);
  p_advance(parser); // Consume the element type

  p_consume(parser, TOK_SEMICOLON, "Expected ';' after array element type");
  Expr *size_expr = parse_expr(parser, BP_LOWEST);

  if (p_current(parser).type_ != TOK_RBRACKET) {
    parser_error(parser, "SyntaxError", "Unknown",
                 "Expected ']' to close array type declaration",
                 p_current(parser).line, p_current(parser).col,
                 CURRENT_TOKEN_LENGTH(parser));
    return NULL; // Error, return NULL
  }

  return create_array_type(parser->arena, element_type, size_expr,
                           p_current(parser).line, p_current(parser).col);
}

Type *tnud(Parser *parser) {
  switch (p_current(parser).type_) {
  case TOK_INT:
    return create_basic_type(parser->arena, "int", p_current(parser).line,
                             p_current(parser).col);
  case TOK_UINT:
    return create_basic_type(parser->arena, "uint", p_current(parser).line,
                             p_current(parser).col);
  case TOK_DOUBLE:
    return create_basic_type(parser->arena, "double", p_current(parser).line,
                             p_current(parser).col);
  case TOK_FLOAT:
    return create_basic_type(parser->arena, "float", p_current(parser).line,
                             p_current(parser).col);
  case TOK_BOOL:
    return create_basic_type(parser->arena, "bool", p_current(parser).line,
                             p_current(parser).col);
  case TOK_STRINGT:
    return create_basic_type(parser->arena, "str", p_current(parser).line,
                             p_current(parser).col);
  case TOK_VOID:
    return create_basic_type(parser->arena, "void", p_current(parser).line,
                             p_current(parser).col);
  case TOK_CHAR:
    return create_basic_type(parser->arena, "char", p_current(parser).line,
                             p_current(parser).col);
  case TOK_STAR:       // Pointer type
    p_advance(parser); // Consume the '*' token
    return pointer(parser);
  case TOK_LBRACKET:   // Array type
    p_advance(parser); // Consume the '[' token
    return array_type(parser);
  default:
    fprintf(stderr, "Unexpected token in type: %d\n", p_current(parser).type_);
    return NULL;
  }
}

Type *tled(Parser *parser, Type *left, BindingPower bp) {
  (void)left;
  (void)bp; // Suppress unused variable warnings
  fprintf(stderr, "Parsing type led: %.*s\n", CURRENT_TOKEN_LENGTH(parser),
          CURRENT_TOKEN_VALUE(parser));
  return NULL; // No valid type found
}
