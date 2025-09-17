#include "llvm.h"

#include <stdlib.h>

// =============================================================================
// MULTI-MODULE PROGRAM HANDLER
// =============================================================================

LLVMValueRef codegen_stmt_program_multi_module(CodeGenContext *ctx,
                                               AstNode *node) {
  if (!node || node->type != AST_PROGRAM) {
    return NULL;
  }

  // First pass: Create all module units and process module declarations
  for (size_t i = 0; i < node->stmt.program.module_count; i++) {
    AstNode *module_node = node->stmt.program.modules[i];

    if (module_node->type == AST_PREPROCESSOR_MODULE) {
      const char *module_name = module_node->preprocessor.module.name;

      // Create module compilation unit
      ModuleCompilationUnit *unit = create_module_unit(ctx, module_name);

      // Set as current module for processing
      set_current_module(ctx, unit);
      ctx->module = unit->module; // Update legacy field

      // printf("Created module: %s\n", module_name);
    }
  }

  // Second pass: Process module contents and handle @use directives
  for (size_t i = 0; i < node->stmt.program.module_count; i++) {
    AstNode *module_node = node->stmt.program.modules[i];

    if (module_node->type == AST_PREPROCESSOR_MODULE) {
      const char *module_name = module_node->preprocessor.module.name;
      ModuleCompilationUnit *unit = find_module(ctx, module_name);

      if (unit) {
        set_current_module(ctx, unit);
        ctx->module = unit->module; // Update legacy field

        // Process module body
        codegen_stmt_module(ctx, module_node);
      }
    }
  }

  return NULL;
}

// =============================================================================
// MODULE DECLARATION HANDLER
// =============================================================================

LLVMValueRef codegen_stmt_module(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_PREPROCESSOR_MODULE) {
    return NULL;
  }

  // printf("Processing module body: %s\n", node->preprocessor.module.name);

  // Process each statement in the module body
  for (size_t i = 0; i < node->preprocessor.module.body_count; i++) {
    AstNode *stmt = node->preprocessor.module.body[i];

    // Handle @use directives specially
    if (stmt->type == AST_PREPROCESSOR_USE) {
      codegen_stmt_use(ctx, stmt);
    } else {
      // Process regular statements
      codegen_stmt(ctx, stmt);
    }
  }

  return NULL;
}

// =============================================================================
// @USE DIRECTIVE HANDLER
// =============================================================================

LLVMValueRef codegen_stmt_use(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_PREPROCESSOR_USE) {
    return NULL;
  }

  const char *module_name = node->preprocessor.use.module_name;
  const char *alias = node->preprocessor.use.alias;

  // printf("Processing @use directive: module '%s' as '%s'\n", module_name,
  //        alias ? alias : module_name);

  // Find the referenced module
  ModuleCompilationUnit *referenced_module = find_module(ctx, module_name);
  if (!referenced_module) {
    fprintf(stderr, "Error: Module '%s' not found\n", module_name);
    return NULL;
  }

  // Import public symbols from the referenced module
  import_module_symbols(ctx, referenced_module, alias);

  return NULL;
}

// =============================================================================
// SYMBOL IMPORT FUNCTIONALITY
// =============================================================================

void import_module_symbols(CodeGenContext *ctx,
                           ModuleCompilationUnit *source_module,
                           const char *alias) {
  if (!ctx->current_module || !source_module) {
    return;
  }

  // Import all public symbols from source module
  for (LLVM_Symbol *sym = source_module->symbols; sym; sym = sym->next) {
    if (sym->is_function) {
      // For functions, check if they have external linkage (public)
      LLVMValueRef func_value =
          LLVMGetNamedFunction(source_module->module, sym->name);
      if (func_value && LLVMGetLinkage(func_value) == LLVMExternalLinkage) {
        import_function_symbol(ctx, sym, source_module, alias);
      }
    } else {
      // For variables, import public globals
      LLVMValueRef global_value =
          LLVMGetNamedGlobal(source_module->module, sym->name);
      if (global_value && LLVMGetLinkage(global_value) == LLVMExternalLinkage) {
        import_variable_symbol(ctx, sym, source_module, alias);
      }
    }
  }
}

void import_function_symbol(CodeGenContext *ctx, LLVM_Symbol *source_symbol,
                            ModuleCompilationUnit *source_module,
                            const char *alias) {
  (void)source_module;
  // Create the imported symbol name
  char imported_name[256];
  if (alias) {
    snprintf(imported_name, sizeof(imported_name), "%s.%s", alias,
             source_symbol->name);
  } else {
    snprintf(imported_name, sizeof(imported_name), "%s", source_symbol->name);
  }

  // Check if already imported
  if (LLVMGetNamedFunction(ctx->current_module->module, imported_name)) {
    return; // Already imported
  }

  // Create external declaration in current module
  LLVMTypeRef func_type = LLVMGlobalGetValueType(source_symbol->value);
  LLVMValueRef external_func = LLVMAddFunction(ctx->current_module->module,
                                               source_symbol->name, func_type);
  LLVMSetLinkage(external_func, LLVMExternalLinkage);

  // Add to current module's symbol table with imported name
  add_symbol_to_module(ctx->current_module, imported_name, external_func,
                       func_type, true);

  // printf("Imported function: %s -> %s\n", source_symbol->name,
  // imported_name);
}

void import_variable_symbol(CodeGenContext *ctx, LLVM_Symbol *source_symbol,
                            ModuleCompilationUnit *source_module,
                            const char *alias) {
  (void)source_module;
  // Create the imported symbol name
  char imported_name[256];
  if (alias) {
    snprintf(imported_name, sizeof(imported_name), "%s.%s", alias,
             source_symbol->name);
  } else {
    snprintf(imported_name, sizeof(imported_name), "%s", source_symbol->name);
  }

  // Check if already imported
  if (LLVMGetNamedGlobal(ctx->current_module->module, imported_name)) {
    return; // Already imported
  }

  // Create external declaration in current module
  LLVMValueRef external_global = LLVMAddGlobal(
      ctx->current_module->module, source_symbol->type, source_symbol->name);
  LLVMSetLinkage(external_global, LLVMExternalLinkage);

  // Add to current module's symbol table with imported name
  add_symbol_to_module(ctx->current_module, imported_name, external_global,
                       source_symbol->type, false);

  // printf("Imported variable: %s -> %s\n", source_symbol->name,
  // imported_name);
}

// =============================================================================
// MEMBER ACCESS HANDLER (for module.symbol syntax)
// =============================================================================

LLVMValueRef codegen_expr_member_access(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_EXPR_MEMBER) {
    return NULL;
  }

  // For module member access, we expect:
  // - object to be an identifier (module alias)
  // - member to be an identifier (symbol name)

  AstNode *object = node->expr.member.object;
  const char *member = node->expr.member.member;

  if (object->type != AST_EXPR_IDENTIFIER) {
    fprintf(stderr, "Error: Invalid member access syntax\n");
    return NULL;
  }

  const char *module_alias = object->expr.identifier.name;
  const char *symbol_name = member;

  // Create the full imported symbol name
  char full_name[256];
  snprintf(full_name, sizeof(full_name), "%s.%s", module_alias, symbol_name);

  // Look up the imported symbol
  LLVM_Symbol *sym = find_symbol_in_module(ctx->current_module, full_name);
  if (sym) {
    if (sym->is_function) {
      return sym->value;
    } else {
      // Load variable value
      return LLVMBuildLoad2(ctx->builder, sym->type, sym->value, "load");
    }
  }

  fprintf(stderr, "Error: Symbol '%s' not found in module '%s'\n", symbol_name,
          module_alias);
  return NULL;
}

// =============================================================================
// ENHANCED SYMBOL LOOKUP WITH MODULE SUPPORT
// =============================================================================

LLVM_Symbol *find_symbol_with_module_support(CodeGenContext *ctx,
                                             const char *name) {
  // First, try to find in current module
  if (ctx->current_module) {
    LLVM_Symbol *sym = find_symbol_in_module(ctx->current_module, name);
    if (sym) {
      return sym;
    }
  }

  // Then, try to find in other modules (for public symbols)
  for (ModuleCompilationUnit *unit = ctx->modules; unit; unit = unit->next) {
    if (unit == ctx->current_module) {
      continue; // Already checked
    }

    LLVM_Symbol *sym = find_symbol_in_module(unit, name);
    if (sym && sym->is_function) {
      // Check if function has external linkage (is public)
      LLVMValueRef func = LLVMGetNamedFunction(unit->module, name);
      if (func && LLVMGetLinkage(func) == LLVMExternalLinkage) {
        return sym;
      }
    }
  }

  return NULL;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

bool is_main_module(ModuleCompilationUnit *unit) {
  return unit && unit->is_main_module;
}

void set_module_as_main(ModuleCompilationUnit *unit) {
  if (unit) {
    unit->is_main_module = true;
  }
}

// Print module information for debugging
void print_module_info(CodeGenContext *ctx) {
  printf("\n=== MODULE INFORMATION ===\n");
  for (ModuleCompilationUnit *unit = ctx->modules; unit; unit = unit->next) {
    printf("Module: %s %s\n", unit->module_name,
           unit->is_main_module ? "(main)" : "");

    printf("  Symbols:\n");
    for (LLVM_Symbol *sym = unit->symbols; sym; sym = sym->next) {
      printf("    %s %s\n", sym->name,
             sym->is_function ? "(function)" : "(variable)");
    }
  }
  printf("========================\n\n");
}

// Also add this debug function to help diagnose linking issues:
void debug_object_files(const char *output_dir) {
  printf("\n=== OBJECT FILE DEBUG INFO ===\n");

  char command[512];
  snprintf(command, sizeof(command), "ls -la %s/*.o", output_dir);
  printf("Object files in %s:\n", output_dir);
  system(command);

  // Check if files are actually generated
  snprintf(command, sizeof(command), "file %s/*.o", output_dir);
  printf("\nFile types:\n");
  system(command);

  // Check symbols in object files
  snprintf(command, sizeof(command), "nm %s/*.o | head -20", output_dir);
  printf("\nSymbols (first 20):\n");
  system(command);

  printf("==============================\n\n");
}
