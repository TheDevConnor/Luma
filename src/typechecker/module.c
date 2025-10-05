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
  Scope *current = scope;
  while (current) {
    for (size_t i = 0; i < current->imported_modules.count; i++) {
      ModuleImport *import =
          (ModuleImport *)((char *)current->imported_modules.data +
                           i * sizeof(ModuleImport));

      if (strcmp(import->alias, module_alias) == 0) {
        // Check what symbols exist in the module
        for (size_t j = 0; j < import->module_scope->symbols.count; j++) {
          Symbol *s = (Symbol *)((char *)import->module_scope->symbols.data +
                                 j * sizeof(Symbol));
        }

        Scope *requesting_module = find_containing_module(scope);
        Symbol *result = scope_lookup_current_only_with_visibility(
            import->module_scope, symbol_name, requesting_module);

        return result;
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

void build_dependency_graph(AstNode **modules, size_t module_count,
                            GrowableArray *dep_graph, ArenaAllocator *arena) {
  for (size_t i = 0; i < module_count; i++) {
    AstNode *module = modules[i];
    if (!module || module->type != AST_PREPROCESSOR_MODULE)
      continue;

    const char *module_name = module->preprocessor.module.name;

    ModuleDependency *dep = (ModuleDependency *)growable_array_push(dep_graph);
    dep->module_name = module_name;
    dep->processed = false;
    growable_array_init(&dep->dependencies, arena, 4, sizeof(const char *));

    // Scan for @use statements to find dependencies
    AstNode **body = module->preprocessor.module.body;
    int body_count = module->preprocessor.module.body_count;

    for (int j = 0; j < body_count; j++) {
      if (body[j] && body[j]->type == AST_PREPROCESSOR_USE) {
        const char *imported_module = body[j]->preprocessor.use.module_name;
        const char **dep_slot =
            (const char **)growable_array_push(&dep->dependencies);
        *dep_slot = imported_module;
      }
    }
  }
}

/**
 * @brief Process modules in dependency order (topological sort)
 */
bool process_module_in_order(const char *module_name, GrowableArray *dep_graph,
                             AstNode **modules, size_t module_count,
                             Scope *global_scope, ArenaAllocator *arena) {
  // Find this module's dependency info
  ModuleDependency *current_dep = NULL;
  for (size_t i = 0; i < dep_graph->count; i++) {
    ModuleDependency *dep = (ModuleDependency *)((char *)dep_graph->data +
                                                 i * sizeof(ModuleDependency));
    if (strcmp(dep->module_name, module_name) == 0) {
      current_dep = dep;
      break;
    }
  }

  if (!current_dep)
    return true; // Module not found, skip
  if (current_dep->processed)
    return true; // Already processed

  // First, process all dependencies recursively
  for (size_t i = 0; i < current_dep->dependencies.count; i++) {
    const char **dep_name =
        (const char **)((char *)current_dep->dependencies.data +
                        i * sizeof(const char *));
    if (!process_module_in_order(*dep_name, dep_graph, modules, module_count,
                                 global_scope, arena)) {
      return false;
    }
  }

  AstNode *module = NULL;
  for (size_t i = 0; i < module_count; i++) {
    if (modules[i] && modules[i]->type == AST_PREPROCESSOR_MODULE &&
        strcmp(modules[i]->preprocessor.module.name, module_name) == 0) {
      module = modules[i];
      break;
    }
  }

  if (!module) {
    fprintf(stderr, "Error: Module '%s' not found\n", module_name);
    return false;
  }

  // UPDATE ERROR CONTEXT FOR THIS MODULE - ADD THIS BLOCK
  g_tokens = module->preprocessor.module.tokens;
  g_token_count = module->preprocessor.module.token_count;
  g_file_path = module->preprocessor.module.file_path;

  tc_error_init(g_tokens, g_token_count, g_file_path, arena);

  Scope *module_scope = find_module_scope(global_scope, module_name);
  if (!module_scope) {
    fprintf(stderr, "Error: Module scope not found for '%s'\n", module_name);
    return false;
  }

  AstNode **body = module->preprocessor.module.body;
  int body_count = module->preprocessor.module.body_count;

  // Process all non-@use statements
  for (int j = 0; j < body_count; j++) {
    if (!body[j])
      continue;
    if (body[j]->type == AST_PREPROCESSOR_USE)
      continue;

    if (!typecheck(body[j], module_scope, arena, global_scope->config)) {
      tc_error(body[j], "Module Error",
               "Failed to typecheck statement in module '%s'", module_name);
      return false;
    }
  }

  StaticMemoryAnalyzer *analyzer = get_static_analyzer(module_scope);
  if (analyzer && g_tokens && g_token_count > 0 && g_file_path && global_scope->config->check_mem) {
    static_memory_check_and_report(analyzer, arena);
  }

  current_dep->processed = true;
  return true;
}
