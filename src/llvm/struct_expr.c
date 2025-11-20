#include "llvm.h"

// Helper function to infer struct type from context
static StructInfo *infer_struct_type_from_context(CodeGenContext *ctx,
                                                  char **field_names,
                                                  size_t field_count) {
  // Try to find a struct that has all these field names
  for (StructInfo *info = ctx->struct_types; info; info = info->next) {
    if (info->field_count != field_count) {
      continue;
    }

    // Check if all field names match (order-independent)
    bool all_match = true;
    for (size_t i = 0; i < field_count; i++) {
      bool found = false;
      for (size_t j = 0; j < info->field_count; j++) {
        if (strcmp(field_names[i], info->field_names[j]) == 0) {
          found = true;
          break;
        }
      }
      if (!found) {
        all_match = false;
        break;
      }
    }

    if (all_match) {
      return info;
    }
  }

  return NULL;
}

// Helper function to map user-provided field order to struct definition order
static bool map_field_order(StructInfo *struct_info, char **provided_names,
                            AstNode **provided_values, size_t provided_count,
                            LLVMValueRef *ordered_values) {

  // Check that all provided fields exist in struct
  for (size_t i = 0; i < provided_count; i++) {
    int field_idx = get_field_index(struct_info, provided_names[i]);
    if (field_idx < 0) {
      fprintf(stderr, "Error: Field '%s' not found in struct '%s'\n",
              provided_names[i], struct_info->name);
      return false;
    }
  }

  // Map provided values to struct field order
  for (size_t i = 0; i < struct_info->field_count; i++) {
    bool found = false;

    for (size_t j = 0; j < provided_count; j++) {
      if (strcmp(struct_info->field_names[i], provided_names[j]) == 0) {
        ordered_values[i] =
            (LLVMValueRef)provided_values[j]; // Store AST node temporarily
        found = true;
        break;
      }
    }

    if (!found) {
      fprintf(stderr,
              "Error: Missing field '%s' in struct initialization for '%s'\n",
              struct_info->field_names[i], struct_info->name);
      return false;
    }
  }

  return true;
}

LLVMValueRef codegen_expr_struct_literal(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_EXPR_STRUCT) {
    fprintf(stderr, "Error: Expected struct expression node\n");
    return NULL;
  }

  const char *struct_name = node->expr.struct_expr.name;
  char **field_names = node->expr.struct_expr.field_names;
  AstNode **field_values = node->expr.struct_expr.field_value;
  size_t field_count = node->expr.struct_expr.field_count;

  StructInfo *struct_info = NULL;

  // Case 1: Explicit struct type (Point { x: 20, y: 40 })
  if (struct_name) {
    struct_info = find_struct_type(ctx, struct_name);
    if (!struct_info) {
      fprintf(stderr, "Error: Struct type '%s' not found\n", struct_name);
      return NULL;
    }
  }
  // Case 2: Inferred struct type ({ x: 50, y: 10 })
  else {
    struct_info = infer_struct_type_from_context(ctx, field_names, field_count);
    if (!struct_info) {
      fprintf(stderr, "Error: Could not infer struct type from field names. ");
      fprintf(stderr, "Provided fields: ");
      for (size_t i = 0; i < field_count; i++) {
        fprintf(stderr, "%s%s", field_names[i],
                i < field_count - 1 ? ", " : "\n");
      }
      return NULL;
    }
  }

  // Verify field count matches
  if (field_count != struct_info->field_count) {
    fprintf(stderr, "Error: Struct '%s' expects %zu fields, got %zu\n",
            struct_info->name, struct_info->field_count, field_count);
    return NULL;
  }

  // Allocate array for ordered field values (in struct definition order)
  LLVMValueRef *ordered_ast_nodes = (LLVMValueRef *)arena_alloc(
      ctx->arena, sizeof(LLVMValueRef) * field_count, alignof(LLVMValueRef));

  // Map user-provided field order to struct definition order
  if (!map_field_order(struct_info, field_names, field_values, field_count,
                       ordered_ast_nodes)) {
    return NULL;
  }

  // Now generate code for each field value in the correct order
  LLVMValueRef *llvm_field_values = (LLVMValueRef *)arena_alloc(
      ctx->arena, sizeof(LLVMValueRef) * field_count, alignof(LLVMValueRef));

  bool all_constant = true;

  for (size_t i = 0; i < field_count; i++) {
    // Generate the field value expression
    AstNode *field_value_node = (AstNode *)ordered_ast_nodes[i];
    llvm_field_values[i] = codegen_expr(ctx, field_value_node);

    if (!llvm_field_values[i]) {
      fprintf(stderr,
              "Error: Failed to generate value for field '%s' in struct '%s'\n",
              struct_info->field_names[i], struct_info->name);
      return NULL;
    }

    // Check type compatibility
    LLVMTypeRef expected_type = struct_info->field_types[i];
    LLVMTypeRef actual_type = LLVMTypeOf(llvm_field_values[i]);

    if (expected_type != actual_type) {
      // Try type conversion
      LLVMTypeKind expected_kind = LLVMGetTypeKind(expected_type);
      LLVMTypeKind actual_kind = LLVMGetTypeKind(actual_type);

      // Integer conversions
      if (expected_kind == LLVMIntegerTypeKind &&
          actual_kind == LLVMIntegerTypeKind) {
        unsigned expected_bits = LLVMGetIntTypeWidth(expected_type);
        unsigned actual_bits = LLVMGetIntTypeWidth(actual_type);

        if (expected_bits > actual_bits) {
          llvm_field_values[i] = LLVMBuildSExt(
              ctx->builder, llvm_field_values[i], expected_type, "sext_field");
        } else if (expected_bits < actual_bits) {
          llvm_field_values[i] = LLVMBuildTrunc(
              ctx->builder, llvm_field_values[i], expected_type, "trunc_field");
        }
      }
      // Float conversions
      else if (expected_kind == LLVMDoubleTypeKind &&
               actual_kind == LLVMFloatTypeKind) {
        llvm_field_values[i] = LLVMBuildFPExt(
            ctx->builder, llvm_field_values[i], expected_type, "fpext_field");
      } else if (expected_kind == LLVMFloatTypeKind &&
                 actual_kind == LLVMDoubleTypeKind) {
        llvm_field_values[i] = LLVMBuildFPTrunc(
            ctx->builder, llvm_field_values[i], expected_type, "fptrunc_field");
      }
      // Int to float
      else if ((expected_kind == LLVMFloatTypeKind ||
                expected_kind == LLVMDoubleTypeKind) &&
               actual_kind == LLVMIntegerTypeKind) {
        llvm_field_values[i] = LLVMBuildSIToFP(
            ctx->builder, llvm_field_values[i], expected_type, "sitofp_field");
      }
      // Float to int
      else if (expected_kind == LLVMIntegerTypeKind &&
               (actual_kind == LLVMFloatTypeKind ||
                actual_kind == LLVMDoubleTypeKind)) {
        llvm_field_values[i] = LLVMBuildFPToSI(
            ctx->builder, llvm_field_values[i], expected_type, "fptosi_field");
      } else {
        fprintf(stderr, "Error: Type mismatch for field '%s' in struct '%s'\n",
                struct_info->field_names[i], struct_info->name);
        fprintf(stderr, "  Expected type kind: %d, got type kind: %d\n",
                expected_kind, actual_kind);
        return NULL;
      }
    }

    // Check if all values are constants
    if (all_constant && !LLVMIsConstant(llvm_field_values[i])) {
      all_constant = false;
    }
  }

  // Create the struct value
  if (all_constant) {
    // Create constant struct
    return LLVMConstNamedStruct(struct_info->llvm_type, llvm_field_values,
                                field_count);
  } else {
    // Create runtime struct
    LLVMValueRef struct_alloca =
        LLVMBuildAlloca(ctx->builder, struct_info->llvm_type, "struct_literal");

    // Store each field value
    for (size_t i = 0; i < field_count; i++) {
      LLVMValueRef field_ptr = LLVMBuildStructGEP2(
          ctx->builder, struct_info->llvm_type, struct_alloca, i, "field_ptr");
      LLVMBuildStore(ctx->builder, llvm_field_values[i], field_ptr);
    }

    // Load and return the complete struct
    return LLVMBuildLoad2(ctx->builder, struct_info->llvm_type, struct_alloca,
                          "struct_val");
  }
}
