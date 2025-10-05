#include "../ast.h"
#include <stdio.h>
#include <string.h>

AstNode *create_program_node(ArenaAllocator *arena, AstNode **statements,
                             size_t stmt_count, size_t line, size_t column) {
  AstNode *node = create_stmt_node(arena, AST_PROGRAM, line, column);
  node->stmt.program.modules = statements;
  node->stmt.program.module_count = stmt_count;
  return node;
}

AstNode *create_expr_stmt(ArenaAllocator *arena, Expr *expression, size_t line,
                          size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_EXPRESSION, line, column);
  node->stmt.expr_stmt.expression = expression;
  return node;
}

AstNode *create_var_decl_stmt(ArenaAllocator *arena, const char *name,
                              AstNode *var_type, Expr *initializer,
                              bool is_mutable, bool is_public, size_t line,
                              size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_VAR_DECL, line, column);
  node->stmt.var_decl.name = name;
  node->stmt.var_decl.var_type = var_type;
  node->stmt.var_decl.initializer = initializer;
  node->stmt.var_decl.is_mutable = is_mutable;
  node->stmt.var_decl.is_public = is_public;
  return node;
}

AstNode *create_func_decl_stmt(ArenaAllocator *arena, const char *name,
                               char **param_names, AstNode **param_types,
                               size_t param_count, AstNode *return_type,
                               bool is_public, bool returns_ownership,
                               bool takes_ownership, AstNode *body, size_t line,
                               size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_FUNCTION, line, column);
  node->stmt.func_decl.name = name;
  node->stmt.func_decl.param_names = param_names;
  node->stmt.func_decl.param_types = param_types;
  node->stmt.func_decl.param_count = param_count;
  node->stmt.func_decl.return_type = return_type;
  node->stmt.func_decl.is_public = is_public;
  node->stmt.func_decl.returns_ownership = returns_ownership;
  node->stmt.func_decl.takes_ownership = takes_ownership;
  node->stmt.func_decl.body = body;
  return node;
}

AstNode *create_struct_decl_stmt(ArenaAllocator *arena, const char *name,
                                 AstNode **public_members, size_t public_count,
                                 AstNode **private_members,
                                 size_t private_count, bool is_public,
                                 size_t line, size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_STRUCT, line, column);
  node->stmt.struct_decl.name = name;
  node->stmt.struct_decl.public_members = public_members;
  node->stmt.struct_decl.public_count = public_count;
  node->stmt.struct_decl.private_members = private_members;
  node->stmt.struct_decl.private_count = private_count;
  node->stmt.struct_decl.is_public = is_public;
  return node;
}

AstNode *create_field_decl_stmt(ArenaAllocator *arena, const char *name,
                                AstNode *type, AstNode *function,
                                bool is_public, size_t line, size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_FIELD_DECL, line, column);
  node->stmt.field_decl.name = name;
  node->stmt.field_decl.type = type;
  node->stmt.field_decl.function = function;
  node->stmt.field_decl.is_public = is_public;
  return node;
}

AstNode *create_enum_decl_stmt(ArenaAllocator *arena, const char *name,
                               char **members, size_t member_count,
                               bool is_public, size_t line, size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_ENUM, line, column);
  node->stmt.enum_decl.name = name;
  node->stmt.enum_decl.members = members;
  node->stmt.enum_decl.member_count = member_count;
  node->stmt.enum_decl.is_public = is_public;
  return node;
}

AstNode *create_if_stmt(ArenaAllocator *arena, Expr *condition,
                        AstNode *then_stmt, AstNode **elif_stmts,
                        int elif_count, AstNode *else_stmt, size_t line,
                        size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_IF, line, column);
  node->stmt.if_stmt.condition = condition;
  node->stmt.if_stmt.then_stmt = then_stmt;
  node->stmt.if_stmt.elif_stmts = elif_stmts;
  node->stmt.if_stmt.elif_count = elif_count;
  node->stmt.if_stmt.else_stmt = else_stmt;
  return node;
}

AstNode *create_infinite_loop_stmt(ArenaAllocator *arena, AstNode *body,
                                   size_t line, size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_LOOP, line, column);
  node->stmt.loop_stmt.condition = NULL;
  node->stmt.loop_stmt.optional = NULL;
  node->stmt.loop_stmt.initializer = NULL;
  node->stmt.loop_stmt.init_count = 0;
  node->stmt.loop_stmt.body = body;
  return node;
}

AstNode *create_for_loop_stmt(ArenaAllocator *arena, AstNode **initializers,
                              size_t init_count, Expr *condition,
                              Expr *optional, AstNode *body, size_t line,
                              size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_LOOP, line, column);
  node->stmt.loop_stmt.condition = condition;
  node->stmt.loop_stmt.optional = optional;
  node->stmt.loop_stmt.body = body;
  node->stmt.loop_stmt.initializer = initializers;
  node->stmt.loop_stmt.init_count = init_count;
  return node;
}

AstNode *create_loop_stmt(ArenaAllocator *arena, Expr *condition,
                          Expr *optional, AstNode *body, size_t line,
                          size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_LOOP, line, column);
  node->stmt.loop_stmt.condition = condition;
  node->stmt.loop_stmt.optional = optional;
  node->stmt.loop_stmt.initializer = NULL; // No initializers for standard loops
  node->stmt.loop_stmt.init_count = 0;
  node->stmt.loop_stmt.body = body;
  return node;
}

AstNode *create_return_stmt(ArenaAllocator *arena, Expr *value, size_t line,
                            size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_RETURN, line, column);
  node->stmt.return_stmt.value = value;
  return node;
}

AstNode *create_block_stmt(ArenaAllocator *arena, AstNode **statements,
                           size_t stmt_count, size_t line, size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_BLOCK, line, column);
  node->stmt.block.statements = statements;
  node->stmt.block.stmt_count = stmt_count;
  return node;
}

AstNode *create_print_stmt(ArenaAllocator *arena, Expr **expressions,
                           size_t expr_count, bool ln, size_t line,
                           size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_PRINT, line, column);
  node->stmt.print_stmt.expressions = expressions;
  node->stmt.print_stmt.expr_count = expr_count;
  node->stmt.print_stmt.ln = ln;
  return node;
}

AstNode *create_break_continue_stmt(ArenaAllocator *arena, bool is_continue,
                                    size_t line, size_t column) {
  AstNode *node =
      create_stmt_node(arena, AST_STMT_BREAK_CONTINUE, line, column);
  node->stmt.break_continue.is_continue = is_continue;
  return node;
}

AstNode *create_defer_stmt(ArenaAllocator *arena, AstNode *statement,
                           size_t line, size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_DEFER, line, column);
  node->stmt.defer_stmt.statement = statement;
  return node;
}

AstNode *create_switch_stmt(ArenaAllocator *arena, AstNode *condition,
                            AstNode **cases, size_t case_count,
                            AstNode *default_case, size_t line, size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_SWITCH, line, column);
  node->stmt.switch_stmt.condition = condition;
  node->stmt.switch_stmt.case_count = case_count;
  node->stmt.switch_stmt.cases = cases;
  node->stmt.switch_stmt.default_case = default_case;
  return node;
}

AstNode *create_case_stmt(ArenaAllocator *arena, AstNode **values,
                          size_t value_count, AstNode *body, size_t line,
                          size_t column) {
  AstNode *node = create_stmt_node(arena, AST_STMT_CASE, line, column);
  node->stmt.case_clause.values = values;
  node->stmt.case_clause.value_count = value_count;
  node->stmt.case_clause.body = body;
  return node;
}

AstNode *create_default_stmt(ArenaAllocator *arena, AstNode *body, size_t line,
                             size_t column) {
  AstNode *node = create_stmt(arena, AST_STMT_DEFAULT, line, column);
  node->stmt.default_clause.body = body;
  return node;
}
