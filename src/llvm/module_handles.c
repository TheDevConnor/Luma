#include "llvm.h"

#include <stdlib.h>

ModuleDependencyInfo *build_codegen_dependency_info(AstNode **modules,
                                                    size_t module_count,
                                                    ArenaAllocator *arena) {

  ModuleDependencyInfo *dep_info = (ModuleDependencyInfo *)arena_alloc(
      arena, sizeof(ModuleDependencyInfo) * module_count,
      alignof(ModuleDependencyInfo));

  for (size_t i = 0; i < module_count; i++) {
    AstNode *module = modules[i];
    if (!module || module->type != AST_PREPROCESSOR_MODULE)
      continue;

    dep_info[i].module_name = module->preprocessor.module.name;
    dep_info[i].processed = false;
    dep_info[i].dep_count = 0;

    // Count dependencies
    AstNode **body = module->preprocessor.module.body;
    int body_count = module->preprocessor.module.body_count;

    size_t use_count = 0;
    for (int j = 0; j < body_count; j++) {
      if (body[j] && body[j]->type == AST_PREPROCESSOR_USE) {
        use_count++;
      }
    }

    dep_info[i].dependencies = (char **)arena_alloc(
        arena, sizeof(char *) * use_count, alignof(char *));

    // Collect dependency names
    size_t dep_idx = 0;
    for (int j = 0; j < body_count; j++) {
      if (body[j] && body[j]->type == AST_PREPROCESSOR_USE) {
        dep_info[i].dependencies[dep_idx++] =
            (char *)body[j]->preprocessor.use.module_name;
      }
    }
    dep_info[i].dep_count = use_count;
  }

  return dep_info;
}

// Process a single module and its dependencies recursively
static bool process_module_codegen_recursive(CodeGenContext *ctx,
                                             const char *module_name,
                                             AstNode **modules,
                                             size_t module_count,
                                             ModuleDependencyInfo *dep_info) {

  // Find dependency info for this module
  ModuleDependencyInfo *current_dep = NULL;
  size_t current_idx = 0;
  for (size_t i = 0; i < module_count; i++) {
    if (strcmp(dep_info[i].module_name, module_name) == 0) {
      current_dep = &dep_info[i];
      current_idx = i;
      break;
    }
  }

  if (!current_dep) {
    fprintf(stderr, "Error: Module '%s' not found in dependency info\n",
            module_name);
    return false;
  }

  if (current_dep->processed) {
    return true; // Already processed
  }

  // First, recursively process all dependencies
  for (size_t i = 0; i < current_dep->dep_count; i++) {
    if (!process_module_codegen_recursive(ctx, current_dep->dependencies[i],
                                          modules, module_count, dep_info)) {
      return false;
    }
  }

  // Now process this module's body
  AstNode *module = modules[current_idx];
  ModuleCompilationUnit *unit = find_module(ctx, module_name);
  if (!unit) {
    fprintf(stderr, "Error: Module unit not found for '%s'\n", module_name);
    return false;
  }

  set_current_module(ctx, unit);
  ctx->module = unit->module;

  // Process all non-@use statements
  AstNode **body = module->preprocessor.module.body;
  int body_count = module->preprocessor.module.body_count;

  for (int j = 0; j < body_count; j++) {
    if (!body[j])
      continue;
    if (body[j]->type != AST_PREPROCESSOR_USE) {
      codegen_stmt(ctx, body[j]);
    }
  }

  current_dep->processed = true;
  return true;
}

// =============================================================================
// MULTI-MODULE PROGRAM HANDLER
// =============================================================================

LLVMValueRef codegen_stmt_program_multi_module(CodeGenContext *ctx,
                                               AstNode *node) {
  if (!node || node->type != AST_PROGRAM) {
    return NULL;
  }

  // PASS 1: Create all module units (unchanged)
  for (size_t i = 0; i < node->stmt.program.module_count; i++) {
    AstNode *module_node = node->stmt.program.modules[i];

    if (module_node->type == AST_PREPROCESSOR_MODULE) {
      const char *module_name = module_node->preprocessor.module.name;

      ModuleCompilationUnit *existing = find_module(ctx, module_name);
      if (existing) {
        fprintf(stderr, "Error: Duplicate module definition: %s\n",
                module_name);
        return NULL;
      }

      ModuleCompilationUnit *unit = create_module_unit(ctx, module_name);
      set_current_module(ctx, unit);
      ctx->module = unit->module;
    }
  }

  // PASS 2: Process all @use directives (unchanged)
  for (size_t i = 0; i < node->stmt.program.module_count; i++) {
    AstNode *module_node = node->stmt.program.modules[i];

    if (module_node->type == AST_PREPROCESSOR_MODULE) {
      const char *module_name = module_node->preprocessor.module.name;
      ModuleCompilationUnit *unit = find_module(ctx, module_name);

      if (!unit) {
        fprintf(stderr, "Error: Module unit not found: %s\n", module_name);
        return NULL;
      }

      set_current_module(ctx, unit);
      ctx->module = unit->module;

      AstNode **body = module_node->preprocessor.module.body;
      int body_count = module_node->preprocessor.module.body_count;

      for (int j = 0; j < body_count; j++) {
        if (body[j] && body[j]->type == AST_PREPROCESSOR_USE) {
          codegen_stmt_use(ctx, body[j]);
        }
      }
    }
  }

  // PASS 3: Generate code in DEPENDENCY ORDER
  ModuleDependencyInfo *dep_info = build_codegen_dependency_info(
      node->stmt.program.modules, node->stmt.program.module_count, ctx->arena);

  for (size_t i = 0; i < node->stmt.program.module_count; i++) {
    AstNode *module_node = node->stmt.program.modules[i];
    if (module_node->type == AST_PREPROCESSOR_MODULE) {
      const char *module_name = module_node->preprocessor.module.name;

      if (!process_module_codegen_recursive(
              ctx, module_name, node->stmt.program.modules,
              node->stmt.program.module_count, dep_info)) {
        return NULL;
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

  // Find the referenced module
  ModuleCompilationUnit *referenced_module = find_module(ctx, module_name);
  if (!referenced_module) {
    fprintf(stderr, "Error: Cannot import module '%s' - module not found\n",
            module_name);
    fprintf(stderr,
            "Note: Make sure the module is defined before it's imported\n");
    return NULL;
  }

  // Ensure we're not trying to import ourselves
  if (ctx->current_module &&
      strcmp(ctx->current_module->module_name, module_name) == 0) {
    fprintf(stderr, "Warning: Module '%s' trying to import itself - skipping\n",
            module_name);
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

// Enhanced member access handler that distinguishes between different access
// patterns Enhanced member access handler that distinguishes between different
// access patterns
LLVMValueRef codegen_expr_member_access_enhanced(CodeGenContext *ctx,
                                                 AstNode *node) {
  if (!node || node->type != AST_EXPR_MEMBER) {
    return NULL;
  }

  AstNode *object = node->expr.member.object;
  const char *member = node->expr.member.member;
  bool is_compiletime = node->expr.member.is_compiletime;

  if (object->type != AST_EXPR_IDENTIFIER && 
      !(object->type == AST_EXPR_MEMBER && is_compiletime)) {
    // Handle complex expressions like function_call().field or (*ptr).field
    return codegen_expr_struct_access(ctx, node);
  }

  // **FIX: For compile-time access (::), check module access FIRST**
  if (is_compiletime) {
    // **NEW: Handle chained compile-time access (ast::ExprKind::EXPR_NUMBER)**
    // Check if object itself is a member expression (chained ::)
    if (object->type == AST_EXPR_MEMBER && object->expr.member.is_compiletime) {
      // This is chained: module::Type::Member
      // Example: ast::ExprKind::EXPR_NUMBER
      //   object = ast::ExprKind (another member expr)
      //   member = EXPR_NUMBER
      
      if (object->expr.member.object->type != AST_EXPR_IDENTIFIER) {
        fprintf(stderr, "Error: Expected identifier in chained compile-time access\n");
        return NULL;
      }
      
      const char *module_name = object->expr.member.object->expr.identifier.name;
      const char *type_name = object->expr.member.member;
      
      // Build the fully qualified name: TypeName.Member
      char type_qualified_name[256];
      snprintf(type_qualified_name, sizeof(type_qualified_name), "%s.%s", type_name, member);
      
      // Look in the specified module
      ModuleCompilationUnit *source_module = find_module(ctx, module_name);
      if (source_module) {
        LLVM_Symbol *enum_member = find_symbol_in_module(source_module, type_qualified_name);
        if (enum_member && is_enum_constant(enum_member)) {
          return LLVMGetInitializer(enum_member->value);
        }
      }
      
      // If not found in the named module, try current module (in case it was imported)
      LLVM_Symbol *enum_member = find_symbol_in_module(ctx->current_module, type_qualified_name);
      if (enum_member && is_enum_constant(enum_member)) {
        return LLVMGetInitializer(enum_member->value);
      }
      
      // Try all imported modules as fallback
      for (ModuleCompilationUnit *unit = ctx->modules; unit; unit = unit->next) {
        if (unit == ctx->current_module)
          continue;
          
        LLVM_Symbol *sym = find_symbol_in_module(unit, type_qualified_name);
        if (sym && is_enum_constant(sym)) {
          return LLVMGetInitializer(sym->value);
        }
      }
      
      fprintf(stderr, "Error: Enum member '%s::%s::%s' not found\n", 
              module_name, type_name, member);
      return NULL;
    }

    // Handle simple compile-time access (module::symbol or Type::member)
    const char *object_name = NULL;
    if (object->type == AST_EXPR_IDENTIFIER) {
      object_name = object->expr.identifier.name;
    } else {
      fprintf(stderr, "Error: Expected identifier for compile-time access\n");
      return NULL;
    }

    // 1. First check if it's a direct qualified symbol (module.function or enum.member)
    char qualified_name[256];
    snprintf(qualified_name, sizeof(qualified_name), "%s.%s", object_name, member);

    LLVM_Symbol *qualified_sym = find_symbol_in_module(ctx->current_module, qualified_name);
    if (qualified_sym) {
      if (qualified_sym->is_function) {
        return qualified_sym->value;
      } else if (is_enum_constant(qualified_sym)) {
        return LLVMGetInitializer(qualified_sym->value);
      } else {
        return LLVMBuildLoad2(ctx->builder, qualified_sym->type, qualified_sym->value, "load");
      }
    }

    // 2. Search all modules to find where this symbol exists
    for (ModuleCompilationUnit *search_module = ctx->modules; search_module;
         search_module = search_module->next) {
      if (search_module == ctx->current_module)
        continue;

      LLVM_Symbol *source_sym = find_symbol_in_module(search_module, member);
      if (source_sym) {
        // Found the symbol - import it now with the alias prefix
        if (source_sym->is_function) {
          import_function_symbol(ctx, source_sym, search_module, object_name);
        } else if (is_enum_constant(source_sym)) {
          // For enum constants, return directly from source
          return LLVMGetInitializer(source_sym->value);
        } else {
          import_variable_symbol(ctx, source_sym, search_module, object_name);
        }

        // Now try to find the imported symbol again
        qualified_sym = find_symbol_in_module(ctx->current_module, qualified_name);
        if (qualified_sym) {
          if (qualified_sym->is_function) {
            return qualified_sym->value;
          } else {
            return LLVMBuildLoad2(ctx->builder, qualified_sym->type, qualified_sym->value, "load");
          }
        }
        break; // Found and imported, stop searching
      }
    }

    // 3. Check if this is an imported module function
    for (ModuleCompilationUnit *unit = ctx->modules; unit; unit = unit->next) {
      if (unit == ctx->current_module)
        continue;

      // Check if we have any imported symbols from this module with the alias pattern
      char test_qualified_name[256];
      snprintf(test_qualified_name, sizeof(test_qualified_name), "%s.%s", object_name, member);

      LLVM_Symbol *imported_sym = find_symbol_in_module(ctx->current_module, test_qualified_name);
      if (imported_sym && imported_sym->is_function) {
        return imported_sym->value;
      }

      // Also check the original module for the function
      LLVM_Symbol *original_sym = find_symbol_in_module(unit, member);
      if (original_sym && original_sym->is_function &&
          strcmp(unit->module_name, object_name) == 0) {

        // Create external declaration if not already present
        LLVMModuleRef current_llvm_module =
            ctx->current_module ? ctx->current_module->module : ctx->module;

        LLVMValueRef existing = LLVMGetNamedFunction(current_llvm_module, member);
        if (!existing) {
          LLVMTypeRef func_type = LLVMGlobalGetValueType(original_sym->value);
          existing = LLVMAddFunction(current_llvm_module, member, func_type);
          LLVMSetLinkage(existing, LLVMExternalLinkage);

          // Add to current module's symbol table
          add_symbol_to_module(ctx->current_module, test_qualified_name, existing, func_type, true);
        }
        return original_sym->value;
      }
    }

    // 4. Error for compile-time access that wasn't found
    fprintf(stderr, "Error: No compile-time symbol '%s::%s' found\n", object_name, member);
    return NULL;
  }

  // **NOW check for runtime access (.)**
  const char *object_name = object->expr.identifier.name;
  
  // Only NOW do we check if this is a local struct variable
  LLVM_Symbol *local_sym = find_symbol(ctx, object_name);
  if (local_sym && !local_sym->is_function) {
    // Check if this local variable has a struct type
    LLVMTypeRef sym_type = local_sym->type;
    LLVMTypeKind sym_kind = LLVMGetTypeKind(sym_type);

    // If it's a struct or pointer to struct, this is struct field access
    bool is_struct_access = false;

    if (sym_kind == LLVMStructTypeKind) {
      is_struct_access = true;
    } else if (sym_kind == LLVMPointerTypeKind && local_sym->element_type) {
      if (LLVMGetTypeKind(local_sym->element_type) == LLVMStructTypeKind) {
        is_struct_access = true;
      }
    }

    if (is_struct_access) {
      // This is struct field/method access
      return codegen_expr_struct_access(ctx, node);
    }
  }

  // 1. Check if base_name is a known module (give helpful error)
  bool is_module_access = false;
  for (ModuleCompilationUnit *unit = ctx->modules; unit; unit = unit->next) {
    if (strcmp(unit->module_name, object_name) == 0) {
      is_module_access = true;
      break;
    }

    // Also check for alias pattern
    char test_qualified_name[256];
    snprintf(test_qualified_name, sizeof(test_qualified_name), "%s.%s", object_name, member);
    LLVM_Symbol *test_sym = find_symbol_in_module(ctx->current_module, test_qualified_name);
    if (test_sym && test_sym->is_function) {
      is_module_access = true;
      break;
    }
  }

  if (is_module_access) {
    fprintf(stderr,
            "Error: Cannot use runtime access '.' for module function.\n"
            "  Did you mean '%s::%s'?\n",
            object_name, member);
    return NULL;
  }

  // 2. Check if base_name is a variable with a struct type
  LLVM_Symbol *obj_sym = find_symbol(ctx, object_name);
  if (!obj_sym) {
    fprintf(stderr,
            "Error: Undefined identifier '%s' in member access '%s.%s'\n",
            object_name, object_name, member);
    return NULL;
  }

  if (obj_sym->is_function) {
    fprintf(stderr, "Error: Cannot use member access on function '%s'\n", object_name);
    return NULL;
  }

  // 3. Handle struct field access
  return codegen_expr_struct_access(ctx, node);
}

LLVMValueRef codegen_expr_member_access(CodeGenContext *ctx, AstNode *node) {
  if (!node || node->type != AST_EXPR_MEMBER) {
    return NULL;
  }

  // For member access, we expect:
  // - object to be an identifier (module alias OR enum name)
  // - member to be an identifier (symbol name OR enum member)

  AstNode *object = node->expr.member.object;
  const char *member = node->expr.member.member;

  if (object->type != AST_EXPR_IDENTIFIER) {
    fprintf(stderr, "Error: Invalid member access syntax\n");
    return NULL;
  }

  const char *object_name = object->expr.identifier.name;

  // Create the qualified name once
  char qualified_name[256];
  snprintf(qualified_name, sizeof(qualified_name), "%s.%s", object_name,
           member);

  // Look up the qualified symbol (could be enum member OR module member)
  LLVM_Symbol *sym = find_symbol_in_module(ctx->current_module, qualified_name);
  if (sym) {
    if (sym->is_function) {
      return sym->value;
    } else if (is_enum_constant(sym)) {
      // Enum constant - return the constant value directly
      return LLVMGetInitializer(sym->value);
    } else {
      // Regular variable - load its value
      return LLVMBuildLoad2(ctx->builder, sym->type, sym->value, "load");
    }
  }

  // Check if object_name is an enum type (namespace symbol) for better error
  // reporting
  LLVM_Symbol *enum_type_sym = find_symbol(ctx, object_name);
  if (enum_type_sym && enum_type_sym->value == NULL) {
    // This is an enum namespace, but the member wasn't found
    fprintf(stderr, "Error: Enum member '%s' not found in enum '%s'\n", member,
            object_name);
  } else {
    // Neither enum nor module member found
    fprintf(stderr, "Error: Symbol '%s.%s' not found\n", object_name, member);
  }

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
