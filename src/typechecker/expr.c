#include <stdio.h>
#include <string.h>

#include "../ast/ast_utils.h"
#include "type.h"

AstNode *typecheck_binary_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena) {
  AstNode *left_type =
      typecheck_expression(expr->expr.binary.left, scope, arena);
  AstNode *right_type =
      typecheck_expression(expr->expr.binary.right, scope, arena);

  if (!left_type || !right_type) {
    tc_error(expr, "Type Error", "Failed to determine operand types");
    return NULL;
  }

  BinaryOp op = expr->expr.binary.op;

  // Arithmetic operators
  if (op >= BINOP_ADD && op <= BINOP_POW) {
    if (!is_numeric_type(left_type)) {
      tc_error_help(
          expr, "Type Error",
          "Arithmetic operations require numeric operands (int, float)",
          "Left operand has non-numeric type '%s'",
          type_to_string(left_type, arena));
      return NULL;
    }

    if (!is_numeric_type(right_type)) {
      tc_error_help(
          expr, "Type Error",
          "Arithmetic operations require numeric operands (int, float)",
          "Right operand has non-numeric type '%s'",
          type_to_string(right_type, arena));
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
      tc_error_help(
          expr, "Type Error", "Comparison operands must be of compatible types",
          "Cannot compare incompatible types '%s' and '%s'",
          type_to_string(left_type, arena), type_to_string(right_type, arena));
      return NULL;
    }
    return create_basic_type(arena, "bool", expr->line, expr->column);
  }

  // Logical operators
  if (op == BINOP_AND || op == BINOP_OR) {
    return create_basic_type(arena, "bool", expr->line, expr->column);
  }

  // Range operators
  // In expr.c, update the range operator section:
  if (op == BINOP_RANGE) {
    // confirm that the left and right are of value int
    Type *elem_type = create_basic_type(arena, "int", 0, 0);
    if (types_match(left_type, elem_type) == TYPE_MATCH_NONE ||
        types_match(right_type, elem_type) == TYPE_MATCH_NONE) {
      tc_error_help(
          expr, "Type Error", "Range operator must take a type of int.",
          "Cannot create a range operation with two different types.");
    }

    int left = expr->expr.binary.left->expr.literal.value.int_val;
    int right = expr->expr.binary.right->expr.literal.value.int_val;

    long long size_val = 0;
    if (left < right) {
      size_val = right - left +
                 1; // FIXED: inclusive range (0..5 = 6 elements: 0,1,2,3,4,5)
    } else if (left > right) {
      size_val = left - right + 1; // FIXED: inclusive descending range
    } else {
      size_val = 1; // FIXED: single element (e.g., 5..5 = 1 element: 5)
    }

    Expr *size = create_literal_expr(arena, LITERAL_INT, &size_val, expr->line,
                                     expr->column);
    Type *array =
        create_array_type(arena, elem_type, size, expr->line, expr->column);
    return array;
  }

  tc_error(expr, "Unsupported Operation", "Unsupported binary operation");
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
      tc_error(expr, "Type Error", "Unary negation on non-numeric type");
      return NULL;
    }
    return operand_type; // Negation does not change type
  }

  if (op == UNOP_POST_INC || op == UNOP_POST_DEC || op == UNOP_PRE_INC ||
      op == UNOP_PRE_DEC) {
    if (!is_numeric_type(operand_type)) {
      tc_error(expr, "Type Error", "Increment/decrement on non-numeric type");
      return NULL;
    }
    return operand_type; // Increment/decrement does not change type
  }

  if (op == UNOP_NOT) {
    // In many languages, logical NOT works with any type (truthy/falsy)
    return create_basic_type(arena, "bool", expr->line, expr->column);
  }

  tc_error(expr, "Type Error", "Unsupported unary operation");
  return NULL;
}

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
    // Member function call: could be module.func() or obj.method()
    const char *base_name = callee->expr.member.object->expr.identifier.name;
    const char *member_name = callee->expr.member.member;
    bool is_compiletime = callee->expr.member.is_compiletime;

    if (is_compiletime) {
      // Compile-time member function call: module::func() or Type::static_method()
      func_symbol = lookup_qualified_symbol(scope, base_name, member_name);
      func_name = member_name;

      if (!func_symbol) {
        tc_error(expr, "Compile-time Call Error", 
                 "No compile-time callable '%s::%s' found", 
                 base_name, member_name);
        return NULL;
      }

    } else {
      // Runtime member function call: obj.method()
      // This would be for instance methods on struct objects
      
      // First check if it's actually a module access that should be compile-time
      Symbol *potential_module_symbol = lookup_qualified_symbol(scope, base_name, member_name);
      if (potential_module_symbol && potential_module_symbol->type->type == AST_TYPE_FUNCTION) {
        tc_error_help(expr, "Access Method Error",
                      "Use '::' for compile-time access to module functions",
                      "Cannot use runtime access '.' for module function - did you mean '%s::%s()'?",
                      base_name, member_name);
        return NULL;
      }

      // For now, we don't support instance methods on structs
      // This would need to be implemented when you add method support to structs
      tc_error(expr, "Runtime Call Error",
               "Runtime member function calls not yet supported");
      return NULL;
    }

  } else {
    tc_error(expr, "Call Error", "Unsupported callee type for function call");
    return NULL;
  }

  if (!func_symbol) {
    tc_error(expr, "Call Error", "Undefined function '%s'",
             func_name ? func_name : "unknown");
    return NULL;
  }

  if (func_symbol->type->type != AST_TYPE_FUNCTION) {
    tc_error(expr, "Call Error", "'%s' is not a function", func_name);
    return NULL;
  }

  AstNode *func_type = func_symbol->type;
  AstNode **param_types = func_type->type_data.function.param_types;
  size_t param_count = func_type->type_data.function.param_count;
  AstNode *return_type = func_type->type_data.function.return_type;

  if (arg_count != param_count) {
    tc_error(expr, "Call Error", "Function '%s' expects %zu arguments, got %zu",
             func_name, param_count, arg_count);
    return NULL;
  }

  for (size_t i = 0; i < arg_count; i++) {
    AstNode *arg_type = typecheck_expression(arguments[i], scope, arena);
    if (!arg_type) {
      tc_error(expr, "Call Error",
               "Failed to type-check argument %zu in call to '%s'", i + 1,
               func_name);
      return NULL;
    }

    TypeMatchResult match = types_match(param_types[i], arg_type);
    if (match == TYPE_MATCH_NONE) {
      tc_error_help(expr, "Call Error",
                    "Argument %zu to function '%s' has wrong type.",
                    "Expected '%s', got '%s'", i + 1, func_name,
                    type_to_string(param_types[i], arena),
                    type_to_string(arg_type, arena));
      return NULL;
    }
  }

  return return_type;
}

AstNode *typecheck_member_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena) {
  const char *base_name = expr->expr.member.object->expr.identifier.name;
  const char *member_name = expr->expr.member.member;
  bool is_compiletime = expr->expr.member.is_compiletime;

  if (is_compiletime) {
    // Compile-time access (::) - for modules, enums, static members

    // First try module-qualified symbol lookup
    Symbol *module_symbol =
        lookup_qualified_symbol(scope, base_name, member_name);
    if (module_symbol) {
      return module_symbol->type;
    }

    // Try enum-style lookup (EnumName::Member)
    size_t qualified_len = strlen(base_name) + strlen(member_name) + 2;
    char *qualified_name = arena_alloc(arena, qualified_len, 1);
    snprintf(qualified_name, qualified_len, "%s.%s", base_name, member_name);

    Symbol *member_symbol = scope_lookup(scope, qualified_name);
    if (member_symbol) {
      return member_symbol->type;
    }

    // Error handling for compile-time access
    Symbol *base_symbol = scope_lookup(scope, base_name);
    if (!base_symbol) {
      // Check if it's an unknown module
      for (size_t i = 0; i < scope->imported_modules.count; i++) {
        ModuleImport *import =
            (ModuleImport *)((char *)scope->imported_modules.data +
                             i * sizeof(ModuleImport));
        if (strcmp(import->alias, base_name) == 0) {
          tc_error(expr, "Compile-time Access Error",
                   "Module '%s' has no exported symbol '%s'", base_name,
                   member_name);
          return NULL;
        }
      }
      tc_error(expr, "Compile-time Access Error",
               "Undefined identifier '%s' in compile-time access", base_name);
    } else {
      // Base exists but member doesn't - check what kind of symbol it is
      if (base_symbol->type && base_symbol->type->type == AST_TYPE_BASIC) {
        // Could be an enum type
        tc_error(expr, "Compile-time Access Error",
                 "Type '%s' has no compile-time member '%s'", base_name,
                 member_name);
      } else {
        tc_error(expr, "Compile-time Access Error",
                 "Cannot use compile-time access '::' on runtime value '%s'",
                 base_name);
      }
    }
    return NULL;

  } else {
    // Runtime access (.) - for struct members, instance data

    Symbol *base_symbol = scope_lookup(scope, base_name);
    if (!base_symbol) {
      tc_error(expr, "Runtime Access Error", "Undefined identifier '%s'",
               base_name);
      return NULL;
    }

    if (!base_symbol->type) {
      tc_error(expr, "Runtime Access Error",
               "Symbol '%s' has no type information", base_name);
      return NULL;
    }

    AstNode *base_type = base_symbol->type;

    // printf("DEBUG: Runtime member access - base '%s' has type %p (category %d, "
    //        "type %d)\n",
    //        base_name, (void *)base_type, base_type->category, base_type->type);

    // Handle pointer dereference: if we have a pointer to struct, automatically
    // dereference it
    if (base_type->type == AST_TYPE_POINTER) {
      printf("DEBUG: Base type is pointer, checking pointee\n");
      AstNode *pointee = base_type->type_data.pointer.pointee_type;
      if (pointee && pointee->type == AST_TYPE_BASIC) {
        printf("DEBUG: Pointee is basic type: '%s'\n",
               pointee->type_data.basic.name);
        // Check if the pointee is a struct type name
        Symbol *struct_symbol =
            scope_lookup(scope, pointee->type_data.basic.name);
        if (struct_symbol && struct_symbol->type &&
            struct_symbol->type->type == AST_TYPE_STRUCT) {
          printf("DEBUG: Found struct type through pointer\n");
          base_type = struct_symbol->type; // Use the struct type
        }
      } else if (pointee && pointee->type == AST_TYPE_STRUCT) {
        printf("DEBUG: Pointee is direct struct type\n");
        base_type = pointee;
      }
    }

    // Handle case where base_type is a basic type that references a struct
    if (base_type->type == AST_TYPE_BASIC) {
      // printf("DEBUG: Base type is basic: '%s'\n",
      //        base_type->type_data.basic.name);
      // Look up the struct type by name
      Symbol *struct_symbol =
          scope_lookup(scope, base_type->type_data.basic.name);
      if (struct_symbol && struct_symbol->type &&
          struct_symbol->type->type == AST_TYPE_STRUCT) {
        printf("DEBUG: Found struct type by name lookup\n");
        base_type = struct_symbol->type; // Use the actual struct type
      }
    }

    // Now check if it's a struct type and get the member
    if (base_type->type == AST_TYPE_STRUCT) {
      printf("DEBUG: Checking struct '%s' for member '%s'\n",
             base_type->type_data.struct_type.name, member_name);

      AstNode *member_type = get_struct_member_type(base_type, member_name);
      if (member_type) {
        printf("DEBUG: Found member type: %p\n", (void *)member_type);
        return member_type;
      }

      // Member doesn't exist
      tc_error(expr, "Runtime Access Error", "Struct '%s' has no member '%s'",
               base_type->type_data.struct_type.name, member_name);
      return NULL;

    } else {
      // Base is not a struct - runtime access is invalid
      // printf("DEBUG: Base type is not a struct (type=%d)\n", base_type->type);

      // Check if user should have used compile-time access instead
      if (base_symbol->type && base_symbol->type->type == AST_TYPE_BASIC) {
        // Could be an enum or module
        Symbol *potential_enum =
            scope_lookup(scope, base_symbol->type->type_data.basic.name);
        if (potential_enum) {
          tc_error_help(expr, "Access Method Error",
                        "Use '::' for compile-time access to enum members",
                        "Cannot use runtime access '.' on type '%s' - did you "
                        "mean '%s::%s'?",
                        base_name, base_name, member_name);
        } else {
          tc_error(expr, "Runtime Access Error",
                   "Cannot use runtime access '.' on non-struct type '%s'",
                   base_name);
        }
      } else {
        tc_error(expr, "Runtime Access Error",
                 "Type '%s' does not support member access", base_name);
      }
      return NULL;
    }
  }
}

AstNode *typecheck_deref_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena) {
  AstNode *pointer_type =
      typecheck_expression(expr->expr.deref.object, scope, arena);
  if (!pointer_type) {
    tc_error(expr, "Type Error",
             "Failed to type-check dereferenced expression");
    return NULL;
  }
  if (pointer_type->type != AST_TYPE_POINTER) {
    tc_error(expr, "Type Error", "Cannot dereference non-pointer type");
    return NULL;
  }
  return pointer_type->type_data.pointer.pointee_type;
}

AstNode *typecheck_addr_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena) {
  AstNode *base_type =
      typecheck_expression(expr->expr.addr.object, scope, arena);
  if (!base_type) {
    tc_error(expr, "Type Error", "Failed to type-check address-of expression");
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
    tc_error(expr, "Type Error", "Cannot determine type for alloc size");
    return NULL;
  }

  if (!is_numeric_type(size_type)) {
    tc_error(expr, "Type Error", "alloc size must be numeric type");
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
    tc_error(expr, "Type Error", "Failed to type-check free expression");
    return NULL;
  }

  if (ptr_type->type != AST_TYPE_POINTER) {
    tc_error(expr, "Type Error", "Cannot free non-pointer type");
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
  (void)expr;
  (void)scope;
  (void)arena;
  return NULL;
}

AstNode *typecheck_cast_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena) {
  // Verify the expression being cast is valid
  AstNode *castee_type =
      typecheck_expression(expr->expr.cast.castee, scope, arena);
  if (!castee_type) {
    tc_error(expr, "Type Error", "Cannot determine type of cast operand");
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
    tc_error(expr, "Type Error", "Cannot determine type for sizeof operand");
    return NULL;
  }

  return create_basic_type(arena, "int", expr->line, expr->column);
}
