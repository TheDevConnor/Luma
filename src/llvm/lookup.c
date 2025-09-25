#include "llvm.h"

LLVMValueRef codegen_expr(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->category != Node_Category_EXPR) {
    return NULL;
  }

  switch (node->type) {
  case AST_EXPR_LITERAL:
    return codegen_expr_literal(ctx, node);
  case AST_EXPR_IDENTIFIER:
    return codegen_expr_identifier(ctx, node);
  case AST_EXPR_BINARY:
    return codegen_expr_binary(ctx, node);
  case AST_EXPR_UNARY:
    return codegen_expr_unary(ctx, node);
  case AST_EXPR_CALL:
    return codegen_expr_call(ctx, node);
  case AST_EXPR_ASSIGNMENT:
    return codegen_expr_assignment(ctx, node);
  case AST_EXPR_GROUPING:
    return codegen_expr(ctx, node->expr.grouping.expr);
  case AST_EXPR_INDEX:
    return codegen_expr_index(ctx, node);
  case AST_EXPR_ARRAY:
    return codegen_expr_array(ctx, node);
  case AST_EXPR_CAST:
    return codegen_expr_cast(ctx, node);
  case AST_EXPR_SIZEOF:
    return codegen_expr_sizeof(ctx, node);
  case AST_EXPR_ALLOC:
    return codegen_expr_alloc(ctx, node);
  case AST_EXPR_FREE:
    return codegen_expr_free(ctx, node);
  case AST_EXPR_DEREF:
    return codegen_expr_deref(ctx, node);
  case AST_EXPR_ADDR:
    return codegen_expr_addr(ctx, node);
  case AST_EXPR_MEMBER:
    // Enhanced member access that handles both module.symbol and struct.field
    return codegen_expr_member_access_enhanced(ctx, node);
  default:
    fprintf(stderr, "Error: Unknown expression type: %d\n", node->type);
    return NULL;
  }
}

LLVMValueRef codegen_stmt(CodeGenContext *ctx, AstNode *node) {
  if (!node)
    return NULL;

  switch (node->type) {
  case AST_PROGRAM:
    // Use the new multi-module handler instead of the old one
    return codegen_stmt_program_multi_module(ctx, node);

  case AST_PREPROCESSOR_MODULE:
    // Handle individual module declarations
    return codegen_stmt_module(ctx, node);

  case AST_PREPROCESSOR_USE:
    // Handle @use directives
    return codegen_stmt_use(ctx, node);

  case AST_STMT_EXPRESSION:
    return codegen_stmt_expression(ctx, node);
  case AST_STMT_VAR_DECL:
    return codegen_stmt_var_decl(ctx, node);
  case AST_STMT_FUNCTION:
    return codegen_stmt_function(ctx, node);
  case AST_STMT_STRUCT:
    return codegen_stmt_struct(ctx, node);
  case AST_STMT_FIELD_DECL:
    return codegen_stmt_field(ctx, node);
  case AST_STMT_ENUM:
    return codegen_stmt_enum(ctx, node);
  case AST_STMT_RETURN:
    return codegen_stmt_return(ctx, node);
  case AST_STMT_BLOCK:
    return codegen_stmt_block(ctx, node);
  case AST_STMT_IF:
    return codegen_stmt_if(ctx, node);
  case AST_STMT_PRINT:
    return codegen_stmt_print(ctx, node);
  case AST_STMT_DEFER:
    return codegen_stmt_defer(ctx, node);
  case AST_STMT_LOOP:
    return codegen_loop(ctx, node);
  case AST_STMT_BREAK_CONTINUE:
    return codegen_stmt_break_continue(ctx, node);
  case AST_STMT_SWITCH:
    return codegen_stmt_switch(ctx, node);
  case AST_STMT_CASE:
    return codegen_stmt_case(ctx, node);
  case AST_STMT_DEFAULT:
    return codegen_stmt_default(ctx, node);
  default:
    fprintf(stderr, "Error: Unknown statement type: %d\n", node->type);
    return NULL;
  }
}

LLVMTypeRef codegen_type(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->category != Node_Category_TYPE) {
    return NULL;
  }

  switch (node->type) {
  case AST_TYPE_BASIC:
    return codegen_type_basic(ctx, node);
  case AST_TYPE_POINTER:
    return codegen_type_pointer(ctx, node);
  case AST_TYPE_ARRAY:
    return codegen_type_array(ctx, node);
  case AST_TYPE_FUNCTION:
    return codegen_type_function(ctx, node);
  default:
    fprintf(stderr, "Error: Unknown type: %d\n", node->type);
    return NULL;
  }
}
