#include <stdlib.h>

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
    // IMPORTANT: Check if this literal should be float or double
    // This needs to match what your type checker decided in lookup.c

    // Option 1: Always create double (matches type checker default to double)
    return LLVMConstReal(LLVMDoubleTypeInContext(ctx->context),
                         node->expr.literal.value.float_val);

    // Option 2: Use context to determine type (more sophisticated)
    // You would need to add type annotation to the literal node
    // if (node->expr.literal.target_type == FLOAT_TYPE) {
    //     return LLVMConstReal(LLVMFloatTypeInContext(ctx->context),
    //                          node->expr.literal.value.float_val);
    // } else {
    //     return LLVMConstReal(LLVMDoubleTypeInContext(ctx->context),
    //                          node->expr.literal.value.float_val);
    // }

  case LITERAL_BOOL:
    return LLVMConstInt(LLVMInt1TypeInContext(ctx->context),
                        node->expr.literal.value.bool_val ? 1 : 0, false);
  case LITERAL_CHAR:
    return LLVMConstInt(LLVMInt8TypeInContext(ctx->context),
                        (unsigned char)node->expr.literal.value.char_val,
                        false);
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
    } else if (is_enum_constant(sym)) {
      // Enum constant - return the constant value directly
      return LLVMGetInitializer(sym->value);
    } else {
      // Load variable value
      return LLVMBuildLoad2(ctx->builder, sym->type, sym->value, "load");
    }
  }

  fprintf(stderr, "Error: Undefined symbol '%s'\n", node->expr.identifier.name);
  return NULL;
}

// Enhanced codegen_expr_binary function with float arithmetic support
LLVMValueRef codegen_expr_binary(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef left = codegen_expr(ctx, node->expr.binary.left);
  LLVMValueRef right = codegen_expr(ctx, node->expr.binary.right);

  if (!left || !right)
    return NULL;

  // Get the types of both operands
  LLVMTypeRef left_type = LLVMTypeOf(left);
  LLVMTypeRef right_type = LLVMTypeOf(right);
  LLVMTypeKind left_kind = LLVMGetTypeKind(left_type);
  LLVMTypeKind right_kind = LLVMGetTypeKind(right_type);

  // Determine if we're dealing with floating point operations
  bool is_float_op =
      (left_kind == LLVMFloatTypeKind || left_kind == LLVMDoubleTypeKind ||
       right_kind == LLVMFloatTypeKind || right_kind == LLVMDoubleTypeKind);

  // Handle type promotion if needed
  if (is_float_op) {
    // Promote integers to floats if mixing int and float
    if (left_kind == LLVMIntegerTypeKind &&
        (right_kind == LLVMFloatTypeKind || right_kind == LLVMDoubleTypeKind)) {
      left = LLVMBuildSIToFP(ctx->builder, left, right_type, "int_to_float");
      left_type = right_type;
    } else if (right_kind == LLVMIntegerTypeKind &&
               (left_kind == LLVMFloatTypeKind ||
                left_kind == LLVMDoubleTypeKind)) {
      right = LLVMBuildSIToFP(ctx->builder, right, left_type, "int_to_float");
      right_type = left_type;
    }

    // Promote float to double if mixing float and double
    if (left_kind == LLVMFloatTypeKind && right_kind == LLVMDoubleTypeKind) {
      left = LLVMBuildFPExt(ctx->builder, left, right_type, "float_to_double");
    } else if (right_kind == LLVMFloatTypeKind &&
               left_kind == LLVMDoubleTypeKind) {
      right = LLVMBuildFPExt(ctx->builder, right, left_type, "float_to_double");
    }
  }

  switch (node->expr.binary.op) {
  case BINOP_ADD:
    if (is_float_op) {
      return LLVMBuildFAdd(ctx->builder, left, right, "fadd");
    } else {
      return LLVMBuildAdd(ctx->builder, left, right, "add");
    }

  case BINOP_SUB:
    if (is_float_op) {
      return LLVMBuildFSub(ctx->builder, left, right, "fsub");
    } else {
      return LLVMBuildSub(ctx->builder, left, right, "sub");
    }

  case BINOP_MUL:
    if (is_float_op) {
      return LLVMBuildFMul(ctx->builder, left, right, "fmul");
    } else {
      return LLVMBuildMul(ctx->builder, left, right, "mul");
    }

  case BINOP_DIV:
    if (is_float_op) {
      return LLVMBuildFDiv(ctx->builder, left, right, "fdiv");
    } else {
      return LLVMBuildSDiv(ctx->builder, left, right, "div");
    }

  case BINOP_MOD:
    if (is_float_op) {
      // Option 2: Implement modulo as a - (b * floor(a/b)) using LLVM
      // intrinsics
      LLVMModuleRef current_module =
          ctx->current_module ? ctx->current_module->module : ctx->module;

      // Declare floor intrinsic if needed
      LLVMTypeRef floor_type;
      LLVMValueRef floor_func;

      if (LLVMGetTypeKind(left_type) == LLVMDoubleTypeKind) {
        floor_type = LLVMFunctionType(LLVMDoubleTypeInContext(ctx->context),
                                      &left_type, 1, false);
        floor_func = LLVMGetNamedFunction(current_module, "llvm.floor.f64");
        if (!floor_func) {
          floor_func =
              LLVMAddFunction(current_module, "llvm.floor.f64", floor_type);
        }
      } else {
        floor_type = LLVMFunctionType(LLVMFloatTypeInContext(ctx->context),
                                      &left_type, 1, false);
        floor_func = LLVMGetNamedFunction(current_module, "llvm.floor.f32");
        if (!floor_func) {
          floor_func =
              LLVMAddFunction(current_module, "llvm.floor.f32", floor_type);
        }
      }

      // Calculate a / b
      LLVMValueRef division =
          LLVMBuildFDiv(ctx->builder, left, right, "fdiv_for_mod");

      // Calculate floor(a / b)
      LLVMValueRef floor_result = LLVMBuildCall2(
          ctx->builder, floor_type, floor_func, &division, 1, "floor_result");

      // Calculate b * floor(a / b)
      LLVMValueRef multiply =
          LLVMBuildFMul(ctx->builder, right, floor_result, "fmul_for_mod");

      // Calculate a - (b * floor(a / b))
      return LLVMBuildFSub(ctx->builder, left, multiply, "fmod_result");
    } else {
      return LLVMBuildSRem(ctx->builder, left, right, "mod");
    }
    break;

  // Comparison operations
  case BINOP_EQ:
    if (is_float_op) {
      return LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, left, right, "feq");
    } else {
      return LLVMBuildICmp(ctx->builder, LLVMIntEQ, left, right, "eq");
    }

  case BINOP_NE:
    if (is_float_op) {
      return LLVMBuildFCmp(ctx->builder, LLVMRealONE, left, right, "fne");
    } else {
      return LLVMBuildICmp(ctx->builder, LLVMIntNE, left, right, "ne");
    }

  case BINOP_LT:
    if (is_float_op) {
      return LLVMBuildFCmp(ctx->builder, LLVMRealOLT, left, right, "flt");
    } else {
      return LLVMBuildICmp(ctx->builder, LLVMIntSLT, left, right, "lt");
    }

  case BINOP_LE:
    if (is_float_op) {
      return LLVMBuildFCmp(ctx->builder, LLVMRealOLE, left, right, "fle");
    } else {
      return LLVMBuildICmp(ctx->builder, LLVMIntSLE, left, right, "le");
    }

  case BINOP_GT:
    if (is_float_op) {
      return LLVMBuildFCmp(ctx->builder, LLVMRealOGT, left, right, "fgt");
    } else {
      return LLVMBuildICmp(ctx->builder, LLVMIntSGT, left, right, "gt");
    }

  case BINOP_GE:
    if (is_float_op) {
      return LLVMBuildFCmp(ctx->builder, LLVMRealOGE, left, right, "fge");
    } else {
      return LLVMBuildICmp(ctx->builder, LLVMIntSGE, left, right, "ge");
    }

  // Logical operations (only for integers and booleans)
  case BINOP_AND:
    if (is_float_op) {
      fprintf(stderr,
              "Error: Logical AND not supported for floating point values\n");
      return NULL;
    }
    return LLVMBuildAnd(ctx->builder, left, right, "and");

  case BINOP_OR:
    if (is_float_op) {
      fprintf(stderr,
              "Error: Logical OR not supported for floating point values\n");
      return NULL;
    }
    return LLVMBuildOr(ctx->builder, left, right, "or");

  case BINOP_RANGE:
    // Range operations work with both integers and floats
    return create_range_struct(ctx, left, right);

  default:
    return NULL;
  }
}

// Enhanced codegen_expr_unary function with float support
LLVMValueRef codegen_expr_unary(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef operand = codegen_expr(ctx, node->expr.unary.operand);
  if (!operand)
    return NULL;

  LLVMTypeRef operand_type = LLVMTypeOf(operand);
  LLVMTypeKind operand_kind = LLVMGetTypeKind(operand_type);
  bool is_float =
      (operand_kind == LLVMFloatTypeKind || operand_kind == LLVMDoubleTypeKind);

  switch (node->expr.unary.op) {
  case UNOP_NEG:
    if (is_float) {
      return LLVMBuildFNeg(ctx->builder, operand, "fneg");
    } else {
      return LLVMBuildNeg(ctx->builder, operand, "neg");
    }

  case UNOP_NOT:
    if (is_float) {
      fprintf(stderr,
              "Error: Logical NOT not supported for floating point values\n");
      return NULL;
    }
    return LLVMBuildNot(ctx->builder, operand, "not");

  case UNOP_PRE_INC:
  case UNOP_POST_INC: {
    if (node->expr.unary.operand->type != AST_EXPR_IDENTIFIER) {
      fprintf(stderr, "Error: Increment/decrement requires an lvalue\n");
      return NULL;
    }

    LLVM_Symbol *sym =
        find_symbol(ctx, node->expr.unary.operand->expr.identifier.name);
    if (!sym || sym->is_function) {
      fprintf(stderr, "Error: Undefined variable for increment\n");
      return NULL;
    }

    LLVMValueRef loaded_val =
        LLVMBuildLoad2(ctx->builder, sym->type, sym->value, "load");
    LLVMValueRef one;
    LLVMValueRef incremented;

    if (is_float) {
      one = LLVMConstReal(LLVMTypeOf(loaded_val), 1.0);
      incremented = LLVMBuildFAdd(ctx->builder, loaded_val, one, "finc");
    } else {
      one = LLVMConstInt(LLVMTypeOf(loaded_val), 1, false);
      incremented = LLVMBuildAdd(ctx->builder, loaded_val, one, "inc");
    }

    LLVMBuildStore(ctx->builder, incremented, sym->value);
    return (node->expr.unary.op == UNOP_PRE_INC) ? incremented : loaded_val;
  }

  case UNOP_PRE_DEC:
  case UNOP_POST_DEC: {
    if (node->expr.unary.operand->type != AST_EXPR_IDENTIFIER) {
      fprintf(stderr, "Error: Increment/decrement requires an lvalue\n");
      return NULL;
    }

    LLVM_Symbol *sym =
        find_symbol(ctx, node->expr.unary.operand->expr.identifier.name);
    if (!sym || sym->is_function) {
      fprintf(stderr, "Error: Undefined variable for decrement\n");
      return NULL;
    }

    LLVMValueRef loaded_val =
        LLVMBuildLoad2(ctx->builder, sym->type, sym->value, "load");
    LLVMValueRef one;
    LLVMValueRef decremented;

    if (is_float) {
      one = LLVMConstReal(LLVMTypeOf(loaded_val), 1.0);
      decremented = LLVMBuildFSub(ctx->builder, loaded_val, one, "fdec");
    } else {
      one = LLVMConstInt(LLVMTypeOf(loaded_val), 1, false);
      decremented = LLVMBuildSub(ctx->builder, loaded_val, one, "dec");
    }

    LLVMBuildStore(ctx->builder, decremented, sym->value);
    return (node->expr.unary.op == UNOP_PRE_DEC) ? decremented : loaded_val;
  }

  default:
    return NULL;
  }
}

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

  // Get the function type to check return type
  LLVMTypeRef func_type = LLVMGlobalGetValueType(callee);
  LLVMTypeRef return_type = LLVMGetReturnType(func_type);

  // Check if return type is void
  if (LLVMGetTypeKind(return_type) == LLVMVoidTypeKind) {
    // For void functions, don't assign a name to the call
    LLVMBuildCall2(ctx->builder, func_type, callee, args,
                   node->expr.call.arg_count, "");
    // Return a void constant since we can't return NULL
    return LLVMConstNull(LLVMVoidTypeInContext(ctx->context));
  } else {
    // For non-void functions, assign a name as usual
    return LLVMBuildCall2(ctx->builder, func_type, callee, args,
                          node->expr.call.arg_count, "call");
  }
}

// Unified assignment handler that supports all assignment types
LLVMValueRef codegen_expr_assignment(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_EXPR_ASSIGNMENT) {
    return NULL;
  }

  LLVMValueRef value = codegen_expr(ctx, node->expr.assignment.value);
  if (!value) {
    return NULL;
  }

  AstNode *target = node->expr.assignment.target;

  // Handle direct variable assignment: x = value
  if (target->type == AST_EXPR_IDENTIFIER) {
    LLVM_Symbol *sym = find_symbol(ctx, target->expr.identifier.name);
    if (sym && !sym->is_function) {
      LLVMBuildStore(ctx->builder, value, sym->value);
      return value;
    }
    fprintf(stderr, "Error: Variable %s not found\n",
            target->expr.identifier.name);
    return NULL;
  }

  // Handle pointer dereference assignment: *ptr = value
  else if (target->type == AST_EXPR_DEREF) {
    LLVMValueRef ptr = codegen_expr(ctx, target->expr.deref.object);
    if (!ptr) {
      return NULL;
    }
    LLVMBuildStore(ctx->builder, value, ptr);
    return value;
  }

  // Handle index assignment: arr[i] = value or ptr[i] = value
  else if (target->type == AST_EXPR_INDEX) {
    // Generate the object being indexed
    LLVMValueRef object = codegen_expr(ctx, target->expr.index.object);
    if (!object) {
      return NULL;
    }

    // Generate the index expression
    LLVMValueRef index = codegen_expr(ctx, target->expr.index.index);
    if (!index) {
      return NULL;
    }

    LLVMTypeRef object_type = LLVMTypeOf(object);
    LLVMTypeKind object_kind = LLVMGetTypeKind(object_type);

    if (object_kind == LLVMArrayTypeKind) {
      // Array assignment: arr[i] = value
      // We need the array to be stored somewhere to get its address
      LLVMValueRef array_ptr;

      if (target->expr.index.object->type == AST_EXPR_IDENTIFIER) {
        // Get the symbol directly for proper array access
        const char *var_name = target->expr.index.object->expr.identifier.name;
        LLVM_Symbol *sym = find_symbol(ctx, var_name);
        if (sym && !sym->is_function) {
          array_ptr = sym->value; // This should be the alloca/global
        } else {
          fprintf(stderr, "Error: Array variable %s not found for assignment\n",
                  var_name);
          return NULL;
        }
      } else {
        // Create temporary storage for complex array expressions
        array_ptr =
            LLVMBuildAlloca(ctx->builder, object_type, "temp_array_ptr");
        LLVMBuildStore(ctx->builder, object, array_ptr);
      }

      LLVMValueRef indices[2];
      indices[0] = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false);
      indices[1] = index;

      LLVMValueRef element_ptr = LLVMBuildGEP2(
          ctx->builder, object_type, array_ptr, indices, 2, "array_assign_ptr");

      // Type conversion if needed
      LLVMTypeRef element_type = LLVMGetElementType(object_type);
      LLVMTypeRef value_type = LLVMTypeOf(value);

      if (element_type != value_type) {
        value = convert_value_to_type(ctx, value, value_type, element_type);
        if (!value) {
          fprintf(stderr,
                  "Error: Cannot convert value to array element type\n");
          return NULL;
        }
      }

      LLVMBuildStore(ctx->builder, value, element_ptr);
      return value;

    } else if (object_kind == LLVMPointerTypeKind) {
      // Pointer indexing: ptr[i] = value
      LLVMTypeRef element_type = NULL;
      LLVMTypeRef value_type = LLVMTypeOf(value);

      // Try to get element type from symbol information first
      if (target->expr.index.object->type == AST_EXPR_IDENTIFIER) {
        const char *var_name = target->expr.index.object->expr.identifier.name;
        LLVM_Symbol *sym = find_symbol(ctx, var_name);

        if (sym && !sym->is_function) {
          // CRITICAL FIX: Use the stored element_type from symbol table
          if (sym->element_type) {
            element_type = sym->element_type;
          } else {
            // Fallback: try to infer from variable name (temporary workaround)
            if (strstr(var_name, "int") && !strstr(var_name, "char")) {
              element_type = LLVMInt64TypeInContext(ctx->context);
            } else if (strstr(var_name, "double")) {
              element_type = LLVMDoubleTypeInContext(ctx->context);
            } else if (strstr(var_name, "float")) {
              element_type = LLVMFloatTypeInContext(ctx->context);
            } else if (strstr(var_name, "char")) {
              element_type = LLVMInt8TypeInContext(ctx->context);
            } else {
              // Default to the value type as last resort
              element_type = value_type;
            }
          }
        }
      }

      // Final fallback if we couldn't determine element type
      if (!element_type) {
        element_type = value_type;
        fprintf(stderr, "Warning: Could not determine pointer element type, "
                        "using value type\n");
      }

      // CRITICAL FIX: Ensure the value matches the element type exactly
      LLVMValueRef value_final = value;
      if (LLVMGetTypeKind(element_type) != LLVMGetTypeKind(value_type) ||
          (LLVMGetTypeKind(element_type) == LLVMIntegerTypeKind &&
           LLVMGetIntTypeWidth(element_type) !=
               LLVMGetIntTypeWidth(value_type))) {

        // Handle integer type conversions
        if (LLVMGetTypeKind(element_type) == LLVMIntegerTypeKind &&
            LLVMGetTypeKind(value_type) == LLVMIntegerTypeKind) {
          unsigned element_bits = LLVMGetIntTypeWidth(element_type);
          unsigned value_bits = LLVMGetIntTypeWidth(value_type);

          if (element_bits > value_bits) {
            value_final = LLVMBuildZExt(ctx->builder, value, element_type,
                                        "zext_for_store");
          } else if (element_bits < value_bits) {
            value_final = LLVMBuildTrunc(ctx->builder, value, element_type,
                                         "trunc_for_store");
          }
        }
        // Handle float/int conversions
        else if (LLVMGetTypeKind(element_type) == LLVMIntegerTypeKind &&
                 (LLVMGetTypeKind(value_type) == LLVMFloatTypeKind ||
                  LLVMGetTypeKind(value_type) == LLVMDoubleTypeKind)) {
          value_final = LLVMBuildFPToSI(ctx->builder, value, element_type,
                                        "float_to_int_for_store");
        } else if ((LLVMGetTypeKind(element_type) == LLVMFloatTypeKind ||
                    LLVMGetTypeKind(element_type) == LLVMDoubleTypeKind) &&
                   LLVMGetTypeKind(value_type) == LLVMIntegerTypeKind) {
          value_final = LLVMBuildSIToFP(ctx->builder, value, element_type,
                                        "int_to_float_for_store");
        } else {
          // For other type mismatches, use bitcast as fallback
          value_final = LLVMBuildBitCast(ctx->builder, value, element_type,
                                         "cast_for_store");
        }
      }

      // Use proper typed GEP for pointer arithmetic
      LLVMValueRef element_ptr = LLVMBuildGEP2(
          ctx->builder, element_type, object, &index, 1, "ptr_assign_ptr");
      LLVMBuildStore(ctx->builder, value_final, element_ptr);
      return value;
    } else {
      fprintf(stderr, "Error: Cannot assign to index of this type (kind: %d)\n",
              object_kind);
      return NULL;
    }
  }

  // Handle struct member assignment: obj.field = value
  else if (target->type == AST_EXPR_MEMBER) {
    const char *field_name = target->expr.member.member;
    AstNode *object = target->expr.member.object;

    if (object->type == AST_EXPR_IDENTIFIER) {
      const char *var_name = object->expr.identifier.name;
      LLVM_Symbol *sym = find_symbol(ctx, var_name);
      if (!sym || sym->is_function) {
        fprintf(stderr, "Error: Variable %s not found or is a function\n",
                var_name);
        return NULL;
      }

      // Find the struct info by checking which struct has this field
      StructInfo *struct_info = NULL;
      for (StructInfo *info = ctx->struct_types; info; info = info->next) {
        int field_idx = get_field_index(info, field_name);
        if (field_idx >= 0) {
          struct_info = info;
          break;
        }
      }

      if (!struct_info) {
        fprintf(stderr, "Error: Could not find struct with field '%s'\n",
                field_name);
        return NULL;
      }

      // Find field index and check permissions
      int field_index = get_field_index(struct_info, field_name);
      if (!is_field_access_allowed(ctx, struct_info, field_index)) {
        fprintf(stderr, "Error: Cannot assign to private field '%s'\n",
                field_name);
        return NULL;
      }

      // Handle the different cases for assignment
      LLVMTypeRef symbol_type = sym->type;
      LLVMValueRef struct_ptr;

      if (LLVMGetTypeKind(symbol_type) == LLVMPointerTypeKind) {
        // Pointer to struct - load the pointer value
        LLVMTypeRef ptr_to_struct_type =
            LLVMPointerType(struct_info->llvm_type, 0);
        struct_ptr = LLVMBuildLoad2(ctx->builder, ptr_to_struct_type,
                                    sym->value, "load_struct_ptr");
      } else if (symbol_type == struct_info->llvm_type) {
        // Direct struct variable
        struct_ptr = sym->value;
      } else {
        fprintf(stderr,
                "Error: Variable '%s' is not a struct or pointer to struct\n",
                var_name);
        return NULL;
      }

      // Use GEP to get the field address and store the value
      LLVMValueRef field_ptr =
          LLVMBuildStructGEP2(ctx->builder, struct_info->llvm_type, struct_ptr,
                              field_index, "field_ptr");

      LLVMBuildStore(ctx->builder, value, field_ptr);
      return value;
    }
  }

  fprintf(stderr, "Error: Invalid assignment target\n");
  return NULL;
}

LLVMValueRef codegen_expr_array(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_EXPR_ARRAY) {
    fprintf(stderr, "Error: Expected array expression node\n");
    return NULL;
  }

  AstNode **elements = node->expr.array.elements;
  size_t element_count = node->expr.array.element_count;

  if (element_count == 0) {
    fprintf(stderr, "Error: Empty array literals not supported\n");
    return NULL;
  }

  // Generate the first element to determine the array's element type
  LLVMValueRef first_element = codegen_expr(ctx, elements[0]);
  if (!first_element) {
    fprintf(stderr, "Error: Failed to generate first array element\n");
    return NULL;
  }

  LLVMTypeRef element_type = LLVMTypeOf(first_element);
  LLVMTypeRef array_type = LLVMArrayType(element_type, element_count);

  // Check if all elements are constants
  bool all_constants = LLVMIsConstant(first_element);
  LLVMValueRef *element_values = (LLVMValueRef *)arena_alloc(
      ctx->arena, sizeof(LLVMValueRef) * element_count, alignof(LLVMValueRef));

  element_values[0] = first_element;

  // Generate remaining elements and check for constants
  for (size_t i = 1; i < element_count; i++) {
    element_values[i] = codegen_expr(ctx, elements[i]);
    if (!element_values[i]) {
      fprintf(stderr, "Error: Failed to generate array element %zu\n", i);
      return NULL;
    }

    // Type conversion if needed
    LLVMTypeRef current_type = LLVMTypeOf(element_values[i]);
    if (current_type != element_type) {
      element_values[i] = convert_value_to_type(ctx, element_values[i],
                                                current_type, element_type);
      if (!element_values[i]) {
        fprintf(stderr,
                "Error: Cannot convert element %zu to array element type\n", i);
        return NULL;
      }
    }

    if (all_constants && !LLVMIsConstant(element_values[i])) {
      all_constants = false;
    }
  }

  if (all_constants) {
    // Create constant array
    return LLVMConstArray(element_type, element_values, element_count);
  } else {
    // Create runtime array
    LLVMValueRef array_alloca =
        LLVMBuildAlloca(ctx->builder, array_type, "array_literal");

    for (size_t i = 0; i < element_count; i++) {
      // Create GEP to element
      LLVMValueRef indices[2];
      indices[0] = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false);
      indices[1] = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), i, false);

      LLVMValueRef element_ptr = LLVMBuildGEP2(
          ctx->builder, array_type, array_alloca, indices, 2, "element_ptr");
      LLVMBuildStore(ctx->builder, element_values[i], element_ptr);
    }

    return array_alloca; // Return pointer to array
  }
}

LLVMValueRef codegen_expr_index(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_EXPR_INDEX) {
    fprintf(stderr, "Error: Expected index expression node\n");
    return NULL;
  }

  // Generate the object being indexed
  LLVMValueRef object = codegen_expr(ctx, node->expr.index.object);
  if (!object) {
    fprintf(stderr, "Error: Failed to generate indexed object\n");
    return NULL;
  }

  // Generate the index expression
  LLVMValueRef index = codegen_expr(ctx, node->expr.index.index);
  if (!index) {
    fprintf(stderr, "Error: Failed to generate index expression\n");
    return NULL;
  }

  LLVMTypeRef object_type = LLVMTypeOf(object);
  LLVMTypeKind object_kind = LLVMGetTypeKind(object_type);

  if (object_kind == LLVMArrayTypeKind) {
    // Direct array value indexing (from array literals)
    LLVMTypeRef element_type = LLVMGetElementType(object_type);

    // We need to store the array and get a pointer to it for GEP
    LLVMValueRef array_alloca =
        LLVMBuildAlloca(ctx->builder, object_type, "temp_array");
    LLVMBuildStore(ctx->builder, object, array_alloca);

    // Create GEP indices: [0, index] for array access
    LLVMValueRef indices[2];
    indices[0] = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false);
    indices[1] = index;

    LLVMValueRef element_ptr =
        LLVMBuildGEP2(ctx->builder, object_type, array_alloca, indices, 2,
                      "array_element_ptr");

    // If the element is also an array, return the pointer to it (for chaining)
    if (LLVMGetTypeKind(element_type) == LLVMArrayTypeKind) {
      return element_ptr;
    } else {
      return LLVMBuildLoad2(ctx->builder, element_type, element_ptr,
                            "array_element");
    }

  } else if (object_kind == LLVMPointerTypeKind) {
    // Handle pointer indexing - check if it's a symbol first for better type
    // info
    LLVMTypeRef pointee_type = NULL;

    if (node->expr.index.object->type == AST_EXPR_IDENTIFIER) {
      const char *var_name = node->expr.index.object->expr.identifier.name;
      LLVM_Symbol *sym = find_symbol(ctx, var_name);

      if (sym && !sym->is_function) {
        // CRITICAL FIX: Use the stored element_type from symbol table
        if (sym->element_type) {
          pointee_type = sym->element_type;
        } else {
          // Check if the symbol type is a pointer to array or just array
          LLVMTypeRef sym_type = sym->type;
          LLVMTypeKind sym_kind = LLVMGetTypeKind(sym_type);

          if (sym_kind == LLVMArrayTypeKind) {
            // This is a direct array stored in the symbol
            LLVMTypeRef element_type = LLVMGetElementType(sym_type);

            LLVMValueRef indices[2];
            indices[0] =
                LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false);
            indices[1] = index;

            LLVMValueRef element_ptr =
                LLVMBuildGEP2(ctx->builder, sym_type, sym->value, indices, 2,
                              "array_element_ptr");

            if (LLVMGetTypeKind(element_type) == LLVMArrayTypeKind) {
              return element_ptr;
            } else {
              return LLVMBuildLoad2(ctx->builder, element_type, element_ptr,
                                    "array_element");
            }
          } else {
            // Fallback: try to infer from variable name (temporary workaround)
            if (strstr(var_name, "int") && !strstr(var_name, "char")) {
              pointee_type = LLVMInt64TypeInContext(ctx->context);
            } else if (strstr(var_name, "double")) {
              pointee_type = LLVMDoubleTypeInContext(ctx->context);
            } else if (strstr(var_name, "float")) {
              pointee_type = LLVMFloatTypeInContext(ctx->context);
            } else if (strstr(var_name, "char") || strstr(var_name, "_buf")) {
              pointee_type = LLVMInt8TypeInContext(ctx->context);
            }
          }
        }
      }
    } else if (node->expr.index.object->type == AST_EXPR_INDEX) {
      // Second-level indexing: ptr[i][j]
      // Trace back to find the base variable
      AstNode *base_node = node->expr.index.object;
      while (base_node->type == AST_EXPR_INDEX) {
        base_node = base_node->expr.index.object;
      }
      if (base_node->type == AST_EXPR_IDENTIFIER) {
        const char *base_var_name = base_node->expr.identifier.name;
        LLVM_Symbol *base_sym = find_symbol(ctx, base_var_name);
        if (base_sym && base_sym->element_type) {
          // For multi-dimensional arrays, the element type after first index
          // would be a pointer to the inner type
          if (LLVMGetTypeKind(base_sym->element_type) == LLVMPointerTypeKind) {
            // This needs more sophisticated handling for multi-dimensional
            // arrays
            pointee_type = LLVMInt64TypeInContext(ctx->context); // fallback
          } else {
            pointee_type = base_sym->element_type;
          }
        } else {
          // Old name-based fallback
          if (strstr(base_var_name, "double")) {
            pointee_type = LLVMDoubleTypeInContext(ctx->context);
          } else if (strstr(base_var_name, "float")) {
            pointee_type = LLVMFloatTypeInContext(ctx->context);
          } else if (strstr(base_var_name, "int") &&
                     !strstr(base_var_name, "char")) {
            pointee_type = LLVMInt64TypeInContext(ctx->context);
          } else if (strstr(base_var_name, "char") ||
                     strstr(base_var_name, "_buf")) {
            pointee_type = LLVMInt8TypeInContext(ctx->context);
          }
        }
      }
    } else if (node->expr.index.object->type == AST_EXPR_CAST) {
      // Handle cast expressions specially
      AstNode *cast_node = node->expr.index.object;
      if (cast_node->expr.cast.type->type == AST_TYPE_POINTER) {
        // Get the pointee type from the cast target type
        AstNode *pointee_node =
            cast_node->expr.cast.type->type_data.pointer.pointee_type;
        pointee_type = codegen_type(ctx, pointee_node);
      }
    }

    // CRITICAL: Don't fall back to i8! This causes the bug.
    // If we can't determine the type, it's an error condition.
    if (!pointee_type) {
      fprintf(
          stderr,
          "Error: Could not determine pointer element type for indexing '%s'\n",
          node->expr.index.object->type == AST_EXPR_IDENTIFIER
              ? node->expr.index.object->expr.identifier.name
              : "expression");
      return NULL;
    }

    // Build GEP for pointer arithmetic
    LLVMValueRef element_ptr = LLVMBuildGEP2(ctx->builder, pointee_type, object,
                                             &index, 1, "ptr_element_ptr");

    // Load the actual value
    LLVMValueRef result = LLVMBuildLoad2(ctx->builder, pointee_type,
                                         element_ptr, "ptr_element_val");

#ifdef DEBUG_TYPES
    fprintf(stderr, "Debug: object_type kind = %d, pointee_type kind = %d\n",
            LLVMGetTypeKind(object_type), LLVMGetTypeKind(pointee_type));
    fprintf(stderr, "Debug: result type kind = %d\n",
            LLVMGetTypeKind(LLVMTypeOf(result)));
#endif

    return result;

  } else {
    fprintf(stderr, "Error: Cannot index expression of type kind %d\n",
            object_kind);
    return NULL;
  }
}

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

  LLVMTypeKind kind = LLVMGetTypeKind(type);

  switch (kind) {
  case LLVMIntegerTypeKind: {
    unsigned width = LLVMGetIntTypeWidth(type);
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), width / 8, false);
  }
  case LLVMFloatTypeKind:
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), 4, false);
  case LLVMDoubleTypeKind:
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), 8, false);
  case LLVMPointerTypeKind:
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), 8, false);
  case LLVMStructTypeKind: {
    // Calculate struct size by summing field sizes
    unsigned field_count = LLVMCountStructElementTypes(type);
    LLVMTypeRef *field_types = malloc(field_count * sizeof(LLVMTypeRef));
    LLVMGetStructElementTypes(type, field_types);

    uint64_t total_size = 0;
    for (unsigned i = 0; i < field_count; i++) {
      LLVMTypeKind field_kind = LLVMGetTypeKind(field_types[i]);
      switch (field_kind) {
      case LLVMIntegerTypeKind:
        total_size += LLVMGetIntTypeWidth(field_types[i]) / 8;
        break;
      case LLVMFloatTypeKind:
        total_size += 4;
        break;
      case LLVMDoubleTypeKind:
        total_size += 8;
        break;
      case LLVMPointerTypeKind:
        total_size += 8;
        break;
      default:
        total_size += 8; // fallback
        break;
      }
    }

    free(field_types);
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), total_size,
                        false);
  }
  default:
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), 8, false);
  }
}

// alloc(expr) - allocates memory on heap using malloc
LLVMValueRef codegen_expr_alloc(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef size = codegen_expr(ctx, node->expr.alloc.size);
  if (!size)
    return NULL;

  // Get or declare malloc function
  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;
  LLVMValueRef malloc_func =
      LLVMGetNamedFunction(current_llvm_module, "malloc");

  if (!malloc_func) {
    // Declare malloc: void* malloc(size_t size)
    LLVMTypeRef size_t_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef void_ptr_type =
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef malloc_type =
        LLVMFunctionType(void_ptr_type, &size_t_type, 1, 0);
    malloc_func = LLVMAddFunction(current_llvm_module, "malloc", malloc_type);

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
  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;
  LLVMValueRef free_func = LLVMGetNamedFunction(current_llvm_module, "free");

  if (!free_func) {
    // Declare free: void free(void* ptr)
    LLVMTypeRef void_type = LLVMVoidTypeInContext(ctx->context);
    LLVMTypeRef ptr_type =
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef free_type = LLVMFunctionType(void_type, &ptr_type, 1, 0);
    free_func = LLVMAddFunction(current_llvm_module, "free", free_type);
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

  // Try to infer the element type from the variable's symbol information
  LLVMTypeRef element_type = NULL;

  // If the dereference target is an identifier, try to get type info from
  // symbol table
  if (node->expr.deref.object->type == AST_EXPR_IDENTIFIER) {
    const char *var_name = node->expr.deref.object->expr.identifier.name;
    LLVM_Symbol *sym = find_symbol(ctx, var_name);

    if (sym && !sym->is_function) {
      LLVMTypeRef sym_type = sym->type;

      // If the symbol is a pointer type, we need to determine what it points to
      if (LLVMGetTypeKind(sym_type) == LLVMPointerTypeKind) {
        // For opaque pointers (newer LLVM), we need to infer the pointee type
        // This is a simplified approach - in a real compiler you'd track this
        // through your type system

        // Try to infer based on variable name patterns
        if (strstr(var_name, "ptr") || strstr(var_name, "aligned_ptr")) {
          // Check if this looks like a void** -> void* case
          if (strstr(var_name, "aligned")) {
            element_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context),
                                           0); // void*
          } else if (strstr(var_name, "char") || strstr(var_name, "str")) {
            element_type = LLVMInt8TypeInContext(ctx->context); // char
          } else if (strstr(var_name, "int")) {
            element_type = LLVMInt64TypeInContext(ctx->context); // int
          } else if (strstr(var_name, "float")) {
            element_type = LLVMFloatTypeInContext(ctx->context); // float
          } else if (strstr(var_name, "double")) {
            element_type = LLVMDoubleTypeInContext(ctx->context);
          } else {
            // Default for void** -> void*
            element_type =
                LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
          }
        } else {
          // Generic pointer dereference - assume int64 for safety
          element_type = LLVMInt64TypeInContext(ctx->context);
        }
      }
    }
  }

  // Final fallback if we couldn't determine the type
  if (!element_type) {
    // Default to void* for unknown pointer dereferences
    element_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
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
  } else if (target->type == AST_EXPR_MEMBER) {
    // Handle address of struct member: &obj.field
    const char *field_name = target->expr.member.member;
    AstNode *object = target->expr.member.object;

    if (object->type == AST_EXPR_IDENTIFIER) {
      LLVM_Symbol *sym = find_symbol(ctx, object->expr.identifier.name);
      if (sym && !sym->is_function) {
        // Find the struct type
        StructInfo *struct_info = NULL;
        for (StructInfo *info = ctx->struct_types; info; info = info->next) {
          if (info->llvm_type == sym->type) {
            struct_info = info;
            break;
          }
        }

        if (struct_info) {
          int field_index = get_field_index(struct_info, field_name);
          if (field_index >= 0 &&
              is_field_access_allowed(ctx, struct_info, field_index)) {
            // Return pointer to the field
            return LLVMBuildStructGEP2(ctx->builder, sym->type, sym->value,
                                       field_index, "field_addr");
          }
        }
      }
    }
  }

  fprintf(stderr, "Error: Cannot take address of this expression type\n");
  return NULL;
}
