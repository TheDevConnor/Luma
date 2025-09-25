#include "formatter.h"

void format_module_prep(FormatterContext *ctx, Stmt *module) {
  if (!module || module->type != AST_PREPROCESSOR_MODULE)
    return;

  write_string(ctx, "@module \"");
  if (module->preprocessor.module.name)
    write_string(ctx, module->preprocessor.module.name);
  write_string(ctx, "\"");
  write_newline(ctx);

  // Add blank line after module declaration
  write_newline(ctx);

  // Track if we have any @use statements to add spacing after them
  bool has_use_statements = false;
  size_t first_non_use_index = 0;

  // Find first non-use statement
  for (size_t i = 0; i < module->preprocessor.module.body_count; i++) {
    if (module->preprocessor.module.body[i]->type == AST_PREPROCESSOR_USE) {
      has_use_statements = true;
    } else {
      if (has_use_statements && first_non_use_index == 0) {
        first_non_use_index = i;
      }
    }
  }

  for (size_t i = 0; i < module->preprocessor.module.body_count; i++) {
    // Add blank line after all @use statements and before first declaration
    if (has_use_statements && i == first_non_use_index) {
      write_newline(ctx);
    }

    format_node(ctx, module->preprocessor.module.body[i]);
  }
}

void format_use_prep(FormatterContext *ctx, Stmt *use) {
  write_string(ctx, "@use \"");
  write_string(ctx, use->preprocessor.use.module_name);
  write_string(ctx, "\" as ");
  write_string(ctx, use->preprocessor.use.alias);
  write_newline(ctx);
}

void format_function_definition(FormatterContext *ctx, Stmt *stmt) {
  if (stmt->stmt.func_decl.is_public) {
    write_string(ctx, "pub ");
  } else {
    write_string(ctx, "priv ");
  }

  write_string(ctx, "const ");

  // Write function name - adjust based on your AST structure
  if (stmt->stmt.func_decl.name) {
    write_string(ctx, stmt->stmt.func_decl.name);
  }

  write_string(ctx, " = fn ");

  // Format parameters
  write_string(ctx, "(");
  if (stmt->stmt.func_decl.param_count > 0) {
    for (int i = 0; i < stmt->stmt.func_decl.param_count; i++) {
      if (i > 0) {
        write_string(ctx, ",");
        if (ctx->config.space_after_comma) {
          write_space(ctx);
        }
      }
      // Format parameter: name: type
      if (stmt->stmt.func_decl.param_names[i]) {
        write_string(ctx, stmt->stmt.func_decl.param_names[i]);
        write_string(ctx, ": ");
        // Format type
        if (stmt->stmt.func_decl.param_types[i]) {
          format_type(ctx, stmt->stmt.func_decl.param_types[i]);
        }
      }
    }
  }
  write_string(ctx, ")");

  // Format return type if present
  if (stmt->stmt.func_decl.return_type) {
    write_space(ctx);
    format_type(ctx, stmt->stmt.func_decl.return_type);
  }

  write_space(ctx);
  write_string(ctx, "{");
  write_newline(ctx);

  increase_indent(ctx);

  // Format function body
  if (stmt->stmt.func_decl.body) {
    format_stmt(ctx, stmt->stmt.func_decl.body);
  }

  decrease_indent(ctx);
  write_string(ctx, "}");
  write_newline(ctx);
  write_newline(ctx);
}

void format_variable_declaration(FormatterContext *ctx, Stmt *stmt) {
  // Handle both const and let declarations
  if (!stmt->stmt.var_decl.is_mutable) {
    write_string(ctx, "const ");
  } else {
    write_string(ctx, "let ");
  }

  // Variable name
  if (stmt->stmt.var_decl.name) {
    write_string(ctx, stmt->stmt.var_decl.name);
  }

  // Type annotation
  if (stmt->stmt.var_decl.var_type) {
    write_string(ctx, ": ");
    format_type(ctx, stmt->stmt.var_decl.var_type);
  }

  // Initializer
  if (stmt->stmt.var_decl.initializer) {
    write_string(ctx, " = ");
    format_expr(ctx, stmt->stmt.var_decl.initializer);
  }

  write_string(ctx, ";");
  write_newline(ctx);
}

void format_field_definition(FormatterContext *ctx, Stmt *stmt) {
  write_string(ctx, stmt->stmt.field_decl.name);
  write_string(ctx, ": ");
  format_type(ctx, stmt->stmt.field_decl.type);
  write_string(ctx, ",");
  write_newline(ctx);
}

void format_struct_definition(FormatterContext *ctx, Stmt *stmt) {
  write_string(ctx, "const ");
  if (stmt->stmt.struct_decl.name) {
    write_string(ctx, stmt->stmt.struct_decl.name);
  }
  write_string(ctx, " = struct {");
  write_newline(ctx);

  if (stmt->stmt.struct_decl.is_public) {
    write_string(ctx, "pub:");
    write_newline(ctx);
    increase_indent(ctx);
  }

  for (int i = 0; i < stmt->stmt.struct_decl.public_count; i++) {
    format_stmt(ctx, stmt->stmt.struct_decl.public_members[i]);
  }

  if (!stmt->stmt.struct_decl.is_public) {
    write_string(ctx, "priv:");
    write_newline(ctx);
    increase_indent(ctx);
  }

  for (int i = 0; i < stmt->stmt.struct_decl.private_count; i++) {
    format_stmt(ctx, stmt->stmt.struct_decl.private_members[i]);
  }

  decrease_indent(ctx);
  write_string(ctx, "};");
  write_newline(ctx);
}

void format_enum_definition(FormatterContext *ctx, Stmt *stmt) {
  write_string(ctx, "const ");
  if (stmt->stmt.enum_decl.name) {
    write_string(ctx, stmt->stmt.enum_decl.name);
  }
  write_string(ctx, " = enum {");
  write_newline(ctx);

  increase_indent(ctx);

  for (int i = 0; i < stmt->stmt.enum_decl.member_count; i++) {
    write_string(ctx, stmt->stmt.enum_decl.members[i]);
    if (i < stmt->stmt.enum_decl.member_count - 1) {
      write_string(ctx, ",");
    }
    write_newline(ctx);
  }

  decrease_indent(ctx);
  write_string(ctx, "};");
  write_newline(ctx);
}

void format_if_statement(FormatterContext *ctx, Stmt *stmt) {
  write_string(ctx, "if ");
  write_string(ctx, "(");
  format_expr(ctx, stmt->stmt.if_stmt.condition);
  write_string(ctx, ")");

  if (!ctx->config.compact_blocks) {
    write_space(ctx);
  }

  write_string(ctx, "{");
  write_newline(ctx);
  increase_indent(ctx);

  // Format then block
  if (stmt->stmt.if_stmt.then_stmt) {
    format_stmt(ctx, stmt->stmt.if_stmt.then_stmt);
  }

  decrease_indent(ctx);
  write_string(ctx, "}");

  // Handle else if/else
  if (stmt->stmt.if_stmt.else_stmt) {
    write_string(ctx, " else ");
    if (stmt->stmt.if_stmt.else_stmt->type == AST_STMT_IF) {
      format_if_statement(ctx, stmt->stmt.if_stmt.else_stmt);
    } else {
      write_string(ctx, "{");
      write_newline(ctx);
      increase_indent(ctx);
      format_stmt(ctx, stmt->stmt.if_stmt.else_stmt);
      decrease_indent(ctx);
      write_string(ctx, "}");
    }
  }

  write_newline(ctx);
}

void format_loop_statement(FormatterContext *ctx, Stmt *stmt) {
  // Handle different types of loop statements based on Luma syntax
  write_string(ctx, "loop ");

  // Check if this is a for-style loop with initializers
  if (stmt->stmt.loop_stmt.init_count > 0) {
    write_string(ctx, "[");
    for (size_t i = 0; i < stmt->stmt.loop_stmt.init_count; i++) {
      if (i > 0) {
        write_string(ctx, ",");
        if (ctx->config.space_after_comma) {
          write_space(ctx);
        }
      }
      format_stmt(ctx, stmt->stmt.loop_stmt.initializer[i]);
    }
    write_string(ctx, "]");
  }

  // Format condition if present
  if (stmt->stmt.loop_stmt.condition) {
    write_string(ctx, "(");
    format_expr(ctx, stmt->stmt.loop_stmt.condition);
    write_string(ctx, ")");
  }

  // Format optional post-expression (like increment in for loops)
  if (stmt->stmt.loop_stmt.optional) {
    write_string(ctx, " : (");
    format_expr(ctx, stmt->stmt.loop_stmt.optional);
    write_string(ctx, ")");
  }

  write_string(ctx, " {");
  write_newline(ctx);
  increase_indent(ctx);

  if (stmt->stmt.loop_stmt.body) {
    format_stmt(ctx, stmt->stmt.loop_stmt.body);
  }

  decrease_indent(ctx);
  write_string(ctx, "}");
  write_newline(ctx);
}

void format_switch_statement(FormatterContext *ctx, Stmt *stmt) {
  write_string(ctx, "switch (");
  format_expr(ctx, stmt->stmt.switch_stmt.condition);
  write_string(ctx, ") {");
  write_newline(ctx);

  increase_indent(ctx);

  // Format all case statements
  for (size_t i = 0; i < stmt->stmt.switch_stmt.case_count; i++) {
    AstNode *case_node = stmt->stmt.switch_stmt.cases[i];
    if (case_node->type == AST_STMT_CASE) {
      // Format case values
      for (size_t j = 0; j < case_node->stmt.case_clause.value_count; j++) {
        if (j > 0) {
          write_string(ctx, ",");
          if (ctx->config.space_after_comma) {
            write_space(ctx);
          }
        }
        format_expr(ctx, case_node->stmt.case_clause.values[j]);
      }
      write_string(ctx, ":");
      write_newline(ctx);

      increase_indent(ctx);
      if (case_node->stmt.case_clause.body) {
        format_stmt(ctx, case_node->stmt.case_clause.body);
      }
      decrease_indent(ctx);
    }
  }

  // Format default case if present
  if (stmt->stmt.switch_stmt.default_case) {
    write_string(ctx, "_:");
    write_newline(ctx);
    increase_indent(ctx);
    if (stmt->stmt.switch_stmt.default_case->stmt.default_clause.body) {
      format_stmt(
          ctx, stmt->stmt.switch_stmt.default_case->stmt.default_clause.body);
    }
    decrease_indent(ctx);
  }

  decrease_indent(ctx);
  write_string(ctx, "}");
  write_newline(ctx);
}

void format_defer_statement(FormatterContext *ctx, Stmt *stmt) {
  write_string(ctx, "defer ");
  if (stmt->stmt.defer_stmt.statement->stmt.block.statements) {
    write_string(ctx, "{");
    write_newline(ctx);
    increase_indent(ctx);
    for (size_t i = 0;
         i < stmt->stmt.defer_stmt.statement->stmt.block.stmt_count; i++)
      format_node(ctx,
                  stmt->stmt.defer_stmt.statement->stmt.block.statements[i]);
    decrease_indent(ctx);
    write_string(ctx, "}");
  } else {
    format_stmt(ctx, stmt->stmt.defer_stmt.statement);
  }
  write_newline(ctx);
}

void format_break_continue_statement(FormatterContext *ctx, Stmt *stmt) {
  if (stmt->stmt.break_continue.is_continue)
    write_string(ctx, "continue;");
  else
    write_string(ctx, "break;");
  write_newline(ctx);
}

void format_expr_stmt(FormatterContext *ctx, Stmt *stmt) {
  format_expr(ctx, stmt->stmt.expr_stmt.expression);
  write_string(ctx, ";");
}

void format_return_statement(FormatterContext *ctx, Stmt *stmt) {
  write_string(ctx, "return ");
  format_expr(ctx, stmt->stmt.return_stmt.value);
  write_string(ctx, ";");
  write_newline(ctx);
}

void format_block_statement(FormatterContext *ctx, Stmt *stmt) {
  for (size_t i = 0; i < stmt->stmt.block.stmt_count; i++) {
    format_stmt(ctx, stmt->stmt.block.statements[i]);
  }
}

void format_output_statement(FormatterContext *ctx, Stmt *stmt) {
  if (stmt->stmt.print_stmt.ln) {
    write_string(ctx, "outputln(");
  } else {
    write_string(ctx, "output(");
  }

  for (size_t i = 0; i < stmt->stmt.print_stmt.expr_count; i++) {
    if (i > 0) {
      write_string(ctx, ",");
      if (ctx->config.space_after_comma) {
        write_space(ctx);
      }
    }
    format_expr(ctx, stmt->stmt.print_stmt.expressions[i]);
  }

  write_string(ctx, ");");
  write_newline(ctx);
}