#include "llvm.h"
#include <llvm-c/Core.h>

LLVMTypeRef codegen_type_basic(CodeGenContext *ctx, AstNode *node) {
  const char *type_name = node->type_data.basic.name;
  if (strcmp(type_name, "int") == 0) {
    return LLVMInt64TypeInContext(ctx->context);
  } else if (strcmp(type_name, "float") == 0) {
    return LLVMFloatTypeInContext(ctx->context);
  } else if (strcmp(type_name, "double") == 0) {
    return LLVMDoubleTypeInContext(ctx->context);
  } else if (strcmp(type_name, "bool") == 0) {
    return LLVMInt1TypeInContext(ctx->context);
  } else if (strcmp(type_name, "void") == 0) {
    return LLVMVoidTypeInContext(ctx->context);
  } else if (strcmp(type_name, "str") == 0) {
    return LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
  } else if (strcmp(type_name, "char") == 0) {
    return LLVMInt8TypeInContext(ctx->context);
  }
  return NULL;
}

LLVMTypeRef codegen_type_pointer(CodeGenContext *ctx, AstNode *node) {
  LLVMTypeRef pointee = codegen_type(ctx, node->type_data.pointer.pointee_type);
  if (pointee) {
    return LLVMPointerType(pointee, 0);
  }
  return NULL;
}

LLVMTypeRef codegen_type_array(CodeGenContext *ctx, AstNode *node) {
  LLVMTypeRef element_type =
      codegen_type(ctx, node->type_data.array.element_type);
  if (element_type) {
    // For now, assume size is a literal integer
    if (node->type_data.array.size &&
        node->type_data.array.size->type == AST_EXPR_LITERAL &&
        node->type_data.array.size->expr.literal.lit_type == LITERAL_INT) {
      unsigned size =
          (unsigned)node->type_data.array.size->expr.literal.value.int_val;
      return LLVMArrayType(element_type, size);
    }
  }
  return NULL;
}

LLVMTypeRef codegen_type_function(CodeGenContext *ctx, AstNode *node) {
  LLVMTypeRef return_type =
      codegen_type(ctx, node->type_data.function.return_type);
  if (return_type) {
    LLVMTypeRef *param_types = (LLVMTypeRef *)arena_alloc(
        ctx->arena, sizeof(LLVMTypeRef) * node->type_data.function.param_count,
        alignof(LLVMTypeRef *));

    for (size_t i = 0; i < node->type_data.function.param_count; i++) {
      param_types[i] =
          codegen_type(ctx, node->type_data.function.param_types[i]);
      if (!param_types[i]) {
        return NULL;
      }
    }

    return LLVMFunctionType(return_type, param_types,
                            node->type_data.function.param_count, false);
  }
  return NULL;
}
