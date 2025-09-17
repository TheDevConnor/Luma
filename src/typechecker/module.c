#include "type.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Register a module scope in the global scope
 */
bool register_module(Scope *global_scope, const char *module_name,
                     Scope *module_scope, ArenaAllocator *arena) {
  (void)module_scope;
  size_t prefixed_len = strlen("__module_") + strlen(module_name) + 1;
  char *prefixed_name = arena_alloc(arena, prefixed_len, 1);
  snprintf(prefixed_name, prefixed_len, "__module_%s", module_name);

  AstNode *module_type = create_basic_type(arena, "module", 0, 0);

  return scope_add_symbol(global_scope, prefixed_name, module_type, true, false,
                          arena);
}

/**
 * @brief Find a module scope by name
 */
Scope *find_module_scope(Scope *global_scope, const char *module_name) {
  for (size_t i = 0; i < global_scope->children.count; i++) {
    Scope **child_ptr =
        (Scope **)((char *)global_scope->children.data + i * sizeof(Scope *));
    Scope *child = *child_ptr;

    if (child->is_module_scope &&
        strcmp(child->module_name, module_name) == 0) {
      return child;
    }
  }
  return NULL;
}

/**
 * @brief Add a module import to a scope
 */
bool add_module_import(Scope *importing_scope, const char *module_name,
                       const char *alias, Scope *module_scope,
                       ArenaAllocator *arena) {
  ModuleImport *import =
      (ModuleImport *)growable_array_push(&importing_scope->imported_modules);
  if (!import) {
    return false;
  }

  import->module_name = arena_strdup(arena, module_name);
  import->alias = arena_strdup(arena, alias);
  import->module_scope = module_scope;

  return true;
}

/**
 * @brief Look up a qualified symbol (module_alias.symbol_name) with visibility
 * rules
 */
Symbol *lookup_qualified_symbol(Scope *scope, const char *module_alias,
                                const char *symbol_name) {
  // Search up the scope chain for imports
  Scope *current = scope;
  while (current) {
    for (size_t i = 0; i < current->imported_modules.count; i++) {
      ModuleImport *import =
          (ModuleImport *)((char *)current->imported_modules.data +
                           i * sizeof(ModuleImport));

      if (strcmp(import->alias, module_alias) == 0) {
        // Get the requesting module scope for visibility check
        Scope *requesting_module = find_containing_module(scope);

        // Use visibility-aware lookup - only public symbols can be accessed
        // from other modules
        return scope_lookup_current_only_with_visibility(
            import->module_scope, symbol_name, requesting_module);
      }
    }
    current = current->parent;
  }

  return NULL;
}

/**
 * @brief Create a new module scope
 */
Scope *create_module_scope(Scope *global_scope, const char *module_name,
                           ArenaAllocator *arena) {
  Scope *module_scope = create_child_scope(global_scope, module_name, arena);
  module_scope->is_module_scope = true;
  module_scope->module_name = arena_strdup(arena, module_name);

  growable_array_init(&module_scope->imported_modules, arena, 4,
                      sizeof(ModuleImport));

  return module_scope;
}
