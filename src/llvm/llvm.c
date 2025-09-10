// Enhanced llvm.c - Module system implementation
#include "llvm.h"
#include <llvm-c/TargetMachine.h>
#include <stdlib.h>
#include <sys/stat.h>

// =============================================================================
// MODULE MANAGEMENT FUNCTIONS
// =============================================================================

// Create a new module compilation unit
ModuleCompilationUnit *create_module_unit(CodeGenContext *ctx,
                                          const char *module_name) {
  ModuleCompilationUnit *unit = (ModuleCompilationUnit *)arena_alloc(
      ctx->arena, sizeof(ModuleCompilationUnit),
      alignof(ModuleCompilationUnit));

  unit->module_name = arena_strdup(ctx->arena, module_name);
  unit->module = LLVMModuleCreateWithNameInContext(module_name, ctx->context);
  unit->symbols = NULL;
  unit->is_main_module = (strcmp(module_name, "main") == 0);
  unit->next = ctx->modules;

  ctx->modules = unit;
  return unit;
}

// Find module by name
ModuleCompilationUnit *find_module(CodeGenContext *ctx,
                                   const char *module_name) {
  for (ModuleCompilationUnit *unit = ctx->modules; unit; unit = unit->next) {
    if (strcmp(unit->module_name, module_name) == 0) {
      return unit;
    }
  }
  return NULL;
}

// Set current module for code generation
void set_current_module(CodeGenContext *ctx, ModuleCompilationUnit *module) {
  ctx->current_module = module;
}

// Generate external function declarations for cross-module calls
void generate_external_declarations(CodeGenContext *ctx,
                                    ModuleCompilationUnit *target_module) {
  // Go through all other modules and create external declarations for public
  // functions
  for (ModuleCompilationUnit *source_module = ctx->modules; source_module;
       source_module = source_module->next) {
    if (source_module == target_module)
      continue;

    // Look through source module's symbols for public functions
    for (LLVM_Symbol *sym = source_module->symbols; sym; sym = sym->next) {
      if (sym->is_function) {
        // Check if this function is already declared in target module
        LLVMValueRef existing =
            LLVMGetNamedFunction(target_module->module, sym->name);
        if (!existing) {
          // Create external declaration
          LLVMTypeRef func_type = LLVMGlobalGetValueType(sym->value);
          LLVMValueRef external_func =
              LLVMAddFunction(target_module->module, sym->name, func_type);
          LLVMSetLinkage(external_func, LLVMExternalLinkage);

          // Add to target module's symbol table
          add_symbol_to_module(target_module, sym->name, external_func,
                               func_type, true);
        }
      }
    }
  }
}

// Generate object file for a specific module
bool generate_module_object_file(ModuleCompilationUnit *module,
                                 const char *output_path) {
  char *error = NULL;

  // Get the target triple for the current machine
  char *target_triple = LLVMGetDefaultTargetTriple();
  LLVMSetTarget(module->module, target_triple);

  // Get the target from the triple
  LLVMTargetRef target;
  if (LLVMGetTargetFromTriple(target_triple, &target, &error)) {
    fprintf(stderr, "Failed to get target for module %s: %s\n",
            module->module_name, error);
    LLVMDisposeMessage(error);
    LLVMDisposeMessage(target_triple);
    return false;
  }

  // Create target machine with PIE-compatible settings
  LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(
      target, target_triple, "", "", LLVMCodeGenLevelDefault,
      LLVMRelocPIC,      // Changed from LLVMRelocDefault to LLVMRelocPIC
      LLVMCodeModelSmall // Changed from LLVMCodeModelDefault to
                         // LLVMCodeModelSmall
  );

  if (!target_machine) {
    fprintf(stderr, "Failed to create target machine for module %s\n",
            module->module_name);
    LLVMDisposeMessage(target_triple);
    return false;
  }

  // Set the data layout
  LLVMTargetDataRef target_data = LLVMCreateTargetDataLayout(target_machine);
  char *data_layout = LLVMCopyStringRepOfTargetData(target_data);
  LLVMSetDataLayout(module->module, data_layout);

  // Verify the module
  if (LLVMVerifyModule(module->module, LLVMAbortProcessAction, &error)) {
    fprintf(stderr, "Module verification failed for %s: %s\n",
            module->module_name, error);
    LLVMDisposeMessage(error);
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeTargetData(target_data);
    LLVMDisposeMessage(data_layout);
    LLVMDisposeMessage(target_triple);
    return false;
  }
  if (error) {
    LLVMDisposeMessage(error);
  }

  // Generate object file
  if (LLVMTargetMachineEmitToFile(target_machine, module->module,
                                  (char *)output_path, LLVMObjectFile,
                                  &error)) {
    fprintf(stderr, "Failed to emit object file for module %s: %s\n",
            module->module_name, error);
    LLVMDisposeMessage(error);
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeTargetData(target_data);
    LLVMDisposeMessage(data_layout);
    LLVMDisposeMessage(target_triple);
    return false;
  }

  // Cleanup
  LLVMDisposeTargetMachine(target_machine);
  LLVMDisposeTargetData(target_data);
  LLVMDisposeMessage(data_layout);
  LLVMDisposeMessage(target_triple);

  return true;
}

// Compile all modules to separate object files
bool compile_modules_to_objects(CodeGenContext *ctx, const char *output_dir) {
  // Create output directory if it doesn't exist
  struct stat st = {0};
  if (stat(output_dir, &st) == -1) {
    if (mkdir(output_dir, 0755) != 0) {
      fprintf(stderr, "Failed to create output directory: %s\n", output_dir);
      return false;
    }
  }

  bool success = true;

  // Process each module
  for (ModuleCompilationUnit *unit = ctx->modules; unit; unit = unit->next) {
    // Generate external declarations for cross-module calls
    generate_external_declarations(ctx, unit);

    // Create output file path
    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s.o", output_dir,
             unit->module_name);

    // printf("Compiling module '%s' to '%s'\n", unit->module_name, output_path);

    // Generate object file for this module
    if (!generate_module_object_file(unit, output_path)) {
      fprintf(stderr, "Failed to compile module: %s\n", unit->module_name);
      success = false;
    }
  }

  return success;
}

// =============================================================================
// ENHANCED CORE API FUNCTIONS
// =============================================================================

// Enhanced context initialization
CodeGenContext *init_codegen_context(ArenaAllocator *arena) {
  CodeGenContext *ctx = (CodeGenContext *)arena_alloc(
      arena, sizeof(CodeGenContext), alignof(CodeGenContext));

  // Initialize LLVM targets
  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllAsmParsers();
  LLVMInitializeAllAsmPrinters();

  ctx->context = LLVMContextCreate();
  ctx->builder = LLVMCreateBuilderInContext(ctx->context);
  ctx->modules = NULL;
  ctx->current_module = NULL;
  ctx->current_function = NULL;
  ctx->loop_continue_block = NULL;
  ctx->loop_break_block = NULL;
  ctx->arena = arena;

  return ctx;
}

// Enhanced Symbol Table Operations
void add_symbol_to_module(ModuleCompilationUnit *module, const char *name,
                          LLVMValueRef value, LLVMTypeRef type,
                          bool is_function) {
  LLVM_Symbol *sym = (LLVM_Symbol *)malloc(sizeof(LLVM_Symbol));
  sym->name = strdup(name);
  sym->value = value;
  sym->type = type;
  sym->is_function = is_function;
  sym->next = module->symbols;
  module->symbols = sym;
}

LLVM_Symbol *find_symbol_in_module(ModuleCompilationUnit *module,
                                   const char *name) {
  for (LLVM_Symbol *sym = module->symbols; sym; sym = sym->next) {
    if (strcmp(sym->name, name) == 0) {
      return sym;
    }
  }
  return NULL;
}

LLVM_Symbol *find_symbol_global(CodeGenContext *ctx, const char *name,
                                const char *module_name) {
  if (module_name) {
    ModuleCompilationUnit *module = find_module(ctx, module_name);
    if (module) {
      return find_symbol_in_module(module, name);
    }
  } else {
    // Search current module first, then all modules
    if (ctx->current_module) {
      LLVM_Symbol *sym = find_symbol_in_module(ctx->current_module, name);
      if (sym)
        return sym;
    }

    // Search all modules
    for (ModuleCompilationUnit *unit = ctx->modules; unit; unit = unit->next) {
      if (unit == ctx->current_module)
        continue;
      LLVM_Symbol *sym = find_symbol_in_module(unit, name);
      if (sym)
        return sym;
    }
  }
  return NULL;
}

// Main program generation with module support
bool generate_program_modules(CodeGenContext *ctx, AstNode *ast_root,
                              const char *output_dir) {
  if (!ast_root || ast_root->type != AST_PROGRAM) {
    return false;
  }

  // Generate code for all modules
  codegen_stmt_program_multi_module(ctx, ast_root);

  // Compile all modules to separate object files
  return compile_modules_to_objects(ctx, output_dir);
}

// Cleanup (enhanced)
void cleanup_codegen_context(CodeGenContext *ctx) {
  if (ctx) {
    // Cleanup modules
    ModuleCompilationUnit *unit = ctx->modules;
    while (unit) {
      ModuleCompilationUnit *next = unit->next;

      // Free symbols
      LLVM_Symbol *sym = unit->symbols;
      while (sym) {
        LLVM_Symbol *next_sym = sym->next;
        free(sym->name);
        free(sym);
        sym = next_sym;
      }

      LLVMDisposeModule(unit->module);
      unit = next;
    }

    LLVMDisposeBuilder(ctx->builder);
    LLVMContextDispose(ctx->context);
    LLVMShutdown();
  }
}

// Compatibility functions (delegate to current module)
void add_symbol(CodeGenContext *ctx, const char *name, LLVMValueRef value,
                LLVMTypeRef type, bool is_function) {
  if (ctx->current_module) {
    add_symbol_to_module(ctx->current_module, name, value, type, is_function);
  }
}

LLVM_Symbol *find_symbol(CodeGenContext *ctx, const char *name) {
  return find_symbol_global(ctx, name, NULL);
}

// Print IR for current module
char *print_llvm_ir(CodeGenContext *ctx) {
  if (ctx->current_module) {
    return LLVMPrintModuleToString(ctx->current_module->module);
  }
  return NULL;
}

// Generate object file for current module
bool generate_object_file(CodeGenContext *ctx, const char *object_filename) {
  if (ctx->current_module) {
    return generate_module_object_file(ctx->current_module, object_filename);
  }
  return false;
}

// Generate assembly file for current module
bool generate_assembly_file(CodeGenContext *ctx, const char *asm_filename) {
  if (!ctx->current_module)
    return false;

  char *error = NULL;
  char *target_triple = LLVMGetDefaultTargetTriple();
  LLVMSetTarget(ctx->current_module->module, target_triple);

  LLVMTargetRef target;
  if (LLVMGetTargetFromTriple(target_triple, &target, &error)) {
    fprintf(stderr, "Failed to get target: %s\n", error);
    LLVMDisposeMessage(error);
    LLVMDisposeMessage(target_triple);
    return false;
  }

  LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(
      target, target_triple, "", "", LLVMCodeGenLevelNone,
      LLVMRelocDefault, LLVMCodeModelDefault
  );

  if (!target_machine) {
    fprintf(stderr, "Failed to create target machine\n");
    LLVMDisposeMessage(target_triple);
    return false;
  }

  LLVMTargetDataRef target_data = LLVMCreateTargetDataLayout(target_machine);
  char *data_layout = LLVMCopyStringRepOfTargetData(target_data);
  LLVMSetDataLayout(ctx->current_module->module, data_layout);

  if (LLVMTargetMachineEmitToFile(target_machine, ctx->current_module->module,
                                  (char *)asm_filename, LLVMAssemblyFile,
                                  &error)) {
    fprintf(stderr, "Failed to emit assembly file: %s\n", error);
    LLVMDisposeMessage(error);
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeTargetData(target_data);
    LLVMDisposeMessage(data_layout);
    LLVMDisposeMessage(target_triple);
    return false;
  }

  LLVMDisposeTargetMachine(target_machine);
  LLVMDisposeTargetData(target_data);
  LLVMDisposeMessage(data_layout);
  LLVMDisposeMessage(target_triple);

  return true;
}

// Existing helper functions (unchanged)
LLVMLinkage get_function_linkage(AstNode *node) {
  const char *name = node->stmt.func_decl.name;

  // Main function must always be external
  if (strcmp(name, "main") == 0) {
    return LLVMExternalLinkage;
  }

  // Use the is_public flag for other functions
  if (node->stmt.func_decl.is_public) {
    return LLVMExternalLinkage;
  } else {
    return LLVMInternalLinkage;
  }
}

char *process_escape_sequences(const char *input) {
  size_t len = strlen(input);
  char *output = malloc(len + 1);
  size_t out_idx = 0;

  for (size_t i = 0; i < len; i++) {
    if (input[i] == '\\' && i + 1 < len) {
      switch (input[i + 1]) {
      case 'n':
        output[out_idx++] = '\n';
        i++;
        break;
      case 'r':
        output[out_idx++] = '\r';
        i++;
        break;
      case 't':
        output[out_idx++] = '\t';
        i++;
        break;
      case '\\':
        output[out_idx++] = '\\';
        i++;
        break;
      case '"':
        output[out_idx++] = '"';
        i++;
        break;
      case '0':
        output[out_idx++] = '\0';
        i++;
        break;
      default:
        output[out_idx++] = input[i];
        break;
      }
    } else {
      output[out_idx++] = input[i];
    }
  }
  output[out_idx] = '\0';
  return output;
}
