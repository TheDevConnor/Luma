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
  case AST_EXPR_MEMBER: // NEW: Handle module.symbol syntax
    return codegen_expr_member_access(ctx, node);
  default:
    return NULL;
  }
}

LLVMValueRef codegen_stmt(CodeGenContext *ctx, AstNode *node) {
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
  case AST_STMT_RETURN:
    return codegen_stmt_return(ctx, node);
  case AST_STMT_BLOCK:
    return codegen_stmt_block(ctx, node);
  case AST_STMT_IF:
    return codegen_stmt_if(ctx, node);
  case AST_STMT_PRINT:
    return codegen_stmt_print(ctx, node);
  default:
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
    return NULL;
  }
}
