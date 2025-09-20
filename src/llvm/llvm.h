// Enhanced llvm.h - Complete module system declarations
#pragma once
// LLVM C API Headers
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

// Standard Library Headers
#include <llvm-c/Types.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Project Headers
#include "../ast/ast.h"
#include "../c_libs/memory/memory.h"

typedef struct LLVM_Symbol LLVM_Symbol;
typedef struct CodeGenContext CodeGenContext;
typedef struct ModuleCompilationUnit ModuleCompilationUnit;

// Symbol table entry for variables and functions
struct LLVM_Symbol {
  char *name;
  LLVMValueRef value;
  LLVMTypeRef type;
  bool is_function;
  struct LLVM_Symbol *next;
};

// Individual module compilation unit
struct ModuleCompilationUnit {
  char *module_name;
  LLVMModuleRef module;
  LLVM_Symbol *symbols;
  bool is_main_module;
  struct ModuleCompilationUnit *next;
};

typedef struct DeferredStatement {
  AstNode *statement;
  LLVMBasicBlockRef cleanup_block;
  struct DeferredStatement *next;
} DeferredStatement;

typedef struct StructInfo {
  char *name;
  LLVMTypeRef llvm_type;
  char **field_names;
  LLVMTypeRef *field_types;
  bool *field_is_public;
  size_t field_count;
  bool is_public;
  struct StructInfo *next;
} StructInfo;

// Code generation context
struct CodeGenContext {
  // LLVM Core Components
  LLVMContextRef context;
  LLVMBuilderRef builder;

  // Module Management (New System)
  ModuleCompilationUnit *modules;
  ModuleCompilationUnit *current_module;

  // Legacy Support (for backward compatibility)
  LLVMModuleRef module;

  // Defer statement tracking
  struct DeferredStatement *deferred_statements;
  size_t deferred_count;
  size_t deferred_capacity;

  // Code Generation State
  LLVMValueRef current_function;
  LLVMBasicBlockRef loop_continue_block;
  LLVMBasicBlockRef loop_break_block;

  StructInfo *struct_types;

  // Memory Management
  ArenaAllocator *arena;
};

// =============================================================================
// MODULE MANAGEMENT FUNCTIONS
// =============================================================================

// Create a new module compilation unit
ModuleCompilationUnit *create_module_unit(CodeGenContext *ctx,
                                          const char *module_name);

// Find module by name
ModuleCompilationUnit *find_module(CodeGenContext *ctx,
                                   const char *module_name);

// Set current module for code generation
void set_current_module(CodeGenContext *ctx, ModuleCompilationUnit *module);

// Compile all modules to separate object files
bool compile_modules_to_objects(CodeGenContext *ctx, const char *output_dir);

// Generate external function declarations for cross-module calls
void generate_external_declarations(CodeGenContext *ctx,
                                    ModuleCompilationUnit *target_module);

// =============================================================================
// SYMBOL IMPORT AND MODULE INTEROP
// =============================================================================

// Import symbols from one module to another
void import_module_symbols(CodeGenContext *ctx,
                           ModuleCompilationUnit *source_module,
                           const char *alias);

// Import specific function symbol
void import_function_symbol(CodeGenContext *ctx, LLVM_Symbol *source_symbol,
                            ModuleCompilationUnit *source_module,
                            const char *alias);

// Import specific variable symbol
void import_variable_symbol(CodeGenContext *ctx, LLVM_Symbol *source_symbol,
                            ModuleCompilationUnit *source_module,
                            const char *alias);

// Enhanced symbol lookup with module support
LLVM_Symbol *find_symbol_with_module_support(CodeGenContext *ctx,
                                             const char *name);

// =============================================================================
// MODULE UTILITY FUNCTIONS
// =============================================================================

// Check if module is the main module
bool is_main_module(ModuleCompilationUnit *unit);

// Set a module as the main module
void set_module_as_main(ModuleCompilationUnit *unit);

// Print module information for debugging
void print_module_info(CodeGenContext *ctx);
void debug_object_files(const char *output_dir);

// =============================================================================
// ENHANCED CORE API FUNCTIONS
// =============================================================================

// Context Management
CodeGenContext *init_codegen_context(ArenaAllocator *arena);
void cleanup_codegen_context(CodeGenContext *ctx);

// Enhanced Symbol Table Operations (now module-aware)
void add_symbol_to_module(ModuleCompilationUnit *module, const char *name,
                          LLVMValueRef value, LLVMTypeRef type,
                          bool is_function);
LLVM_Symbol *find_symbol_in_module(ModuleCompilationUnit *module,
                                   const char *name);
LLVM_Symbol *find_symbol_global(CodeGenContext *ctx, const char *name,
                                const char *module_name);

// Main Code Generation
bool generate_program_modules(CodeGenContext *ctx, AstNode *ast_root,
                              const char *output_dir);

// Object File Generation (per module)
bool generate_module_object_file(ModuleCompilationUnit *module,
                                 const char *output_path);

// Existing API (preserved for compatibility)
void add_symbol(CodeGenContext *ctx, const char *name, LLVMValueRef value,
                LLVMTypeRef type, bool is_function);
LLVM_Symbol *find_symbol(CodeGenContext *ctx, const char *name);
char *print_llvm_ir(CodeGenContext *ctx);
bool generate_object_file(CodeGenContext *ctx, const char *object_filename);
bool generate_assembly_file(CodeGenContext *ctx, const char *asm_filename);
char *process_escape_sequences(const char *input);
LLVMLinkage get_function_linkage(AstNode *node);

// =============================================================================
// ENHANCED STRUCT SUPPORT FUNCTIONS
// =============================================================================

// Core struct management
StructInfo *find_struct_type(CodeGenContext *ctx, const char *name);
void add_struct_type(CodeGenContext *ctx, StructInfo *struct_info);
int get_field_index(StructInfo *struct_info, const char *field_name);
bool is_field_access_allowed(CodeGenContext *ctx, StructInfo *struct_info,
                             int field_index);

// Enhanced member access (handles both struct.field and module.symbol)
LLVMValueRef codegen_expr_member_access_enhanced(CodeGenContext *ctx,
                                                 AstNode *node);

// Struct literal/initializer support
LLVMValueRef codegen_struct_literal(CodeGenContext *ctx,
                                    const char *struct_name,
                                    LLVMValueRef *field_values,
                                    size_t field_count);

// =============================================================================
// STRUCT TYPE SYSTEM INTEGRATION
// =============================================================================

// Enhanced type checking for structs
bool is_struct_type(CodeGenContext *ctx, LLVMTypeRef type);
const char *get_struct_name_from_type(CodeGenContext *ctx, LLVMTypeRef type);

// Struct construction helpers
LLVMValueRef create_struct_zero_initializer(CodeGenContext *ctx,
                                            const char *struct_name);
LLVMValueRef create_struct_copy(CodeGenContext *ctx, LLVMValueRef src_struct,
                                StructInfo *struct_info);

// Struct debugging and introspection
void print_struct_info(CodeGenContext *ctx, const char *struct_name);
void debug_struct_layout(CodeGenContext *ctx, const char *struct_name);

StructInfo *find_struct_type(CodeGenContext *ctx, const char *name);
void add_struct_type(CodeGenContext *ctx, StructInfo *struct_info);
LLVMValueRef codegen_stmt_struct(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_field(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_struct_access(CodeGenContext *ctx, AstNode *node);
LLVMTypeRef codegen_type_struct(CodeGenContext *ctx, const char *struct_name);

// =============================================================================
// AST NODE HANDLERS - PUBLIC INTERFACE
// =============================================================================

// Primary Dispatch Functions
LLVMValueRef codegen_expr(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt(CodeGenContext *ctx, AstNode *node);
LLVMTypeRef codegen_type(CodeGenContext *ctx, AstNode *node);

// Enhanced handlers for module system
LLVMValueRef codegen_stmt_program_multi_module(CodeGenContext *ctx,
                                               AstNode *node);
LLVMValueRef codegen_stmt_module(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_use(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_member_access(CodeGenContext *ctx, AstNode *node);

// =============================================================================
// AST NODE HANDLERS - EXPRESSION TYPES
// =============================================================================

LLVMValueRef codegen_expr_literal(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_identifier(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_binary(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_unary(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_call(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_assignment(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_index(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_cast(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_sizeof(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_alloc(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_free(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_deref(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_expr_addr(CodeGenContext *ctx, AstNode *node);

// =============================================================================
// AST NODE HANDLERS - STATEMENT TYPES
// =============================================================================

LLVMValueRef codegen_stmt_program(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_expression(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_var_decl(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_function(CodeGenContext *ctx, AstNode *node);

bool is_enum_constant(LLVM_Symbol *sym);
int64_t get_enum_constant_value(LLVM_Symbol *sym);
LLVMValueRef codegen_stmt_enum(CodeGenContext *ctx, AstNode *node);

LLVMValueRef codegen_stmt_return(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_block(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_if(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_print(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_defer(CodeGenContext *ctx, AstNode *node);

LLVMValueRef codegen_infinite_loop(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_while_loop(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_for_loop(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_loop(CodeGenContext *ctx, AstNode *node);

LLVMValueRef codegen_stmt_break_continue(CodeGenContext *ctx, AstNode *node);

LLVMValueRef codegen_stmt_switch(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_case(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_stmt_default(CodeGenContext *ctx, AstNode *node);
LLVMValueRef codegen_case_value(CodeGenContext *ctx, AstNode *case_value);
LLVMValueRef codegen_enum_member_case(CodeGenContext *ctx,
                                      AstNode *member_expr);

// =============================================================================
// DEFER MANAGEMENT FUNCTIONS
// =============================================================================

void init_defer_stack(CodeGenContext *ctx);
void push_defer_statement(CodeGenContext *ctx, AstNode *statement);
void execute_deferred_statements_inline(CodeGenContext *ctx,
                                        DeferredStatement *defers);
void generate_cleanup_blocks(CodeGenContext *ctx);
void clear_defer_stack(CodeGenContext *ctx);

// =============================================================================
// RANGE SUPPORT FUNCTIONS - Add this new section
// =============================================================================

// Range type checking and manipulation
bool is_range_type(LLVMTypeRef type);
LLVMValueRef get_range_start_value(CodeGenContext *ctx,
                                   LLVMValueRef range_struct);
LLVMValueRef get_range_end_value(CodeGenContext *ctx,
                                 LLVMValueRef range_struct);

// Range creation helper (optional - for consistent range creation)
LLVMValueRef create_range_struct(CodeGenContext *ctx, LLVMValueRef start,
                                 LLVMValueRef end);
LLVMTypeRef get_range_struct_type(CodeGenContext *ctx,
                                  LLVMTypeRef element_type);

// Range operations (add these if you need them later)
LLVMValueRef range_contains(CodeGenContext *ctx, LLVMValueRef range_struct,
                            LLVMValueRef value);
LLVMValueRef range_length(CodeGenContext *ctx, LLVMValueRef range_struct);

// =============================================================================
// AST NODE HANDLERS - TYPE SYSTEM
// =============================================================================

LLVMTypeRef codegen_type_basic(CodeGenContext *ctx, AstNode *node);
LLVMTypeRef codegen_type_pointer(CodeGenContext *ctx, AstNode *node);
LLVMTypeRef codegen_type_array(CodeGenContext *ctx, AstNode *node);
LLVMTypeRef codegen_type_function(CodeGenContext *ctx, AstNode *node);
