#include <stdio.h>

#include "type.h"

// Updated typecheck function in tc.c
bool typecheck(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  if (!node) {
    fprintf(stderr, "Error: Null AST node\n");
    return false;
  }

  switch (node->category) {
  case Node_Category_STMT:
    return typecheck_statement(node, scope, arena);

  case Node_Category_EXPR: {
    AstNode *result_type = typecheck_expression(node, scope, arena);
    return result_type != NULL;
  }

  case Node_Category_TYPE:
    printf("DEBUG: Type node - returning true\n");
    return true;

  case Node_Category_PREPROCESSOR:
    switch (node->type) {
    case AST_PREPROCESSOR_MODULE:
      return typecheck_module_stmt(node, scope, arena);
    case AST_PREPROCESSOR_USE:
      return typecheck_use_stmt(node, scope, scope, arena);
    default:
      fprintf(stderr, "Error: Unknown preprocessor node type %d\n", node->type);
      return false;
    }

  default:
    fprintf(stderr, "Error: Unknown node category %d\n", node->category);
    return false;
  }
}
