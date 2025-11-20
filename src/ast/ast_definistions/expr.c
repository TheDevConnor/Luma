#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ast.h"

AstNode *create_literal_expr(ArenaAllocator *arena, LiteralType lit_type,
                             void *value, size_t line, size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_LITERAL, line, column);
  node->expr.literal.lit_type = lit_type;

  if (value == NULL && lit_type != LITERAL_NULL) {
    fprintf(stderr, "Error: NULL value passed to create_literal_expr with "
                    "non-null literal type.\n");
    exit(1);
  }

  switch (lit_type) {
  case LITERAL_INT:
    node->expr.literal.value.int_val = *(long long *)value;
    break;
  case LITERAL_FLOAT:
    node->expr.literal.value.float_val = *(double *)value;
    break;
  case LITERAL_STRING:
    node->expr.literal.value.string_val = arena_strdup(arena, (char *)value);
    break;
  case LITERAL_CHAR:
    node->expr.literal.value.char_val = *(char *)value;
    break;
  case LITERAL_BOOL:
    node->expr.literal.value.bool_val = *(bool *)value;
    break;
  case LITERAL_NULL:
    break;
  case LITERAL_IDENT:
    node->expr.literal.value.string_val = arena_strdup(arena, (char *)value);
    break;
  default:
    fprintf(stderr,
            "Error: Unsupported literal type %d in create_literal_expr.\n",
            lit_type);
    exit(1);
    break;
  }

  return node;
}

AstNode *create_identifier_expr(ArenaAllocator *arena, const char *name,
                                size_t line, size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_IDENTIFIER, line, column);
  node->expr.identifier.name = arena_strdup(arena, name);
  return node;
}

AstNode *create_binary_expr(ArenaAllocator *arena, BinaryOp op, AstNode *left,
                            AstNode *right, size_t line, size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_BINARY, line, column);
  node->expr.binary.op = op;
  node->expr.binary.left = left;
  node->expr.binary.right = right;
  return node;
}

AstNode *create_unary_expr(ArenaAllocator *arena, UnaryOp op, AstNode *operand,
                           size_t line, size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_UNARY, line, column);
  node->expr.unary.op = op;
  node->expr.unary.operand = operand;
  return node;
}

AstNode *create_call_expr(ArenaAllocator *arena, AstNode *callee,
                          AstNode **args, size_t arg_count, size_t line,
                          size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_CALL, line, column);
  node->expr.call.callee = callee;
  node->expr.call.args = args;
  node->expr.call.arg_count = arg_count;
  return node;
}

AstNode *create_assignment_expr(ArenaAllocator *arena, AstNode *target,
                                AstNode *value, size_t line, size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_ASSIGNMENT, line, column);
  node->expr.assignment.target = target;
  node->expr.assignment.value = value;
  return node;
}

AstNode *create_ternary_expr(ArenaAllocator *arena, AstNode *condition,
                             AstNode *then_expr, AstNode *else_expr,
                             size_t line, size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_TERNARY, line, column);
  node->expr.ternary.condition = condition;
  node->expr.ternary.then_expr = then_expr;
  node->expr.ternary.else_expr = else_expr;
  return node;
}

AstNode *create_member_expr(ArenaAllocator *arena, AstNode *object,
                            bool is_compiletime, const char *member,
                            size_t line, size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_MEMBER, line, column);
  node->expr.member.is_compiletime = is_compiletime;
  node->expr.member.object = object;
  node->expr.member.member = arena_strdup(arena, member);
  return node;
}

AstNode *create_index_expr(ArenaAllocator *arena, AstNode *object,
                           AstNode *index, size_t line, size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_INDEX, line, column);
  node->expr.index.object = object;
  node->expr.index.index = index;
  return node;
}

AstNode *create_grouping_expr(ArenaAllocator *arena, AstNode *expr, size_t line,
                              size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_GROUPING, line, column);
  node->expr.grouping.expr = expr;
  return node;
}

AstNode *create_array_expr(ArenaAllocator *arena, AstNode **elements,
                           size_t element_count, size_t line, size_t column) {
  AstNode *node = create_expr(arena, AST_EXPR_ARRAY, line, column);
  node->expr.array.elements = elements;
  node->expr.array.element_count = element_count;
  node->expr.array.target_size = 0; // Default to 0 (no explicit size)
  return node;
}

AstNode *create_deref_expr(ArenaAllocator *arena, Expr *object, size_t line,
                           size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_DEREF, line, col);
  node->expr.deref.object = object;
  return node;
}

AstNode *create_addr_expr(ArenaAllocator *arena, Expr *object, size_t line,
                          size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_ADDR, line, col);
  node->expr.addr.object = object;
  return node;
}

AstNode *create_alloc_expr(ArenaAllocator *arena, Expr *size, size_t line,
                           size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_ALLOC, line, col);
  node->expr.alloc.size = size;
  return node;
}

AstNode *create_memcpy_expr(ArenaAllocator *arena, Expr *to, Expr *from,
                            Expr *size, size_t line, size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_MEMCPY, line, col);
  node->expr.memcpy.to = to;
  node->expr.memcpy.from = from;
  node->expr.memcpy.size = size;
  return node;
}

AstNode *create_free_expr(ArenaAllocator *arena, Expr *ptr, size_t line,
                          size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_FREE, line, col);
  node->expr.free.ptr = ptr;
  return node;
}

AstNode *create_cast_expr(ArenaAllocator *arena, Expr *type, Expr *castee,
                          size_t line, size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_CAST, line, col);
  node->expr.cast.type = type;
  node->expr.cast.castee = castee;
  return node;
}

AstNode *create_input_expr(ArenaAllocator *arena, Expr *type, Expr *msg,
                           size_t line, size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_INPUT, line, col);
  node->expr.input.type = type;
  node->expr.input.msg = msg;
  return node;
}

AstNode *create_system_expr(ArenaAllocator *arena, Expr *command, size_t line,
                            size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_SYSTEM, line, col);
  node->expr._system.command = command;
  return node;
}

AstNode *create_syscall_expr(ArenaAllocator *arena, Expr **args, size_t count,
                             size_t line, size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_SYSCALL, line, col);
  node->expr.syscall.args = args;
  node->expr.syscall.count = count;
  return node;
}

AstNode *create_sizeof_expr(ArenaAllocator *arena, Expr *object, bool is_type,
                            size_t line, size_t col) {
  AstNode *node = create_expr(arena, AST_EXPR_SIZEOF, line, col);
  node->expr.size_of.object = object;
  node->expr.size_of.is_type = is_type;
  return node;
}

Expr *create_struct_expr(ArenaAllocator *arena, char *name, char **field_names,
                         AstNode **field_values, size_t field_count, int line,
                         int col) {
  AstNode *node = create_expr(arena, AST_EXPR_STRUCT, line, col);
  node->expr.struct_expr.name = name;
  node->expr.struct_expr.field_names = field_names;
  node->expr.struct_expr.field_value = field_values;
  node->expr.struct_expr.field_count = field_count;
  return node;
}
