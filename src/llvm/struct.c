#include "llvm.h"

// Find a struct type by name
StructInfo *find_struct_type(CodeGenContext *ctx, const char *name) {
  for (StructInfo *info = ctx->struct_types; info; info = info->next) {
    if (strcmp(info->name, name) == 0) {
      return info;
    }
  }
  return NULL;
}

// Add a struct type to the context
void add_struct_type(CodeGenContext *ctx, StructInfo *struct_info) {
  struct_info->next = ctx->struct_types;
  ctx->struct_types = struct_info;
}

// Get field index by name in a struct
int get_field_index(StructInfo *struct_info, const char *field_name) {
  for (size_t i = 0; i < struct_info->field_count; i++) {
    if (strcmp(struct_info->field_names[i], field_name) == 0) {
      return (int)i;
    }
  }
  return -1;
}

// Check if field access is allowed (public field or same module)
bool is_field_access_allowed(CodeGenContext *ctx, StructInfo *struct_info,
                             int field_index) {
  (void)ctx; // For future module visibility checks
  if (field_index < 0 || field_index >= (int)struct_info->field_count) {
    return false;
  }
  return struct_info->field_is_public[field_index];
}

// Handle struct declaration
LLVMValueRef codegen_stmt_struct(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_STMT_STRUCT) {
    return NULL;
  }

  const char *struct_name = node->stmt.struct_decl.name;
  size_t total_members = node->stmt.struct_decl.public_count +
                         node->stmt.struct_decl.private_count;

  if (total_members == 0) {
    fprintf(stderr, "Error: Struct %s cannot be empty\n", struct_name);
    return NULL;
  }

  // Check if struct already exists
  if (find_struct_type(ctx, struct_name)) {
    fprintf(stderr, "Error: Struct %s is already defined\n", struct_name);
    return NULL;
  }

  // Create StructInfo
  StructInfo *struct_info = (StructInfo *)arena_alloc(
      ctx->arena, sizeof(StructInfo), alignof(StructInfo));

  struct_info->name = arena_strdup(ctx->arena, struct_name);
  struct_info->field_count = total_members;
  struct_info->is_public = node->stmt.struct_decl.is_public;

  // Allocate arrays for field information
  struct_info->field_names = (char **)arena_alloc(
      ctx->arena, sizeof(char *) * total_members, alignof(char *));
  struct_info->field_types = (LLVMTypeRef *)arena_alloc(
      ctx->arena, sizeof(LLVMTypeRef) * total_members, alignof(LLVMTypeRef));
  struct_info->field_element_types = (LLVMTypeRef *)arena_alloc(  // NEW
      ctx->arena, sizeof(LLVMTypeRef) * total_members, alignof(LLVMTypeRef));
  struct_info->field_is_public = (bool *)arena_alloc(
      ctx->arena, sizeof(bool) * total_members, alignof(bool));

  // Process public members first
  size_t field_index = 0;
  for (size_t i = 0; i < node->stmt.struct_decl.public_count; i++) {
    AstNode *field = node->stmt.struct_decl.public_members[i];
    if (field->type != AST_STMT_FIELD_DECL) {
      fprintf(stderr, "Error: Expected field declaration in struct %s\n",
              struct_name);
      return NULL;
    }

    const char *field_name = field->stmt.field_decl.name;

    // Check for duplicate field names
    for (size_t j = 0; j < field_index; j++) {
      if (strcmp(struct_info->field_names[j], field_name) == 0) {
        fprintf(stderr, "Error: Duplicate field name '%s' in struct %s\n",
                field_name, struct_name);
        return NULL;
      }
    }

    struct_info->field_names[field_index] = arena_strdup(ctx->arena, field_name);
    struct_info->field_types[field_index] = codegen_type(ctx, field->stmt.field_decl.type);
    struct_info->field_element_types[field_index] = extract_element_type_from_ast(ctx, field->stmt.field_decl.type);
    struct_info->field_is_public[field_index] = true;

    if (!struct_info->field_types[field_index]) {
      fprintf(stderr,
              "Error: Failed to resolve type for field %s in struct %s\n",
              field_name, struct_name);
      return NULL;
    }
    field_index++;
  }

  // Process private members
  for (size_t i = 0; i < node->stmt.struct_decl.private_count; i++) {
    AstNode *field = node->stmt.struct_decl.private_members[i];
    if (field->type != AST_STMT_FIELD_DECL) {
      fprintf(stderr, "Error: Expected field declaration in struct %s\n",
              struct_name);
      return NULL;
    }

    const char *field_name = field->stmt.field_decl.name;

    // Check for duplicate field names (including public fields)
    for (size_t j = 0; j < field_index; j++) {
      if (strcmp(struct_info->field_names[j], field_name) == 0) {
        fprintf(stderr, "Error: Duplicate field name '%s' in struct %s\n",
                field_name, struct_name);
        return NULL;
      }
    }

    struct_info->field_names[field_index] = arena_strdup(ctx->arena, field_name);
    struct_info->field_types[field_index] = codegen_type(ctx, field->stmt.field_decl.type);
    struct_info->field_element_types[field_index] = extract_element_type_from_ast(ctx, field->stmt.field_decl.type);
    struct_info->field_is_public[field_index] = field->stmt.field_decl.is_public;

    if (!struct_info->field_types[field_index]) {
      fprintf(stderr,
              "Error: Failed to resolve type for field %s in struct %s\n",
              field_name, struct_name);
      return NULL;
    }
    field_index++;
  }

  // Create LLVM struct type
  struct_info->llvm_type = LLVMStructTypeInContext(
      ctx->context, struct_info->field_types, total_members, false);

  // Note: LLVMStructSetName is not available in all LLVM versions
  // The struct will still work without an explicit name

  // Add to context
  add_struct_type(ctx, struct_info);

  // Add struct type to symbol table as a type symbol
  add_symbol(ctx, struct_name, NULL, struct_info->llvm_type, false);

// Debug output (can be removed in production)
#ifdef DEBUG_STRUCTS
  printf("Defined struct %s with %zu fields:\n", struct_name, total_members);
  for (size_t i = 0; i < total_members; i++) {
    printf("  - %s: %s (%s)\n", struct_info->field_names[i], "type",
           struct_info->field_is_public[i] ? "public" : "private");
  }
#endif

  return NULL;
}

// Handle individual field declarations (mainly for completeness)
LLVMValueRef codegen_stmt_field(CodeGenContext *ctx, AstNode *node) {
  // Field declarations are handled by the parent struct
  // This function exists for completeness and error checking
  (void)ctx; // Suppress unused parameter warning

  if (!node || node->type != AST_STMT_FIELD_DECL) {
    return NULL;
  }

  // If we reach here, it means a field declaration was used outside a struct
  fprintf(stderr, "Error: Field declaration '%s' must be inside a struct\n",
          node->stmt.field_decl.name);
  return NULL;
}

LLVMValueRef codegen_expr_struct_access(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_EXPR_MEMBER) {
    return NULL;
  }

  const char *field_name = node->expr.member.member;
  AstNode *object = node->expr.member.object;

  LLVMValueRef struct_ptr = NULL;
  StructInfo *struct_info = NULL;

  // Handle different object types
  if (object->type == AST_EXPR_IDENTIFIER) {
    // Access like: ptr->field or ptr.field
    LLVM_Symbol *sym = find_symbol(ctx, object->expr.identifier.name);
    if (!sym || sym->is_function) {
      fprintf(stderr, "Error: Variable %s not found or is a function\n",
              object->expr.identifier.name);
      return NULL;
    }

    LLVMTypeRef symbol_type = sym->type;

    // Check if this is a pointer to struct (like *Drop)
    if (LLVMGetTypeKind(symbol_type) == LLVMPointerTypeKind) {
      // CRITICAL FIX: Use the element_type from symbol if available
      if (sym->element_type) {
        // Find struct info by LLVM type
        for (StructInfo *info = ctx->struct_types; info; info = info->next) {
          if (info->llvm_type == sym->element_type) {
            struct_info = info;
            break;
          }
        }

        if (!struct_info) {
          // Try to find by field name as fallback
          for (StructInfo *info = ctx->struct_types; info; info = info->next) {
            int field_idx = get_field_index(info, field_name);
            if (field_idx >= 0) {
              struct_info = info;
              break;
            }
          }
        }

        if (struct_info) {
          // Load the pointer value (the actual struct instance address)
          struct_ptr = LLVMBuildLoad2(ctx->builder, symbol_type, sym->value,
                                      "load_struct_ptr");
        }
      } else {
        // Fallback: Try to find struct by field name
        for (StructInfo *info = ctx->struct_types; info; info = info->next) {
          int field_idx = get_field_index(info, field_name);
          if (field_idx >= 0) {
            struct_info = info;
            break;
          }
        }

        if (struct_info) {
          // Load the pointer
          struct_ptr = LLVMBuildLoad2(ctx->builder, symbol_type, sym->value,
                                      "load_struct_ptr");
        }
      }

    } else {
      // Direct struct type (stored by value)
      for (StructInfo *info = ctx->struct_types; info; info = info->next) {
        if (info->llvm_type == symbol_type) {
          struct_info = info;
          struct_ptr = sym->value; // This is already the struct address
          break;
        }
      }
    }

  } else if (object->type == AST_EXPR_DEREF) {
    // Pointer dereference: (*struct_ptr).field
    LLVMValueRef ptr = codegen_expr(ctx, object->expr.deref.object);
    if (!ptr) {
      return NULL;
    }

    struct_ptr = ptr;

    // Find matching struct by field name
    for (StructInfo *info = ctx->struct_types; info; info = info->next) {
      int field_idx = get_field_index(info, field_name);
      if (field_idx >= 0) {
        struct_info = info;
        break;
      }
    }

  } else {
    // Handle other cases (function calls, etc.)
    fprintf(stderr, "Error: Unsupported struct access pattern\n");
    return NULL;
  }

  if (!struct_info || !struct_ptr) {
    fprintf(
        stderr,
        "Error: Could not determine struct type for member access '%s.%s'\n",
        object->type == AST_EXPR_IDENTIFIER ? object->expr.identifier.name
                                            : "?",
        field_name);
    return NULL;
  }

  // Find field index
  int field_index = get_field_index(struct_info, field_name);
  if (field_index < 0) {
    fprintf(stderr, "Error: Field '%s' not found in struct '%s'\n", field_name,
            struct_info->name);
    return NULL;
  }

  // Check access permissions
  if (!is_field_access_allowed(ctx, struct_info, field_index)) {
    fprintf(stderr, "Error: Field '%s' in struct '%s' is private\n", field_name,
            struct_info->name);
    return NULL;
  }

  // Generate GEP to get field address
  LLVMValueRef field_ptr =
      LLVMBuildStructGEP2(ctx->builder, struct_info->llvm_type, struct_ptr,
                          field_index, "field_ptr");

  // Load the field value
  return LLVMBuildLoad2(ctx->builder, struct_info->field_types[field_index],
                        field_ptr, "field_val");
}

// Handle struct member assignment (obj.field = value)
LLVMValueRef codegen_expr_struct_assignment(CodeGenContext *ctx,
                                            AstNode *node) {
  if (!node || node->type != AST_EXPR_ASSIGNMENT) {
    return NULL;
  }

  AstNode *target = node->expr.assignment.target;
  LLVMValueRef value = codegen_expr(ctx, node->expr.assignment.value);
  if (!value) {
    return NULL;
  }

  if (target->type != AST_EXPR_MEMBER) {
    return NULL; // Not a struct field assignment
  }

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
      struct_ptr = LLVMBuildLoad2(ctx->builder, ptr_to_struct_type, sym->value,
                                  "load_struct_ptr");
    } else if (symbol_type == struct_info->llvm_type) {
      // Direct struct variable
      struct_ptr = sym->value;
    } else {
      fprintf(stderr,
              "Error: Variable '%s' is not a struct or pointer to struct\n",
              var_name);
      return NULL;
    }

    // Check type compatibility
    LLVMTypeRef expected_type = struct_info->field_types[field_index];
    LLVMTypeRef actual_type = LLVMTypeOf(value);

    if (expected_type != actual_type) {
      // Try basic type conversions
      if (LLVMGetTypeKind(expected_type) == LLVMIntegerTypeKind &&
          LLVMGetTypeKind(actual_type) == LLVMIntegerTypeKind) {
        unsigned expected_bits = LLVMGetIntTypeWidth(expected_type);
        unsigned actual_bits = LLVMGetIntTypeWidth(actual_type);

        if (expected_bits > actual_bits) {
          value = LLVMBuildSExt(ctx->builder, value, expected_type, "extend");
        } else if (expected_bits < actual_bits) {
          value = LLVMBuildTrunc(ctx->builder, value, expected_type, "trunc");
        }
      }
    }

    // Use GEP to get the field address and store the value
    LLVMValueRef field_ptr =
        LLVMBuildStructGEP2(ctx->builder, struct_info->llvm_type, struct_ptr,
                            field_index, "field_ptr");

    LLVMBuildStore(ctx->builder, value, field_ptr);
    return value;
  }

  fprintf(stderr, "Error: Unsupported struct assignment pattern\n");
  return NULL;
}

// Handle struct type in type system
LLVMTypeRef codegen_type_struct(CodeGenContext *ctx, const char *struct_name) {
  StructInfo *struct_info = find_struct_type(ctx, struct_name);
  if (struct_info) {
    return struct_info->llvm_type;
  }

  fprintf(stderr, "Error: Struct type '%s' not found\n", struct_name);
  return NULL;
}

// Helper function to create a struct literal/initializer (if needed)
LLVMValueRef codegen_struct_literal(CodeGenContext *ctx,
                                    const char *struct_name,
                                    LLVMValueRef *field_values,
                                    size_t field_count) {
  StructInfo *struct_info = find_struct_type(ctx, struct_name);
  if (!struct_info) {
    fprintf(stderr, "Error: Struct type '%s' not found for literal\n",
            struct_name);
    return NULL;
  }

  if (field_count != struct_info->field_count) {
    fprintf(stderr,
            "Error: Struct literal field count mismatch for '%s': expected "
            "%zu, got %zu\n",
            struct_name, struct_info->field_count, field_count);
    return NULL;
  }

  // Create struct value using LLVMConstStruct for constants
  // or build it piece by piece for runtime values
  bool all_constant = true;
  for (size_t i = 0; i < field_count; i++) {
    if (!LLVMIsConstant(field_values[i])) {
      all_constant = false;
      break;
    }
  }

  if (all_constant) {
    return LLVMConstStruct(field_values, field_count, false);
  } else {
    // Runtime construction
    LLVMValueRef struct_alloca =
        LLVMBuildAlloca(ctx->builder, struct_info->llvm_type, "struct_lit");

    for (size_t i = 0; i < field_count; i++) {
      LLVMValueRef field_ptr = LLVMBuildStructGEP2(
          ctx->builder, struct_info->llvm_type, struct_alloca, i, "init_field");
      LLVMBuildStore(ctx->builder, field_values[i], field_ptr);
    }

    return LLVMBuildLoad2(ctx->builder, struct_info->llvm_type, struct_alloca,
                          "struct_val");
  }
}
