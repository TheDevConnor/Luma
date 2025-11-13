#include <stdio.h>

#include "../ast/ast.h"
#include "parser.h"

// *Type
Type *pointer(Parser *parser) {
  // parse_type() will handle advancing through the pointee type
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
  int line = p_current(parser).line;
  int col = p_current(parser).col;
  
  Type *element_type = parse_type(parser);

  p_consume(parser, TOK_SEMICOLON, "Expected ';' after array element type");
  Expr *size_expr = parse_expr(parser, BP_LOWEST);

  p_consume(parser, TOK_RBRACKET, "Expected ']' to close array type declaration");

  return create_array_type(parser->arena, element_type, size_expr, line, col);
}

// Handle namespace::Type resolution
// Returns a type and DOES advance past all consumed tokens
Type *resolution_type(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;
  
  // We start with an identifier (the namespace or first part)
  char *first_name = get_name(parser);
  p_advance(parser); // Consume the identifier
  
  // Check if we have a resolution operator
  if (p_current(parser).type_ != TOK_RESOLVE) {
    // Just a simple identifier type, not a resolution
    // We already advanced, so we're done
    return create_basic_type(parser->arena, first_name, line, col);
  }
  
  // Build the resolution chain: namespace::name or namespace::sub::name
  GrowableArray parts;
  if (!growable_array_init(&parts, parser->arena, 4, sizeof(char *))) {
    fprintf(stderr, "Failed to initialize resolution parts array\n");
    return NULL;
  }
  
  // Add the first part (namespace)
  char **slot = (char **)growable_array_push(&parts);
  if (!slot) {
    fprintf(stderr, "Out of memory while growing resolution parts\n");
    return NULL;
  }
  *slot = first_name;
  
  // Parse remaining parts separated by '::'
  while (p_current(parser).type_ == TOK_RESOLVE) {
    p_advance(parser); // Consume '::'
    
    if (p_current(parser).type_ != TOK_IDENTIFIER) {
      parser_error(parser, "SyntaxError", parser->file_path,
                   "Expected identifier after '::'",
                   p_current(parser).line, p_current(parser).col,
                   p_current(parser).length);
      return NULL;
    }
    
    char *part = get_name(parser);
    p_advance(parser); // Consume the identifier
    
    slot = (char **)growable_array_push(&parts);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing resolution parts\n");
      return NULL;
    }
    *slot = part;
  }
  
  // Create a resolution type with the collected parts
  return create_resolution_type(parser->arena, (char **)parts.data, 
                                parts.count, line, col);
}

// tnud() is responsible for parsing a type and ADVANCING past it
Type *tnud(Parser *parser) {
  int line = p_current(parser).line;
  int col = p_current(parser).col;
  
  switch (p_current(parser).type_) {
  case TOK_INT:
    p_advance(parser);
    return create_basic_type(parser->arena, "int", line, col);
  case TOK_UINT:
    p_advance(parser);
    return create_basic_type(parser->arena, "uint", line, col);
  case TOK_DOUBLE:
    p_advance(parser);
    return create_basic_type(parser->arena, "double", line, col);
  case TOK_FLOAT:
    p_advance(parser);
    return create_basic_type(parser->arena, "float", line, col);
  case TOK_BOOL:
    p_advance(parser);
    return create_basic_type(parser->arena, "bool", line, col);
  case TOK_STRINGT:
    p_advance(parser);
    return create_basic_type(parser->arena, "str", line, col);
  case TOK_VOID:
    p_advance(parser);
    return create_basic_type(parser->arena, "void", line, col);
  case TOK_CHAR:
    p_advance(parser);
    return create_basic_type(parser->arena, "char", line, col);
  case TOK_STAR:       // Pointer type
    p_advance(parser); // Consume the '*' token
    return pointer(parser);
  case TOK_LBRACKET:   // Array type
    p_advance(parser); // Consume the '[' token
    return array_type(parser);
  case TOK_IDENTIFIER: // Could be simple type or namespace::Type
    return resolution_type(parser); // This handles its own advancing
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