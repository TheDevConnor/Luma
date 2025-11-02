#include <stdio.h>

#include "../ast.h"

AstNode *create_basic_type(ArenaAllocator *arena, const char *name, size_t line, size_t column) {
  AstNode *node = create_type_node(arena, AST_TYPE_BASIC, line, column);
  node->type_data.basic.name = name; // Store the name directly
  return node;
}

AstNode *create_pointer_type(ArenaAllocator *arena, AstNode *pointee_type, size_t line, size_t column) {
  AstNode *node = create_type_node(arena, AST_TYPE_POINTER, line, column);
  node->type_data.pointer.pointee_type = pointee_type;
  return node;
}

AstNode *create_array_type(ArenaAllocator *arena, AstNode *element_type, Expr *size, size_t line, size_t column) {
  AstNode *node = create_type_node(arena, AST_TYPE_ARRAY, line, column);
  node->type_data.array.element_type = element_type;
  node->type_data.array.size = size;
  return node;
}

AstNode *create_function_type(ArenaAllocator *arena, AstNode **param_types, size_t param_count, AstNode *return_type, size_t line, size_t column) {
  AstNode *node = create_type_node(arena, AST_TYPE_FUNCTION, line, column);
  node->type_data.function.param_types = param_types;
  node->type_data.function.param_count = param_count;
  node->type_data.function.return_type = return_type;
  return node;
}

AstNode *create_resolution_type(ArenaAllocator *arena, char **parts, size_t part_count, size_t line, size_t column) {
  AstNode *node = create_type_node(arena, AST_TYPE_RESOLUTION, line, column);
  node->type_data.resolution.parts = parts;
  node->type_data.resolution.part_count = part_count;
  return node;
}