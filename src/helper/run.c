#include "../ast/ast_utils.h"
#include "../c_libs/error/error.h"
#include "../llvm/llvm.h"
#include "../parser/parser.h"
#include "../typechecker/type.h"
#include "help.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

// Helper function to create directory if it doesn't exist
bool create_directory(const char *path) {
#ifdef _WIN32
  return mkdir(path) == 0 || errno == EEXIST;
#else
  return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

void handle_segfault(int sig) {
  (void)sig;
  fprintf(stderr, "\nSegmentation fault caught during LLVM operations!\n");
  fprintf(stderr, "This likely indicates a problem in LLVM IR generation.\n");
  exit(1);
}

void handle_illegal_instruction(int sig) {
  (void)sig;
  fprintf(stderr, "\nIllegal instruction caught!\n");
  fprintf(stderr, "This suggests LLVM generated invalid machine code.\n");
  fprintf(stderr,
          "Check your target architecture and LLVM version compatibility.\n");
  exit(1);
}

void save_module_output_files(CodeGenContext *ctx, const char *output_dir) {
  // Create output directory if it doesn't exist
  if (!create_directory(output_dir)) {
    fprintf(stderr, "Warning: Failed to create output directory: %s\n",
            output_dir);
  }

  // Save IR and assembly for each module
  for (ModuleCompilationUnit *unit = ctx->modules; unit; unit = unit->next) {
    char filename[512];

    // Set current module for legacy functions
    set_current_module(ctx, unit);
    ctx->module = unit->module; // Update legacy field

    // Save readable LLVM IR
    snprintf(filename, sizeof(filename), "%s/%s.ll", output_dir,
             unit->module_name);
    char *ir = print_llvm_ir(ctx);
    if (ir) {
      FILE *f = fopen(filename, "w");
      if (f) {
        fprintf(f, "%s", ir);
        fclose(f);
        // printf("Generated IR file: %s\n", filename);
      }
      LLVMDisposeMessage(ir);
    }

    // Generate assembly file
    snprintf(filename, sizeof(filename), "%s/%s.s", output_dir,
             unit->module_name);
    generate_assembly_file(ctx, filename);
    // printf("Generated assembly file: %s\n", filename);
  }
}

// Update your generate_llvm_code_modules function:
bool generate_llvm_code_modules(AstNode *root, BuildConfig config,
                                ArenaAllocator *allocator, int *step) {
  CodeGenContext *ctx = init_codegen_context(allocator);
  if (!ctx) {
    return false;
  }

  const char *base_name = config.name ? config.name : "output";
  const char *output_dir = config.save ? "output" : "obj";

  // Create output directory
  if (!create_directory(output_dir)) {
    fprintf(stderr, "Failed to create output directory: %s\n", output_dir);
    cleanup_codegen_context(ctx);
    return false;
  }

  signal(SIGSEGV, handle_segfault);
  signal(SIGILL, handle_illegal_instruction);

  // Generate LLVM IR for all modules using the new multi-module system
  bool success = generate_program_modules(ctx, root, output_dir);
  if (!success) {
    fprintf(stderr, "Failed to generate LLVM modules\n");
    cleanup_codegen_context(ctx);
    return false;
  }

  print_progress(++(*step), 9, "LLVM IR Generation");

  if (config.save) {
    // Save detailed output files for debugging
    save_module_output_files(ctx, output_dir);

    // Print module information for debugging
    print_module_info(ctx);

    // Debug object files
    debug_object_files(output_dir);
  }

  // Link all object files together to create final executable
  char exe_file[256];
  snprintf(exe_file, sizeof(exe_file), "%s", base_name);

  // printf("Linking modules into executable: %s\n", exe_file);
  if (!link_object_files(output_dir, exe_file)) {
    fprintf(stderr, "Failed to link object files\n");

    // Try to provide more helpful error information
    printf("\nTrying to diagnose linking issues...\n");
    debug_object_files(output_dir);

    cleanup_codegen_context(ctx);
    return false;
  }

  print_progress(++(*step), 9, "Linking");

  cleanup_codegen_context(ctx);
  return true;
}

// Helper function to link all object files in a directory
bool link_object_files(const char *output_dir, const char *executable_name) {
  char command[2048];

  // Build the linking command with PIE-compatible flags
  snprintf(command, sizeof(command), "cc -pie %s/*.o -o %s", output_dir,
           executable_name);

  // printf("Linking command: %s\n", command);

  int result = system(command);
  if (result != 0) {
    fprintf(stderr, "Linking failed with exit code %d\n", result);

    // Try alternative linking approach
    printf("Trying alternative linking approach...\n");
    snprintf(command, sizeof(command), "gcc -no-pie %s/*.o -o %s", output_dir,
             executable_name);

    printf("Alternative linking command: %s\n", command);
    result = system(command);

    if (result != 0) {
      fprintf(stderr, "Alternative linking also failed with exit code %d\n",
              result);
      return false;
    }
  }

  return true;
}

// Alternative approach: Enhanced linking with better error handling
bool link_object_files_enhanced(const char *output_dir,
                                const char *executable_name) {
  char command[2048];

  // Try different linking strategies in order of preference
  const char *link_strategies[] = {
      "gcc -pie %s/*.o -o %s",     // PIE (position independent executable)
      "gcc -no-pie %s/*.o -o %s",  // No PIE
      "gcc -static %s/*.o -o %s",  // Static linking
      "clang -pie %s/*.o -o %s",   // Try clang instead of gcc
      "clang -no-pie %s/*.o -o %s" // Clang without PIE
  };

  const char *strategy_names[] = {"PIE linking", "No-PIE linking",
                                  "Static linking", "Clang PIE linking",
                                  "Clang no-PIE linking"};

  size_t num_strategies = sizeof(link_strategies) / sizeof(link_strategies[0]);

  for (size_t i = 0; i < num_strategies; i++) {
    // printf("Attempting %s...\n", strategy_names[i]);
    snprintf(command, sizeof(command), link_strategies[i], output_dir,
             executable_name);
    // printf("Command: %s\n", command);

    int result = system(command);
    if (result == 0) {
      // printf("Successfully linked using %s\n", strategy_names[i]);
      return true;
    } else {
      printf("Failed with exit code %d\n", result);
    }
  }

  fprintf(stderr, "All linking strategies failed\n");
  return false;
}

// Helper function to parse a single file and extract its module
// Helper function to parse a single file and extract its module
Stmt *parse_file_to_module(const char *path, size_t position,
                           ArenaAllocator *allocator, BuildConfig *config) {
  const char *source = read_file(path);
  if (!source) {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    return NULL;
  }

  Lexer lexer;
  init_lexer(&lexer, source, allocator);

  GrowableArray tokens;
  if (!growable_array_init(&tokens, allocator, MAX_TOKENS, sizeof(Token))) {
    fprintf(stderr, "Failed to initialize token array for %s.\n", path);
    free((void *)source);
    return NULL;
  }

  Token tk;
  while ((tk = next_token(&lexer)).type_ != TOK_EOF) {
    Token *slot = (Token *)growable_array_push(&tokens);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing token array\n");
      free((void *)source);
      return NULL;
    }
    *slot = tk;
  }

  if (error_report()) {
    free((void *)source);
    return NULL;
  }

  config->tokens = tokens.data;
  config->token_count = tokens.item_size;  // Note: This should probably be tokens.count

  // Parse and extract the module from the program
  AstNode *program_root = parse(&tokens, allocator, config);
  free((void *)source);

  if (!program_root) {
    return NULL;
  }

  // Extract the first (and should be only) module from the program
  if (program_root->type == AST_PROGRAM &&
      program_root->stmt.program.module_count > 0) {
    Stmt *module = (Stmt *)program_root->stmt.program.modules[0];

    // Update the module's position if it's actually a module
    if (module && module->type == AST_PREPROCESSOR_MODULE) {
      // Assuming your module structure has a position field - adjust based on
      // your actual AST structure This might be module->data.module.position =
      // position; depending on your struct layout
      module->preprocessor.module.potions = position;
    }

    return module;
  }

  return NULL;
}

AstNode *lex_and_parse_file(const char *path, ArenaAllocator *allocator,
                            BuildConfig *config) {
  const char *source = read_file(path);
  if (!source) {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    return NULL;
  }

  Lexer lexer;
  init_lexer(&lexer, source, allocator);

  GrowableArray tokens;
  if (!growable_array_init(&tokens, allocator, MAX_TOKENS, sizeof(Token))) {
    fprintf(stderr, "Failed to initialize token array for %s.\n", path);
    free((void *)source);
    return NULL;
  }

  Token tk;
  while ((tk = next_token(&lexer)).type_ != TOK_EOF) {
    Token *slot = (Token *)growable_array_push(&tokens);
    if (!slot) {
      fprintf(stderr, "Out of memory while growing token array\n");
      free((void *)source);
      return NULL;
    }
    *slot = tk;
  }

  if (error_report()) {
    free((void *)source);
    return NULL;
  }

  AstNode *root = parse(&tokens, allocator, config);

  free((void *)source);
  return root;
}

bool run_build(BuildConfig config, ArenaAllocator *allocator) {
  bool success = false;
  int total_stages = 10;
  int step = 0;

  GrowableArray modules;
  if (!growable_array_init(&modules, allocator, 16, sizeof(AstNode *))) {
    return false;
  }

  // Stage 1: Lexing
  print_progress(++step, total_stages, "Lexing");

  // Parse additional files
  for (size_t i = 0; i < config.file_count; i++) {
    char **files_array = (char **)config.files.data;
    Stmt *module = parse_file_to_module(files_array[i], i, allocator, &config);
    if (!module || error_report())
      goto cleanup;

    AstNode **slot = (AstNode **)growable_array_push(&modules);
    if (!slot)
      goto cleanup;
    *slot = (AstNode *)module;
  }

  // Stage 2: Parsing
  print_progress(++step, total_stages, "Parsing");

  Stmt *main_module = parse_file_to_module(config.filepath, config.file_count,
                                           allocator, &config);
  if (!main_module || error_report())
    goto cleanup;

  AstNode **main_slot = (AstNode **)growable_array_push(&modules);
  if (!main_slot)
    goto cleanup;
  *main_slot = (AstNode *)main_module;

  // Stage 3: Combining modules
  print_progress(++step, total_stages, "Module Combination");

  AstNode *combined_program = create_program_node(
      allocator, (AstNode **)modules.data, modules.count, 0, 0);
  if (!combined_program)
    goto cleanup;

  // print_ast(combined_program, "", false, false);

  // Stage 4: Typechecking
  print_progress(++step, total_stages, "Typechecking");

  Scope root_scope;
  init_scope(&root_scope, NULL, "global", allocator);
  tc_error_init(config.tokens, config.token_count, config.filepath, allocator);
  bool tc = typecheck(combined_program, &root_scope, allocator);
  error_report();
  // debug_print_scope(&root_scope, 0);

  if (tc) {
    if (config.check_mem) {
      // Stage 5: Memory Analysis - ensure clean line before output
      print_progress(++step, total_stages, "Memory Analysis");

      StaticMemoryAnalyzer *analyzer = get_static_analyzer(&root_scope);
      if (analyzer && analyzer->allocations.count > 0) {
        // Use the error system instead of direct printing
        int memory_issues = static_memory_check_and_report(
            analyzer,
            allocator,          // Your arena allocator
            config.tokens,      // Your token array
            config.token_count, // Number of tokens
            config.filepath);   // Source file path

        // Optional: Print a brief summary after progress bar
        if (memory_issues > 0) {
          ensure_clean_line(); // Clean up progress bar
          error_report();
        } else {
          error_report();
        }
      }
    } else {
      ++step;
    }

    // Stage 6: LLVM IR (UPDATED - now uses module system)
    print_progress(++step, total_stages, "LLVM IR");

    success =
        generate_llvm_code_modules(combined_program, config, allocator, &step);
  }

  // Stage 7: Finalizing
  print_progress(++step, total_stages, "Finalizing");
  print_progress(++step, total_stages, "Completed");
  printf("Build succeeded! Written to '%s'\n",
         config.name ? config.name : "output");

cleanup:
  return success;
}
