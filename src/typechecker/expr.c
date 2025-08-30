#include <stdio.h>
#include <string.h>

#include "../ast/ast_utils.h"
#include "type.h"

AstNode *typecheck_binary_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    AstNode *left_type = typecheck_expression(expr->expr.binary.left, scope, arena);
    AstNode *right_type = typecheck_expression(expr->expr.binary.right, scope, arena);
    
    if (!left_type || !right_type) return NULL;
    
    BinaryOp op = expr->expr.binary.op;
    
    // Arithmetic operators
    if (op >= BINOP_ADD && op <= BINOP_POW) {
        if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
            fprintf(stderr, "Error: Arithmetic operation on non-numeric types at line %zu\n", 
                   expr->line);
            return NULL;
        }
        
        // Return the "wider" type (float > int)
        if (types_match(left_type, create_basic_type(arena, "float", 0, 0)) == TYPE_MATCH_EXACT ||
            types_match(right_type, create_basic_type(arena, "float", 0, 0)) == TYPE_MATCH_EXACT) {
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

AstNode *typecheck_call_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    const char *func_name = expr->expr.call.callee->expr.identifier.name;
    AstNode **arguments = expr->expr.call.args;
    size_t arg_count = expr->expr.call.arg_count;
    
    // 1. Look up the function symbol
    Symbol *func_symbol = scope_lookup(scope, func_name);
    if (!func_symbol) {
        fprintf(stderr, "Error: Undefined function '%s' at line %zu\n", 
               func_name, expr->line);
        return NULL;
    }
    
    // 2. Verify it's a function type
    if (func_symbol->type->type != AST_TYPE_FUNCTION) {
        fprintf(stderr, "Error: '%s' is not a function at line %zu\n", 
               func_name, expr->line);
        return NULL;
    }
    
    AstNode *func_type = func_symbol->type;
    AstNode **param_types = func_type->type_data.function.param_types;
    size_t param_count = func_type->type_data.function.param_count;
    AstNode *return_type = func_type->type_data.function.return_type;
    
    // 3. Check argument count
    if (arg_count != param_count) {
        fprintf(stderr, "Error: Function '%s' expects %zu arguments, got %zu at line %zu\n",
               func_name, param_count, arg_count, expr->line);
        return NULL;
    }
    
    // 4. Type-check each argument
    for (size_t i = 0; i < arg_count; i++) {
        AstNode *arg_type = typecheck_expression(arguments[i], scope, arena);
        if (!arg_type) {
            fprintf(stderr, "Error: Failed to type-check argument %zu in call to '%s'\n",
                   i + 1, func_name);
            return NULL;
        }
        
        TypeMatchResult match = types_match(param_types[i], arg_type);
        if (match == TYPE_MATCH_NONE) {
            fprintf(stderr, "Error: Argument %zu to function '%s' has wrong type. "
                           "Expected '%s', got '%s' at line %zu\n",
                   i + 1, func_name, 
                   type_to_string(param_types[i], arena),
                   type_to_string(arg_type, arena),
                   expr->line);
            return NULL;
        }
    }
    
    // 5. Return the function's return type
    return return_type;
}

AstNode *typecheck_member_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    // Handle Color.RED syntax
    const char *base_name = expr->expr.member.object->expr.identifier.name;
    const char *member_name = expr->expr.member.member;
    
    // Build qualified name and look it up directly
    size_t qualified_len = strlen(base_name) + strlen(member_name) + 2;
    char *qualified_name = arena_alloc(arena, qualified_len, 1);
    snprintf(qualified_name, qualified_len, "%s.%s", base_name, member_name);
    
    Symbol *member_symbol = scope_lookup(scope, qualified_name);
    if (!member_symbol) {
        // Check if the base name exists to give a better error message
        Symbol *base_symbol = scope_lookup(scope, base_name);
        if (!base_symbol) {
            fprintf(stderr, "Error: Undefined identifier '%s' at line %zu\n", 
                    base_name, expr->line);
        } else {
            fprintf(stderr, "Error: '%s' has no member '%s' at line %zu\n", 
                    base_name, member_name, expr->line);
        }
        return NULL;
    }
    
    return member_symbol->type;
}

AstNode *typecheck_deref_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    AstNode *pointer_type = typecheck_expression(expr->expr.deref.object, scope, arena);
    if (!pointer_type) {
        fprintf(stderr, "Error: Failed to type-check dereferenced expression at line %zu\n", expr->line);
        return NULL;
    }
    if (pointer_type->type != AST_TYPE_POINTER) {
        fprintf(stderr, "Error: Cannot dereference non-pointer type at line %zu\n", expr->line);
        return NULL;
    }
    return pointer_type->type_data.pointer.pointee_type;
}

AstNode *typecheck_addr_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    AstNode *base_type = typecheck_expression(expr->expr.addr.object, scope, arena);
    if (!base_type) {
        fprintf(stderr, "Error: Failed to type-check address-of expression at line %zu\n", expr->line);
        return NULL;
    }
    AstNode *pointer_type = create_pointer_type(arena, base_type, expr->line, expr->column);
    return pointer_type;
}

AstNode *typecheck_alloc_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    // Verify size argument is numeric
    AstNode *size_type = typecheck_expression(expr->expr.alloc.size, scope, arena);
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
    return create_pointer_type(arena, NULL, expr->line, expr->column);
}

AstNode *typecheck_free_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    return NULL;
}

AstNode *typecheck_memcpy_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    return NULL;
}

AstNode *typecheck_cast_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    // Verify the expression being cast is valid
    AstNode *castee_type = typecheck_expression(expr->expr.cast.castee, scope, arena);
    if (!castee_type) {
        fprintf(stderr, "Error: Cannot determine type of cast operand at line %zu\n", 
               expr->line);
        return NULL;
    }
    
    // Return the target type (the cast always succeeds in this system)
    return expr->expr.cast.type;
}

AstNode *typecheck_sizeof_expr(AstNode *expr, Scope *scope, ArenaAllocator *arena) {
    // sizeof always returns size_t (or int in simplified systems)
    AstNode *object_type = NULL;
    
    // Check if it is a type or an expression
    if (expr->expr.size_of.is_type) {
        object_type = expr->expr.size_of.object;
    } else {
        object_type = typecheck_expression(expr->expr.size_of.object, scope, arena);
    }

    if (!object_type) {
        fprintf(stderr, "Error: Cannot determine type for sizeof operand at line %zu\n", 
               expr->line);
        return NULL;
    }

    return create_basic_type(arena, "int", expr->line, expr->column);
}
