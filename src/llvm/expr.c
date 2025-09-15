#include "llvm.h"

// Add these utility functions to help with consistent range creation and type
// management You can put these in expr.c or create a new ranges.c file

LLVMTypeRef get_range_struct_type(CodeGenContext *ctx,
                                  LLVMTypeRef element_type) {
  // Create or reuse a range struct type for the given element type
  // This ensures all ranges with the same element type use the same struct
  // layout
  LLVMTypeRef field_types[] = {element_type, element_type};
  return LLVMStructTypeInContext(ctx->context, field_types, 2, false);
}

LLVMValueRef create_range_struct(CodeGenContext *ctx, LLVMValueRef start,
                                 LLVMValueRef end) {
  // Helper function to create a range struct consistently
  // This can be used in your BINOP_RANGE case and elsewhere

  LLVMTypeRef element_type =
      LLVMTypeOf(start); // Assume both operands have same type
  LLVMTypeRef range_struct_type = get_range_struct_type(ctx, element_type);

  // Allocate space for the range struct
  LLVMValueRef range_alloca =
      LLVMBuildAlloca(ctx->builder, range_struct_type, "range");

  // Store the start value
  LLVMValueRef start_ptr = LLVMBuildStructGEP2(ctx->builder, range_struct_type,
                                               range_alloca, 0, "start_ptr");
  LLVMBuildStore(ctx->builder, start, start_ptr);

  // Store the end value
  LLVMValueRef end_ptr = LLVMBuildStructGEP2(ctx->builder, range_struct_type,
                                             range_alloca, 1, "end_ptr");
  LLVMBuildStore(ctx->builder, end, end_ptr);

  // Return the struct value
  return LLVMBuildLoad2(ctx->builder, range_struct_type, range_alloca,
                        "range_val");
}

// Optional: Add these functions for future range operations
LLVMValueRef range_contains(CodeGenContext *ctx, LLVMValueRef range_struct,
                            LLVMValueRef value) {
  LLVMValueRef start = get_range_start_value(ctx, range_struct);
  LLVMValueRef end = get_range_end_value(ctx, range_struct);

  // Check if value >= start && value <= end
  LLVMValueRef ge_start =
      LLVMBuildICmp(ctx->builder, LLVMIntSGE, value, start, "ge_start");
  LLVMValueRef le_end =
      LLVMBuildICmp(ctx->builder, LLVMIntSLE, value, end, "le_end");

  return LLVMBuildAnd(ctx->builder, ge_start, le_end, "in_range");
}

LLVMValueRef range_length(CodeGenContext *ctx, LLVMValueRef range_struct) {
  LLVMValueRef start = get_range_start_value(ctx, range_struct);
  LLVMValueRef end = get_range_end_value(ctx, range_struct);

  // Calculate end - start + 1 (inclusive range length)
  LLVMValueRef diff = LLVMBuildSub(ctx->builder, end, start, "diff");
  LLVMValueRef one = LLVMConstInt(LLVMTypeOf(diff), 1, false);
  return LLVMBuildAdd(ctx->builder, diff, one, "range_length");
}

LLVMValueRef codegen_expr_literal(CodeGenContext *ctx, AstNode *node) {
  switch (node->expr.literal.lit_type) {
  case LITERAL_INT:
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->context),
                        node->expr.literal.value.int_val, false);
  case LITERAL_FLOAT:
    return LLVMConstReal(LLVMFloatTypeInContext(ctx->context),
                         node->expr.literal.value.float_val);
  case LITERAL_BOOL:
    return LLVMConstInt(LLVMInt1TypeInContext(ctx->context),
                        node->expr.literal.value.bool_val ? 1 : 0, false);
  case LITERAL_STRING:
    return LLVMBuildGlobalStringPtr(ctx->builder,
                                    node->expr.literal.value.string_val, "str");
  case LITERAL_NULL:
    return LLVMConstNull(
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0));
  default:
    return NULL;
  }
}

// Expression identifier handler
LLVMValueRef codegen_expr_identifier(CodeGenContext *ctx, AstNode *node) {
  LLVM_Symbol *sym = find_symbol(ctx, node->expr.identifier.name);
  if (sym) {
    if (sym->is_function) {
      return sym->value;
    } else {
      // Load variable value
      return LLVMBuildLoad2(ctx->builder, sym->type, sym->value, "load");
    }
  }
  return NULL;
}

// Expression binary operation handler
LLVMValueRef codegen_expr_binary(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef left = codegen_expr(ctx, node->expr.binary.left);
  LLVMValueRef right = codegen_expr(ctx, node->expr.binary.right);

  if (!left || !right)
    return NULL;

  switch (node->expr.binary.op) {
  case BINOP_ADD:
    return LLVMBuildAdd(ctx->builder, left, right, "add");
  case BINOP_SUB:
    return LLVMBuildSub(ctx->builder, left, right, "sub");
  case BINOP_MUL:
    return LLVMBuildMul(ctx->builder, left, right, "mul");
  case BINOP_DIV:
    return LLVMBuildSDiv(ctx->builder, left, right, "div");
  case BINOP_MOD:
    return LLVMBuildSRem(ctx->builder, left, right, "mod");
  case BINOP_EQ:
    return LLVMBuildICmp(ctx->builder, LLVMIntEQ, left, right, "eq");
  case BINOP_NE:
    return LLVMBuildICmp(ctx->builder, LLVMIntNE, left, right, "ne");
  case BINOP_LT:
    return LLVMBuildICmp(ctx->builder, LLVMIntSLT, left, right, "lt");
  case BINOP_LE:
    return LLVMBuildICmp(ctx->builder, LLVMIntSLE, left, right, "le");
  case BINOP_GT:
    return LLVMBuildICmp(ctx->builder, LLVMIntSGT, left, right, "gt");
  case BINOP_GE:
    return LLVMBuildICmp(ctx->builder, LLVMIntSGE, left, right, "ge");
  case BINOP_AND:
    return LLVMBuildAnd(ctx->builder, left, right, "and");
  case BINOP_OR:
    return LLVMBuildOr(ctx->builder, left, right, "or");
  case BINOP_RANGE:
    return create_range_struct(ctx, left, right);
  default:
    return NULL;
  }
}

// Expression unary operation handler
LLVMValueRef codegen_expr_unary(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef operand = codegen_expr(ctx, node->expr.unary.operand);
  if (!operand)
    return NULL;

  switch (node->expr.unary.op) {
  case UNOP_NEG:
    return LLVMBuildNeg(ctx->builder, operand, "neg");
  case UNOP_NOT:
    return LLVMBuildNot(ctx->builder, operand, "not");
  case UNOP_PRE_INC:
  case UNOP_POST_INC: {
    LLVM_Symbol *sym =
        find_symbol(ctx, node->expr.unary.operand->expr.identifier.name);
    if (!sym || sym->is_function) {
      fprintf(stderr, "Error: Undefined variable for increment\n");
      return NULL;
    }

    LLVMValueRef loaded_val =
        LLVMBuildLoad2(ctx->builder, sym->type, sym->value, "load");
    LLVMValueRef one = LLVMConstInt(LLVMTypeOf(loaded_val), 1, false);
    LLVMValueRef incremented =
        LLVMBuildAdd(ctx->builder, loaded_val, one, "inc");
    LLVMBuildStore(ctx->builder, incremented, sym->value);
    return (node->expr.unary.op == UNOP_PRE_INC) ? incremented : loaded_val;
  }
  case UNOP_PRE_DEC:
  case UNOP_POST_DEC: {
    LLVM_Symbol *sym =
        find_symbol(ctx, node->expr.unary.operand->expr.identifier.name);
    if (!sym || sym->is_function) {
      fprintf(stderr, "Error: Undefined variable for decrement\n");
      return NULL;
    }
    LLVMValueRef loaded_val =
        LLVMBuildLoad2(ctx->builder, sym->type, sym->value, "load");
    LLVMValueRef one = LLVMConstInt(LLVMTypeOf(loaded_val), 1, false);
    LLVMValueRef decremented =
        LLVMBuildSub(ctx->builder, loaded_val, one, "dec");
    LLVMBuildStore(ctx->builder, decremented, sym->value);
    return (node->expr.unary.op == UNOP_PRE_DEC) ? decremented : loaded_val;
  }
  default:
    return NULL;
  }
}

// Expression function call handler
LLVMValueRef codegen_expr_call(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef callee = codegen_expr(ctx, node->expr.call.callee);
  if (!callee)
    return NULL;

  LLVMValueRef *args = (LLVMValueRef *)arena_alloc(
      ctx->arena, sizeof(LLVMValueRef) * node->expr.call.arg_count,
      alignof(LLVMValueRef));

  for (size_t i = 0; i < node->expr.call.arg_count; i++) {
    args[i] = codegen_expr(ctx, node->expr.call.args[i]);
    if (!args[i])
      return NULL;
  }

  return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(callee), callee,
                        args, node->expr.call.arg_count, "call");
}

// assignment handler that supports pointer dereference assignments
LLVMValueRef codegen_expr_assignment(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef value = codegen_expr(ctx, node->expr.assignment.value);
  if (!value)
    return NULL;

  AstNode *target = node->expr.assignment.target;

  // Handle direct variable assignment: x = value
  if (target->type == AST_EXPR_IDENTIFIER) {
    LLVM_Symbol *sym = find_symbol(ctx, target->expr.identifier.name);
    if (sym && !sym->is_function) {
      LLVMBuildStore(ctx->builder, value, sym->value);
      return value;
    }
  }

  // Handle pointer dereference assignment: *ptr = value
  else if (target->type == AST_EXPR_DEREF) {
    LLVMValueRef ptr = codegen_expr(ctx, target->expr.deref.object);
    if (!ptr) {
      return NULL;
    }

    // Store the value at the address pointed to by ptr
    LLVMBuildStore(ctx->builder, value, ptr);
    return value;
  }

  // Handle other lvalue types (array indexing, struct members, etc.)
  // Add more cases as needed for your language

  fprintf(stderr, "Error: Invalid assignment target\n");
  return NULL;
}

// cast<type>(value)
// cast<type>(value)
LLVMValueRef codegen_expr_cast(CodeGenContext *ctx, AstNode *node) {
  LLVMTypeRef target_type = codegen_type(ctx, node->expr.cast.type);
  LLVMValueRef value = codegen_expr(ctx, node->expr.cast.castee);
  if (!target_type || !value)
    return NULL;

  LLVMTypeRef source_type = LLVMTypeOf(value);

  // If types are the same, no cast needed
  if (source_type == target_type)
    return value;

  LLVMTypeKind source_kind = LLVMGetTypeKind(source_type);
  LLVMTypeKind target_kind = LLVMGetTypeKind(target_type);

  // Float to Integer
  if (source_kind == LLVMFloatTypeKind || source_kind == LLVMDoubleTypeKind) {
    if (target_kind == LLVMIntegerTypeKind) {
      // Float to signed integer (truncates decimal part)
      return LLVMBuildFPToSI(ctx->builder, value, target_type, "fptosi");
    }
  }

  // Integer to Float
  if (source_kind == LLVMIntegerTypeKind) {
    if (target_kind == LLVMFloatTypeKind || target_kind == LLVMDoubleTypeKind) {
      // Signed integer to float
      return LLVMBuildSIToFP(ctx->builder, value, target_type, "sitofp");
    }
  }

  // Integer to Integer (different sizes)
  if (source_kind == LLVMIntegerTypeKind &&
      target_kind == LLVMIntegerTypeKind) {
    unsigned source_bits = LLVMGetIntTypeWidth(source_type);
    unsigned target_bits = LLVMGetIntTypeWidth(target_type);

    if (source_bits > target_bits) {
      // Truncate
      return LLVMBuildTrunc(ctx->builder, value, target_type, "trunc");
    } else if (source_bits < target_bits) {
      // Sign extend (for signed integers)
      return LLVMBuildSExt(ctx->builder, value, target_type, "sext");
    }
  }

  // Float to Float (different precision)
  if ((source_kind == LLVMFloatTypeKind || source_kind == LLVMDoubleTypeKind) &&
      (target_kind == LLVMFloatTypeKind || target_kind == LLVMDoubleTypeKind)) {
    if (source_kind == LLVMFloatTypeKind && target_kind == LLVMDoubleTypeKind) {
      // Float to double
      return LLVMBuildFPExt(ctx->builder, value, target_type, "fpext");
    } else if (source_kind == LLVMDoubleTypeKind &&
               target_kind == LLVMFloatTypeKind) {
      // Double to float
      return LLVMBuildFPTrunc(ctx->builder, value, target_type, "fptrunc");
    }
  }

  // Pointer casts
  if (source_kind == LLVMPointerTypeKind &&
      target_kind == LLVMPointerTypeKind) {
    return LLVMBuildPointerCast(ctx->builder, value, target_type, "ptrcast");
  }

  // Integer to Pointer
  if (source_kind == LLVMIntegerTypeKind &&
      target_kind == LLVMPointerTypeKind) {
    return LLVMBuildIntToPtr(ctx->builder, value, target_type, "inttoptr");
  }

  // Pointer to Integer
  if (source_kind == LLVMPointerTypeKind &&
      target_kind == LLVMIntegerTypeKind) {
    return LLVMBuildPtrToInt(ctx->builder, value, target_type, "ptrtoint");
  }

  // Fallback to bitcast (use sparingly)
  return LLVMBuildBitCast(ctx->builder, value, target_type, "bitcast");
}

// sizeof<type || expr>
LLVMValueRef codegen_expr_sizeof(CodeGenContext *ctx, AstNode *node) {
  LLVMTypeRef type;
  if (node->expr.size_of.is_type) {
    type = codegen_type(ctx, node->expr.size_of.object);
  } else {
    LLVMValueRef expr = codegen_expr(ctx, node->expr.size_of.object);
    if (!expr)
      return NULL;
    type = LLVMTypeOf(expr);
  }
  if (!type)
    return NULL;

  return LLVMSizeOf(type);
}

// alloc(expr) - allocates memory on heap using malloc
LLVMValueRef codegen_expr_alloc(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef size = codegen_expr(ctx, node->expr.alloc.size);
  if (!size)
    return NULL;

  // Get or declare malloc function
  LLVMValueRef malloc_func = LLVMGetNamedFunction(ctx->module, "malloc");
  if (!malloc_func) {
    // Declare malloc: void* malloc(size_t size)
    LLVMTypeRef size_t_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef void_ptr_type =
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef malloc_type =
        LLVMFunctionType(void_ptr_type, &size_t_type, 1, 0);
    malloc_func = LLVMAddFunction(ctx->module, "malloc", malloc_type);

    // Set malloc as external linkage
    LLVMSetLinkage(malloc_func, LLVMExternalLinkage);
  }

  // Call malloc with the size
  LLVMTypeRef malloc_func_type = LLVMGlobalGetValueType(malloc_func);
  return LLVMBuildCall2(ctx->builder, malloc_func_type, malloc_func, &size, 1,
                        "alloc");
}

// free(expr)
LLVMValueRef codegen_expr_free(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef ptr = codegen_expr(ctx, node->expr.free.ptr);
  if (!ptr)
    return NULL;

  // Get or declare free function
  LLVMValueRef free_func = LLVMGetNamedFunction(ctx->module, "free");
  if (!free_func) {
    // Declare free: void free(void* ptr)
    LLVMTypeRef void_type = LLVMVoidTypeInContext(ctx->context);
    LLVMTypeRef ptr_type =
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef free_type = LLVMFunctionType(void_type, &ptr_type, 1, 0);
    free_func = LLVMAddFunction(ctx->module, "free", free_type);
    LLVMSetLinkage(free_func, LLVMExternalLinkage);
  }

  // Cast pointer to void* if needed
  LLVMTypeRef void_ptr_type =
      LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
  LLVMValueRef void_ptr = LLVMBuildPointerCast(ctx->builder, ptr, void_ptr_type,
                                               "cast_to_void_ptr");

  // Call free with the void pointer (no name since it returns void)
  LLVMTypeRef free_func_type = LLVMGlobalGetValueType(free_func);
  LLVMBuildCall2(ctx->builder, free_func_type, free_func, &void_ptr, 1, "");

  // Return a void constant since free() doesn't return a value
  return LLVMConstNull(LLVMVoidTypeInContext(ctx->context));
}

// *ptr - dereference pointer
LLVMValueRef codegen_expr_deref(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef ptr = codegen_expr(ctx, node->expr.deref.object);
  if (!ptr)
    return NULL;

  LLVMTypeRef ptr_type = LLVMTypeOf(ptr);

  // Ensure we have a pointer type
  if (LLVMGetTypeKind(ptr_type) != LLVMPointerTypeKind) {
    fprintf(stderr, "Error: Attempting to dereference non-pointer type\n");
    return NULL;
  }

  // For opaque pointers (newer LLVM), we need to know the target type
  // This is a simplified approach - in practice you'd track types through your
  // type system

  // Try to get element type (works for older LLVM versions)
  LLVMTypeRef element_type = NULL;

  // If we can't get the element type, we need to infer it from context
  // For this example, let's assume we're dereferencing to get an int
  // In a real compiler, you'd track this through your type system
  if (!element_type) {
    element_type = LLVMInt64TypeInContext(ctx->context); // Default to int64
  }

  return LLVMBuildLoad2(ctx->builder, element_type, ptr, "deref");
}

// &expr - get address of expression
LLVMValueRef codegen_expr_addr(CodeGenContext *ctx, AstNode *node) {
  // The address-of operator should only work on lvalues (variables,
  // dereferenced pointers, etc.)
  AstNode *target = node->expr.addr.object;

  if (target->type == AST_EXPR_IDENTIFIER) {
    // Get address of a variable
    LLVM_Symbol *sym = find_symbol(ctx, target->expr.identifier.name);
    if (sym && !sym->is_function) {
      // Return the alloca/global directly (it's already a pointer)
      return sym->value;
    }
  } else if (target->type == AST_EXPR_DEREF) {
    // &(*ptr) == ptr
    return codegen_expr(ctx, target->expr.deref.object);
  }

  // For other expressions, we'd need to create a temporary
  // This is a simplified implementation
  fprintf(stderr, "Error: Cannot take address of this expression type\n");
  return NULL;
}
