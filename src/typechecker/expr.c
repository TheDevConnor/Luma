#include <stdio.h>
#include <string.h>

#include "type.h"

AstNode *typecheck_binary_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena) {
  AstNode *left_type =
      typecheck_expression(expr->expr.binary.left, scope, arena);
  AstNode *right_type =
      typecheck_expression(expr->expr.binary.right, scope, arena);

  if (!left_type || !right_type)
    return NULL;

  BinaryOp op = expr->expr.binary.op;

  // Arithmetic operators
  if (op >= BINOP_ADD && op <= BINOP_POW) {
    if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
      fprintf(stderr,
              "Error: Arithmetic operation on non-numeric types at line %zu\n",
              expr->line);
      return NULL;
    }

    // Return the "wider" type (float > int)
    if (types_match(left_type, create_basic_type(arena, "float", 0, 0)) ==
            TYPE_MATCH_EXACT ||
        types_match(right_type, create_basic_type(arena, "float", 0, 0)) ==
            TYPE_MATCH_EXACT) {
      return create_basic_type(arena, "float", expr->line, expr->column);
    }
    return create_basic_type(arena, "int", expr->line, expr->column);
  }

  // Comparison operators
  if (op >= BINOP_EQ && op <= BINOP_GE) {
    TypeMatchResult match = types_match(left_type, right_type);
    if (match == TYPE_MATCH_NONE) {
      fprintf(stderr, "Error: Cannot compare incompatible types at line %zu\n",
              expr->line);
      return NULL;
    }
    return create_basic_type(arena, "bool", expr->line, expr->column);
  }

  // Logical operators
  if (op == BINOP_AND || op == BINOP_OR) {
    // In many languages, these work with any type (truthy/falsy)
    return create_basic_type(arena, "bool", expr->line, expr->column);
  }

  return NULL;
}

AstNode *typecheck_unary_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena) {
  AstNode *operand_type =
      typecheck_expression(expr->expr.unary.operand, scope, arena);
  if (!operand_type)
    return NULL;

  UnaryOp op = expr->expr.unary.op;

  if (op == UNOP_NEG) {
    if (!is_numeric_type(operand_type)) {
      fprintf(stderr, "Error: Unary negation on non-numeric type at line %zu\n",
              expr->line);
      return NULL;
    }
    return operand_type; // Negation does not change type
  }

  if (op == UNOP_POST_INC || op == UNOP_POST_DEC || op == UNOP_PRE_INC ||
      op == UNOP_PRE_DEC) {
    if (!is_numeric_type(operand_type)) {
      fprintf(stderr,
              "Error: Increment/decrement on non-numeric type at line %zu\n",
              expr->line);
      return NULL;
    }
    return operand_type; // Increment/decrement does not change type
  }

  if (op == UNOP_NOT) {
    // In many languages, logical NOT works with any type (truthy/falsy)
    return create_basic_type(arena, "bool", expr->line, expr->column);
  }

  return NULL;
}

// Fixed version of typecheck_call_expr for expr.c
AstNode *typecheck_call_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena) {
  AstNode *callee = expr->expr.call.callee;
  AstNode **arguments = expr->expr.call.args;
  size_t arg_count = expr->expr.call.arg_count;

  Symbol *func_symbol = NULL;
  const char *func_name = NULL;

  if (callee->type == AST_EXPR_IDENTIFIER) {
    // Simple function call: func()
    func_name = callee->expr.identifier.name;
    func_symbol = scope_lookup(scope, func_name);

  } else if (callee->type == AST_EXPR_MEMBER) {
    // Member function call: module.func()
    const char *base_name = callee->expr.member.object->expr.identifier.name;
    const char *member_name = callee->expr.member.member;

    // Use qualified symbol lookup with proper visibility checking
    func_symbol = lookup_qualified_symbol(scope, base_name, member_name);
    func_name = member_name;

    // No fallback for function calls - functions must respect module visibility
    // rules The previous fallback to scope_lookup bypassed visibility checks
    // and allowed access to private functions, which was the source of the bug

  } else {
    fprintf(stderr, "Error: Unsupported callee type %d at line %zu\n",
            callee->type, expr->line);
    return NULL;
  }

  if (!func_symbol) {
    fprintf(stderr, "Error: Undefined function '%s' at line %zu\n",
            func_name ? func_name : "unknown", expr->line);
    return NULL;
  }

  if (func_symbol->type->type != AST_TYPE_FUNCTION) {
    fprintf(stderr, "Error: '%s' is not a function at line %zu\n", func_name,
            expr->line);
    return NULL;
  }

  AstNode *func_type = func_symbol->type;
  AstNode **param_types = func_type->type_data.function.param_types;
  size_t param_count = func_type->type_data.function.param_count;
  AstNode *return_type = func_type->type_data.function.return_type;

  if (arg_count != param_count) {
    fprintf(stderr,
            "Error: Function '%s' expects %zu arguments, got %zu at line %zu\n",
            func_name, param_count, arg_count, expr->line);
    return NULL;
  }

  for (size_t i = 0; i < arg_count; i++) {
    AstNode *arg_type = typecheck_expression(arguments[i], scope, arena);
    if (!arg_type) {
      fprintf(stderr,
              "Error: Failed to type-check argument %zu in call to '%s'\n",
              i + 1, func_name);
      return NULL;
    }

    TypeMatchResult match = types_match(param_types[i], arg_type);
    if (match == TYPE_MATCH_NONE) {
      fprintf(stderr,
              "Error: Argument %zu to function '%s' has wrong type. "
              "Expected '%s', got '%s' at line %zu\n",
              i + 1, func_name, type_to_string(param_types[i], arena),
              type_to_string(arg_type, arena), expr->line);
      return NULL;
    }
  }

  return return_type;
}

// Updated version of typecheck_member_expr in expr.c
AstNode *typecheck_member_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena) {
  // Handle both Color.RED syntax and module.function syntax
  const char *base_name = expr->expr.member.object->expr.identifier.name;
  const char *member_name = expr->expr.member.member;

  // First, try to look up as a qualified module symbol (module.symbol)
  Symbol *module_symbol =
      lookup_qualified_symbol(scope, base_name, member_name);
  if (module_symbol) {
    return module_symbol->type;
  }

  // If not found as module access, try the original enum-style lookup
  // Build qualified name and look it up directly (for enums like Color.RED)
  size_t qualified_len = strlen(base_name) + strlen(member_name) + 2;
  char *qualified_name = arena_alloc(arena, qualified_len, 1);
  snprintf(qualified_name, qualified_len, "%s.%s", base_name, member_name);

  Symbol *member_symbol = scope_lookup(scope, qualified_name);
  if (member_symbol) {
    return member_symbol->type;
  }

  // If neither worked, provide appropriate error message
  Symbol *base_symbol = scope_lookup(scope, base_name);
  if (!base_symbol) {
    // Check if it's a module import that wasn't found
    for (size_t i = 0; i < scope->imported_modules.count; i++) {
      ModuleImport *import =
          (ModuleImport *)((char *)scope->imported_modules.data +
                           i * sizeof(ModuleImport));
      if (strcmp(import->alias, base_name) == 0) {
        fprintf(stderr,
                "Error: Module '%s' has no exported symbol '%s' at line %zu\n",
                base_name, member_name, expr->line);
        return NULL;
      }
    }
    fprintf(stderr, "Error: Undefined identifier '%s' at line %zu\n", base_name,
            expr->line);
  } else {
    fprintf(stderr, "Error: '%s' has no member '%s' at line %zu\n", base_name,
            member_name, expr->line);
  }
  return NULL;
}

AstNode *typecheck_deref_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena) {
  AstNode *pointer_type =
      typecheck_expression(expr->expr.deref.object, scope, arena);
  if (!pointer_type) {
    fprintf(stderr,
            "Error: Failed to type-check dereferenced expression at line %zu\n",
            expr->line);
    return NULL;
  }
  if (pointer_type->type != AST_TYPE_POINTER) {
    fprintf(stderr, "Error: Cannot dereference non-pointer type at line %zu\n",
            expr->line);
    return NULL;
  }
  return pointer_type->type_data.pointer.pointee_type;
}

AstNode *typecheck_addr_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena) {
  AstNode *base_type =
      typecheck_expression(expr->expr.addr.object, scope, arena);
  if (!base_type) {
    fprintf(stderr,
            "Error: Failed to type-check address-of expression at line %zu\n",
            expr->line);
    return NULL;
  }
  AstNode *pointer_type =
      create_pointer_type(arena, base_type, expr->line, expr->column);
  return pointer_type;
}

AstNode *typecheck_alloc_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena) {
  // Verify size argument is numeric
  AstNode *size_type =
      typecheck_expression(expr->expr.alloc.size, scope, arena);
  if (!size_type) {
    fprintf(stderr, "Error: Cannot determine type for alloc size at line %zu\n",
            expr->line);
    return NULL;
  }

  if (!is_numeric_type(size_type)) {
    fprintf(stderr, "Error: alloc size must be numeric type at line %zu\n",
            expr->line);
    return NULL;
  }

  // alloc returns void* (generic pointer)
  AstNode *void_type =
      create_basic_type(arena, "void", expr->line, expr->column);
  return create_pointer_type(arena, void_type, expr->line, expr->column);
}

AstNode *typecheck_free_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena) {
  AstNode *ptr_type = typecheck_expression(expr->expr.free.ptr, scope, arena);
  if (!ptr_type) {
    fprintf(stderr, "Error: Failed to type-check free expression at line %zu\n",
            expr->line);
    return NULL;
  }

  if (ptr_type->type != AST_TYPE_POINTER) {
    fprintf(stderr, "Error: Cannot free non-pointer type at line %zu\n",
            expr->line);
    return NULL;
  }

  // Track memory deallocation if tracker is available
  StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
  if (analyzer) {
      // Try to get the variable name from the free expression
      const char *var_name = extract_variable_name_from_free(expr);
      static_memory_track_free(analyzer, var_name);
  }

  return create_basic_type(arena, "void", expr->line, expr->column);
}

AstNode *typecheck_memcpy_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena) {
  return NULL;
}

AstNode *typecheck_cast_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena) {
  // Verify the expression being cast is valid
  AstNode *castee_type =
      typecheck_expression(expr->expr.cast.castee, scope, arena);
  if (!castee_type) {
    fprintf(stderr,
            "Error: Cannot determine type of cast operand at line %zu\n",
            expr->line);
    return NULL;
  }

  // Return the target type (the cast always succeeds in this system)
  return expr->expr.cast.type;
}

AstNode *typecheck_sizeof_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena) {
  // sizeof always returns size_t (or int in simplified systems)
  AstNode *object_type = NULL;

  // Check if it is a type or an expression
  if (expr->expr.size_of.is_type) {
    object_type = expr->expr.size_of.object;
  } else {
    object_type = typecheck_expression(expr->expr.size_of.object, scope, arena);
  }

  if (!object_type) {
    fprintf(stderr,
            "Error: Cannot determine type for sizeof operand at line %zu\n",
            expr->line);
    return NULL;
  }

  return create_basic_type(arena, "int", expr->line, expr->column);
}
