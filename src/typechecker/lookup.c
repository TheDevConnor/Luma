#include <stdio.h>

#include "type.h"

bool typecheck_statement(AstNode *stmt, Scope *scope, ArenaAllocator *arena) {
  switch (stmt->type) {
  case AST_PROGRAM:
    return typecheck_program_multipass(stmt, scope, arena);
  case AST_STMT_VAR_DECL:
    return typecheck_var_decl(stmt, scope, arena);
  case AST_STMT_FUNCTION:
    return typecheck_func_decl(stmt, scope, arena);
  case AST_STMT_STRUCT:
    return typecheck_struct_decl(stmt, scope, arena);
  case AST_STMT_ENUM:
    return typecheck_enum_decl(stmt, scope, arena);
  case AST_STMT_EXPRESSION:
    return typecheck_expression(stmt->stmt.expr_stmt.expression, scope, arena);
  case AST_STMT_RETURN:
    return typecheck_return_decl(stmt, scope, arena);
  case AST_STMT_IF:
    return typecheck_if_decl(stmt, scope, arena);
  case AST_STMT_BLOCK: {
    // Create new scope for block
    Scope *block_scope = create_child_scope(scope, "block", arena);
    for (size_t i = 0; i < stmt->stmt.block.stmt_count; i++) {
      if (!typecheck(stmt->stmt.block.statements[i], block_scope, arena)) {
        return false;
      }
    }
    return true;
  }
  case AST_STMT_PRINT: {
    // Typecheck each argument expression
    for (size_t i = 0; i < stmt->stmt.print_stmt.expr_count; i++) {
      if (!typecheck_expression(stmt->stmt.print_stmt.expressions[i], scope,
                                arena)) {
        return false;
      }
    }
    return true;
  }
  case AST_STMT_DEFER:
    return typecheck_statement(stmt->stmt.defer_stmt.statement, scope, arena);
  case AST_STMT_LOOP:
    return typecheck_loop_decl(stmt, scope, arena);
  case AST_STMT_BREAK_CONTINUE:
    return true; // Nothing to typecheck
  case AST_STMT_SWITCH:
    return typecheck_switch_stmt(stmt, scope, arena);

  default:
    tc_error(stmt, "Unsupported Statement",
             "Warning: Unhandled statement type %d", stmt->type);
    return true; // Don't fail on unimplemented statements yet
  }
}

AstNode *typecheck_expression(AstNode *expr, Scope *scope,
                              ArenaAllocator *arena) {
  switch (expr->type) {
  case AST_EXPR_LITERAL: {
    // Return appropriate type based on literal type
    switch (expr->expr.literal.lit_type) {
    case LITERAL_INT:
      return create_basic_type(arena, "int", expr->line, expr->column);
    case LITERAL_FLOAT:
      // Check if the literal has a suffix to determine float vs double
      // You'll need to add suffix information to your literal structure
      // For now, we can use a heuristic or default behavior:

      // Option 1: Default to double (more precise)
      return create_basic_type(arena, "double", expr->line, expr->column);

      // Option 2: Check for 'f' suffix (requires lexer changes)
      // if (expr->expr.literal.has_float_suffix) {
      //     return create_basic_type(arena, "float", expr->line, expr->column);
      // } else {
      //     return create_basic_type(arena, "double", expr->line,
      //     expr->column);
      // }

    case LITERAL_STRING:
      return create_basic_type(arena, "string", expr->line, expr->column);
    case LITERAL_BOOL:
      return create_basic_type(arena, "bool", expr->line, expr->column);
    case LITERAL_CHAR:
      return create_basic_type(arena, "char", expr->line, expr->column);
    case LITERAL_NULL:
      return create_basic_type(arena, "null", expr->line, expr->column);
    default:
      return NULL;
    }
  }
  case AST_EXPR_IDENTIFIER: {
    Symbol *symbol = scope_lookup(scope, expr->expr.identifier.name);
    if (!symbol) {
      tc_error(expr, "Undefined Identifier",
               "Undefined identifier '%s' at line %zu",
               expr->expr.identifier.name, expr->line);
      return NULL;
    }
    return symbol->type;
  }
  case AST_EXPR_BINARY:
    return typecheck_binary_expr(expr, scope, arena);
  case AST_EXPR_UNARY:
    return typecheck_unary_expr(expr, scope, arena);
  case AST_EXPR_CALL:
    return typecheck_call_expr(expr, scope, arena);
  case AST_EXPR_ASSIGNMENT:
    return typecheck_assignment_expr(expr, scope, arena);
  case AST_EXPR_GROUPING:
    return typecheck_expression(expr->expr.grouping.expr, scope, arena);
  case AST_EXPR_INDEX:
    return typecheck_index_expr(expr, scope, arena);
  case AST_EXPR_ARRAY:
    return typecheck_array_expr(expr, scope, arena);
  case AST_EXPR_MEMBER:
    return typecheck_member_expr(expr, scope, arena);
  case AST_EXPR_DEREF:
    return typecheck_deref_expr(expr, scope, arena);
  case AST_EXPR_ADDR:
    return typecheck_addr_expr(expr, scope, arena);
  case AST_EXPR_CAST:
    return typecheck_cast_expr(expr, scope, arena);
  case AST_EXPR_ALLOC:
    return typecheck_alloc_expr(expr, scope, arena);
  case AST_EXPR_FREE:
    return typecheck_free_expr(expr, scope, arena);
  case AST_EXPR_MEMCPY:
    return typecheck_memcpy_expr(expr, scope, arena);
  case AST_EXPR_SIZEOF:
    return typecheck_sizeof_expr(expr, scope, arena);
  default:
    tc_error(expr, "Unsupported Expression", "Unsupported expression type %d",
             expr->type);
    return create_basic_type(arena, "unknown", expr->line, expr->column);
  }
}

/**
 * @brief Three-pass typechecking for modules to handle forward references
 *
 * This allows modules to be declared and used in any order by:
 * - Pass 1: Registering all module scopes (creates namespaces)
 * - Pass 2: Processing all @use statements (resolves imports)
 * - Pass 3: Typechecking module bodies (validates code)
 *
 * @param program AST_PROGRAM node containing all modules
 * @param global_scope Global scope to register modules in
 * @param arena Arena allocator for memory management
 * @return true if all passes succeed, false on any error
 */
bool typecheck_program_multipass(AstNode *program, Scope *global_scope,
                                 ArenaAllocator *arena) {
  if (program->type != AST_PROGRAM) {
    tc_error(program, "Internal Error", "Expected program node");
    return false;
  }

  AstNode **modules = program->stmt.program.modules;
  size_t module_count = program->stmt.program.module_count;

  // -------------------------------------------------------------------------
  // PASS 1: Register all module scopes
  // -------------------------------------------------------------------------
  // This creates empty module scopes so they can be found by @use statements
  // in any module, regardless of declaration order.
  for (size_t i = 0; i < module_count; i++) {
    AstNode *module = modules[i];
    if (!module || module->type != AST_PREPROCESSOR_MODULE) {
      continue;
    }

    const char *module_name = module->preprocessor.module.name;

    // Check if module already exists (duplicate module definition)
    Scope *existing = find_module_scope(global_scope, module_name);
    if (existing) {
      tc_error(module, "Duplicate Module", "Module '%s' is already defined",
               module_name);
      return false;
    }

    // Create and register the module scope
    Scope *module_scope = create_module_scope(global_scope, module_name, arena);
    if (!register_module(global_scope, module_name, module_scope, arena)) {
      tc_error(module, "Module Error", "Failed to register module '%s'",
               module_name);
      return false;
    }
  }

  // -------------------------------------------------------------------------
  // PASS 2: Process all @use statements
  // -------------------------------------------------------------------------
  // Now that all modules exist, we can resolve imports. This must happen
  // before typechecking bodies because code may reference imported symbols.
  for (size_t i = 0; i < module_count; i++) {
    AstNode *module = modules[i];
    if (!module || module->type != AST_PREPROCESSOR_MODULE) {
      continue;
    }

    const char *module_name = module->preprocessor.module.name;
    Scope *module_scope = find_module_scope(global_scope, module_name);
    if (!module_scope) {
      tc_error(module, "Internal Error",
               "Module scope not found for '%s' after registration",
               module_name);
      return false;
    }

    AstNode **body = module->preprocessor.module.body;
    int body_count = module->preprocessor.module.body_count;

    // Process only @use statements in this pass
    // In lookup.c - typecheck_program_multipass, Pass 2
    for (int j = 0; j < body_count; j++) {
      if (!body[j])
        continue;

      if (body[j]->type == AST_PREPROCESSOR_USE) {
        if (!typecheck_use_stmt(body[j], module_scope, global_scope, arena)) {
          // error
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // PASS 3: Typecheck all module bodies (in dependency order)
  // -------------------------------------------------------------------------
  GrowableArray dep_graph;
  growable_array_init(&dep_graph, arena, module_count,
                      sizeof(ModuleDependency));
  build_dependency_graph(modules, module_count, &dep_graph, arena);

  // Process each module in dependency order
  for (size_t i = 0; i < module_count; i++) {
    AstNode *module = modules[i];
    if (!module || module->type != AST_PREPROCESSOR_MODULE)
      continue;

    const char *module_name = module->preprocessor.module.name;

    if (!process_module_in_order(module_name, &dep_graph, modules, module_count,
                                 global_scope, arena)) {
      return false;
    }
  }

  return true;
}
