#include <stdio.h>
#include <string.h>

#include "type.h"

bool contains_alloc_expression(AstNode *expr) {
  if (!expr)
    return false;

  switch (expr->type) {
  case AST_EXPR_ALLOC:
    return true;
  case AST_EXPR_CAST:
    return contains_alloc_expression(expr->expr.cast.castee);
  case AST_EXPR_BINARY:
    return contains_alloc_expression(expr->expr.binary.left) ||
           contains_alloc_expression(expr->expr.binary.right);
  // Add other expression types as needed
  default:
    return false;
  }
}

bool typecheck_var_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
    const char *name = node->stmt.var_decl.name;
    AstNode *declared_type = node->stmt.var_decl.var_type;
    AstNode *initializer = node->stmt.var_decl.initializer;
    bool is_public = node->stmt.var_decl.is_public;
    bool is_mutable = node->stmt.var_decl.is_mutable;

    // Track memory allocation
    if (initializer && contains_alloc_expression(initializer)) {
        StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
        if (analyzer) {
            static_memory_track_alloc(analyzer, node->line, node->column, name);
        }
    }

    if (initializer) {
        AstNode *init_type = typecheck_expression(initializer, scope, arena);
        if (!init_type) {
            tc_error(node, "Type Error", "Cannot determine type of initializer for variable '%s'", name);
            return false;
        }

        if (declared_type) {
            TypeMatchResult match = types_match(declared_type, init_type);
            if (match == TYPE_MATCH_NONE) {
                tc_error_help(node, "Type Mismatch", 
                             "Check that the initializer type matches the declared type",
                             "Cannot assign '%s' to variable '%s' of type '%s'",
                             type_to_string(init_type, arena), name,
                             type_to_string(declared_type, arena));
                return false;
            }
        } else {
            declared_type = init_type;
        }
    }

    if (!declared_type) {
        tc_error_help(node, "Missing Type", 
                     "Provide either a type annotation or initializer",
                     "Variable '%s' has no type information", name);
        return false;
    }

    if (!scope_add_symbol(scope, name, declared_type, is_public, is_mutable, arena)) {
        tc_error_id(node, name, "Duplicate Symbol", 
                   "Variable '%s' is already declared in this scope", name);
        return false;
    }

    return true;
}

// Stub implementations for remaining functions
bool typecheck_func_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  const char *name = node->stmt.func_decl.name;
  AstNode *return_type = node->stmt.func_decl.return_type;
  AstNode **param_types = node->stmt.func_decl.param_types;
  char **param_names = node->stmt.func_decl.param_names;
  size_t param_count = node->stmt.func_decl.param_count;
  AstNode *body = node->stmt.func_decl.body;
  bool is_public = node->stmt.func_decl.is_public;

  // Validate return type
  if (!return_type || return_type->category != Node_Category_TYPE) {
    tc_error(node, "Function Error", "Function '%s' has invalid return type", name);
    return false;
  }

  // Main function validation
  if (strcmp(name, "main") == 0) {
    if (strcmp(return_type->type_data.basic.name, "int") != 0) {
      tc_error_help(node, "Main Return Type",
        "The main function must return 'int'",
        "Function '%s' must return 'int' but got '%s'",
        name, type_to_string(return_type, arena));
      return false;
    }

    // Ensure main is public
    if (!is_public) {
      tc_error_help(node, "Main Visibility",
        "The main function should be public",
        "Function 'main' should be public; automatically making it public");
      node->stmt.func_decl.is_public = true;
      is_public = true;
    }
  }

  // Validate parameters
  for (size_t i = 0; i < param_count; i++) {
    if (!param_names[i] || !param_types[i] ||
        param_types[i]->category != Node_Category_TYPE) {
      tc_error(node, "Function Parameter Error",
        "Function '%s' has invalid parameter %zu", name, i);
      return false;
    }
  }

  // Create function type
  AstNode *func_type = create_function_type(
      arena, param_types, param_count, return_type, node->line, node->column);

  // Add function to current scope with proper visibility
  if (!scope_add_symbol(scope, name, func_type, is_public, false, arena)) {
    tc_error_id(node, name, "Duplicate Symbol",
      "Function '%s' is already declared in this scope", name);
    return false;
  }

  // Create function scope for parameters and body
  Scope *func_scope = create_child_scope(scope, name, arena);
  func_scope->is_function_scope = true;
  func_scope->associated_node = node;

  // Add parameters to function scope (parameters are always local)
  for (size_t i = 0; i < param_count; i++) {
    if (!scope_add_symbol(func_scope, param_names[i], param_types[i], false,
                          true, arena)) {
      tc_error(node, "Function Parameter Error",
        "Could not add parameter '%s' to function '%s' scope", param_names[i], name);
      return false;
    }
  }

  // Typecheck function body
  if (body) {
    if (!typecheck_statement(body, func_scope, arena)) {
      tc_error(node, "Function Body Error",
        "Function '%s' body failed typechecking", name);
      return false;
    }
  }

  return true;
}

bool typecheck_struct_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  // TODO: Implement struct declaration typechecking
  const char *name = node->stmt.struct_decl.name;
  return scope_add_symbol(scope, name, create_basic_type(arena, "struct", 0, 0),
                          node->stmt.struct_decl.is_public, false, arena);
}

bool typecheck_enum_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  const char *enum_name = node->stmt.enum_decl.name;
  char **member_names = node->stmt.enum_decl.members;
  size_t member_count = node->stmt.enum_decl.member_count;
  bool is_public = node->stmt.enum_decl.is_public;

  // Add enum type with proper visibility
  AstNode *int_type = create_basic_type(arena, "int", node->line, node->column);
  if (!scope_add_symbol(scope, enum_name, int_type, is_public, false, arena)) {
    tc_error_id(node, enum_name, "Duplicate Symbol",
      "Enum '%s' is already declared in this scope", enum_name);
    return false;
  }

  // Add enum members - they inherit the enum's visibility
  for (size_t i = 0; i < member_count; i++) {
    size_t qualified_len = strlen(enum_name) + strlen(member_names[i]) + 2;
    char *qualified_name = arena_alloc(arena, qualified_len, 1);
    snprintf(qualified_name, qualified_len, "%s.%s", enum_name,
             member_names[i]);

    // Enum members have same visibility as the enum itself
    if (!scope_add_symbol(scope, qualified_name, int_type, is_public, false,
                          arena)) {
      tc_error(node, "Enum Member Error",
        "Could not add enum member '%s'", qualified_name);
      return false;
    }
  }

  return true;
}

bool typecheck_return_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  // Find the enclosing function's return type
  AstNode *expected_return_type = get_enclosing_function_return_type(scope);
  if (!expected_return_type) {
    tc_error(node, "Return Error", "Return statement outside of function");
    return false;
  }

  AstNode *return_value = node->stmt.return_stmt.value;

  // Check if function expects void
  bool expects_void =
      (expected_return_type->type == AST_TYPE_BASIC &&
       strcmp(expected_return_type->type_data.basic.name, "void") == 0);

  if (expects_void && return_value != NULL) {
    tc_error(node, "Return Error", "Void function cannot return a value");
    return false;
  }

  if (!expects_void) {
    if (!return_value) {
      tc_error(node, "Return Error", "Non-void function must return a value");
      return false;
    }

    // Typecheck with current scope where x is visible
    AstNode *actual_return_type =
        typecheck_expression(return_value, scope, arena);
    if (!actual_return_type)
      return false;

    TypeMatchResult match =
        types_match(expected_return_type, actual_return_type);
    if (match == TYPE_MATCH_NONE) {
      tc_error_help(node, "Return Type Mismatch",
        "Check that the returned value matches the function's return type",
        "Return type mismatch: expected '%s', got '%s'",
        type_to_string(expected_return_type, arena),
        type_to_string(actual_return_type, arena));
      return false;
    }
  }

  return true;
}

bool typecheck_if_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  Scope *then_branch = create_child_scope(scope, "then_branch", arena);
  Scope *else_branch = create_child_scope(scope, "else_branch", arena);

  Type *expected =
      create_basic_type(arena, "bool", node->stmt.if_stmt.condition->line,
                        node->stmt.if_stmt.condition->column);
  Type *user = typecheck_expression(node->stmt.if_stmt.condition, scope, arena);
  TypeMatchResult condition = types_match(expected, user);
  if (condition == TYPE_MATCH_NONE) {
    tc_error_help(node, "If Condition Error",
      "The condition of an if statement must be of type 'bool'",
      "If condition expected to be of type 'bool', but got '%s' instead",
      type_to_string(user, arena));
    return false;
  }

  if (node->stmt.if_stmt.then_stmt != NULL) {
    typecheck_statement(node->stmt.if_stmt.then_stmt, then_branch, arena);
  }

  for (int i = 0; i < node->stmt.if_stmt.elif_count; i++) {
    if (node->stmt.if_stmt.elif_stmts[i] != NULL) {
      typecheck_statement(node->stmt.if_stmt.elif_stmts[i], then_branch, arena);
    }
  }

  if (node->stmt.if_stmt.else_stmt != NULL) {
    typecheck_statement(node->stmt.if_stmt.else_stmt, else_branch, arena);
  }

  return true;
}

bool typecheck_module_stmt(AstNode *node, Scope *global_scope,
                           ArenaAllocator *arena) {
  if (node->type != AST_PREPROCESSOR_MODULE) {
    tc_error(node, "Module Error", "Expected module statement");
    return false;
  }

  const char *module_name = node->preprocessor.module.name;
  AstNode **body = node->preprocessor.module.body;
  int body_count = node->preprocessor.module.body_count;

  // Create module scope if it doesn't exist
  Scope *module_scope = find_module_scope(global_scope, module_name);
  if (!module_scope) {
    module_scope = create_module_scope(global_scope, module_name, arena);
    if (!register_module(global_scope, module_name, module_scope, arena)) {
      tc_error(node, "Module Error", "Failed to register module '%s'", module_name);
      return false;
    }
  }

  // First pass: Process all use statements to establish imports
  for (int i = 0; i < body_count; i++) {
    if (!body[i])
      continue;

    if (body[i]->type == AST_PREPROCESSOR_USE) {
      if (!typecheck_use_stmt(body[i], module_scope, global_scope, arena)) {
        tc_error(node, "Module Use Error",
          "Failed to process use statement in module '%s'", module_name);
        return false;
      }
    }
  }

  // Second pass: Process all non-use statements
  for (int i = 0; i < body_count; i++) {
    if (!body[i])
      continue;

    if (body[i]->type != AST_PREPROCESSOR_USE) {
      if (!typecheck(body[i], module_scope, arena)) {
        tc_error(node, "Module Error",
          "Failed to typecheck statement in module '%s'", module_name);
        return false;
      }
    }
  }

  return true;
}

bool typecheck_use_stmt(AstNode *node, Scope *current_scope,
                        Scope *global_scope, ArenaAllocator *arena) {
  if (node->type != AST_PREPROCESSOR_USE) {
    tc_error(node, "Use Error", "Expected use statement");
    return false;
  }

  const char *module_name = node->preprocessor.use.module_name;
  const char *alias = node->preprocessor.use.alias;

  // Find the module scope
  Scope *module_scope = find_module_scope(global_scope, module_name);
  if (!module_scope) {
    tc_error(node, "Use Error", "Module '%s' not found", module_name);
    return false;
  }

  // Add the import to the current scope
  if (!add_module_import(current_scope, module_name, alias, module_scope,
                         arena)) {
    tc_error(node, "Use Error", "Failed to import module '%s' as '%s'", module_name, alias);
    return false;
  }

  return true;
}

bool typecheck_infinite_loop_decl(AstNode *node, Scope *scope,
                                  ArenaAllocator *arena) {
  Scope *loop_scope = create_child_scope(scope, "infinite_loop", arena);

  if (node->stmt.loop_stmt.body == NULL) {
    tc_error(node, "Loop Error", "Loop body cannot be null");
    return false;
  }

  if (!typecheck_statement(node->stmt.loop_stmt.body, loop_scope, arena)) {
    tc_error(node, "Loop Error", "Loop body failed typechecking");
    return false;
  }

  return true;
}
bool typecheck_while_loop_decl(AstNode *node, Scope *scope,
                               ArenaAllocator *arena) {
  Scope *while_loop = create_child_scope(scope, "while_loop", arena);
  return true;
}
bool typecheck_for_loop_decl(AstNode *node, Scope *scope,
                             ArenaAllocator *arena) {
  Scope *lookup_scope = create_child_scope(scope, "for_loop", arena);
  return true;
}

bool typecheck_loop_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  // check what type of loop it is
  if (node->stmt.loop_stmt.condition == NULL &&
      node->stmt.loop_stmt.initializer == NULL)
    return typecheck_infinite_loop_decl(node, scope, arena);
  else if (node->stmt.loop_stmt.condition != NULL &&
           node->stmt.loop_stmt.initializer == NULL)
    return typecheck_while_loop_decl(node, scope, arena);
  else
    return typecheck_for_loop_decl(node, scope, arena);
}

// NOTE: I will be back in a bit going to go get dinner.

