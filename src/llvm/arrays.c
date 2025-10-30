#include "llvm.h"

// Helper function for type conversions between array elements
LLVMValueRef convert_value_to_type(CodeGenContext *ctx, LLVMValueRef value,
                                   LLVMTypeRef from_type, LLVMTypeRef to_type) {
  if (from_type == to_type) {
    return value;
  }

  LLVMTypeKind from_kind = LLVMGetTypeKind(from_type);
  LLVMTypeKind to_kind = LLVMGetTypeKind(to_type);

  // Integer to integer conversions
  if (from_kind == LLVMIntegerTypeKind && to_kind == LLVMIntegerTypeKind) {
    unsigned from_bits = LLVMGetIntTypeWidth(from_type);
    unsigned to_bits = LLVMGetIntTypeWidth(to_type);

    if (from_bits < to_bits) {
      return LLVMBuildSExt(ctx->builder, value, to_type, "sext");
    } else if (from_bits > to_bits) {
      return LLVMBuildTrunc(ctx->builder, value, to_type, "trunc");
    }
    return value;
  }

  // Float conversions
  if (from_kind == LLVMFloatTypeKind && to_kind == LLVMDoubleTypeKind) {
    return LLVMBuildFPExt(ctx->builder, value, to_type, "fpext");
  }
  if (from_kind == LLVMDoubleTypeKind && to_kind == LLVMFloatTypeKind) {
    return LLVMBuildFPTrunc(ctx->builder, value, to_type, "fptrunc");
  }

  // Integer to float conversions
  if (from_kind == LLVMIntegerTypeKind &&
      (to_kind == LLVMFloatTypeKind || to_kind == LLVMDoubleTypeKind)) {
    return LLVMBuildSIToFP(ctx->builder, value, to_type, "sitofp");
  }

  // Float to integer conversions
  if ((from_kind == LLVMFloatTypeKind || from_kind == LLVMDoubleTypeKind) &&
      to_kind == LLVMIntegerTypeKind) {
    return LLVMBuildFPToSI(ctx->builder, value, to_type, "fptosi");
  }

  // If no conversion available, return NULL to signal error
  return NULL;
}

// Array bounds checking helper (optional)
bool check_array_bounds_runtime(CodeGenContext *ctx, LLVMValueRef array_ptr,
                                LLVMTypeRef array_type, LLVMValueRef index,
                                const char *var_name) {
  (void)ctx;
  (void)array_ptr;
  if (LLVMGetTypeKind(array_type) != LLVMArrayTypeKind) {
    return true; // Can't check bounds for non-arrays
  }

  unsigned array_length = LLVMGetArrayLength(array_type);

  // Only check if index is a constant
  if (LLVMIsConstant(index) &&
      LLVMGetTypeKind(LLVMTypeOf(index)) == LLVMIntegerTypeKind) {
    long long index_val = LLVMConstIntGetSExtValue(index);

    if (index_val < 0) {
      fprintf(stderr, "Error: Negative array index %lld for array '%s'\n",
              index_val, var_name ? var_name : "unknown");
      return false;
    }

    if (index_val >= (long long)array_length) {
      fprintf(
          stderr,
          "Error: Array index %lld out of bounds for array '%s' of length %u\n",
          index_val, var_name ? var_name : "unknown", array_length);
      return false;
    }
  }

  // For runtime indices, we could insert runtime bounds checking here
  // This would involve creating conditional branches and error handling

  return true;
}

// Multi-dimensional array support
LLVMValueRef codegen_multidim_array_access(CodeGenContext *ctx,
                                           AstNode *base_expr,
                                           AstNode **indices,
                                           size_t index_count) {
  LLVMValueRef current_value = codegen_expr(ctx, base_expr);
  if (!current_value) {
    return NULL;
  }

  for (size_t i = 0; i < index_count; i++) {
    LLVMValueRef index = codegen_expr(ctx, indices[i]);
    if (!index) {
      return NULL;
    }

    LLVMTypeRef current_type = LLVMTypeOf(current_value);
    LLVMTypeKind current_kind = LLVMGetTypeKind(current_type);

    if (current_kind == LLVMArrayTypeKind) {
      LLVMTypeRef element_type = LLVMGetElementType(current_type);

      // Store array and get pointer for GEP
      LLVMValueRef array_alloca =
          LLVMBuildAlloca(ctx->builder, current_type, "temp_array");
      LLVMBuildStore(ctx->builder, current_value, array_alloca);

      LLVMValueRef gep_indices[2];
      gep_indices[0] =
          LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false);
      gep_indices[1] = index;

      LLVMValueRef element_ptr =
          LLVMBuildGEP2(ctx->builder, current_type, array_alloca, gep_indices,
                        2, "element_ptr");

      if (i == index_count - 1 ||
          LLVMGetTypeKind(element_type) != LLVMArrayTypeKind) {
        // Last dimension or element is not an array - load the value
        current_value = LLVMBuildLoad2(ctx->builder, element_type, element_ptr,
                                       "element_val");
      } else {
        // Still have more dimensions - return pointer for next iteration
        current_value = element_ptr;
      }

    } else if (current_kind == LLVMPointerTypeKind) {
      // Pointer arithmetic for remaining dimensions
      // This assumes we know the element type somehow
      // In practice, you'd need better type tracking

      fprintf(stderr, "Warning: Multi-dimensional pointer indexing needs "
                      "better type information\n");
      // Use simple pointer arithmetic as fallback
      current_value =
          LLVMBuildGEP2(ctx->builder, LLVMInt8TypeInContext(ctx->context),
                        current_value, &index, 1, "ptr_index");
    } else {
      fprintf(stderr,
              "Error: Cannot index into non-array, non-pointer type at "
              "dimension %zu\n",
              i);
      return NULL;
    }
  }

  return current_value;
}

void copy_array_elements(CodeGenContext *ctx, LLVMValueRef dest_array,
                         LLVMTypeRef dest_type, LLVMValueRef src_array,
                         LLVMTypeRef src_type) {
  if (LLVMGetTypeKind(dest_type) != LLVMArrayTypeKind ||
      LLVMGetTypeKind(src_type) != LLVMArrayTypeKind) {
    return;
  }

  unsigned dest_length = LLVMGetArrayLength(dest_type);
  unsigned src_length = LLVMGetArrayLength(src_type);
  unsigned copy_length = (dest_length < src_length) ? dest_length : src_length;

  LLVMTypeRef dest_element_type = LLVMGetElementType(dest_type);
  LLVMTypeRef src_element_type = LLVMGetElementType(src_type);

  // Create temporary for source array if it's not already in memory
  LLVMValueRef src_ptr;
  if (LLVMIsConstant(src_array)) {
    src_ptr = LLVMBuildAlloca(ctx->builder, src_type, "temp_src_array");
    LLVMBuildStore(ctx->builder, src_array, src_ptr);
  } else {
    src_ptr = LLVMBuildAlloca(ctx->builder, src_type, "temp_src_array");
    LLVMBuildStore(ctx->builder, src_array, src_ptr);
  }

  // Copy each element
  for (unsigned i = 0; i < copy_length; i++) {
    // Get source element
    LLVMValueRef src_indices[2];
    src_indices[0] =
        LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false);
    src_indices[1] =
        LLVMConstInt(LLVMInt32TypeInContext(ctx->context), i, false);

    LLVMValueRef src_element_ptr = LLVMBuildGEP2(
        ctx->builder, src_type, src_ptr, src_indices, 2, "src_element_ptr");
    LLVMValueRef src_element = LLVMBuildLoad2(ctx->builder, src_element_type,
                                              src_element_ptr, "src_element");

    // Convert type if needed
    if (dest_element_type != src_element_type) {
      src_element = convert_value_to_type(ctx, src_element, src_element_type,
                                          dest_element_type);
      if (!src_element) {
        fprintf(stderr, "Error: Cannot convert array element type\n");
        return;
      }
    }

    // Store in destination
    LLVMValueRef dest_indices[2];
    dest_indices[0] =
        LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false);
    dest_indices[1] =
        LLVMConstInt(LLVMInt32TypeInContext(ctx->context), i, false);

    LLVMValueRef dest_element_ptr =
        LLVMBuildGEP2(ctx->builder, dest_type, dest_array, dest_indices, 2,
                      "dest_element_ptr");
    LLVMBuildStore(ctx->builder, src_element, dest_element_ptr);
  }
}
