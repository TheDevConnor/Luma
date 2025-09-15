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
#include "../lexer/lexer.h"

// ============================================================================
// Data Structures
// ============================================================================

static Token *g_tokens = NULL;
static int g_token_count = 0;
static const char *g_file_path = NULL;
static ArenaAllocator *g_arena = NULL;

typedef struct {
  size_t line;
  size_t column;
  const char *variable_name; // Variable that holds the allocated pointer
  bool has_matching_free;
  int free_count; // NEW: Track how many times freed
} StaticAllocation;

typedef struct {
  GrowableArray allocations; // Array of StaticAllocation
  ArenaAllocator *arena;
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
} Symbol;

/**
 * @brief Represents a lexical scope with hierarchical relationships.
 */
typedef struct Scope {
  struct Scope *parent;   /**< Parent scope */
  GrowableArray symbols;  /**< Array of Symbol entries */
  GrowableArray children; /**< Array of child scopes */
  const char *scope_name; /**< Debugging name (function, block, etc.) */
  size_t depth;           /**< Nesting depth from global scope */
  bool is_function_scope;
  AstNode *associated_node; /**< AST node that created this scope */

  // Module-related metadata
  bool is_module_scope;
  const char *module_name;
  GrowableArray imported_modules;

  // Memory tracking
  StaticMemoryAnalyzer *memory_analyzer;
} Scope;

/**
 * @brief Represents an imported module with optional aliasing.
 */
typedef struct {
  const char *module_name; /**< Original module name */
  const char *alias;       /**< Alias in importing module */
  Scope *module_scope;
} ModuleImport;

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
                               size_t column, const char *var_name);
void static_memory_track_free(StaticMemoryAnalyzer *analyzer,
                              const char *var_name);
void static_memory_report_leaks(StaticMemoryAnalyzer *analyzer,
                                ArenaAllocator *arena, Token *tokens,
                                int token_count, const char *file_path);
int static_memory_check_and_report(StaticMemoryAnalyzer *analyzer,
                                   ArenaAllocator *arena, Token *tokens,
                                   int token_count, const char *file_path);
StaticMemoryAnalyzer *get_static_analyzer(Scope *scope);
const char *extract_variable_name_from_free(AstNode *free_expr);

// ============================================================================
// Scope Management
// ============================================================================

void init_scope(Scope *scope, Scope *parent, const char *name,
                ArenaAllocator *arena);
bool scope_add_symbol(Scope *scope, const char *name, AstNode *type,
                      bool is_public, bool is_mutable, ArenaAllocator *arena);
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

bool typecheck(AstNode *node, Scope *scope, ArenaAllocator *arena);
AstNode *typecheck_expression(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena);
bool typecheck_statement(AstNode *stmt, Scope *scope, ArenaAllocator *arena);

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
AstNode *typecheck_sizeof_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena);

AstNode *get_enclosing_function_return_type(Scope *scope);

// ============================================================================
// Error Handling
// ============================================================================

void print_type_error(TypeError *error);
