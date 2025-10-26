/**
 * @file type.h
 * @brief Type checking and symbol table management for AST nodes with memory
 * tracking.
 *
 * Provides type checking for the Abstract Syntax Tree (AST), including:
 * - Scoped symbol table management
 * - Type compatibility and inference
 * - Function, struct, and enum validation
 * - Module imports and visibility rules
 * - Memory allocation tracking for alloc/free operations
 * - Comprehensive error reporting
 */

#pragma once

#include "../ast/ast.h"
#include "../c_libs/memory/memory.h"
#include "../helper/help.h"
#include "../lexer/lexer.h"

// ============================================================================
// Data Structures
// ============================================================================

extern Token *g_tokens;
extern int g_token_count;
extern const char *g_file_path;
extern ArenaAllocator *g_arena;

typedef struct {
  size_t line;
  size_t column;
  const char *variable_name;
  const char *original_variable;
  bool has_matching_free;
  int free_count;
  int use_after_free_count;
  GrowableArray aliases;
  bool reported;
  const char *function_name;
  const char *file_path;
} StaticAllocation;

typedef struct {
  GrowableArray allocations;
  ArenaAllocator *arena;
  bool skip_memory_tracking;
} StaticMemoryAnalyzer;

/**
 * @brief Represents a symbol with associated type and metadata.
 */
typedef struct {
  const char *name;   /**< Symbol name */
  AstNode *type;      /**< AST type node */
  bool is_public;     /**< Public accessibility flag */
  bool is_mutable;    /**< Mutability flag */
  size_t scope_depth; /**< Nesting level for debugging */
  bool returns_ownership;
  bool takes_ownership;
} Symbol;

/**
 * @brief Represents a lexical scope with hierarchical relationships.
 */
typedef struct Scope {
  struct Scope *parent;
  GrowableArray symbols;
  GrowableArray children;
  const char *scope_name;
  size_t depth;
  bool is_function_scope;
  AstNode *associated_node;

  // Module-related metadata
  bool is_module_scope;
  const char *module_name;
  GrowableArray imported_modules;

  bool returns_ownership;
  bool takes_ownership;

  // Memory tracking
  StaticMemoryAnalyzer *memory_analyzer;
  GrowableArray deferred_frees;

  // Build configuration
  BuildConfig *config;
} Scope;

/**
 * @brief Represents an imported module with optional aliasing.
 */
typedef struct {
  const char *module_name; /**< Original module name */
  const char *alias;       /**< Alias in importing module */
  Scope *module_scope;
} ModuleImport;

typedef struct {
  const char *module_name;
  GrowableArray dependencies; // Array of const char* (module names)
  bool processed;
} ModuleDependency;

/**
 * @brief Result of type compatibility checking.
 */
typedef enum {
  TYPE_MATCH_EXACT,      /**< Types match exactly */
  TYPE_MATCH_COMPATIBLE, /**< Implicitly convertible */
  TYPE_MATCH_NONE        /**< Incompatible */
} TypeMatchResult;

/**
 * @brief Represents an error encountered during type checking.
 */
typedef struct {
  const char *message; /**< Error description */
  size_t line;         /**< Line number */
  size_t column;       /**< Column number */
  const char *context; /**< Additional context info */
} TypeError;

// ============================================================================
// Memory Tracking
// ============================================================================

void static_memory_analyzer_init(StaticMemoryAnalyzer *analyzer,
                                 ArenaAllocator *arena);
void static_memory_track_alloc(StaticMemoryAnalyzer *analyzer, size_t line,
                               size_t column, const char *var_name,
                               const char *function_name, Token *tokens,
                               size_t token_count, const char *file_path);
void static_memory_track_free(StaticMemoryAnalyzer *analyzer,
                              const char *var_name, const char *function_name);
void static_memory_report_leaks(StaticMemoryAnalyzer *analyzer,
                                ArenaAllocator *arena, Token *tokens,
                                int token_count, const char *file_path);
int static_memory_check_and_report(StaticMemoryAnalyzer *analyzer,
                                   ArenaAllocator *arena);
bool static_memory_check_use_after_free(StaticMemoryAnalyzer *analyzer,
                                        const char *var_name, size_t line,
                                        size_t column, ArenaAllocator *arena,
                                        Token *tokens, int token_count,
                                        const char *file_path,
                                        const char *function_name);

StaticMemoryAnalyzer *get_static_analyzer(Scope *scope);
const char *extract_variable_name_from_free(AstNode *free_expr);

void static_memory_track_alias(StaticMemoryAnalyzer *analyzer,
                               const char *new_var, const char *source_var);
void static_memory_invalidate_alias(StaticMemoryAnalyzer *analyzer,
                                    const char *var_name);
bool is_pointer_assignment(AstNode *assignment);

// ============================================================================
// Scope Management
// ============================================================================

void init_scope(Scope *scope, Scope *parent, const char *name,
                ArenaAllocator *arena);
bool scope_add_symbol(Scope *scope, const char *name, AstNode *type,
                      bool is_public, bool is_mutable, ArenaAllocator *arena);
bool scope_add_symbol_with_ownership(Scope *scope, const char *name,
                                     AstNode *type, bool is_public,
                                     bool is_mutable, bool returns_ownership,
                                     bool takes_ownership,
                                     ArenaAllocator *arena);
Symbol *scope_lookup(Scope *scope, const char *name);
Symbol *scope_lookup_current_only(Scope *scope, const char *name);
Symbol *scope_lookup_with_visibility(Scope *scope, const char *name,
                                     Scope *requesting_module_scope);
Symbol *
scope_lookup_current_only_with_visibility(Scope *scope, const char *name,
                                          Scope *requesting_module_scope);
Scope *find_containing_module(Scope *scope);

Scope *create_child_scope(Scope *parent, const char *name,
                          ArenaAllocator *arena);
void debug_print_scope(Scope *scope, int indent_level);
void debug_print_struct_type(AstNode *struct_type, int indent);
void build_dependency_graph(AstNode **modules, size_t module_count,
                            GrowableArray *dep_graph, ArenaAllocator *arena);
bool process_module_in_order(const char *module_name, GrowableArray *dep_graph,
                             AstNode **modules, size_t module_count,
                             Scope *global_scope, ArenaAllocator *arena);
bool typecheck_program_multipass(AstNode *program, Scope *global_scope,
                                 ArenaAllocator *arena);
const char *get_current_function_name(Scope *scope);

// ============================================================================
// Module Management
// ============================================================================

bool register_module(Scope *global_scope, const char *module_name,
                     Scope *module_scope, ArenaAllocator *arena);
Scope *find_module_scope(Scope *global_scope, const char *module_name);
bool add_module_import(Scope *importing_scope, const char *module_name,
                       const char *alias, Scope *module_scope,
                       ArenaAllocator *arena);
Symbol *lookup_qualified_symbol(Scope *scope, const char *module_alias,
                                const char *symbol_name);
Scope *create_module_scope(Scope *global_scope, const char *module_name,
                           ArenaAllocator *arena);

bool typecheck_module_stmt(AstNode *node, Scope *global_scope,
                           ArenaAllocator *arena);
bool typecheck_use_stmt(AstNode *node, Scope *current_scope,
                        Scope *global_scope, ArenaAllocator *arena);

// ============================================================================
// Error Management
// ============================================================================

void tc_error_init(Token *tokens, int token_count, const char *file_path,
                   ArenaAllocator *arena);
void tc_error(AstNode *node, const char *error_type, const char *format, ...);
void tc_error_help(AstNode *node, const char *error_type, const char *help,
                   const char *format, ...);
void tc_error_id(AstNode *node, const char *identifier, const char *error_type,
                 const char *format, ...);

// ============================================================================
// Type Utilities
// ============================================================================

TypeMatchResult types_match(AstNode *type1, AstNode *type2);
bool is_numeric_type(AstNode *type);
bool is_pointer_type(AstNode *type);
bool is_array_type(AstNode *type);
AstNode *get_element_type(AstNode *array_or_pointer_type,
                          ArenaAllocator *arena);
const char *type_to_string(AstNode *type, ArenaAllocator *arena);

// ============================================================================
// Type Checking
// ============================================================================

bool typecheck(AstNode *node, Scope *scope, ArenaAllocator *arena,
               BuildConfig *config);
AstNode *typecheck_expression(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena);
bool typecheck_statement(AstNode *stmt, Scope *scope, ArenaAllocator *arena);
const char *extract_variable_name(AstNode *expr);

// Declarations
bool typecheck_var_decl(AstNode *node, Scope *scope, ArenaAllocator *arena);
bool typecheck_func_decl(AstNode *node, Scope *scope, ArenaAllocator *arena);

AstNode *create_struct_type(ArenaAllocator *arena, const char *name,
                            AstNode **member_types, const char **member_names,
                            size_t member_count, size_t line, size_t column);
AstNode *get_struct_member_type(AstNode *struct_type, const char *member_name);
bool struct_has_member(AstNode *struct_type, const char *member_name);

bool typecheck_struct_decl(AstNode *node, Scope *scope, ArenaAllocator *arena);

bool typecheck_enum_decl(AstNode *node, Scope *scope, ArenaAllocator *arena);
bool typecheck_return_decl(AstNode *node, Scope *scope, ArenaAllocator *arena);
bool typecheck_if_decl(AstNode *node, Scope *scope, ArenaAllocator *arena);
bool typecheck_defer_decl(AstNode *node, Scope *scope, ArenaAllocator *arena);

bool typecheck_switch_stmt(AstNode *node, Scope *scope, ArenaAllocator *arena);
bool typecheck_case_stmt(AstNode *node, Scope *scope, ArenaAllocator *arena,
                         AstNode *expected_type);
bool typecheck_default_stmt(AstNode *node, Scope *scope, ArenaAllocator *arena);

bool typecheck_infinite_loop_decl(AstNode *node, Scope *scope,
                                  ArenaAllocator *arena);
bool typecheck_while_loop_decl(AstNode *node, Scope *scope,
                               ArenaAllocator *arena);
bool typecheck_for_loop_decl(AstNode *node, Scope *scope,
                             ArenaAllocator *arena);
bool typecheck_loop_decl(AstNode *node, Scope *scope, ArenaAllocator *arena);

// Expressions
AstNode *typecheck_binary_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena);
AstNode *typecheck_unary_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena);
AstNode *typecheck_call_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena);
AstNode *typecheck_member_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena);
AstNode *typecheck_index_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena);
AstNode *typecheck_deref_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena);
AstNode *typecheck_addr_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena);
AstNode *typecheck_alloc_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena);
AstNode *typecheck_free_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena);
AstNode *typecheck_memcpy_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena);
AstNode *typecheck_cast_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena);
AstNode *typecheck_input_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena);
AstNode *typecheck_system_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena);
AstNode *typecheck_sizeof_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena);
AstNode *typecheck_assignment_expr(AstNode *expr, Scope *scope,
                                   ArenaAllocator *arena);
AstNode *typecheck_array_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena);
AstNode *typecheck_syscall_expr(AstNode *expr, Scope *scope,
                                ArenaAllocator *arena);

AstNode *typecheck_struct_expr_internal(AstNode *expr, Scope *scope,
                                        ArenaAllocator *arena,
                                        AstNode *expected_type);
AstNode *typecheck_struct_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena);

AstNode *get_enclosing_function_return_type(Scope *scope);

bool validate_array_type(AstNode *array_type, Scope *scope,
                         ArenaAllocator *arena);
bool check_array_bounds(AstNode *array_type, AstNode *index_expr,
                        ArenaAllocator *arena);
AstNode *typecheck_multidim_array_access(AstNode *base_type, AstNode **indices,
                                         size_t index_count,
                                         ArenaAllocator *arena);
TypeMatchResult check_array_compatibility(AstNode *type1, AstNode *type2);
bool validate_array_initializer(AstNode *declared_type, AstNode *initializer,
                                Scope *scope, ArenaAllocator *arena);

// ============================================================================
// Error Handling
// ============================================================================

void print_type_error(TypeError *error);
