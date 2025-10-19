#include <stdio.h>
#include <string.h>

// #include "../ast/ast_utils.h"
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
          "Arithmetic operations require numeric operands (int, float, double)",
          "Left operand has non-numeric type '%s'",
          type_to_string(left_type, arena));
      return NULL;
    }

    if (!is_numeric_type(right_type)) {
      tc_error_help(
          expr, "Type Error",
          "Arithmetic operations require numeric operands (int, float, double)",
          "Right operand has non-numeric type '%s'",
          type_to_string(right_type, arena));
      return NULL;
    }

    // Enhanced type promotion: double > float > int
    AstNode *double_type = create_basic_type(arena, "double", 0, 0);
    AstNode *float_type = create_basic_type(arena, "float", 0, 0);

    if (types_match(left_type, double_type) == TYPE_MATCH_EXACT ||
        types_match(right_type, double_type) == TYPE_MATCH_EXACT) {
      return create_basic_type(arena, "double", expr->line, expr->column);
    }

    if (types_match(left_type, float_type) == TYPE_MATCH_EXACT ||
        types_match(right_type, float_type) == TYPE_MATCH_EXACT) {
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

AstNode *typecheck_assignment_expr(AstNode *expr, Scope *scope,
                                   ArenaAllocator *arena) {
  AstNode *target_type =
      typecheck_expression(expr->expr.assignment.target, scope, arena);
  AstNode *value_type =
      typecheck_expression(expr->expr.assignment.value, scope, arena);

  if (!target_type || !value_type)
    return NULL;

  // Check for pointer assignment that might transfer ownership
  if (is_pointer_type(target_type) && is_pointer_type(value_type)) {
    // fprintf(stderr, "DEBUG: Detected pointer assignment\n");

    // Extract variable names from both sides
    const char *target_var =
        extract_variable_name(expr->expr.assignment.target);
    const char *source_var = extract_variable_name(expr->expr.assignment.value);

    // fprintf(stderr, "DEBUG: target_var='%s', source_var='%s'\n",
    //         target_var ? target_var : "NULL", source_var ? source_var :
    //         "NULL");

    if (target_var && source_var) {
      StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
      // fprintf(stderr, "DEBUG: Got analyzer: %p\n", (void *)analyzer);
      if (analyzer) {
        // fprintf(stderr, "DEBUG: Calling track_alias('%s', '%s')\n",
        // target_var,
        //         source_var);
        static_memory_track_alias(analyzer, target_var, source_var);
      }
    }
  }

  TypeMatchResult match = types_match(target_type, value_type);
  if (match == TYPE_MATCH_NONE) {
    const char *msg = "Type mismatch in assignment";
    tc_error_help(expr, "Type Mismatch", msg,
                  "Cannot assign value of type '%s' to variable of type '%s'",
                  type_to_string(value_type, arena),
                  type_to_string(target_type, arena));
    return NULL;
  }

  return target_type;
}

AstNode *typecheck_call_expr(AstNode *expr, Scope *scope,
                             ArenaAllocator *arena) {
  AstNode *callee = expr->expr.call.callee;
  AstNode **arguments = expr->expr.call.args;
  size_t arg_count = expr->expr.call.arg_count;

  Symbol *func_symbol = NULL;
  const char *func_name = NULL;
  bool is_method_call = false; // Track if this is a method call with injected self

  if (callee->type == AST_EXPR_IDENTIFIER) {
    // Simple function call: func()
    func_name = callee->expr.identifier.name;
    func_symbol = scope_lookup(scope, func_name);

  } else if (callee->type == AST_EXPR_MEMBER) {
    
    // Member function call: could be module::func() or obj.method()
    const char *base_name = callee->expr.member.object->expr.identifier.name;
    const char *member_name = callee->expr.member.member;
    bool is_compiletime = callee->expr.member.is_compiletime;

    if (is_compiletime) {
      // Compile-time member function call: module::func() or
      // Type::static_method()
      func_symbol = lookup_qualified_symbol(scope, base_name, member_name);
      func_name = member_name;

      if (!func_symbol) {
        tc_error(expr, "Compile-time Call Error",
                 "No compile-time callable '%s::%s' found", base_name,
                 member_name);
        return NULL;
      }

    } else {
      // Runtime member function call: obj.method()
      // This should be for instance methods on struct objects

      // First, check if base_name is a known module (without calling
      // lookup_qualified_symbol)
      bool is_module_access = false;
      Scope *current = scope;
      while (current) {
        for (size_t i = 0; i < current->imported_modules.count; i++) {
          ModuleImport *import =
              (ModuleImport *)((char *)current->imported_modules.data +
                               i * sizeof(ModuleImport));
          if (strcmp(import->alias, base_name) == 0) {
            is_module_access = true;
            break;
          }
        }
        if (is_module_access)
          break;
        current = current->parent;
      }

      if (is_module_access) {
        tc_error_help(expr, "Access Method Error",
                      "Use '::' for compile-time access to module functions",
                      "Cannot use runtime access '.' for module function - did "
                      "you mean '%s::%s()'?",
                      base_name, member_name);
        return NULL;
      }

      // Check if base_name is a variable with a struct type
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
      // Handle pointer dereference: if we have a pointer to struct,
      // automatically dereference it
      if (base_type->type == AST_TYPE_POINTER) {
        AstNode *pointee = base_type->type_data.pointer.pointee_type;
        if (pointee && pointee->type == AST_TYPE_BASIC) {
          // Check if the pointee is a struct type name
          Symbol *struct_symbol =
              scope_lookup(scope, pointee->type_data.basic.name);
          if (struct_symbol && struct_symbol->type &&
              struct_symbol->type->type == AST_TYPE_STRUCT) {
            base_type = struct_symbol->type; // Use the struct type
          }
        } else if (pointee && pointee->type == AST_TYPE_STRUCT) {
          base_type = pointee;
        }
      }

      // Handle case where base_type is a basic type that references a struct
      if (base_type->type == AST_TYPE_BASIC) {
        // Look up the struct type by name
        Symbol *struct_symbol =
            scope_lookup(scope, base_type->type_data.basic.name);
        if (struct_symbol && struct_symbol->type &&
            struct_symbol->type->type == AST_TYPE_STRUCT) {
          base_type = struct_symbol->type; // Use the actual struct type
        }
      }

      // Now check if it's a struct type and get the member
      if (base_type->type == AST_TYPE_STRUCT) {
        AstNode *member_type = get_struct_member_type(base_type, member_name);

        if (member_type && member_type->type == AST_TYPE_FUNCTION) {
          // This is a method call on a struct instance
          func_symbol = arena_alloc(arena, sizeof(Symbol), alignof(Symbol));
          if (!func_symbol) {
            tc_error(expr, "Memory Error", "Failed to allocate symbol for method '%s'", member_name);
            return NULL;
          }
          
          func_symbol->name = member_name;
          
          func_symbol->type = member_type;
          
          func_symbol->is_public = true;
          func_symbol->is_mutable = false;
          func_symbol->scope_depth = 0;
          func_symbol->returns_ownership = false;
          func_symbol->takes_ownership = false;
          func_name = member_name;

          // CRITICAL: For method calls, we need to inject 'self' as the first argument
          // Verify we have a valid object before injectin
          if (!callee->expr.member.object) {
            tc_error(expr, "Internal Error", "Method call has no object");
            return NULL;
          }

          // Create a new arguments array with 'self' prepended
          size_t new_arg_count = arg_count + 1;
          AstNode **new_arguments = arena_alloc(arena, new_arg_count * sizeof(AstNode *), 
                                                alignof(AstNode *));
          
          if (!new_arguments) {
            tc_error(expr, "Memory Error", "Failed to allocate arguments array for method call");
            return NULL;
          }
          
          // CRITICAL: Check if we need to take the address of the object
          // The method expects a pointer to the struct (self is Person*)
          // But obj.method() gives us the struct value (Person)
          // We need to automatically inject &obj instead of just obj
          
          // Get the method's parameter types to check what self expects
          if (!member_type || member_type->type != AST_TYPE_FUNCTION) {
            tc_error(expr, "Internal Error", "Method type is not a function");
            return NULL;
          }
          
          AstNode **method_param_types = member_type->type_data.function.param_types;
          if (!method_param_types || member_type->type_data.function.param_count == 0) {
            tc_error(expr, "Internal Error", "Method has no parameters (missing self?)");
            return NULL;
          }
          
          AstNode *self_param_type = method_param_types[0]; // First param is always self
          AstNode *object_node = callee->expr.member.object;
          
          // Check what the method expects for self
          bool expects_pointer = (self_param_type->type == AST_TYPE_POINTER);
          
          // Check what we have
          Symbol *obj_symbol = scope_lookup(scope, base_name);
          bool have_pointer = (obj_symbol && obj_symbol->type && 
                               obj_symbol->type->type == AST_TYPE_POINTER);
          
          if (expects_pointer && !have_pointer) {
            // Method expects pointer but we have value - take address
            AstNode *addr_expr = arena_alloc(arena, sizeof(AstNode), alignof(AstNode));
            addr_expr->type = AST_EXPR_ADDR;
            addr_expr->category = Node_Category_EXPR;
            addr_expr->line = object_node->line;
            addr_expr->column = object_node->column;
            addr_expr->expr.addr.object = object_node;
            new_arguments[0] = addr_expr;
          } else {
            // Either method expects value, or we already have pointer
            new_arguments[0] = object_node;
          }
          
          // Copy the rest of the user-provided arguments
          for (size_t i = 0; i < arg_count; i++) {
            new_arguments[i + 1] = arguments[i];
          }
          
          // Update the arguments and count for the rest of the function
          arguments = new_arguments;
          arg_count = new_arg_count;
          is_method_call = true; // Mark that we injected self

          expr->expr.call.args = new_arguments;
          expr->expr.call.arg_count = new_arg_count;
          
        } else if (member_type) {
          tc_error(expr, "Runtime Call Error",
                   "Cannot call non-function member '%s' on struct '%s'",
                   member_name, base_type->type_data.struct_type.name);
          return NULL;
        } else {
          tc_error(expr, "Runtime Call Error", "Struct '%s' has no member '%s'",
                   base_type->type_data.struct_type.name, member_name);
          return NULL;
        }
      } else {
        // Base is not a struct - runtime access is invalid
        tc_error(expr, "Runtime Call Error",
                 "Cannot use runtime access '.' on non-struct type '%s'",
                 type_to_string(base_type, arena));
        return NULL;
      }
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

  AstNode *func_type = func_symbol->type;

  if (func_type->type != AST_TYPE_FUNCTION) {
    tc_error(expr, "Call Error", "'%s' is not a function", func_name);
    return NULL;
  }

  AstNode **param_types = func_type->type_data.function.param_types;
  size_t param_count = func_type->type_data.function.param_count;
  AstNode *return_type = func_type->type_data.function.return_type;

  if (arg_count != param_count) {
    tc_error(expr, "Call Error", "Function '%s' expects %zu arguments, got %zu",
             func_name, param_count, arg_count);
    return NULL;
  }

  // For method calls, start from index 1 to skip the injected 'self' parameter
  size_t start_index = is_method_call ? 1 : 0;
  
  for (size_t i = start_index; i < arg_count; i++) {
    // Validate the argument pointer before dereferencing
    if (!arguments) {
      tc_error(expr, "Call Error",
               "Arguments array is NULL for call to '%s'", func_name);
      return NULL;
    }
    
    if (!arguments[i]) {
      tc_error(expr, "Call Error",
               "Argument %zu in call to '%s' is NULL", i + 1, func_name);
      return NULL;
    }
    
    AstNode *arg_type = typecheck_expression(arguments[i], scope, arena);
    
    if (!arg_type) {
      tc_error(expr, "Call Error",
               "Failed to type-check argument %zu in call to '%s'", i + 1,
               func_name);
      return NULL;
    }
    
    // Validate param_types array
    if (!param_types || !param_types[i]) {
      tc_error(expr, "Call Error",
               "Parameter %zu type in function '%s' is NULL", i + 1, func_name);
      return NULL;
    }
    
    TypeMatchResult match = types_match(param_types[i], arg_type);
    
    if (match == TYPE_MATCH_NONE) {
      fprintf(stderr, "DEBUG: Type mismatch - NONE - about to report error\n");
      
      const char *param_str = type_to_string(param_types[i], arena);
      const char *arg_str = type_to_string(arg_type, arena);
      
      tc_error_help(expr, "Call Error",
                    "Argument %zu to function '%s' has wrong type.",
                    "Expected '%s', got '%s'", i + 1, func_name,
                    param_str, arg_str);
      return NULL;
    }
  }

  // Handle ownership transfer from caller to function
  if (func_symbol->takes_ownership) {
    for (size_t i = 0; i < arg_count; i++) {
      if (param_types[i] && is_pointer_type(param_types[i])) {
        const char *arg_var = extract_variable_name(arguments[i]);
        if (arg_var) {
          StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
          if (analyzer) {
            const char *current_func = get_current_function_name(scope);
            static_memory_track_free(analyzer, arg_var, current_func);
          }
        }
      }
    }
  }
  
  return return_type;
}

AstNode *typecheck_index_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena) {
  if (expr->type != AST_EXPR_INDEX) {
    tc_error(expr, "Internal Error", "Expected index expression node");
    return NULL;
  }

  if (expr->expr.index.object->type == AST_EXPR_IDENTIFIER) {
    StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
    if (analyzer && g_tokens && g_token_count > 0 && g_file_path) {
      const char *var_name = expr->expr.index.object->expr.identifier.name;

      // NEW: Get current function name
      const char *func_name = NULL;
      Scope *func_scope = scope;
      while (func_scope && !func_scope->is_function_scope) {
        func_scope = func_scope->parent;
      }
      if (func_scope && func_scope->associated_node) {
        func_name = func_scope->associated_node->stmt.func_decl.name;
      }

      static_memory_check_use_after_free(analyzer, var_name,
                                         expr->expr.index.object->line,
                                         expr->expr.index.object->column, arena,
                                         g_tokens, g_token_count, g_file_path,
                                         func_name); // PASS FUNC NAME
    }
  }

  AstNode *object_type =
      typecheck_expression(expr->expr.index.object, scope, arena);
  if (!object_type) {
    tc_error(expr, "Index Error",
             "Failed to determine type of indexed expression");
    return NULL;
  }

  AstNode *index_type =
      typecheck_expression(expr->expr.index.index, scope, arena);
  if (!index_type) {
    tc_error(expr, "Index Error",
             "Failed to determine type of index expression");
    return NULL;
  }

  if (!is_numeric_type(index_type)) {
    tc_error_help(
        expr, "Index Type Error",
        "Array and pointer indices must be numeric types (typically int)",
        "Index has type '%s', expected numeric type",
        type_to_string(index_type, arena));
    return NULL;
  }

  if (object_type->type == AST_TYPE_ARRAY) {
    AstNode *element_type = object_type->type_data.array.element_type;
    if (!element_type) {
      tc_error(expr, "Array Index Error", "Array has invalid element type");
      return NULL;
    }
    return element_type;

  } else if (object_type->type == AST_TYPE_POINTER) {
    AstNode *pointee_type = object_type->type_data.pointer.pointee_type;
    if (!pointee_type) {
      tc_error(expr, "Pointer Index Error", "Pointer has invalid pointee type");
      return NULL;
    }
    return pointee_type;

  } else if (object_type->type == AST_TYPE_BASIC &&
             strcmp(object_type->type_data.basic.name, "string") == 0) {
    return create_basic_type(arena, "char", expr->line, expr->column);

  } else {
    tc_error_help(expr, "Index Error",
                  "Only arrays, pointers, and strings can be indexed",
                  "Cannot index expression of type '%s'",
                  type_to_string(object_type, arena));
    return NULL;
  }
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

    // printf("DEBUG: Runtime member access - base '%s' has type %p (category
    // %d, "
    //        "type %d)\n",
    //        base_name, (void *)base_type, base_type->category,
    //        base_type->type);

    // Handle pointer dereference: if we have a pointer to struct, automatically
    // dereference it
    if (base_type->type == AST_TYPE_POINTER) {
      // printf("DEBUG: Base type is pointer, checking pointee\n");
      AstNode *pointee = base_type->type_data.pointer.pointee_type;
      if (pointee && pointee->type == AST_TYPE_BASIC) {
        // printf("DEBUG: Pointee is basic type: '%s'\n",
        //        pointee->type_data.basic.name);
        // Check if the pointee is a struct type name
        Symbol *struct_symbol =
            scope_lookup(scope, pointee->type_data.basic.name);
        if (struct_symbol && struct_symbol->type &&
            struct_symbol->type->type == AST_TYPE_STRUCT) {
          // printf("DEBUG: Found struct type through pointer\n");
          base_type = struct_symbol->type; // Use the struct type
        }
      } else if (pointee && pointee->type == AST_TYPE_STRUCT) {
        // printf("DEBUG: Pointee is direct struct type\n");
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
        // printf("DEBUG: Found struct type by name lookup\n");
        base_type = struct_symbol->type; // Use the actual struct type
      }
    }

    // Now check if it's a struct type and get the member
    if (base_type->type == AST_TYPE_STRUCT) {
      // printf("DEBUG: Checking struct '%s' for member '%s'\n",
      //        base_type->type_data.struct_type.name, member_name);

      AstNode *member_type = get_struct_member_type(base_type, member_name);
      if (member_type) {
        // printf("DEBUG: Found member type: %p\n", (void *)member_type);
        return member_type;
      }

      // Member doesn't exist
      tc_error(expr, "Runtime Access Error", "Struct '%s' has no member '%s'",
               base_type->type_data.struct_type.name, member_name);
      return NULL;

    } else {
      // Base is not a struct - runtime access is invalid
      // printf("DEBUG: Base type is not a struct (type=%d)\n",
      // base_type->type);

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
  if (expr->expr.deref.object->type == AST_EXPR_IDENTIFIER) {
    StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
    if (analyzer && g_tokens && g_token_count > 0 && g_file_path) {
      const char *var_name = expr->expr.deref.object->expr.identifier.name;

      // Get current function name
      const char *func_name = NULL;
      Scope *func_scope = scope;
      while (func_scope && !func_scope->is_function_scope) {
        func_scope = func_scope->parent;
      }
      if (func_scope && func_scope->associated_node) {
        func_name = func_scope->associated_node->stmt.func_decl.name;
      }

      static_memory_check_use_after_free(analyzer, var_name,
                                         expr->expr.deref.object->line,
                                         expr->expr.deref.object->column, arena,
                                         g_tokens, g_token_count, g_file_path,
                                         func_name); // ADD THIS PARAMETER
    }
  }

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

  StaticMemoryAnalyzer *analyzer = get_static_analyzer(scope);
  if (analyzer->skip_memory_tracking) {
    // We're in a defer - add to the FUNCTION scope's deferred list
    Scope *func_scope = scope;
    while (func_scope && !func_scope->is_function_scope) {
      func_scope = func_scope->parent;
    }

    if (func_scope) {
      const char **slot =
          (const char **)growable_array_push(&func_scope->deferred_frees);
      if (slot) {
        if (expr->expr.free.ptr->type == AST_EXPR_IDENTIFIER) {
          const char *var_name = expr->expr.free.ptr->expr.identifier.name;
          *slot = var_name;
        } else {
          *slot = NULL; // Non-identifier, cannot track
        }
      }
    }
  } else {
    // Normal free - track in static analyzer
    if (expr->expr.free.ptr->type == AST_EXPR_IDENTIFIER) {
      const char *var_name = expr->expr.free.ptr->expr.identifier.name;
      static_memory_track_free(analyzer, var_name, NULL);
      // Error already reported if use-after-free detected, continue
      // typechecking
    }
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

// Add this function to expr.c

AstNode *typecheck_input_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena) {
  if (expr->type != AST_EXPR_INPUT) {
    tc_error(expr, "Internal Error", "Expected input expression node");
    return NULL;
  }

  AstNode *input_type = expr->expr.input.type;
  AstNode *msg = expr->expr.input.msg;

  // Validate that a type is provided
  if (!input_type) {
    tc_error(expr, "Input Error", "Input expression must specify a type");
    return NULL;
  }

  // Validate that the type is actually a type node
  if (input_type->category != Node_Category_TYPE) {
    tc_error(expr, "Input Error", "Input type parameter must be a valid type");
    return NULL;
  }

  // Validate the message expression if provided
  if (msg) {
    AstNode *msg_type = typecheck_expression(msg, scope, arena);
    if (!msg_type) {
      tc_error(expr, "Input Error",
               "Failed to determine type of input message");
      return NULL;
    }

    // Check that message is a string type
    AstNode *expected_string = create_basic_type(arena, "str", 0, 0);
    TypeMatchResult match = types_match(expected_string, msg_type);

    if (match == TYPE_MATCH_NONE) {
      tc_error_help(
          expr, "Input Message Type Error", "Input message must be a string",
          "Expected 'string', got '%s'", type_to_string(msg_type, arena));
      return NULL;
    }
  }

  // Validate that the input type is a supported type for input operations
  // Typically, input should work with basic types (int, float, string, etc.)
  if (input_type->type == AST_TYPE_BASIC) {
    const char *type_name = input_type->type_data.basic.name;

    // Check if it's a supported input type
    bool is_supported =
        strcmp(type_name, "int") == 0 || strcmp(type_name, "float") == 0 ||
        strcmp(type_name, "double") == 0 || strcmp(type_name, "string") == 0 ||
        strcmp(type_name, "char") == 0 || strcmp(type_name, "bool") == 0;

    if (!is_supported) {
      tc_error_help(expr, "Unsupported Input Type",
                    "Input only supports basic types (int, float, double, "
                    "string, char, bool)",
                    "Cannot read input as type '%s'", type_name);
      return NULL;
    }
  } else {
    // Complex types (pointers, arrays, structs) are not supported for input
    tc_error_help(expr, "Unsupported Input Type",
                  "Input only supports basic types",
                  "Cannot read input as complex type '%s'",
                  type_to_string(input_type, arena));
    return NULL;
  }

  // Return the specified input type
  return input_type;
}

AstNode *typecheck_system_expr(AstNode *expr, Scope *scope,
                               ArenaAllocator *arena) {
  if (expr->type != AST_EXPR_SYSTEM) {
    tc_error(expr, "Internal Error", "Expected system expression node");
    return NULL;
  }

  AstNode *command = expr->expr._system.command;

  // Validate that a command expression is provided
  if (!command) {
    tc_error(expr, "System Error", "System expression must have a command");
    return NULL;
  }

  // Typecheck the command expression
  AstNode *command_type = typecheck_expression(command, scope, arena);
  if (!command_type) {
    tc_error(expr, "System Error",
             "Failed to determine type of system command");
    return NULL;
  }

  // Verify the command is a string type
  AstNode *expected_string = create_basic_type(arena, "str", 0, 0);
  TypeMatchResult match = types_match(expected_string, command_type);

  if (match == TYPE_MATCH_NONE) {
    tc_error_help(
        expr, "System Command Type Error", "System command must be a string",
        "Expected 'string', got '%s'", type_to_string(command_type, arena));
    return NULL;
  }

  // system() returns an int (the exit status of the command)
  return create_basic_type(arena, "int", expr->line, expr->column);
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

AstNode *typecheck_array_expr(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena) {
  if (expr->type != AST_EXPR_ARRAY) {
    tc_error(expr, "Internal Error", "Expected array expression node");
    return NULL;
  }

  AstNode **elements = expr->expr.array.elements;
  size_t element_count = expr->expr.array.element_count;

  // Empty array - cannot infer type
  if (element_count == 0) {
    tc_error(expr, "Array Type Error",
             "Cannot infer type of empty array literal - provide explicit type "
             "annotation");
    return NULL;
  }

  // Type check the first element to establish the array's element type
  AstNode *first_element_type = typecheck_expression(elements[0], scope, arena);
  if (!first_element_type) {
    tc_error(expr, "Array Type Error",
             "Failed to determine type of first array element");
    return NULL;
  }

  // Check all remaining elements match the first element's type
  for (size_t i = 1; i < element_count; i++) {
    AstNode *element_type = typecheck_expression(elements[i], scope, arena);
    if (!element_type) {
      tc_error(expr, "Array Type Error",
               "Failed to determine type of array element %zu", i);
      return NULL;
    }

    TypeMatchResult match = types_match(first_element_type, element_type);
    if (match == TYPE_MATCH_NONE) {
      tc_error_help(
          expr, "Array Element Type Mismatch",
          "All array elements must have the same type",
          "Element %zu has type '%s', but first element has type '%s'", i,
          type_to_string(element_type, arena),
          type_to_string(first_element_type, arena));
      return NULL;
    }
  }

  // Create array size literal
  long long size_val = (long long)element_count;
  AstNode *size_expr = create_literal_expr(arena, LITERAL_INT, &size_val,
                                           expr->line, expr->column);

  // Create and return array type
  AstNode *array_type = create_array_type(arena, first_element_type, size_expr,
                                          expr->line, expr->column);
  return array_type;
}
