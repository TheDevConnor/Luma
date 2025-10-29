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
  case AST_EXPR_INDEX:
    return contains_alloc_expression(expr->expr.index.object) ||
           contains_alloc_expression(expr->expr.index.index);
  case AST_EXPR_MEMBER:
    return contains_alloc_expression(expr->expr.member.object);
  case AST_EXPR_DEREF:
    return contains_alloc_expression(expr->expr.deref.object);
  case AST_EXPR_ADDR:
    return contains_alloc_expression(expr->expr.addr.object);
  case AST_EXPR_UNARY:
    return contains_alloc_expression(expr->expr.unary.operand);
  case AST_EXPR_CALL: {
    for (size_t i = 0; i < expr->expr.call.arg_count; i++) {
      if (contains_alloc_expression(expr->expr.call.args[i])) {
        return true;
      }
    }
    return contains_alloc_expression(expr->expr.call.callee);
  }
  case AST_EXPR_GROUPING:
    return contains_alloc_expression(expr->expr.grouping.expr);
  case AST_EXPR_ASSIGNMENT:
    return contains_alloc_expression(expr->expr.assignment.target) ||
           contains_alloc_expression(expr->expr.assignment.value);
  case AST_EXPR_ARRAY: {
    for (size_t i = 0; i < expr->expr.array.element_count; i++) {
      if (contains_alloc_expression(expr->expr.array.elements[i])) {
        return true;
      }
    }
    return false;
  }
  default:
    return false;
  }
}

const char *extract_variable_name(AstNode *expr) {
  if (!expr)
    return NULL;

  switch (expr->type) {
  case AST_EXPR_IDENTIFIER:
    return expr->expr.identifier.name;
  case AST_EXPR_DEREF:
    return extract_variable_name(expr->expr.deref.object);
  case AST_EXPR_ADDR:
    return extract_variable_name(expr->expr.addr.object);
  case AST_EXPR_MEMBER:
    return extract_variable_name(expr->expr.member.object);
  case AST_EXPR_INDEX:
    return extract_variable_name(expr->expr.index.object);
  case AST_EXPR_CAST:
    return extract_variable_name(expr->expr.cast.castee);
  default:
    return NULL;
  }
}

bool is_pointer_assignment(AstNode *assignment) {
  if (!assignment || assignment->type != AST_EXPR_ASSIGNMENT) {
    return false;
  }

  // Simple heuristic: check if we're assigning between identifiers
  AstNode *target = assignment->expr.assignment.target;
  AstNode *value = assignment->expr.assignment.value;

  return (target && target->type == AST_EXPR_IDENTIFIER && value &&
          value->type == AST_EXPR_IDENTIFIER);
}

bool typecheck_var_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  const char *name = node->stmt.var_decl.name;
  AstNode *declared_type = node->stmt.var_decl.var_type;
  AstNode *initializer = node->stmt.var_decl.initializer;
  bool is_public = node->stmt.var_decl.is_public;
  bool is_mutable = node->stmt.var_decl.is_mutable;

  // Track memory allocation
  // Track memory allocation
  if (initializer && contains_alloc_expression(initializer)) {
    StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
    if (analyzer) {
      // Get the containing function name
      const char *func_name = NULL;
      Scope *func_scope = scope;
      while (func_scope && !func_scope->is_function_scope) {
        func_scope = func_scope->parent;
      }
      if (func_scope && func_scope->associated_node) {
        func_name = func_scope->associated_node->stmt.func_decl.name;

        // NEW: Check if this function returns ownership
        bool returns_ownership =
            func_scope->associated_node->stmt.func_decl.returns_ownership;

        // If the function returns ownership, we assume any allocation
        // will be returned and ownership transferred to the caller
        // So we DON'T track it as a potential leak within this function
        if (returns_ownership) {
          // Skip tracking - ownership will be transferred to caller
          goto skip_allocation_tracking;
        }
      }

      static_memory_track_alloc(analyzer, node->line, node->column, name,
                                func_name, g_tokens, g_token_count,
                                g_file_path);

    skip_allocation_tracking:;
    }
  }
  // Track pointer aliasing in variable initialization
  else if (initializer && declared_type && is_pointer_type(declared_type)) {
    const char *source_var = extract_variable_name(initializer);
    if (source_var) {
      StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
      if (analyzer) {
        static_memory_track_alias(analyzer, name, source_var);
      }
    }
  }

  // Track ownership transfer from function return values
  if (initializer && initializer->type == AST_EXPR_CALL) {
    AstNode *callee = initializer->expr.call.callee;
    Symbol *func_symbol = NULL;

    // Look up the function symbol
    if (callee->type == AST_EXPR_IDENTIFIER) {
      func_symbol = scope_lookup(scope, callee->expr.identifier.name);
    } else if (callee->type == AST_EXPR_MEMBER) {
      const char *base_name = callee->expr.member.object->expr.identifier.name;
      const char *member_name = callee->expr.member.member;
      if (callee->expr.member.is_compiletime) {
        func_symbol = lookup_qualified_symbol(scope, base_name, member_name);
      }
    }

    // If function returns ownership, track as allocation
    if (func_symbol && func_symbol->returns_ownership) {
      StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
      if (analyzer) {
        const char *func_name = get_current_function_name(scope);
        static_memory_track_alloc(analyzer, node->line, node->column, name,
                                  func_name, g_tokens, g_token_count,
                                  g_file_path);
      }
    }
  }

  if (initializer) {
    AstNode *init_type = NULL;

    // SPECIAL HANDLING: If initializer is an anonymous struct expression
    // and we have a declared type, pass it for validation
    if (initializer->type == AST_EXPR_STRUCT &&
        !initializer->expr.struct_expr.name && declared_type) {
      // Call the internal version with expected type for anonymous structs
      init_type = typecheck_struct_expr_internal(initializer, scope, arena,
                                                 declared_type);
    } else {
      init_type = typecheck_expression(initializer, scope, arena);
    }

    if (!init_type) {
      tc_error(node, "Type Error",
               "Cannot determine type of initializer for variable '%s'", name);
      return false;
    }

    if (declared_type) {
      TypeMatchResult match = types_match(declared_type, init_type);
      if (match == TYPE_MATCH_NONE) {
        // Provide specific error messages based on type categories
        if (declared_type && declared_type->type == AST_TYPE_ARRAY) {
          if (!validate_array_type(declared_type, scope, arena)) {
            return false;
          }

          if (initializer && !validate_array_initializer(
                                 declared_type, initializer, scope, arena)) {
            return false;
          }
        } else {
          // General type mismatch
          tc_error_help(
              node, "Type Mismatch",
              "Check that the initializer type matches the declared type",
              "Cannot assign '%s' to variable '%s' of type '%s'",
              type_to_string(init_type, arena), name,
              type_to_string(declared_type, arena));
        }
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

  if (!scope_add_symbol(scope, name, declared_type, is_public, is_mutable,
                        arena)) {
    tc_error_id(node, name, "Duplicate Symbol",
                "Variable '%s' is already declared in this scope", name);
    return false;
  }

  return true;
}

bool typecheck_func_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  const char *name = node->stmt.func_decl.name;
  AstNode *return_type = node->stmt.func_decl.return_type;
  AstNode **param_types = node->stmt.func_decl.param_types;
  char **param_names = node->stmt.func_decl.param_names;
  size_t param_count = node->stmt.func_decl.param_count;
  AstNode *body = node->stmt.func_decl.body;
  bool is_public = node->stmt.func_decl.is_public;
  bool returns_ownership = node->stmt.func_decl.returns_ownership;
  bool takes_ownership = node->stmt.func_decl.takes_ownership;

  // Validate return type
  if (!return_type || return_type->category != Node_Category_TYPE) {
    tc_error(node, "Function Error", "Function '%s' has invalid return type",
             name);
    return false;
  }

  // Main function validation
  if (strcmp(name, "main") == 0) {
    if (strcmp(return_type->type_data.basic.name, "int") != 0) {
      tc_error_help(node, "Main Return Type",
                    "The main function must return 'int'",
                    "Function '%s' must return 'int' but got '%s'", name,
                    type_to_string(return_type, arena));
      return false;
    }

    // Ensure main is public
    if (!is_public) {
      tc_error_help(
          node, "Main Visibility", "The main function should be public",
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

  // Add function to scope WITH ownership flags
  if (!scope_add_symbol_with_ownership(scope, name, func_type, is_public, false,
                                       returns_ownership, takes_ownership,
                                       arena)) {
    tc_error_id(node, name, "Duplicate Symbol",
                "Function '%s' is already declared in this scope", name);
    return false;
  }

  // Create function scope for parameters and body
  Scope *func_scope = create_child_scope(scope, name, arena);
  func_scope->is_function_scope = true;
  func_scope->associated_node = node;

  node->stmt.func_decl.scope = (void *)func_scope;

  // Add parameters to function scope (parameters are always local)
  for (size_t i = 0; i < param_count; i++) {
    if (!scope_add_symbol(func_scope, param_names[i], param_types[i], false,
                          true, arena)) {
      tc_error(node, "Function Parameter Error",
               "Could not add parameter '%s' to function '%s' scope",
               param_names[i], name);
      return false;
    }

    // If this function takes ownership of a pointer parameter, track it as an
    // allocation
    if (takes_ownership && is_pointer_type(param_types[i])) {
      StaticMemoryAnalyzer *analyzer = get_static_analyzer(func_scope);
      if (analyzer) {
        static_memory_track_alloc(analyzer, node->line, node->column,
                                  param_names[i], name, g_tokens, g_token_count,
                                  g_file_path);
      }
    }
  }

  // Typecheck function body
  if (body) {
    if (!typecheck_statement(body, func_scope, arena)) {
      tc_error(node, "Function Body Error",
               "Function '%s' body failed typechecking", name);
      return false;
    }

    // Process deferred frees - these represent cleanup at function exit
    StaticMemoryAnalyzer *analyzer = get_static_analyzer(func_scope);
    if (analyzer && func_scope->deferred_frees.count > 0) {
      for (size_t i = 0; i < func_scope->deferred_frees.count; i++) {
        const char **var_ptr =
            (const char **)((char *)func_scope->deferred_frees.data +
                            i * sizeof(const char *));
        if (*var_ptr) {
          // Mark as freed at function exit
          static_memory_track_free(analyzer, *var_ptr, name);
        }
      }
    }
  }

  return true;
}

bool typecheck_struct_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  if (node->type != AST_STMT_STRUCT) {
    tc_error(node, "Internal Error", "Expected struct declaration node");
    return false;
  }

  const char *struct_name = node->stmt.struct_decl.name;
  AstNode **public_members = node->stmt.struct_decl.public_members;
  size_t public_count = node->stmt.struct_decl.public_count;
  AstNode **private_members = node->stmt.struct_decl.private_members;
  size_t private_count = node->stmt.struct_decl.private_count;
  bool is_public = node->stmt.struct_decl.is_public;

  // Validate struct name
  if (!struct_name) {
    tc_error(node, "Struct Error", "Struct declaration missing name");
    return false;
  }

  // Check for duplicate struct declaration
  Symbol *existing = scope_lookup_current_only(scope, struct_name);
  if (existing) {
    tc_error_id(node, struct_name, "Duplicate Symbol",
                "Struct '%s' is already declared in this scope", struct_name);
    return false;
  }

  // Collect all member info for struct type creation
  size_t total_members = public_count + private_count;
  AstNode **all_member_types =
      arena_alloc(arena, total_members * sizeof(AstNode *), alignof(AstNode *));
  const char **all_member_names =
      arena_alloc(arena, total_members * sizeof(char *), alignof(char *));
  size_t member_index = 0;

  // Track member names to check for duplicates
  GrowableArray seen_names;
  growable_array_init(&seen_names, arena, total_members, sizeof(char *));

  // FIRST PASS: Collect all data fields (non-methods) to create the struct type
  for (size_t i = 0; i < public_count; i++) {
    AstNode *member = public_members[i];
    if (!member || member->type != AST_STMT_FIELD_DECL) {
      tc_error(node, "Struct Error", "Invalid public member %zu in struct '%s'",
               i, struct_name);
      return false;
    }

    const char *field_name = member->stmt.field_decl.name;
    AstNode *field_type = member->stmt.field_decl.type;
    AstNode *field_function = member->stmt.field_decl.function;

    if (!field_name) {
      tc_error(node, "Struct Field Error",
               "Field %zu in struct '%s' has no name", i, struct_name);
      return false;
    }

    // Check for duplicate member names
    for (size_t j = 0; j < seen_names.count; j++) {
      char **existing_name =
          (char **)((char *)seen_names.data + j * sizeof(char *));
      if (strcmp(*existing_name, field_name) == 0) {
        tc_error_id(node, field_name, "Duplicate Member",
                    "Struct member '%s' is already declared in struct '%s'",
                    field_name, struct_name);
        return false;
      }
    }

    char **name_slot = (char **)growable_array_push(&seen_names);
    *name_slot = (char *)field_name;

    // If it's a data field (not a method), add it now
    if (field_type && !field_function) {
      if (field_type->category != Node_Category_TYPE) {
        tc_error(node, "Struct Field Error",
                 "Field '%s' in struct '%s' has invalid type category",
                 field_name, struct_name);
        return false;
      }

      all_member_types[member_index] = field_type;
      all_member_names[member_index] = field_name;
      member_index++;
    }
  }

  // Process private data fields
  for (size_t i = 0; i < private_count; i++) {
    AstNode *member = private_members[i];
    if (!member || member->type != AST_STMT_FIELD_DECL) {
      tc_error(node, "Struct Error",
               "Invalid private member %zu in struct '%s'", i, struct_name);
      return false;
    }

    const char *field_name = member->stmt.field_decl.name;
    AstNode *field_type = member->stmt.field_decl.type;
    AstNode *field_function = member->stmt.field_decl.function;

    if (!field_name) {
      tc_error(node, "Struct Field Error",
               "Field %zu in struct '%s' has no name", i, struct_name);
      return false;
    }

    // Check for duplicate member names
    for (size_t j = 0; j < seen_names.count; j++) {
      char **existing_name =
          (char **)((char *)seen_names.data + j * sizeof(char *));
      if (strcmp(*existing_name, field_name) == 0) {
        tc_error_id(node, field_name, "Duplicate Member",
                    "Struct member '%s' is already declared in struct '%s'",
                    field_name, struct_name);
        return false;
      }
    }

    char **name_slot = (char **)growable_array_push(&seen_names);
    *name_slot = (char *)field_name;

    if (field_type && !field_function) {
      if (field_type->category != Node_Category_TYPE) {
        tc_error(node, "Struct Field Error",
                 "Field '%s' in struct '%s' has invalid type category",
                 field_name, struct_name);
        return false;
      }

      all_member_types[member_index] = field_type;
      all_member_names[member_index] = field_name;
      member_index++;
    }
  }

  // Create the struct type with just data fields
  size_t data_field_count = member_index;
  AstNode *struct_type =
      create_struct_type(arena, struct_name, all_member_types, all_member_names,
                         data_field_count, node->line, node->column);

  if (!struct_type) {
    tc_error(node, "Struct Creation Error",
             "Failed to create struct type for '%s'", struct_name);
    return false;
  }

  // Add struct type to scope BEFORE processing methods
  if (!scope_add_symbol(scope, struct_name, struct_type, is_public, false,
                        arena)) {
    tc_error_id(node, struct_name, "Symbol Error",
                "Failed to add struct '%s' to scope", struct_name);
    return false;
  }

  // Now we need to reserve space for methods in the arrays BEFORE processing
  // them Count total fields (data + methods)
  size_t method_count = 0;
  for (size_t i = 0; i < public_count; i++) {
    if (public_members[i]->stmt.field_decl.function) {
      method_count++;
    }
  }
  for (size_t i = 0; i < private_count; i++) {
    if (private_members[i]->stmt.field_decl.function) {
      method_count++;
    }
  }

  size_t total_member_count = data_field_count + method_count;

  // Reallocate the arrays to hold all members (data + methods)
  AstNode **full_member_types = arena_alloc(
      arena, total_member_count * sizeof(AstNode *), alignof(AstNode *));
  const char **full_member_names =
      arena_alloc(arena, total_member_count * sizeof(char *), alignof(char *));

  // Copy existing data fields
  for (size_t i = 0; i < data_field_count; i++) {
    full_member_types[i] = all_member_types[i];
    full_member_names[i] = all_member_names[i];
  }

  // Update struct type to use new arrays
  struct_type->type_data.struct_type.member_types = full_member_types;
  struct_type->type_data.struct_type.member_names = full_member_names;
  struct_type->type_data.struct_type.member_count =
      data_field_count; // Start with data fields

  member_index = data_field_count; // Continue from where we left off

  // SECOND PASS: Process methods with explicit 'self' parameter
  for (size_t i = 0; i < public_count; i++) {
    AstNode *member = public_members[i];
    const char *field_name = member->stmt.field_decl.name;
    AstNode *field_function = member->stmt.field_decl.function;

    if (field_function) {
      // This is a method - typecheck it with 'self' parameter

      AstNode *func_node = field_function;
      if (func_node->type != AST_STMT_FUNCTION) {
        tc_error(node, "Internal Error", "Expected function node for method");
        return false;
      }

      // Create the method's scope
      Scope *method_scope = create_child_scope(scope, field_name, arena);
      method_scope->is_function_scope = true;
      method_scope->associated_node = func_node;

      // Add 'self' as the first parameter
      // 'self' is a reference to the struct instance (could be pointer or
      // value) For simplicity, let's make it a pointer to the struct
      AstNode *self_type = create_pointer_type(
          arena, struct_type, func_node->line, func_node->column);

      if (!scope_add_symbol(method_scope, "self", self_type, false, true,
                            arena)) {
        tc_error(func_node, "Method Error",
                 "Failed to add 'self' parameter to method '%s'", field_name);
        return false;
      }

      // Add method parameters to the scope
      size_t param_count = func_node->stmt.func_decl.param_count;
      char **param_names = func_node->stmt.func_decl.param_names;
      AstNode **param_types = func_node->stmt.func_decl.param_types;

      for (size_t j = 0; j < param_count; j++) {
        if (!scope_add_symbol(method_scope, param_names[j], param_types[j],
                              false, true, arena)) {
          tc_error(func_node, "Method Parameter Error",
                   "Could not add parameter '%s' to method '%s' scope",
                   param_names[j], field_name);
          return false;
        }
      }

      // Typecheck the method body
      AstNode *body = func_node->stmt.func_decl.body;
      if (body) {
        if (!typecheck_statement(body, method_scope, arena)) {
          tc_error(node, "Struct Method Error",
                   "Method '%s' in struct '%s' failed type checking",
                   field_name, struct_name);
          return false;
        }
      }

      // Register the method in the parent scope with QUALIFIED NAME
      // CRITICAL: Include 'self' as the first parameter in the method type
      AstNode *return_type = func_node->stmt.func_decl.return_type;

      size_t method_param_count = param_count + 1; // +1 for self
      AstNode **method_param_types = arena_alloc(
          arena, method_param_count * sizeof(AstNode *), alignof(AstNode *));

      // First parameter is self
      method_param_types[0] = self_type;

      // Copy the rest of the parameters
      for (size_t j = 0; j < param_count; j++) {
        method_param_types[j + 1] = param_types[j];
      }

      AstNode *method_type =
          create_function_type(arena, method_param_types, method_param_count,
                               return_type, func_node->line, func_node->column);

      // Create qualified method name: StructName.MethodName
      size_t qualified_len = strlen(struct_name) + strlen(field_name) + 2;
      char *qualified_method_name = arena_alloc(arena, qualified_len, 1);
      snprintf(qualified_method_name, qualified_len, "%s.%s", struct_name,
               field_name);

      if (!scope_add_symbol(scope, qualified_method_name, method_type,
                            is_public, false, arena)) {
        tc_error(func_node, "Method Registration Error",
                 "Failed to register method '%s' in scope", field_name);
        return false;
      }

      // Add method to struct type's member list
      full_member_types[member_index] = method_type;
      full_member_names[member_index] = field_name;
      member_index++;

      // Update the struct's member count to include this method
      struct_type->type_data.struct_type.member_count = member_index;
    }
  }

  // Process private methods similarly
  for (size_t i = 0; i < private_count; i++) {
    AstNode *member = private_members[i];
    const char *field_name = member->stmt.field_decl.name;
    AstNode *field_function = member->stmt.field_decl.function;

    if (field_function) {
      AstNode *func_node = field_function;
      if (func_node->type != AST_STMT_FUNCTION) {
        tc_error(node, "Internal Error", "Expected function node for method");
        return false;
      }

      Scope *method_scope = create_child_scope(scope, field_name, arena);
      method_scope->is_function_scope = true;
      method_scope->associated_node = func_node;

      // Add 'self' parameter
      AstNode *self_type = create_pointer_type(
          arena, struct_type, func_node->line, func_node->column);

      if (!scope_add_symbol(method_scope, "self", self_type, false, true,
                            arena)) {
        tc_error(func_node, "Method Error",
                 "Failed to add 'self' parameter to method '%s'", field_name);
        return false;
      }

      size_t param_count = func_node->stmt.func_decl.param_count;
      char **param_names = func_node->stmt.func_decl.param_names;
      AstNode **param_types = func_node->stmt.func_decl.param_types;

      for (size_t j = 0; j < param_count; j++) {
        if (!scope_add_symbol(method_scope, param_names[j], param_types[j],
                              false, true, arena)) {
          tc_error(func_node, "Method Parameter Error",
                   "Could not add parameter '%s' to method '%s' scope",
                   param_names[j], field_name);
          return false;
        }
      }

      AstNode *body = func_node->stmt.func_decl.body;
      if (body) {
        if (!typecheck_statement(body, method_scope, arena)) {
          tc_error(node, "Struct Method Error",
                   "Method '%s' in struct '%s' failed type checking",
                   field_name, struct_name);
          return false;
        }
      }

      AstNode *return_type = func_node->stmt.func_decl.return_type;

      // CRITICAL: Include 'self' as the first parameter in the method type
      size_t method_param_count = param_count + 1; // +1 for self
      AstNode **method_param_types = arena_alloc(
          arena, method_param_count * sizeof(AstNode *), alignof(AstNode *));

      // First parameter is self
      method_param_types[0] = self_type;

      // Copy the rest of the parameters
      for (size_t j = 0; j < param_count; j++) {
        method_param_types[j + 1] = param_types[j];
      }

      AstNode *method_type =
          create_function_type(arena, method_param_types, method_param_count,
                               return_type, func_node->line, func_node->column);

      // Create qualified method name: StructName.MethodName
      size_t qualified_len = strlen(struct_name) + strlen(field_name) + 2;
      char *qualified_method_name = arena_alloc(arena, qualified_len, 1);
      snprintf(qualified_method_name, qualified_len, "%s.%s", struct_name,
               field_name);

      if (!scope_add_symbol(scope, qualified_method_name, method_type, false,
                            false, arena)) {
        tc_error(func_node, "Method Registration Error",
                 "Failed to register method '%s' in scope", field_name);
        return false;
      }

      // Add method to struct type's member list
      full_member_types[member_index] = method_type;
      full_member_names[member_index] = field_name;
      member_index++;

      // Update the struct's member count to include this method
      struct_type->type_data.struct_type.member_count = member_index;
    }
  }

  return true;
}

bool typecheck_enum_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  const char *enum_name = node->stmt.enum_decl.name;
  char **member_names = node->stmt.enum_decl.members;
  size_t member_count = node->stmt.enum_decl.member_count;
  bool is_public = node->stmt.enum_decl.is_public;

  // Create a distinct enum type (not just "int")
  AstNode *enum_type =
      create_basic_type(arena, enum_name, node->line, node->column);

  // Add the enum type itself to the symbol table
  if (!scope_add_symbol(scope, enum_name, enum_type, is_public, false, arena)) {
    tc_error_id(node, enum_name, "Duplicate Symbol",
                "Enum '%s' is already declared in this scope", enum_name);
    return false;
  }

  // Add enum members - they also have the enum type (not int type)
  for (size_t i = 0; i < member_count; i++) {
    size_t qualified_len = strlen(enum_name) + strlen(member_names[i]) + 2;
    char *qualified_name = arena_alloc(arena, qualified_len, 1);
    snprintf(qualified_name, qualified_len, "%s.%s", enum_name,
             member_names[i]);

    // Enum members have the same type as the enum itself
    if (!scope_add_symbol(scope, qualified_name, enum_type, is_public, false,
                          arena)) {
      tc_error(node, "Enum Member Error", "Could not add enum member '%s'",
               qualified_name);
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
      tc_error_help(
          node, "Return Type Mismatch",
          "Check that the returned value matches the function's return type",
          "Return type mismatch: expected '%s', got '%s'",
          type_to_string(expected_return_type, arena),
          type_to_string(actual_return_type, arena));
      return false;
    }

    Scope *func_scope = scope;
    while (func_scope && !func_scope->is_function_scope) {
      func_scope = func_scope->parent;
    }

    if (func_scope && func_scope->associated_node) {
      bool returns_ownership =
          func_scope->associated_node->stmt.func_decl.returns_ownership;

      if (returns_ownership && is_pointer_type(actual_return_type)) {
        const char *returned_var = extract_variable_name(return_value);
        if (returned_var) {
          StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
          if (analyzer) {
            const char *func_name =
                func_scope->associated_node->stmt.func_decl.name;

            // ONLY mark as freed if this variable came from a parameter
            // (ownership transfer from caller), NOT if it's a fresh allocation
            // Check if this variable is a parameter
            bool is_parameter = false;
            AstNode *func_node = func_scope->associated_node;
            for (size_t i = 0; i < func_node->stmt.func_decl.param_count; i++) {
              if (strcmp(func_node->stmt.func_decl.param_names[i],
                         returned_var) == 0) {
                is_parameter = true;
                break;
              }
            }

            // Only track as freed if it's a parameter (ownership passthrough)
            // Don't track if it's a local allocation (fresh ownership)
            if (is_parameter) {
              static_memory_track_free(analyzer, returned_var, func_name);
            }
          }
        }
      }
    }
  }

  return true;
}

bool typecheck_if_decl(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  Scope *then_branch = create_child_scope(scope, "then_branch", arena);
  Scope *else_branch = create_child_scope(scope, "else_branch", arena);
  node->stmt.if_stmt.scope = (void *)scope;
  node->stmt.if_stmt.then_scope = (void *)then_branch;
  node->stmt.if_stmt.else_scope = (void *)else_branch;

  // Typecheck main if condition
  Type *expected =
      create_basic_type(arena, "bool", node->stmt.if_stmt.condition->line,
                        node->stmt.if_stmt.condition->column);
  Type *user = typecheck_expression(node->stmt.if_stmt.condition, scope, arena);
  TypeMatchResult condition = types_match(expected, user);
  if (condition == TYPE_MATCH_NONE) {
    tc_error_help(
        node, "If Condition Error",
        "The condition of an if statement must be of type 'bool'",
        "If condition expected to be of type 'bool', but got '%s' instead",
        type_to_string(user, arena));
    return false;
  }

  // Typecheck then branch
  if (node->stmt.if_stmt.then_stmt != NULL) {
    if (!typecheck_statement(node->stmt.if_stmt.then_stmt, then_branch,
                             arena)) {
      return false;
    }

    if (then_branch->deferred_frees.count > 0) {
      StaticMemoryAnalyzer *analyzer = get_static_analyzer(then_branch);
      if (analyzer) {
        const char *func_name = get_current_function_name(scope);
        for (size_t i = 0; i < then_branch->deferred_frees.count; i++) {
          const char **var_ptr =
              (const char **)((char *)then_branch->deferred_frees.data +
                             i * sizeof(const char *));
          if (*var_ptr) {
            static_memory_track_free(analyzer, *var_ptr, func_name);
          }
        }
      }
    }
  }

  // Typecheck elif branches
  for (int i = 0; i < node->stmt.if_stmt.elif_count; i++) {
    if (node->stmt.if_stmt.elif_stmts[i] != NULL) {
      Scope *elif_scope = create_child_scope(scope, "elif_branch", arena);

      // CRITICAL FIX: Extract the condition and body from the elif node
      // The elif_stmts[i] is actually an AST_STMT_IF node with its own
      // condition/then_stmt
      AstNode *elif_node = node->stmt.if_stmt.elif_stmts[i];

      if (elif_node->type != AST_STMT_IF) {
        tc_error(elif_node, "Internal Error",
                 "Expected if statement node for elif");
        return false;
      }

      // Typecheck elif condition
      Type *elif_expected = create_basic_type(
          arena, "bool", elif_node->stmt.if_stmt.condition->line,
          elif_node->stmt.if_stmt.condition->column);
      Type *elif_user =
          typecheck_expression(elif_node->stmt.if_stmt.condition, scope, arena);
      TypeMatchResult elif_condition = types_match(elif_expected, elif_user);

      if (elif_condition == TYPE_MATCH_NONE) {
        tc_error_help(
            elif_node, "Elif Condition Error",
            "The condition of an elif statement must be of type 'bool'",
            "Elif condition expected to be of type 'bool', but got '%s' "
            "instead",
            type_to_string(elif_user, arena));
        return false;
      }

      // Typecheck elif body
      if (elif_node->stmt.if_stmt.then_stmt != NULL) {
        if (!typecheck_statement(elif_node->stmt.if_stmt.then_stmt, elif_scope,
                                 arena)) {
          return false;
        }

        if (elif_scope->deferred_frees.count > 0) {
          StaticMemoryAnalyzer *analyzer = get_static_analyzer(elif_scope);
          if (analyzer) {
            const char *func_name = get_current_function_name(scope);
            for (size_t i = 0; i < elif_scope->deferred_frees.count; i++) {
              const char **var_ptr =
                  (const char **)((char *)elif_scope->deferred_frees.data +
                                 i * sizeof(const char *));
              if (*var_ptr) {
                static_memory_track_free(analyzer, *var_ptr, func_name);
              }
            }
          }
        }
      }
    }
  }

  // Typecheck else branch
  if (node->stmt.if_stmt.else_stmt != NULL) {
    if (!typecheck_statement(node->stmt.if_stmt.else_stmt, else_branch,
                             arena)) {
      return false;
    }

    if (else_branch->deferred_frees.count > 0) {
      StaticMemoryAnalyzer *analyzer = get_static_analyzer(else_branch);
      if (analyzer) {
        const char *func_name = get_current_function_name(scope);
        for (size_t i = 0; i < else_branch->deferred_frees.count; i++) {
          const char **var_ptr =
              (const char **)((char *)else_branch->deferred_frees.data +
                             i * sizeof(const char *));
          if (*var_ptr) {
            static_memory_track_free(analyzer, *var_ptr, func_name);
          }
        }
      }
    }
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
      tc_error(node, "Module Error", "Failed to register module '%s'",
               module_name);
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
      if (!typecheck(body[i], module_scope, arena, global_scope->config)) {
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
    tc_error(node, "Use Error", "Failed to import module '%s' as '%s'",
             module_name, alias);
    return false;
  }

  return true;
}

bool typecheck_infinite_loop_decl(AstNode *node, Scope *scope,
                                  ArenaAllocator *arena) {
  Scope *loop_scope = create_child_scope(scope, "infinite_loop", arena);
  node->stmt.loop_stmt.scope = (void *)loop_scope;

  if (node->stmt.loop_stmt.body == NULL) {
    fprintf(stderr, "Error: Loop body cannot be null at line %zu\n",
            node->line);
    return false;
  }

  if (!typecheck_statement(node->stmt.loop_stmt.body, loop_scope, arena)) {
    fprintf(stderr, "Error: Loop body failed typechecking at line %zu\n",
            node->line);
    return false;
  }

  return true;
}
bool typecheck_while_loop_decl(AstNode *node, Scope *scope,
                               ArenaAllocator *arena) {
  Scope *while_loop = create_child_scope(scope, "while_loop", arena);
  node->stmt.loop_stmt.scope = (void *)while_loop;

  if (!typecheck_expression(node->stmt.loop_stmt.condition, while_loop,
                            arena)) {
    fprintf(stderr,
            "Error: While loop condition failed typechecking at line %zu\n",
            node->line);
    return false;
  }

  if (!typecheck_statement(node->stmt.loop_stmt.body, while_loop, arena)) {
    fprintf(stderr, "Error: While loop body failed typechecking at line %zu\n",
            node->line);
    return false;
  }

  if (node->stmt.loop_stmt.optional) {
    if (!typecheck_expression(node->stmt.loop_stmt.optional, while_loop,
                              arena)) {
      fprintf(stderr,
              "Error: While loop optional failed typechecking at line %zu\n",
              node->line);
      return false;
    }
  }

  return true;
}
bool typecheck_for_loop_decl(AstNode *node, Scope *scope,
                             ArenaAllocator *arena) {
  Scope *lookup_scope = create_child_scope(scope, "for_loop", arena);
  node->stmt.loop_stmt.scope = (void *)lookup_scope;

  // Define the initializer
  for (size_t i = 0; i < node->stmt.loop_stmt.init_count; i++) {
    if (!typecheck_statement(node->stmt.loop_stmt.initializer[i], lookup_scope,
                             arena)) {
      fprintf(stderr,
              "Error: Loop initializer failed typechecking at line %zu\n",
              node->line);
      return false;
    }
  }

  // Define the condition
  if (!typecheck_expression(node->stmt.loop_stmt.condition, lookup_scope,
                            arena)) {
    fprintf(stderr, "Error: Loop condition failed typechecking at line %zu\n",
            node->line);
    return false;
  }

  // Define the body
  if (!typecheck_statement(node->stmt.loop_stmt.body, lookup_scope, arena)) {
    fprintf(stderr, "Error: Loop body failed typechecking at line %zu\n",
            node->line);
    return false;
  }

  // define the optional if it is defined
  if (node->stmt.loop_stmt.optional) {
    if (!typecheck_expression(node->stmt.loop_stmt.optional, lookup_scope,
                              arena)) {
      fprintf(stderr, "Error: Loop optional failed typechecking at line %zu\n",
              node->line);
      return false;
    }
  }

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

void find_enum_members(Scope *scope, const char *enum_name,
                       GrowableArray *members, ArenaAllocator *arena) {
  size_t enum_name_len = strlen(enum_name);

  // Recursively search up the scope chain
  Scope *current = scope;
  while (current) {
    // Check all symbols in current scope
    for (size_t i = 0; i < current->symbols.count; i++) {
      Symbol *symbol =
          (Symbol *)((char *)current->symbols.data + i * sizeof(Symbol));

      // Look for symbols that match the pattern "EnumName.MemberName"
      if (strncmp(symbol->name, enum_name, enum_name_len) == 0 &&
          symbol->name[enum_name_len] == '.') {

        // Extract the member name (everything after the dot)
        const char *member_name = symbol->name + enum_name_len + 1;

        // Check if we already have this member (avoid duplicates)
        bool already_exists = false;
        for (size_t j = 0; j < members->count; j++) {
          char **existing =
              (char **)((char *)members->data + j * sizeof(char *));
          if (strcmp(*existing, member_name) == 0) {
            already_exists = true;
            break;
          }
        }

        if (!already_exists) {
          char **slot = (char **)growable_array_push(members);
          if (slot) {
            *slot = arena_strdup(arena, member_name);
            // printf("DEBUG: Found enum member: %s.%s\n", enum_name,
            // member_name);
          }
        }
      }
    }
    current = current->parent;
  }
}

bool typecheck_switch_stmt(AstNode *node, Scope *scope, ArenaAllocator *arena) {
  if (node->type != AST_STMT_SWITCH) {
    tc_error(node, "Internal Error", "Expected switch statement node");
    return false;
  }

  // Typecheck the switch condition
  AstNode *condition_type =
      typecheck_expression(node->stmt.switch_stmt.condition, scope, arena);
  if (!condition_type) {
    tc_error(node, "Switch Error", "Failed to typecheck switch condition");
    return false;
  }

  // printf("DEBUG: Switch condition type: %s\n", type_to_string(condition_type,
  // arena));

  // Create a new scope for the switch body
  Scope *switch_scope = create_child_scope(scope, "switch", arena);
  node->stmt.switch_stmt.scope = (void *)switch_scope;

  // Track if we've seen a default case
  bool has_default = false;

  // For enum exhaustiveness checking
  GrowableArray covered_enum_members;
  bool is_enum_switch = false;
  const char *enum_name = NULL;

  // Check if this is an enum switch
  if (condition_type->type == AST_TYPE_BASIC) {
    const char *type_name = condition_type->type_data.basic.name;
    bool is_builtin =
        (strcmp(type_name, "int") == 0 || strcmp(type_name, "float") == 0 ||
         strcmp(type_name, "bool") == 0 || strcmp(type_name, "string") == 0 ||
         strcmp(type_name, "char") == 0 || strcmp(type_name, "void") == 0);

    if (!is_builtin) {
      is_enum_switch = true;
      enum_name = type_name;
      growable_array_init(&covered_enum_members, arena, 16, sizeof(char *));
      // printf("DEBUG: Detected enum switch on '%s'\n", enum_name);
    }
  }

  // Typecheck all case statements and track covered enum members
  for (size_t i = 0; i < node->stmt.switch_stmt.case_count; i++) {
    AstNode *case_stmt = node->stmt.switch_stmt.cases[i];
    if (!case_stmt) {
      tc_error(node, "Switch Error", "Case statement %zu is NULL", i);
      return false;
    }

    // Pass the condition type to case typechecking for value compatibility
    if (!typecheck_case_stmt(case_stmt, switch_scope, arena, condition_type)) {
      tc_error(node, "Switch Error", "Failed to typecheck case statement %zu",
               i);
      return false;
    }

    // Track covered enum members
    if (is_enum_switch) {
      for (size_t j = 0; j < case_stmt->stmt.case_clause.value_count; j++) {
        AstNode *case_value = case_stmt->stmt.case_clause.values[j];
        if (case_value && case_value->type == AST_EXPR_MEMBER) {
          const char *member_name = case_value->expr.member.member;
          if (member_name) {
            // Check if we already covered this member
            bool already_covered = false;
            for (size_t k = 0; k < covered_enum_members.count; k++) {
              char **existing = (char **)((char *)covered_enum_members.data +
                                          k * sizeof(char *));
              if (strcmp(*existing, member_name) == 0) {
                already_covered = true;
                tc_error_help(node, "Duplicate Case",
                              "Each enum member should only appear in one case",
                              "Enum member '%s.%s' appears in multiple cases",
                              enum_name, member_name);
                break;
              }
            }

            if (!already_covered) {
              char **slot = (char **)growable_array_push(&covered_enum_members);
              if (slot) {
                *slot = arena_strdup(arena, member_name);
                // printf("DEBUG: Covered enum member: %s.%s\n", enum_name,
                // member_name);
              }
            }
          }
        }
      }
    }
  }

  // Typecheck default case if present
  if (node->stmt.switch_stmt.default_case) {
    has_default = true;
    if (!typecheck_default_stmt(node->stmt.switch_stmt.default_case,
                                switch_scope, arena)) {
      tc_error(node, "Switch Error", "Failed to typecheck default case");
      return false;
    }
  }

  // Perform exhaustiveness analysis for enum switches
  if (is_enum_switch) {
    // Find all enum members by looking up the enum symbol
    Symbol *enum_symbol = scope_lookup(scope, enum_name);
    if (enum_symbol && enum_symbol->type &&
        enum_symbol->type->type == AST_TYPE_BASIC) {
      // Get all enum members by scanning the symbol table for EnumName.Member
      // patterns
      GrowableArray all_enum_members;
      growable_array_init(&all_enum_members, arena, 16, sizeof(char *));

      // Scan the scope for enum members (this is a simplified approach)
      find_enum_members(scope, enum_name, &all_enum_members, arena);

      // printf("DEBUG: Found %zu total enum members, covered %zu\n",
      //  all_enum_members.count, covered_enum_members.count);

      // Check if all members are covered
      bool is_exhaustive =
          (covered_enum_members.count >= all_enum_members.count);

      if (is_exhaustive && has_default) {
        // All cases covered, default is unnecessary
        tc_error_help(node, "Unnecessary Default",
                      "When all enum members are covered in cases, a default "
                      "clause is not needed",
                      "Switch on enum '%s' covers all %zu members, default "
                      "case is redundant",
                      enum_name, all_enum_members.count);
      } else if (!is_exhaustive && !has_default) {
        // Missing cases and no default
        tc_error_help(node, "Non-Exhaustive Switch",
                      "Add a default case or handle all enum members",
                      "Switch on enum '%s' covers only %zu of %zu members and "
                      "has no default case",
                      enum_name, covered_enum_members.count,
                      all_enum_members.count);

        // List missing members
        // printf("Missing enum members:\n");
        for (size_t i = 0; i < all_enum_members.count; i++) {
          char **all_member =
              (char **)((char *)all_enum_members.data + i * sizeof(char *));
          bool is_covered = false;

          for (size_t j = 0; j < covered_enum_members.count; j++) {
            char **covered_member =
                (char **)((char *)covered_enum_members.data +
                          j * sizeof(char *));
            if (strcmp(*all_member, *covered_member) == 0) {
              is_covered = true;
              break;
            }
          }

          if (!is_covered) {
            // printf("  - %s.%s\n", enum_name, *all_member);
          }
        }
      }
      // If (!is_exhaustive && has_default) - this is fine, default catches
      // missing cases If (is_exhaustive && has_default) - also fine, just
      // redundant (warned above)
    }
  }

  return true;
}

bool typecheck_case_stmt(AstNode *node, Scope *scope, ArenaAllocator *arena,
                         AstNode *expected_type) {
  if (node->type != AST_STMT_CASE) {
    tc_error(node, "Internal Error", "Expected case statement node");
    return false;
  }

  // printf("DEBUG: Typechecking case with %zu values\n",
  // node->stmt.case_clause.value_count);

  // Typecheck each case value
  for (size_t i = 0; i < node->stmt.case_clause.value_count; i++) {
    AstNode *case_value = node->stmt.case_clause.values[i];
    if (!case_value) {
      tc_error(node, "Case Error", "Case value %zu is NULL", i);
      return false;
    }

    AstNode *value_type = typecheck_expression(case_value, scope, arena);
    if (!value_type) {
      tc_error(node, "Case Error", "Failed to typecheck case value %zu", i);
      return false;
    }

    // printf("DEBUG: Case value %zu type: %s\n", i, type_to_string(value_type,
    // arena));

    // If we know the expected type, check compatibility
    if (expected_type) {
      TypeMatchResult match = types_match(expected_type, value_type);
      if (match == TYPE_MATCH_NONE) {
        tc_error_help(
            node, "Case Type Mismatch",
            "All case values must be compatible with the switch condition type",
            "Case value %zu has type '%s', but switch condition expects '%s'",
            i, type_to_string(value_type, arena),
            type_to_string(expected_type, arena));
        return false;
      }

      // Special validation for enum cases
      if (expected_type->type == AST_TYPE_BASIC &&
          value_type->type == AST_TYPE_BASIC) {
        const char *expected_name = expected_type->type_data.basic.name;
        const char *value_name = value_type->type_data.basic.name;

        // Check if they're the same enum type
        if (strcmp(expected_name, value_name) != 0) {
          // Check if this is an enum member access pattern (EnumName.Member)
          if (case_value->type == AST_EXPR_MEMBER) {
            const char *base_name =
                case_value->expr.member.object->expr.identifier.name;
            if (strcmp(base_name, expected_name) != 0) {
              tc_error_help(node, "Enum Case Mismatch",
                            "Case values must belong to the same enum as the "
                            "switch condition",
                            "Case value references enum '%s', but switch "
                            "condition is of type '%s'",
                            base_name, expected_name);
              return false;
            }
          }
        }
      }
    }
  }

  // Create a new scope for the case body to handle any local declarations
  Scope *case_scope = create_child_scope(scope, "case", arena);

  // Typecheck the case body
  if (node->stmt.case_clause.body) {
    if (!typecheck_statement(node->stmt.case_clause.body, case_scope, arena)) {
      tc_error(node, "Case Error", "Failed to typecheck case body");
      return false;
    }
  }

  return true;
}

bool typecheck_default_stmt(AstNode *node, Scope *scope,
                            ArenaAllocator *arena) {
  if (node->type != AST_STMT_DEFAULT) {
    tc_error(node, "Internal Error", "Expected default statement node");
    return false;
  }

  // printf("DEBUG: Typechecking default case\n");

  // Create a new scope for the default body
  Scope *default_scope = create_child_scope(scope, "default", arena);

  // Typecheck the default body
  if (node->stmt.default_clause.body) {
    if (!typecheck_statement(node->stmt.default_clause.body, default_scope,
                             arena)) {
      tc_error(node, "Default Error", "Failed to typecheck default case body");
      return false;
    }
  }

  return true;
}
