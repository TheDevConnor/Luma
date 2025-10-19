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

LLVMValueRef codegen_stmt_struct(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_STMT_STRUCT) {
    return NULL;
  }

  const char *struct_name = node->stmt.struct_decl.name;
  size_t public_count = node->stmt.struct_decl.public_count;
  size_t private_count = node->stmt.struct_decl.private_count;

  // Separate data fields from methods
  size_t data_field_count = 0;
  for (size_t i = 0; i < public_count; i++) {
    AstNode *member = node->stmt.struct_decl.public_members[i];
    if (member->type == AST_STMT_FIELD_DECL &&
        !member->stmt.field_decl.function) {
      data_field_count++;
    }
  }
  for (size_t i = 0; i < private_count; i++) {
    AstNode *member = node->stmt.struct_decl.private_members[i];
    if (member->type == AST_STMT_FIELD_DECL &&
        !member->stmt.field_decl.function) {
      data_field_count++;
    }
  }

  if (data_field_count == 0) {
    fprintf(stderr, "Error: Struct %s must have at least one data field\n",
            struct_name);
    return NULL;
  }

  // Check if struct already exists
  if (find_struct_type(ctx, struct_name)) {
    fprintf(stderr, "Error: Struct %s is already defined\n", struct_name);
    return NULL;
  }

  // Create StructInfo for data fields only
  StructInfo *struct_info = (StructInfo *)arena_alloc(
      ctx->arena, sizeof(StructInfo), alignof(StructInfo));

  struct_info->name = arena_strdup(ctx->arena, struct_name);
  struct_info->field_count = data_field_count;
  struct_info->is_public = node->stmt.struct_decl.is_public;

  // Allocate arrays for field information
  struct_info->field_names = (char **)arena_alloc(
      ctx->arena, sizeof(char *) * data_field_count, alignof(char *));
  struct_info->field_types = (LLVMTypeRef *)arena_alloc(
      ctx->arena, sizeof(LLVMTypeRef) * data_field_count, alignof(LLVMTypeRef));
  struct_info->field_element_types = (LLVMTypeRef *)arena_alloc(
      ctx->arena, sizeof(LLVMTypeRef) * data_field_count, alignof(LLVMTypeRef));
  struct_info->field_is_public = (bool *)arena_alloc(
      ctx->arena, sizeof(bool) * data_field_count, alignof(bool));

  // CRITICAL FIX: Create an OPAQUE struct type FIRST (forward declaration)
  // This allows self-referential structs like: struct Node { next: *Node; }
  struct_info->llvm_type = LLVMStructCreateNamed(ctx->context, struct_name);

  // Add to context IMMEDIATELY so it can be found during field type resolution
  add_struct_type(ctx, struct_info);
  add_symbol(ctx, struct_name, NULL, struct_info->llvm_type, false);

  // Process public data fields
  size_t field_index = 0;
  for (size_t i = 0; i < public_count; i++) {
    AstNode *member = node->stmt.struct_decl.public_members[i];
    if (member->type != AST_STMT_FIELD_DECL)
      continue;

    // Skip methods for now, we'll process them after the struct type is created
    if (member->stmt.field_decl.function)
      continue;

    const char *field_name = member->stmt.field_decl.name;

    // Check for duplicate field names
    for (size_t j = 0; j < field_index; j++) {
      if (strcmp(struct_info->field_names[j], field_name) == 0) {
        fprintf(stderr, "Error: Duplicate field name '%s' in struct %s\n",
                field_name, struct_name);
        return NULL;
      }
    }

    struct_info->field_names[field_index] =
        arena_strdup(ctx->arena, field_name);
    struct_info->field_types[field_index] =
        codegen_type(ctx, member->stmt.field_decl.type);
    struct_info->field_element_types[field_index] =
        extract_element_type_from_ast(ctx, member->stmt.field_decl.type);
    struct_info->field_is_public[field_index] = true;

    if (!struct_info->field_types[field_index]) {
      fprintf(stderr,
              "Error: Failed to resolve type for field %s in struct %s\n",
              field_name, struct_name);
      return NULL;
    }
    field_index++;
  }

  // Process private data fields
  for (size_t i = 0; i < private_count; i++) {
    AstNode *member = node->stmt.struct_decl.private_members[i];
    if (member->type != AST_STMT_FIELD_DECL)
      continue;

    if (member->stmt.field_decl.function)
      continue;

    const char *field_name = member->stmt.field_decl.name;

    for (size_t j = 0; j < field_index; j++) {
      if (strcmp(struct_info->field_names[j], field_name) == 0) {
        fprintf(stderr, "Error: Duplicate field name '%s' in struct %s\n",
                field_name, struct_name);
        return NULL;
      }
    }

    struct_info->field_names[field_index] =
        arena_strdup(ctx->arena, field_name);
    struct_info->field_types[field_index] =
        codegen_type(ctx, member->stmt.field_decl.type);
    struct_info->field_element_types[field_index] =
        extract_element_type_from_ast(ctx, member->stmt.field_decl.type);
    struct_info->field_is_public[field_index] =
        member->stmt.field_decl.is_public;

    if (!struct_info->field_types[field_index]) {
      fprintf(stderr,
              "Error: Failed to resolve type for field %s in struct %s\n",
              field_name, struct_name);
      return NULL;
    }
    field_index++;
  }

  // CRITICAL: Set the struct body AFTER all field types are resolved
  // This completes the opaque struct declaration with its actual fields
  LLVMStructSetBody(struct_info->llvm_type, struct_info->field_types,
                    data_field_count, false);

  // NOW process methods with access to the complete struct type
  for (size_t i = 0; i < public_count; i++) {
    AstNode *member = node->stmt.struct_decl.public_members[i];
    if (member->type != AST_STMT_FIELD_DECL)
      continue;

    // Only process methods
    if (!member->stmt.field_decl.function)
      continue;

    AstNode *func_node = member->stmt.field_decl.function;
    const char *method_name = member->stmt.field_decl.name;

    // Generate the method with implicit 'self' parameter
    if (!codegen_struct_method(ctx, func_node, struct_info, method_name,
                               true)) {
      fprintf(stderr, "Error: Failed to generate method '%s' for struct '%s'\n",
              method_name, struct_name);
      return NULL;
    }
  }

  // Process private methods
  for (size_t i = 0; i < private_count; i++) {
    AstNode *member = node->stmt.struct_decl.private_members[i];
    if (member->type != AST_STMT_FIELD_DECL)
      continue;

    if (!member->stmt.field_decl.function)
      continue;

    AstNode *func_node = member->stmt.field_decl.function;
    const char *method_name = member->stmt.field_decl.name;

    if (!codegen_struct_method(ctx, func_node, struct_info, method_name,
                               false)) {
      fprintf(stderr,
              "Error: Failed to generate private method '%s' for struct '%s'\n",
              method_name, struct_name);
      return NULL;
    }
  }

  return NULL;
}

LLVMValueRef codegen_struct_method(CodeGenContext *ctx, AstNode *func_node,
                                   StructInfo *struct_info,
                                   const char *method_name, bool is_public) {
  if (!func_node || func_node->type != AST_STMT_FUNCTION) {
    fprintf(stderr, "Error: Invalid function node for method '%s'\n",
            method_name);
    return NULL;
  }

  // Get method signature
  AstNode *return_type_node = func_node->stmt.func_decl.return_type;
  size_t original_param_count = func_node->stmt.func_decl.param_count;
  AstNode **original_param_type_nodes = func_node->stmt.func_decl.param_types;
  char **original_param_names = func_node->stmt.func_decl.param_names;

  // CRITICAL: Methods need an implicit 'self' parameter as the FIRST parameter
  // The typechecker injects 'self' when calling methods, so the method
  // definition must match
  size_t param_count = original_param_count + 1; // +1 for 'self'

  // Allocate arrays for ALL parameters (including self)
  LLVMTypeRef *llvm_param_types = (LLVMTypeRef *)arena_alloc(
      ctx->arena, sizeof(LLVMTypeRef) * param_count, alignof(LLVMTypeRef));

  char **param_names = (char **)arena_alloc(
      ctx->arena, sizeof(char *) * param_count, alignof(char *));

  AstNode **param_type_nodes = (AstNode **)arena_alloc(
      ctx->arena, sizeof(AstNode *) * param_count, alignof(AstNode *));

  // First parameter is 'self' - a pointer to the struct
  llvm_param_types[0] = LLVMPointerType(struct_info->llvm_type, 0);
  param_names[0] = "self";
  param_type_nodes[0] =
      NULL; // We'll handle this specially for element type extraction

  // Copy the rest of the original parameters (shifted by 1)
  for (size_t i = 0; i < original_param_count; i++) {
    llvm_param_types[i + 1] = codegen_type(ctx, original_param_type_nodes[i]);
    if (!llvm_param_types[i + 1]) {
      fprintf(stderr,
              "Error: Failed to resolve parameter type %zu for method '%s'\n",
              i, method_name);
      return NULL;
    }
    param_names[i + 1] = original_param_names[i];
    param_type_nodes[i + 1] = original_param_type_nodes[i];
  }

  // Create function type
  LLVMTypeRef llvm_return_type = codegen_type(ctx, return_type_node);
  if (!llvm_return_type) {
    fprintf(stderr, "Error: Failed to resolve return type for method '%s'\n",
            method_name);
    return NULL;
  }

  LLVMTypeRef func_type =
      LLVMFunctionType(llvm_return_type, llvm_param_types, param_count, 0);

  // Get the current LLVM module
  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;

  // Create the function in the current module
  LLVMValueRef func =
      LLVMAddFunction(current_llvm_module, method_name, func_type);

  if (!func) {
    fprintf(stderr, "Error: Failed to create LLVM function for method '%s'\n",
            method_name);
    return NULL;
  }

  // Set linkage
  if (is_public) {
    LLVMSetLinkage(func, LLVMExternalLinkage);
  } else {
    LLVMSetLinkage(func, LLVMInternalLinkage);
  }

  // CRITICAL: Save the old function context before starting method generation
  LLVMValueRef old_function = ctx->current_function;

  // Set current function context
  ctx->current_function = func;

  // Create entry basic block
  LLVMBasicBlockRef entry =
      LLVMAppendBasicBlockInContext(ctx->context, func, "entry");
  LLVMPositionBuilderAtEnd(ctx->builder, entry);

  // Add all parameters to symbol table (including self at index 0)
  for (size_t i = 0; i < param_count; i++) {
    LLVMValueRef param = LLVMGetParam(func, i);
    const char *param_name = param_names[i];

    LLVMSetValueName2(param, param_name, strlen(param_name));

    // Allocate stack space and store parameter
    LLVMValueRef alloca =
        LLVMBuildAlloca(ctx->builder, llvm_param_types[i], param_name);
    LLVMBuildStore(ctx->builder, param, alloca);

    // Extract element type for pointer parameters (needed for self which is
    // *Person)
    LLVMTypeRef element_type =
        extract_element_type_from_ast(ctx, param_type_nodes[i]);

    // Add to symbol table with element type information
    add_symbol_with_element_type(ctx, param_name, alloca, llvm_param_types[i],
                                 element_type, false);
  }

  // Generate method body
  AstNode *body = func_node->stmt.func_decl.body;
  if (body) {
    codegen_stmt(ctx, body);
  }

  // Add return if missing for void functions
  if (LLVMGetTypeKind(llvm_return_type) == LLVMVoidTypeKind) {
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildRetVoid(ctx->builder);
    }
  }

  // Verify the function
  if (LLVMVerifyFunction(func, LLVMReturnStatusAction)) {
    fprintf(stderr, "Error: Function verification failed for method '%s'\n",
            method_name);
    LLVMDumpValue(func);
    // Restore context even on error
    ctx->current_function = old_function;
    return NULL;
  }

  // CRITICAL: Restore the old function context
  ctx->current_function = old_function;

  return func;
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
