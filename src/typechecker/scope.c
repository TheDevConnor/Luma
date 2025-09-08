/**
 * @file scope.c
 * @brief Implementation of scope and symbol table management functions
 *
 * This file contains the implementation of the hierarchical scope system for
 * symbol table management. It provides functionality for creating, managing,
 * and searching through nested scopes with proper parent-child relationships
 * and symbol resolution.
 *
 * The scope system supports:
 * - Hierarchical scope creation with automatic depth tracking
 * - Symbol addition with duplicate detection
 * - Multi-level symbol lookup with parent traversal
 * - Automatic parent-child relationship management
 */

#include <stdio.h>
#include <string.h>

#include "../c_libs/memory/memory.h"
#include "type.h"

/**
 * @brief Initialize a scope structure with enhanced metadata
 *
 * Sets up a new scope with proper initialization of all fields, including
 * parent-child relationships, depth calculation, and growable arrays for
 * symbols and children. The scope is fully prepared for symbol addition
 * and child scope creation.
 *
 * @param scope Pointer to the scope structure to initialize (must be
 * pre-allocated)
 * @param parent Parent scope for hierarchical lookup (NULL for global scope)
 * @param name Descriptive name for debugging and identification (NULL becomes
 * "unnamed")
 * @param arena Arena allocator for all memory allocations within this scope
 *
 * @details
 * Initialization includes:
 * - Parent pointer assignment and depth calculation
 * - Name duplication into arena memory for persistence
 * - Default flag initialization (not a function scope, no associated AST node)
 * - Growable array setup with reasonable initial capacities (16 symbols, 8
 * children)
 *
 * @note The scope name is duplicated using arena allocation, ensuring it
 * remains valid for the lifetime of the scope without external string
 * management
 *
 * @warning The scope parameter must point to valid, allocated memory. This
 * function does not allocate the scope structure itself, only initializes it.
 */
// Updated init_scope function in scope.c
void init_scope(Scope *scope, Scope *parent, const char *name,
                ArenaAllocator *arena) {
  // Set up parent-child relationship
  scope->parent = parent;
  scope->scope_name = name ? arena_strdup(arena, name) : "unnamed";
  scope->depth = parent ? parent->depth + 1 : 0;

  // Initialize scope type flags
  scope->is_function_scope = false;
  scope->is_module_scope = false;
  scope->associated_node = NULL;
  scope->module_name = NULL;

  // Initialize memory tracker (only for global scope)
  if (!parent) {
    scope->memory_analyzer = arena_alloc(arena, sizeof(StaticMemoryAnalyzer),
                                         alignof(StaticMemoryAnalyzer));
    static_memory_analyzer_init(scope->memory_analyzer, arena);
  } else {
    scope->memory_analyzer = parent->memory_analyzer;
  }

  // Initialize growable arrays
  growable_array_init(&scope->symbols, arena, 16, sizeof(Symbol));
  growable_array_init(&scope->children, arena, 8, sizeof(Scope *));
  growable_array_init(&scope->imported_modules, arena, 4, sizeof(ModuleImport));
}
/**
 * @brief Add a symbol to the specified scope with duplicate checking
 *
 * Creates a new symbol entry in the given scope after verifying that no
 * symbol with the same name already exists in the current scope. The symbol
 * is fully initialized with the provided type information and attributes.
 *
 * @param scope Target scope to add the symbol to
 * @param name Symbol name identifier (will be duplicated into arena memory)
 * @param type AST node representing the symbol's type
 * @param is_public Public accessibility flag for the symbol
 * @param is_mutable Mutability flag indicating if the symbol can be modified
 * @param arena Arena allocator for memory management
 * @return true if symbol was added successfully, false on error or duplicate
 * name
 *
 * @details
 * Error conditions that return false:
 * - Symbol name already exists in the current scope (not parent scopes)
 * - Memory allocation failure during symbol array expansion
 *
 * Success conditions:
 * - Symbol name is unique within the current scope
 * - All memory allocations succeed
 * - Symbol is fully initialized and added to the scope's symbol array
 *
 * @note This function only checks for duplicates within the current scope,
 *       allowing for symbol shadowing of parent scope symbols
 *
 * @warning The symbol name is duplicated into arena memory. The original name
 *          string can be freed or modified after this call without affecting
 *          the stored symbol.
 */
bool scope_add_symbol(Scope *scope, const char *name, AstNode *type,
                      bool is_public, bool is_mutable, ArenaAllocator *arena) {
  // Check for duplicate symbols in current scope only (shadowing is allowed)
  Symbol *existing = scope_lookup_current_only(scope, name);
  if (existing) {
    fprintf(stderr, "Error: Symbol '%s' already declared in current scope\n",
            name);
    return false;
  }

  // Attempt to expand the symbols array and get a pointer to the new slot
  Symbol *s = (Symbol *)growable_array_push(&scope->symbols);
  if (!s) {
    fprintf(stderr, "Out of memory while adding symbol '%s'\n", name);
    return false;
  }

  // Initialize the new symbol with all provided information
  s->name = arena_strdup(arena, name); // Duplicate name into arena memory
  s->type = type;                      // Store type AST node reference
  s->is_public = is_public;            // Set accessibility flag
  s->is_mutable = is_mutable;          // Set mutability flag
  s->scope_depth = scope->depth;       // Record the scope depth for debugging

  return true;
}

Symbol *scope_lookup_with_visibility(Scope *scope, const char *name,
                                     Scope *requesting_module_scope) {
  Scope *current = scope;

  while (current) {
    // Linear search through current scope's symbols
    for (size_t i = 0; i < current->symbols.count; ++i) {
      Symbol *s =
          (Symbol *)((char *)current->symbols.data + i * sizeof(Symbol));
      if (strcmp(s->name, name) == 0) {

        // Check visibility rules
        if (s->is_public) {
          return s; // Public symbols are always accessible
        }

        // Private symbols: check if we're in the same module
        Scope *symbol_module = find_containing_module(current);
        Scope *requesting_module = requesting_module_scope
                                       ? requesting_module_scope
                                       : find_containing_module(scope);

        if (symbol_module == requesting_module) {
          return s; // Same module - private symbol is accessible
        }

        // Different module and symbol is private - not accessible
        return NULL;
      }
    }
    current = current->parent;
  }

  return NULL;
}

/**
 * @brief Find the containing module scope for a given scope
 */
Scope *find_containing_module(Scope *scope) {
  Scope *current = scope;
  while (current) {
    if (current->is_module_scope) {
      return current;
    }
    current = current->parent;
  }
  return NULL; // Global scope or not in a module
}

/**
 * @brief Original scope_lookup - now wraps the visibility version
 */
Symbol *scope_lookup(Scope *scope, const char *name) {
  return scope_lookup_with_visibility(scope, name, NULL);
}

/**
 * @brief Look up a symbol only in the current scope with visibility rules
 */
Symbol *
scope_lookup_current_only_with_visibility(Scope *scope, const char *name,
                                          Scope *requesting_module_scope) {
  // Linear search through current scope's symbols only
  for (size_t i = 0; i < scope->symbols.count; ++i) {
    Symbol *s = (Symbol *)((char *)scope->symbols.data + i * sizeof(Symbol));
    if (strcmp(s->name, name) == 0) {

      // Check visibility rules
      if (s->is_public) {
        return s; // Public symbols are always accessible
      }

      // Private symbols: check if we're in the same module
      Scope *symbol_module = find_containing_module(scope);
      Scope *requesting_module = requesting_module_scope
                                     ? requesting_module_scope
                                     : find_containing_module(scope);

      if (symbol_module == requesting_module) {
        return s; // Same module - private symbol is accessible
      }

      // Different module and symbol is private - not accessible
      return NULL;
    }
  }

  return NULL;
}

/**
 * @brief Original scope_lookup_current_only - now wraps the visibility version
 */
Symbol *scope_lookup_current_only(Scope *scope, const char *name) {
  return scope_lookup_current_only_with_visibility(scope, name, NULL);
}

/**
 * @brief Create a new child scope under the specified parent
 *
 * Allocates and initializes a new scope as a child of the given parent,
 * automatically establishing the parent-child relationship and adding
 * the new scope to the parent's children list.
 *
 * @param parent Parent scope for the new child (NULL for root scope)
 * @param name Descriptive name for the new scope
 * @param arena Arena allocator for memory management
 * @return Pointer to the newly created and initialized child scope
 *
 * @details
 * Creation process:
 * 1. Allocate memory for new Scope structure from arena
 * 2. Initialize the scope with proper parent linkage
 * 3. Add the new scope to parent's children array (if parent exists)
 * 4. Return pointer to fully initialized child scope
 *
 * @note The child scope is automatically added to the parent's children list,
 *       maintaining the bidirectional parent-child relationship for debugging
 *       and traversal purposes
 *
 * @warning If memory allocation fails, this function may return NULL or
 *          an incompletely initialized scope. The caller should verify
 *          successful creation before using the returned scope.
 *
 * @warning If adding the child to the parent's children array fails due to
 *          memory constraints, the child scope will still be created and
 *          functional, but won't appear in the parent's children list.
 */
Scope *create_child_scope(Scope *parent, const char *name,
                          ArenaAllocator *arena) {
  // Allocate memory for new scope structure from arena
  Scope *child = arena_alloc(arena, sizeof(Scope), alignof(Scope));

  // Initialize the child scope with proper parent linkage
  init_scope(child, parent, name, arena);

  // Add child to parent's children list for bidirectional relationship
  if (parent) {
    // Get a slot in the parent's children array
    Scope **child_ptr = (Scope **)growable_array_push(&parent->children);
    if (child_ptr) {
      *child_ptr = child; // Store pointer to child scope
    }
    // Note: If growable_array_push fails, the child is still valid
    // but won't appear in the parent's children list
  }

  return child;
}
