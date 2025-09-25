#include "formatter.h"

void format_binary_expression(FormatterContext *ctx, Expr *expr) {
  format_expr(ctx, expr->expr.binary.left);

  if (ctx->config.space_around_operator) {
    write_space(ctx);
  }

  // Map operator tokens to strings
  switch (expr->expr.binary.op) {
  case BINOP_ADD:
    write_string(ctx, "+");
    break;
  case BINOP_SUB:
    write_string(ctx, "-");
    break;
  case BINOP_MUL:
    write_string(ctx, "*");
    break;
  case BINOP_DIV:
    write_string(ctx, "/");
    break;
  case BINOP_MOD:
    write_string(ctx, "%");
    break;
  case BINOP_POW:
    write_string(ctx, "**");
    break;
  case BINOP_EQ:
    write_string(ctx, "==");
    break;
  case BINOP_NE:
    write_string(ctx, "!=");
    break;
  case BINOP_LT:
    write_string(ctx, "<");
    break;
  case BINOP_LE:
    write_string(ctx, "<=");
    break;
  case BINOP_GT:
    write_string(ctx, ">");
    break;
  case BINOP_GE:
    write_string(ctx, ">=");
    break;
  case BINOP_AND:
    write_string(ctx, "&&");
    break;
  case BINOP_OR:
    write_string(ctx, "||");
    break;
  case BINOP_BIT_AND:
    write_string(ctx, "&");
    break;
  case BINOP_BIT_OR:
    write_string(ctx, "|");
    break;
  case BINOP_BIT_XOR:
    write_string(ctx, "^");
    break;
  case BINOP_SHL:
    write_string(ctx, "<<");
    break;
  case BINOP_SHR:
    write_string(ctx, ">>");
    break;
  case BINOP_RANGE:
    write_string(ctx, "..");
    break;
  }

  if (ctx->config.space_around_operator) {
    write_space(ctx);
  }

  format_expr(ctx, expr->expr.binary.right);
}

void format_unary_expression(FormatterContext *ctx, Expr *expr) {
    // Handle prefix unary operators
    switch (expr->expr.unary.op) {
    case UNOP_NOT:
        write_string(ctx, "!");
        break;
    case UNOP_NEG:
        write_string(ctx, "-");
        break;
    case UNOP_POS:
        write_string(ctx, "+");
        break;
    case UNOP_BIT_NOT:
        write_string(ctx, "~");
        break;
    case UNOP_PRE_INC:
        write_string(ctx, "++");
        break;
    case UNOP_PRE_DEC:
        write_string(ctx, "--");
        break;
    case UNOP_DEREF:
        write_string(ctx, "*");
        break;
    case UNOP_ADDR:
        write_string(ctx, "&");
        break;
    }
    
    // For postfix operators, we need to format the operand first
    if (expr->expr.unary.op == UNOP_POST_INC || expr->expr.unary.op == UNOP_POST_DEC) {
        format_expr(ctx, expr->expr.unary.operand);
        if (expr->expr.unary.op == UNOP_POST_INC) {
            write_string(ctx, "++");
        } else {
            write_string(ctx, "--");
        }
    } else {
        format_expr(ctx, expr->expr.unary.operand);
    }
}


void format_function_call(FormatterContext *ctx, Expr *expr) {
  // Format the function being called
  format_expr(ctx, expr->expr.call.callee);
  
  write_string(ctx, "(");
  
  // Format arguments
  for (int i = 0; i < expr->expr.call.arg_count; i++) {
    if (i > 0) {
      write_string(ctx, ",");
      if (ctx->config.space_after_comma) {
        write_space(ctx);
      }
    }
    format_expr(ctx, expr->expr.call.args[i]);
  }
  
  write_string(ctx, ")");
}

void format_literal_expression(FormatterContext *ctx, Expr *expr) {
  switch (expr->expr.literal.lit_type) {
  case LITERAL_INT: {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%lld", expr->expr.literal.value.int_val);
    write_string(ctx, buffer);
    break;
  }
  case LITERAL_FLOAT: {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%g", expr->expr.literal.value.float_val);
    write_string(ctx, buffer);
    break;
  }
  case LITERAL_STRING:
    write_string(ctx, "\"");
    write_string(ctx, expr->expr.literal.value.string_val);
    write_string(ctx, "\"");
    break;
  case LITERAL_CHAR:
    write_string(ctx, "'");
    fputc(expr->expr.literal.value.char_val, ctx->output);
    write_string(ctx, "'");
    break;
  case LITERAL_BOOL:
    write_string(ctx, expr->expr.literal.value.bool_val ? "true" : "false");
    break;
  }
}

void format_identifier_expression(FormatterContext *ctx, Expr *expr) {
  write_string(ctx, expr->expr.identifier.name);
}

void format_array(FormatterContext *ctx, Expr *expr) {
    write_string(ctx, "[");
    
    for (size_t i = 0; i < expr->expr.array.element_count; i++) {
        if (i > 0) {
            write_string(ctx, ",");
            if (ctx->config.space_after_comma) {
                write_space(ctx);
            }
        }
        format_expr(ctx, expr->expr.array.elements[i]);
    }
    
    write_string(ctx, "]");
}

void format_member(FormatterContext *ctx, Expr *expr) {
    format_expr(ctx, expr->expr.member.object);
    
    if (expr->expr.member.is_compiletime) {
        write_string(ctx, "::");
    } else {
        write_string(ctx, ".");
    }
    
    write_string(ctx, expr->expr.member.member);
}

void format_index_expression(FormatterContext *ctx, Expr *expr) {
    format_expr(ctx, expr->expr.index.object);
    write_string(ctx, "[");
    format_expr(ctx, expr->expr.index.index);
    write_string(ctx, "]");
}

void format_grouping_expression(FormatterContext *ctx, Expr *expr) {
    write_string(ctx, "(");
    format_expr(ctx, expr->expr.grouping.expr);
    write_string(ctx, ")");
}

void format_deref_expression(FormatterContext *ctx, Expr *expr) {
    write_string(ctx, "*");
    format_expr(ctx, expr->expr.deref.object);
}

void format_addr_expression(FormatterContext *ctx, Expr *expr) {
    write_string(ctx, "&");
    format_expr(ctx, expr->expr.addr.object);
}

void format_alloc_expression(FormatterContext *ctx, Expr *expr) {
    write_string(ctx, "alloc(");
    format_expr(ctx, expr->expr.alloc.size);
    write_string(ctx, ")");
}

void format_memcpy_expression(FormatterContext *ctx, Expr *expr) {
    write_string(ctx, "memcpy(");
    format_expr(ctx, expr->expr.memcpy.to);
    write_string(ctx, ",");
    if (ctx->config.space_after_comma) {
        write_space(ctx);
    }
    format_expr(ctx, expr->expr.memcpy.from);
    write_string(ctx, ",");
    if (ctx->config.space_after_comma) {
        write_space(ctx);
    }
    format_expr(ctx, expr->expr.memcpy.size);
    write_string(ctx, ")");
}

void format_free_expression(FormatterContext *ctx, Expr *expr) {
    write_string(ctx, "free(");
    format_expr(ctx, expr->expr.free.ptr);
    write_string(ctx, ")");
}

void format_cast_expression(FormatterContext *ctx, Expr *expr) {
    write_string(ctx, "cast<");
    format_type(ctx, expr->expr.cast.type);
    write_string(ctx, ">(");
    format_expr(ctx, expr->expr.cast.castee);
    write_string(ctx, ")");
}

void format_sizeof_expression(FormatterContext *ctx, Expr *expr) {
    if (expr->expr.size_of.is_type) {
        write_string(ctx, "sizeof<");
        format_expr(ctx, expr->expr.size_of.object);
        write_string(ctx, ">");
    } else {
        write_string(ctx, "sizeof(");
        format_expr(ctx, expr->expr.size_of.object);
        write_string(ctx, ")");
    }
}