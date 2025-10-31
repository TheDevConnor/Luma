#include "llvm.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdio.h>
#include <stdlib.h>

// Legacy program handler (now redirects to multi-module handler)
LLVMValueRef codegen_stmt_program(CodeGenContext *ctx, AstNode *node) {
  return codegen_stmt_program_multi_module(ctx, node);
}

LLVMValueRef codegen_stmt_expression(CodeGenContext *ctx, AstNode *node) {
  return codegen_expr(ctx, node->stmt.expr_stmt.expression);
}

void add_symbol_with_element_type(CodeGenContext *ctx, const char *name,
                                  LLVMValueRef value, LLVMTypeRef type,
                                  LLVMTypeRef element_type, bool is_function) {
  if (ctx->current_module) {
    add_symbol_to_module_with_element_type(ctx->current_module, name, value,
                                           type, element_type, is_function);
  }
}

void add_symbol_to_module_with_element_type(
    ModuleCompilationUnit *module, const char *name, LLVMValueRef value,
    LLVMTypeRef type, LLVMTypeRef element_type, bool is_function) {
  LLVM_Symbol *sym = (LLVM_Symbol *)malloc(sizeof(LLVM_Symbol));
  sym->name = strdup(name);
  sym->value = value;
  sym->type = type;
  sym->element_type = element_type; // Store the element type for pointers
  sym->is_function = is_function;
  sym->next = module->symbols;
  module->symbols = sym;
}

LLVMTypeRef extract_element_type_from_ast(CodeGenContext *ctx,
                                          AstNode *type_node) {
  if (!type_node)
    return NULL;

  if (type_node->type == AST_TYPE_POINTER) {
    // This is a pointer type, get what it points to
    AstNode *pointee = type_node->type_data.pointer.pointee_type;

    // Generate the full type for what this pointer points to
    // This correctly handles **char -> *char, ***int -> **int, etc.
    return codegen_type(ctx, pointee);
  }

  return NULL; // Not a pointer type
}

// Modified variable declaration function
LLVMValueRef codegen_stmt_var_decl(CodeGenContext *ctx, AstNode *node) {
  LLVMTypeRef var_type = codegen_type(ctx, node->stmt.var_decl.var_type);
  if (!var_type)
    return NULL;

  // Extract element type if this is a pointer
  LLVMTypeRef element_type =
      extract_element_type_from_ast(ctx, node->stmt.var_decl.var_type);

  LLVMValueRef var_ref;
  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;

  if (ctx->current_function == NULL) {
    var_ref =
        LLVMAddGlobal(current_llvm_module, var_type, node->stmt.var_decl.name);
    if (node->stmt.var_decl.is_public) {
      LLVMSetLinkage(var_ref, LLVMExternalLinkage);
    } else {
      LLVMSetLinkage(var_ref, LLVMInternalLinkage);
    }
  } else {
    var_ref = LLVMBuildAlloca(ctx->builder, var_type, node->stmt.var_decl.name);
  }

  // Handle initializer with type checking
  if (node->stmt.var_decl.initializer) {
    LLVMValueRef init_val = codegen_expr(ctx, node->stmt.var_decl.initializer);
    if (init_val) {
      LLVMTypeRef init_type = LLVMTypeOf(init_val);

      if (var_type != init_type) {
        // Handle type conversions
        LLVMTypeKind var_kind = LLVMGetTypeKind(var_type);
        LLVMTypeKind init_kind = LLVMGetTypeKind(init_type);

        if (var_kind == LLVMDoubleTypeKind && init_kind == LLVMFloatTypeKind) {
          init_val = LLVMBuildFPExt(ctx->builder, init_val, var_type,
                                    "float_to_double");
        } else if (var_kind == LLVMFloatTypeKind &&
                   init_kind == LLVMDoubleTypeKind) {
          init_val = LLVMBuildFPTrunc(ctx->builder, init_val, var_type,
                                      "double_to_float");
        } else if (var_kind == LLVMIntegerTypeKind &&
                   (init_kind == LLVMFloatTypeKind ||
                    init_kind == LLVMDoubleTypeKind)) {
          init_val =
              LLVMBuildFPToSI(ctx->builder, init_val, var_type, "float_to_int");
        } else if ((var_kind == LLVMFloatTypeKind ||
                    var_kind == LLVMDoubleTypeKind) &&
                   init_kind == LLVMIntegerTypeKind) {
          init_val =
              LLVMBuildSIToFP(ctx->builder, init_val, var_type, "int_to_float");
        }
      }

      if (ctx->current_function == NULL) {
        if (LLVMIsConstant(init_val)) {
          LLVMSetInitializer(var_ref, init_val);
        } else {
          fprintf(stderr,
                  "Error: Global variable initializer must be constant\n");
          LLVMSetInitializer(var_ref, LLVMConstNull(var_type));
        }
      } else {
        LLVMBuildStore(ctx->builder, init_val, var_ref);
      }
    } else {
      if (ctx->current_function == NULL) {
        LLVMSetInitializer(var_ref, LLVMConstNull(var_type));
      }
    }
  } else {
    if (ctx->current_function == NULL) {
      LLVMSetInitializer(var_ref, LLVMConstNull(var_type));
    }
  }

  // Add symbol with element type information
  add_symbol_with_element_type(ctx, node->stmt.var_decl.name, var_ref, var_type,
                               element_type, false);
  return var_ref;
}

LLVMValueRef codegen_stmt_function(CodeGenContext *ctx, AstNode *node) {
  const char *func_name = node->stmt.func_decl.name;
  bool forward_declared = node->stmt.func_decl.forward_declared;
  
  // Generate parameter types
  LLVMTypeRef *param_types = (LLVMTypeRef *)arena_alloc(
      ctx->arena, sizeof(LLVMTypeRef) * node->stmt.func_decl.param_count,
      alignof(LLVMTypeRef));

  for (size_t i = 0; i < node->stmt.func_decl.param_count; i++) {
    param_types[i] = codegen_type(ctx, node->stmt.func_decl.param_types[i]);
    if (!param_types[i]) {
      fprintf(stderr, "Error: Failed to generate parameter type %zu for function '%s'\n", 
              i, func_name);
      return NULL;
    }
  }

  // Generate return type
  LLVMTypeRef return_type = codegen_type(ctx, node->stmt.func_decl.return_type);
  if (!return_type) {
    fprintf(stderr, "Error: Failed to generate return type for function '%s'\n", func_name);
    return NULL;
  }

  // Create function type
  LLVMTypeRef func_type = LLVMFunctionType(
      return_type, param_types, node->stmt.func_decl.param_count, false);

  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;

  // Check if function already exists
  LLVMValueRef existing_function = LLVMGetNamedFunction(current_llvm_module, func_name);

  if (existing_function) {
    // Function already declared - validate signature matches
    LLVMTypeRef existing_type = LLVMGlobalGetValueType(existing_function);
    
    // Compare return types
    if (LLVMGetReturnType(existing_type) != return_type) {
      fprintf(stderr, "Error: Function '%s' redeclared with different return type\n", 
              func_name);
      return NULL;
    }
    
    // Compare parameter counts
    if (LLVMCountParamTypes(existing_type) != node->stmt.func_decl.param_count) {
      fprintf(stderr, "Error: Function '%s' redeclared with different parameter count\n", 
              func_name);
      return NULL;
    }
    
    // Compare parameter types
    LLVMTypeRef *existing_param_types = (LLVMTypeRef *)arena_alloc(
        ctx->arena, sizeof(LLVMTypeRef) * node->stmt.func_decl.param_count,
        alignof(LLVMTypeRef));
    LLVMGetParamTypes(existing_type, existing_param_types);
    
    for (size_t i = 0; i < node->stmt.func_decl.param_count; i++) {
      if (existing_param_types[i] != param_types[i]) {
        fprintf(stderr, "Error: Function '%s' redeclared with different parameter %zu type\n",
                func_name, i);
        return NULL;
      }
    }
    
    // Signatures match
    if (forward_declared) {
      // This is another prototype - already exists, just return it
      return existing_function;
    }
    
    // This is the implementation - check if it already has a body
    if (LLVMCountBasicBlocks(existing_function) > 0) {
      fprintf(stderr, "Error: Function '%s' already has an implementation\n", func_name);
      return NULL;
    }
    
    // Use the existing declaration for the implementation
    LLVMValueRef function = existing_function;
    
    // Update symbol table if needed (it should already be there)
    // Just proceed to generate the body
    goto generate_body;
    
  } else {
    // First declaration - create new function
    LLVMValueRef function = LLVMAddFunction(current_llvm_module, func_name, func_type);
    
    if (!function) {
      fprintf(stderr, "Error: Failed to create LLVM function '%s'\n", func_name);
      return NULL;
    }

    // Set linkage
    LLVMSetLinkage(function, get_function_linkage(node));
    
    // Add to symbol table
    add_symbol(ctx, func_name, function, func_type, true);

    // Set parameter names
    for (size_t i = 0; i < node->stmt.func_decl.param_count; i++) {
      LLVMValueRef param = LLVMGetParam(function, i);
      LLVMSetValueName2(param, node->stmt.func_decl.param_names[i],
                        strlen(node->stmt.func_decl.param_names[i]));
    }

    // If this is just a forward declaration (prototype), we're done
    if (forward_declared) {
      return function;
    }
    
    // Otherwise, proceed to generate the body
    goto generate_body;
  }

generate_body:
  {
    // At this point, 'function' or 'existing_function' is set
    LLVMValueRef function = existing_function ? existing_function : 
                            LLVMGetNamedFunction(current_llvm_module, func_name);
    
    if (!function) {
      fprintf(stderr, "Error: Function reference lost for '%s'\n", func_name);
      return NULL;
    }

    // Function must have a body at this point
    if (!node->stmt.func_decl.body) {
      fprintf(stderr, "Error: Function '%s' implementation missing body\n", func_name);
      return NULL;
    }

    // Create entry block
    LLVMBasicBlockRef entry_block =
        LLVMAppendBasicBlockInContext(ctx->context, function, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry_block);

    // Save old function context
    LLVMValueRef old_function = ctx->current_function;
    DeferredStatement *old_deferred = ctx->deferred_statements;
    size_t old_defer_count = ctx->deferred_count;

    // Set new function context
    ctx->current_function = function;
    init_defer_stack(ctx);

    // Add parameters to symbol table as allocas
    for (size_t i = 0; i < node->stmt.func_decl.param_count; i++) {
      LLVMValueRef param = LLVMGetParam(function, i);
      LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, param_types[i],
                                            node->stmt.func_decl.param_names[i]);
      LLVMBuildStore(ctx->builder, param, alloca);

      // Extract element type for the pointer
      LLVMTypeRef element_type =
          extract_element_type_from_ast(ctx, node->stmt.func_decl.param_types[i]);
      add_symbol_with_element_type(ctx, node->stmt.func_decl.param_names[i],
                                   alloca, param_types[i], element_type, false);
    }

    // Create blocks for normal return and cleanup
    LLVMBasicBlockRef normal_return =
        LLVMAppendBasicBlockInContext(ctx->context, function, "normal_return");
    LLVMBasicBlockRef cleanup_entry =
        LLVMAppendBasicBlockInContext(ctx->context, function, "cleanup_entry");

    // Generate function body
    codegen_stmt(ctx, node->stmt.func_decl.body);

    // If we reach the end without an explicit return, branch to cleanup
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildBr(ctx->builder, cleanup_entry);
    }

    // Generate cleanup blocks for deferred statements
    generate_cleanup_blocks(ctx);

    // Set up cleanup entry point
    LLVMPositionBuilderAtEnd(ctx->builder, cleanup_entry);

    if (ctx->deferred_statements) {
      // Branch to the first cleanup block
      LLVMBuildBr(ctx->builder, ctx->deferred_statements->cleanup_block);
    } else {
      // No deferred statements, go straight to normal return
      LLVMBuildBr(ctx->builder, normal_return);
    }

    // Set up normal return block
    LLVMPositionBuilderAtEnd(ctx->builder, normal_return);

    if (LLVMGetTypeKind(return_type) == LLVMVoidTypeKind) {
      LLVMBuildRetVoid(ctx->builder);
    } else {
      // Return default value for the type
      LLVMValueRef default_val = LLVMConstNull(return_type);
      LLVMBuildRet(ctx->builder, default_val);
    }

    // Restore old function context
    ctx->current_function = old_function;
    ctx->deferred_statements = old_deferred;
    ctx->deferred_count = old_defer_count;

    return function;
  }
}

bool is_enum_constant(LLVM_Symbol *sym) {
  if (!sym || sym->is_function) {
    return false;
  }

  // Check if the value is a global constant
  if (LLVMIsAGlobalVariable(sym->value)) {
    return LLVMIsGlobalConstant(sym->value);
  }

  return false;
}

// Helper function to get enum constant value
int64_t get_enum_constant_value(LLVM_Symbol *sym) {
  if (!is_enum_constant(sym)) {
    return -1;
  }

  LLVMValueRef initializer = LLVMGetInitializer(sym->value);
  if (initializer && LLVMIsConstant(initializer)) {
    return LLVMConstIntGetSExtValue(initializer);
  }

  return -1;
}

LLVMValueRef codegen_stmt_enum(CodeGenContext *ctx, AstNode *node) {
  LLVMTypeRef enum_type = LLVMInt64TypeInContext(ctx->context);
  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;

  // First, register the enum type itself as a namespace symbol
  // This helps with type resolution
  add_symbol(ctx, node->stmt.enum_decl.name, NULL, enum_type, false);

  // Then create the enum constants
  for (size_t i = 0; i < node->stmt.enum_decl.member_count; i++) {
    const char *member_name = node->stmt.enum_decl.members[i];
    char full_name[256];
    snprintf(full_name, sizeof(full_name), "%s.%s", node->stmt.enum_decl.name,
             member_name);

    char llvm_name[256];
    snprintf(llvm_name, sizeof(llvm_name), "%s_%s", node->stmt.enum_decl.name,
             member_name);

    LLVMValueRef enum_constant =
        LLVMAddGlobal(current_llvm_module, enum_type, llvm_name);

    // Set the constant value (i as the enum ordinal)
    LLVMValueRef const_value = LLVMConstInt(enum_type, i, false);
    LLVMSetInitializer(enum_constant, const_value);

    // Make it a constant (immutable)
    LLVMSetGlobalConstant(enum_constant, true);

    // Set linkage based on visibility
    if (node->stmt.enum_decl.is_public) {
      LLVMSetLinkage(enum_constant, LLVMExternalLinkage);
    } else {
      LLVMSetLinkage(enum_constant, LLVMInternalLinkage);
    }

    // Add to symbol table with qualified name for member access
    add_symbol(ctx, full_name, enum_constant, enum_type, false);
  }
  return NULL;
}

LLVMValueRef codegen_stmt_return(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef ret_val = NULL;

  if (node->stmt.return_stmt.value) {
    ret_val = codegen_expr(ctx, node->stmt.return_stmt.value);
    if (!ret_val)
      return NULL;

    // CRITICAL: Ensure return value matches function return type
    if (ctx->current_function) {
      LLVMTypeRef func_type = LLVMGlobalGetValueType(ctx->current_function);
      LLVMTypeRef expected_return_type = LLVMGetReturnType(func_type);
      LLVMTypeRef actual_return_type = LLVMTypeOf(ret_val);

      if (expected_return_type != actual_return_type) {
        // Handle type conversions
        LLVMTypeKind expected_kind = LLVMGetTypeKind(expected_return_type);
        LLVMTypeKind actual_kind = LLVMGetTypeKind(actual_return_type);

        if (expected_kind == LLVMDoubleTypeKind &&
            actual_kind == LLVMFloatTypeKind) {
          ret_val = LLVMBuildFPExt(ctx->builder, ret_val, expected_return_type,
                                   "ret_float_to_double");
        } else if (expected_kind == LLVMFloatTypeKind &&
                   actual_kind == LLVMDoubleTypeKind) {
          ret_val =
              LLVMBuildFPTrunc(ctx->builder, ret_val, expected_return_type,
                               "ret_double_to_float");
        } else if (expected_kind == LLVMIntegerTypeKind &&
                   (actual_kind == LLVMFloatTypeKind ||
                    actual_kind == LLVMDoubleTypeKind)) {
          ret_val = LLVMBuildFPToSI(ctx->builder, ret_val, expected_return_type,
                                    "ret_float_to_int");
        } else if ((expected_kind == LLVMFloatTypeKind ||
                    expected_kind == LLVMDoubleTypeKind) &&
                   actual_kind == LLVMIntegerTypeKind) {
          ret_val = LLVMBuildSIToFP(ctx->builder, ret_val, expected_return_type,
                                    "ret_int_to_float");
        }
        // Add more conversion cases as needed
      }
    }
  }

  // Handle deferred statements as before...
  if (ctx->deferred_statements) {
    LLVMValueRef return_val_storage = NULL;

    if (ret_val) {
      LLVMTypeRef ret_type = LLVMTypeOf(ret_val);
      return_val_storage =
          LLVMBuildAlloca(ctx->builder, ret_type, "return_val_storage");
      LLVMBuildStore(ctx->builder, ret_val, return_val_storage);
    }

    execute_deferred_statements_inline(ctx, ctx->deferred_statements);

    if (return_val_storage) {
      LLVMValueRef final_ret_val =
          LLVMBuildLoad2(ctx->builder, LLVMTypeOf(ret_val), return_val_storage,
                         "final_return_val");
      return LLVMBuildRet(ctx->builder, final_ret_val);
    } else {
      return LLVMBuildRetVoid(ctx->builder);
    }
  } else {
    if (ret_val) {
      return LLVMBuildRet(ctx->builder, ret_val);
    } else {
      return LLVMBuildRetVoid(ctx->builder);
    }
  }
}

LLVMValueRef codegen_stmt_block(CodeGenContext *ctx, AstNode *node) {
    // Save the current defer context
    DeferredStatement *saved_defers = ctx->deferred_statements;
    size_t saved_count = ctx->deferred_count;

    // Create new defer scope for this block
    ctx->deferred_statements = NULL;
    ctx->deferred_count = 0;

    // Process all statements in the block
    for (size_t i = 0; i < node->stmt.block.stmt_count; i++) {
        AstNode *stmt = node->stmt.block.statements[i];
        
        // ADD THESE CHECKS:
        if (!stmt) {
            fprintf(stderr, "ERROR: Statement %zu is NULL!\n", i);
            continue;
        }
        
        // Stop processing if we hit a terminator
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
            break;
        }
        
        codegen_stmt(ctx, stmt);   
    }

    // Execute any deferred statements from this block scope (in reverse order)
    if (ctx->deferred_statements) {
        execute_deferred_statements_inline(ctx, ctx->deferred_statements);
    }

    // Restore the previous defer context
    ctx->deferred_statements = saved_defers;
    ctx->deferred_count = saved_count;

    return NULL;
}

LLVMValueRef codegen_stmt_if(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef condition = codegen_expr(ctx, node->stmt.if_stmt.condition);
  if (!condition)
    return NULL;

  // Create all blocks we'll need
  LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "then");
  LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "merge");

  // Create blocks for elif conditions and bodies
  LLVMBasicBlockRef *elif_cond_blocks = NULL;
  LLVMBasicBlockRef *elif_body_blocks = NULL;
  if (node->stmt.if_stmt.elif_count > 0) {
    elif_cond_blocks =
        malloc(node->stmt.if_stmt.elif_count * sizeof(LLVMBasicBlockRef));
    elif_body_blocks =
        malloc(node->stmt.if_stmt.elif_count * sizeof(LLVMBasicBlockRef));

    for (int i = 0; i < node->stmt.if_stmt.elif_count; i++) {
      char block_name[64];
      snprintf(block_name, sizeof(block_name), "elif_cond_%d", i);
      elif_cond_blocks[i] = LLVMAppendBasicBlockInContext(
          ctx->context, ctx->current_function, block_name);

      snprintf(block_name, sizeof(block_name), "elif_body_%d", i);
      elif_body_blocks[i] = LLVMAppendBasicBlockInContext(
          ctx->context, ctx->current_function, block_name);
    }
  }

  // Create else block if needed
  LLVMBasicBlockRef else_block = NULL;
  if (node->stmt.if_stmt.else_stmt) {
    else_block = LLVMAppendBasicBlockInContext(ctx->context,
                                               ctx->current_function, "else");
  }

  // Determine where to jump if initial condition is false
  LLVMBasicBlockRef false_target;
  if (node->stmt.if_stmt.elif_count > 0) {
    false_target = elif_cond_blocks[0];
  } else if (else_block) {
    false_target = else_block;
  } else {
    false_target = merge_block;
  }

  // Build initial if condition branch
  LLVMBuildCondBr(ctx->builder, condition, then_block, false_target);

  // Generate then block
  LLVMPositionBuilderAtEnd(ctx->builder, then_block);
  codegen_stmt(ctx, node->stmt.if_stmt.then_stmt);
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
    LLVMBuildBr(ctx->builder, merge_block);
  }

  // Generate elif chains
  for (int i = 0; i < node->stmt.if_stmt.elif_count; i++) {
    // Generate condition check
    LLVMPositionBuilderAtEnd(ctx->builder, elif_cond_blocks[i]);

    // Generate elif condition - adjust this based on your AST structure
    LLVMValueRef elif_condition = codegen_expr(
        ctx, node->stmt.if_stmt.elif_stmts[i]->stmt.if_stmt.condition);
    if (!elif_condition) {
      if (elif_cond_blocks)
        free(elif_cond_blocks);
      if (elif_body_blocks)
        free(elif_body_blocks);
      return NULL;
    }

    // Determine where to jump if this elif condition is false
    LLVMBasicBlockRef elif_false_target;
    if (i + 1 < node->stmt.if_stmt.elif_count) {
      elif_false_target = elif_cond_blocks[i + 1];
    } else if (else_block) {
      elif_false_target = else_block;
    } else {
      elif_false_target = merge_block;
    }

    // Build conditional branch for this elif
    LLVMBuildCondBr(ctx->builder, elif_condition, elif_body_blocks[i],
                    elif_false_target);

    // Generate elif body
    LLVMPositionBuilderAtEnd(ctx->builder, elif_body_blocks[i]);
    codegen_stmt(ctx, node->stmt.if_stmt.elif_stmts[i]->stmt.if_stmt.then_stmt);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildBr(ctx->builder, merge_block);
    }
  }

  // Generate else block if it exists
  if (else_block) {
    LLVMPositionBuilderAtEnd(ctx->builder, else_block);
    codegen_stmt(ctx, node->stmt.if_stmt.else_stmt);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildBr(ctx->builder, merge_block);
    }
  }

  // Clean up
  if (elif_cond_blocks)
    free(elif_cond_blocks);
  if (elif_body_blocks)
    free(elif_body_blocks);

  // Continue with merge block
  LLVMPositionBuilderAtEnd(ctx->builder, merge_block);
  return NULL;
}

bool is_range_type(LLVMTypeRef type) {
  // Check if this is a struct with exactly 2 fields of the same integer type
  if (LLVMGetTypeKind(type) != LLVMStructTypeKind) {
    return false;
  }

  unsigned field_count = LLVMCountStructElementTypes(type);
  if (field_count != 2) {
    return false;
  }

  // Get field types
  LLVMTypeRef field_types[2];
  LLVMGetStructElementTypes(type, field_types);

  // Check if both fields are the same integer type
  return (field_types[0] == field_types[1] &&
          LLVMGetTypeKind(field_types[0]) == LLVMIntegerTypeKind);
}

LLVMValueRef get_range_start_value(CodeGenContext *ctx,
                                   LLVMValueRef range_struct) {
  LLVMTypeRef struct_type = LLVMTypeOf(range_struct);

  // Create a temporary alloca to store the struct
  LLVMValueRef temp_alloca =
      LLVMBuildAlloca(ctx->builder, struct_type, "temp_range");
  LLVMBuildStore(ctx->builder, range_struct, temp_alloca);

  // Get pointer to first field (start)
  LLVMValueRef start_ptr = LLVMBuildStructGEP2(ctx->builder, struct_type,
                                               temp_alloca, 0, "start_ptr");

  // Get field type for load
  LLVMTypeRef field_types[2];
  LLVMGetStructElementTypes(struct_type, field_types);

  return LLVMBuildLoad2(ctx->builder, field_types[0], start_ptr, "range_start");
}

LLVMValueRef get_range_end_value(CodeGenContext *ctx,
                                 LLVMValueRef range_struct) {
  LLVMTypeRef struct_type = LLVMTypeOf(range_struct);

  // Create a temporary alloca to store the struct
  LLVMValueRef temp_alloca =
      LLVMBuildAlloca(ctx->builder, struct_type, "temp_range");
  LLVMBuildStore(ctx->builder, range_struct, temp_alloca);

  // Get pointer to second field (end)
  LLVMValueRef end_ptr =
      LLVMBuildStructGEP2(ctx->builder, struct_type, temp_alloca, 1, "end_ptr");

  // Get field type for load
  LLVMTypeRef field_types[2];
  LLVMGetStructElementTypes(struct_type, field_types);

  return LLVMBuildLoad2(ctx->builder, field_types[1], end_ptr, "range_end");
}

// Enhanced codegen_stmt_print function with better float support
LLVMValueRef codegen_stmt_print(CodeGenContext *ctx, AstNode *node) {
  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;

  // Declare printf once
  LLVMValueRef printf_func =
      LLVMGetNamedFunction(current_llvm_module, "printf");
  LLVMTypeRef printf_type = NULL;

  if (!printf_func) {
    LLVMTypeRef printf_arg_types[] = {
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0)};
    printf_type = LLVMFunctionType(LLVMInt32TypeInContext(ctx->context),
                                   printf_arg_types, 1, true);
    printf_func = LLVMAddFunction(current_llvm_module, "printf", printf_type);
    add_symbol(ctx, "printf", printf_func, printf_type, true);
  } else {
    printf_type = LLVMGlobalGetValueType(printf_func);
  }

  for (size_t i = 0; i < node->stmt.print_stmt.expr_count; i++) {
    AstNode *expr = node->stmt.print_stmt.expressions[i];

    // Check if this is a nested indexing expression that might fail
    if (expr->type == AST_EXPR_INDEX) {
      // Validate the indexing chain before attempting to generate
      AstNode *current = expr;
      int depth = 0;

      while (current->type == AST_EXPR_INDEX) {
        depth++;
        current = current->expr.index.object;
      }

      if (depth > 1) {
        // This is nested indexing - add debug info
        printf("Debug: Processing nested indexing with depth %d\n", depth);
      }
    }

    LLVMValueRef value = codegen_expr(ctx, expr);
    if (!value) {
      fprintf(stderr, "Error: Failed to generate expression for printing\n");
      continue; // Skip this expression but continue with others
    }

    const char *format_str = NULL;
    LLVMTypeRef value_type = LLVMTypeOf(value);
    LLVMTypeKind value_kind = LLVMGetTypeKind(value_type);

    // Handle string literals specially
    if (expr->type == AST_EXPR_LITERAL &&
        expr->expr.literal.lit_type == LITERAL_STRING) {
      char *processed_str =
          process_escape_sequences(expr->expr.literal.value.string_val);
      value = LLVMBuildGlobalStringPtr(ctx->builder, processed_str, "str");
      format_str = "%s";
      free(processed_str);
    } else {
      // Handle different value types
      if (is_range_type(value_type)) {
        // Handle range printing
        LLVMValueRef start_val = get_range_start_value(ctx, value);
        LLVMValueRef end_val = get_range_end_value(ctx, value);

        LLVMTypeRef field_types[2];
        LLVMGetStructElementTypes(value_type, field_types);
        LLVMTypeKind element_kind = LLVMGetTypeKind(field_types[0]);

        const char *element_format =
            (element_kind == LLVMFloatTypeKind)    ? "%.6f"
            : (element_kind == LLVMDoubleTypeKind) ? "%.6lf"
            : (element_kind == LLVMIntegerTypeKind &&
               LLVMGetIntTypeWidth(field_types[0]) == 64)
                ? "%lld"
                : "%d";

        char range_format[64];
        snprintf(range_format, sizeof(range_format), "%s..%s", element_format,
                 element_format);

        LLVMValueRef range_format_str =
            LLVMBuildGlobalStringPtr(ctx->builder, range_format, "range_fmt");

        LLVMValueRef range_args[] = {range_format_str, start_val, end_val};
        LLVMBuildCall2(ctx->builder, printf_type, printf_func, range_args, 3,
                       "");
        continue;
      }

      // Regular value type handling
      if (value_kind == LLVMIntegerTypeKind) {
        unsigned int bits = LLVMGetIntTypeWidth(value_type);
        if (bits == 1) {
          format_str = "%s";
          LLVMValueRef true_str =
              LLVMBuildGlobalStringPtr(ctx->builder, "true", "true_str");
          LLVMValueRef false_str =
              LLVMBuildGlobalStringPtr(ctx->builder, "false", "false_str");
          value = LLVMBuildSelect(ctx->builder, value, true_str, false_str,
                                  "bool_str");
        } else if (bits <= 32) {
          format_str = "%d";
        } else {
          format_str = "%lld";
        }
      } else if (value_kind == LLVMFloatTypeKind) {
        format_str = "%.6f";
        value = LLVMBuildFPExt(ctx->builder, value,
                               LLVMDoubleTypeInContext(ctx->context),
                               "float_to_double");
      } else if (value_kind == LLVMDoubleTypeKind) {
        format_str = "%.6lf";
      } else if (value_kind == LLVMPointerTypeKind) {
        format_str = "%s"; // Assume string pointer
      } else {
        format_str = "%p"; // Fallback for unknown types
      }
    }

    // Print the value
    if (format_str) {
      LLVMValueRef format_str_val =
          LLVMBuildGlobalStringPtr(ctx->builder, format_str, "fmt");
      LLVMValueRef args[] = {format_str_val, value};
      LLVMBuildCall2(ctx->builder, printf_type, printf_func, args, 2, "");
    }
  }
  return LLVMConstNull(LLVMVoidTypeInContext(ctx->context));
}

LLVMValueRef codegen_stmt_defer(CodeGenContext *ctx, AstNode *node) {
  // Push the deferred statement onto the context's deferred stack
  push_defer_statement(ctx, node->stmt.defer_stmt.statement);
  return NULL;
}

LLVMValueRef codegen_stmt_break_continue(CodeGenContext *ctx, AstNode *node) {
  if (node->stmt.break_continue.is_continue) {
    if (ctx->loop_continue_block) {
      LLVMBuildBr(ctx->builder, ctx->loop_continue_block);
    } else {
      fprintf(stderr, "Error: 'continue' used outside of a loop\n");
    }
  } else {
    if (ctx->loop_break_block) {
      LLVMBuildBr(ctx->builder, ctx->loop_break_block);
    } else {
      fprintf(stderr, "Error: 'break' used outside of a loop\n");
    }
  }
  return NULL;
}

// loop { ... }
LLVMValueRef codegen_infinite_loop(CodeGenContext *ctx, AstNode *node) {
  LLVMBasicBlockRef loop_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "infinite_loop");
  LLVMBasicBlockRef after_loop_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "after_infinite_loop");

  // Branch to loop block
  LLVMBuildBr(ctx->builder, loop_block);

  // Generate loop block
  LLVMPositionBuilderAtEnd(ctx->builder, loop_block);

  // Save old loop context
  LLVMBasicBlockRef old_continue = ctx->loop_continue_block;
  LLVMBasicBlockRef old_break = ctx->loop_break_block;

  // Set new loop context
  ctx->loop_continue_block = loop_block;
  ctx->loop_break_block = after_loop_block;

  // Generate loop body
  codegen_stmt(ctx, node->stmt.loop_stmt.body);

  // If we reach the end of the loop body without a terminator, branch back to
  // the start
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
    LLVMBuildBr(ctx->builder, loop_block);
  }

  // Restore old loop context
  ctx->loop_continue_block = old_continue;
  ctx->loop_break_block = old_break;

  // Continue with after loop block
  LLVMPositionBuilderAtEnd(ctx->builder, after_loop_block);
  return NULL;
}

// loop (i < 10) { ... }
// loop (i < 10) : (i++) { ... }
LLVMValueRef codegen_while_loop(CodeGenContext *ctx, AstNode *node) {
  LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "while_cond");
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "while_body");
  LLVMBasicBlockRef after_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "while_end");

  // ADD THIS: Save and set loop context
  LLVMBasicBlockRef old_continue = ctx->loop_continue_block;
  LLVMBasicBlockRef old_break = ctx->loop_break_block;
  ctx->loop_continue_block = cond_block; // Continue jumps back to condition
  ctx->loop_break_block = after_block;   // Break jumps to after loop

  LLVMBuildBr(ctx->builder, cond_block);

  LLVMPositionBuilderAtEnd(ctx->builder, cond_block);
  if (node->stmt.loop_stmt.condition) {
    LLVMValueRef cond = codegen_expr(ctx, node->stmt.loop_stmt.condition);
    if (!cond) {
      // ADD THIS: Restore on error
      ctx->loop_continue_block = old_continue;
      ctx->loop_break_block = old_break;
      return NULL;
    }
    LLVMBuildCondBr(ctx->builder, cond, body_block, after_block);
  } else {
    LLVMBuildBr(ctx->builder, body_block);
  }

  LLVMPositionBuilderAtEnd(ctx->builder, body_block);
  codegen_stmt(ctx, node->stmt.loop_stmt.body);

  if (node->stmt.loop_stmt.optional) {
    codegen_expr(ctx, node->stmt.loop_stmt.optional);
  }

  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
    LLVMBuildBr(ctx->builder, cond_block);
  }

  // ADD THIS: Restore old loop context
  ctx->loop_continue_block = old_continue;
  ctx->loop_break_block = old_break;

  LLVMPositionBuilderAtEnd(ctx->builder, after_block);
  return NULL;
}

// loop [i: int = 0](i < 10) { ... }
// loop [i: int = 0](i < 10) : (i++) { ... }
// loop [i: int = 0, j: int = 0](i < 10 && j < 20) { ... }
LLVMValueRef codegen_for_loop(CodeGenContext *ctx, AstNode *node) {
  // Create basic blocks for the loop structure
  LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "for_cond");
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "for_body");
  LLVMBasicBlockRef increment_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "for_inc");
  LLVMBasicBlockRef after_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "for_end");

  // Store old loop blocks for nested loop support
  LLVMBasicBlockRef old_continue = ctx->loop_continue_block;
  LLVMBasicBlockRef old_break = ctx->loop_break_block;
  ctx->loop_continue_block = increment_block;
  ctx->loop_break_block = after_block;

  // Generate initializers in current block
  for (size_t i = 0; i < node->stmt.loop_stmt.init_count; i++) {
    if (!codegen_stmt(ctx, node->stmt.loop_stmt.initializer[i])) {
      fprintf(
          stderr,
          "Error: Failed to generate initializer for for loop at line %zu\n",
          node->line);
      // Restore old blocks
      ctx->loop_continue_block = old_continue;
      ctx->loop_break_block = old_break;
      return NULL;
    }
  }

  // Jump to condition check
  LLVMBuildBr(ctx->builder, cond_block);

  // Generate condition block
  LLVMPositionBuilderAtEnd(ctx->builder, cond_block);
  if (node->stmt.loop_stmt.condition) {
    LLVMValueRef cond = codegen_expr(ctx, node->stmt.loop_stmt.condition);
    if (!cond) {
      // Restore old blocks
      ctx->loop_continue_block = old_continue;
      ctx->loop_break_block = old_break;
      return NULL;
    }
    LLVMBuildCondBr(ctx->builder, cond, body_block, after_block);
  } else {
    // Infinite loop if no condition
    LLVMBuildBr(ctx->builder, body_block);
  }

  // Generate loop body
  LLVMPositionBuilderAtEnd(ctx->builder, body_block);
  codegen_stmt(ctx, node->stmt.loop_stmt.body);

  // If body doesn't have a terminator, jump to increment
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
    LLVMBuildBr(ctx->builder, increment_block);
  }

  // Generate increment block
  LLVMPositionBuilderAtEnd(ctx->builder, increment_block);

  // Generate increment expressions if they exist
  if (node->stmt.loop_stmt.optional) {
    codegen_expr(ctx, node->stmt.loop_stmt.optional);
  }

  // Jump back to condition check
  LLVMBuildBr(ctx->builder, cond_block);

  // Restore old loop blocks
  ctx->loop_continue_block = old_continue;
  ctx->loop_break_block = old_break;

  // Continue with after loop block
  LLVMPositionBuilderAtEnd(ctx->builder, after_block);
  return NULL;
}

LLVMValueRef codegen_loop(CodeGenContext *ctx, AstNode *node) {
  if (node->stmt.loop_stmt.condition == NULL &&
      node->stmt.loop_stmt.initializer == NULL)
    return codegen_infinite_loop(ctx, node);
  else if (node->stmt.loop_stmt.condition != NULL &&
           node->stmt.loop_stmt.initializer == NULL)
    return codegen_while_loop(ctx, node);
  else
    return codegen_for_loop(ctx, node);
}

LLVMValueRef codegen_stmt_switch(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_STMT_SWITCH) {
    return NULL;
  }

  // Generate the switch condition
  LLVMValueRef switch_value =
      codegen_expr(ctx, node->stmt.switch_stmt.condition);
  if (!switch_value) {
    fprintf(stderr, "Error: Failed to generate switch condition\n");
    return NULL;
  }

  // Create basic blocks
  LLVMBasicBlockRef default_block = NULL;
  LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "switch_end");

  // Handle default case
  if (node->stmt.switch_stmt.default_case) {
    default_block = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "switch_default");
  } else {
    // If no default, merge_block serves as the default
    default_block = merge_block;
  }

  // Create the switch instruction
  LLVMValueRef switch_inst =
      LLVMBuildSwitch(ctx->builder, switch_value, default_block,
                      node->stmt.switch_stmt.case_count);

  // Arrays to store case blocks for later generation
  LLVMBasicBlockRef *case_blocks =
      malloc(node->stmt.switch_stmt.case_count * sizeof(LLVMBasicBlockRef));

  // Create basic blocks for each case
  for (size_t i = 0; i < node->stmt.switch_stmt.case_count; i++) {
    char block_name[64];
    snprintf(block_name, sizeof(block_name), "case_%zu", i);
    case_blocks[i] = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, block_name);
  }

  // Add case values to switch instruction
  for (size_t i = 0; i < node->stmt.switch_stmt.case_count; i++) {
    AstNode *case_stmt = node->stmt.switch_stmt.cases[i];
    if (!case_stmt || case_stmt->type != AST_STMT_CASE) {
      continue;
    }

    // Add each case value to the switch
    for (size_t j = 0; j < case_stmt->stmt.case_clause.value_count; j++) {
      AstNode *case_value = case_stmt->stmt.case_clause.values[j];
      LLVMValueRef case_const = codegen_case_value(ctx, case_value);

      if (case_const && LLVMIsConstant(case_const)) {
        LLVMAddCase(switch_inst, case_const, case_blocks[i]);
      } else {
        fprintf(stderr, "Error: Case value must be a compile-time constant\n");
        free(case_blocks);
        return NULL;
      }
    }
  }

  // Generate code for each case body
  for (size_t i = 0; i < node->stmt.switch_stmt.case_count; i++) {
    AstNode *case_stmt = node->stmt.switch_stmt.cases[i];

    LLVMPositionBuilderAtEnd(ctx->builder, case_blocks[i]);

    if (case_stmt->stmt.case_clause.body) {
      codegen_stmt(ctx, case_stmt->stmt.case_clause.body);
    }

    // If no terminator was added by the case body, fall through to merge
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildBr(ctx->builder, merge_block);
    }
  }

  // Generate default case if it exists
  if (node->stmt.switch_stmt.default_case &&
      node->stmt.switch_stmt.default_case->type == AST_STMT_DEFAULT) {

    LLVMPositionBuilderAtEnd(ctx->builder, default_block);

    if (node->stmt.switch_stmt.default_case->stmt.default_clause.body) {
      codegen_stmt(
          ctx, node->stmt.switch_stmt.default_case->stmt.default_clause.body);
    }

    // If no terminator, branch to merge
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildBr(ctx->builder, merge_block);
    }
  }

  free(case_blocks);

  // Continue execution from merge block
  LLVMPositionBuilderAtEnd(ctx->builder, merge_block);
  return NULL;
}

// Helper function to generate case values (constants)
LLVMValueRef codegen_case_value(CodeGenContext *ctx, AstNode *case_value) {
  switch (case_value->type) {
  case AST_EXPR_LITERAL:
    // Handle literal constants
    switch (case_value->expr.literal.lit_type) {
    case LITERAL_INT:
      return LLVMConstInt(LLVMInt64TypeInContext(ctx->context),
                          case_value->expr.literal.value.int_val, false);
    case LITERAL_CHAR:
      return LLVMConstInt(LLVMInt8TypeInContext(ctx->context),
                          case_value->expr.literal.value.char_val, false);
    default:
      fprintf(stderr, "Error: Unsupported literal type in case value\n");
      return NULL;
    }

  case AST_EXPR_MEMBER:
    // Handle enum member access (EnumName.Member)
    return codegen_enum_member_case(ctx, case_value);

  default:
    fprintf(stderr, "Error: Case values must be compile-time constants\n");
    return NULL;
  }
}

// Helper function to generate enum member constants for switch cases
LLVMValueRef codegen_enum_member_case(CodeGenContext *ctx,
                                      AstNode *member_expr) {
  if (!member_expr || member_expr->type != AST_EXPR_MEMBER) {
    return NULL;
  }

  // Get the enum name and member name
  const char *enum_name = member_expr->expr.member.object->expr.identifier.name;
  const char *member_name = member_expr->expr.member.member;

  // Create qualified member name
  char qualified_name[256];
  snprintf(qualified_name, sizeof(qualified_name), "%s.%s", enum_name,
           member_name);

  // Look up the enum member symbol
  LLVM_Symbol *enum_member = find_symbol(ctx, qualified_name);
  if (!enum_member) {
    fprintf(stderr, "Error: Enum member '%s' not found\n", qualified_name);
    return NULL;
  }

  // Get the constant value from the enum member
  if (is_enum_constant(enum_member)) {
    return LLVMGetInitializer(enum_member->value);
  } else {
    fprintf(stderr, "Error: '%s' is not an enum constant\n", qualified_name);
    return NULL;
  }
}

// Individual case statement handler (for completeness, though not directly
// called in switch)
LLVMValueRef codegen_stmt_case(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_STMT_CASE) {
    return NULL;
  }

  // Case bodies are handled by the switch statement itself
  // This function mainly exists for completeness
  if (node->stmt.case_clause.body) {
    return codegen_stmt(ctx, node->stmt.case_clause.body);
  }

  return NULL;
}

// Default case statement handler
LLVMValueRef codegen_stmt_default(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_STMT_DEFAULT) {
    return NULL;
  }

  // Default bodies are handled by the switch statement itself
  // This function mainly exists for completeness
  if (node->stmt.default_clause.body) {
    return codegen_stmt(ctx, node->stmt.default_clause.body);
  }

  return NULL;
}
