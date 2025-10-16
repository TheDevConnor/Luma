#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform-specific includes
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#define mkstemp(template)                                                      \
  _mktemp_s(template, strlen(template) + 1)                                    \
      ? -1                                                                     \
      : _open(template, _O_CREAT | _O_EXCL | _O_RDWR, _S_IREAD | _S_IWRITE)
#define unlink _unlink
#define close _close
#else
#include <errno.h>  // for errno
#include <unistd.h> // for close(), unlink(), mkstemp()
#endif

#include "../../c_libs/error/error.h"
#include "../../helper/help.h"
#include "formatter.h"

FormatterConfig default_config = {
    .indent_size = 2,
    .use_tabs = false,
    .max_line_length = 50,
    .space_around_operator = true,
    .space_after_comma = true,
    .compact_blocks = false,
    .check_only = false,
    .write_in_place = false,
    .output_file = NULL,
};

void write_indent(FormatterContext *ctx) {
  if (!ctx->at_line_start)
    return;

  if (ctx->config.use_tabs) {
    for (int i = 0; i < ctx->current_indent; i++)
      fputc('\t', ctx->output);
    ctx->current_column = ctx->current_indent * 4;
  } else {
    int spaces = ctx->current_indent * ctx->config.indent_size;
    for (int i = 0; i < spaces; i++)
      fputc(' ', ctx->output);
    ctx->current_column = spaces;
  }
  ctx->at_line_start = false;
}

void write_string(FormatterContext *ctx, const char *str) {
  if (!str)
    return;
  write_indent(ctx);
  fputs(str, ctx->output);
  ctx->current_column += strlen(str);
}

void write_newline(FormatterContext *ctx) {
  fputc('\n', ctx->output);
  ctx->current_column = 0;
  ctx->at_line_start = true;
}

void write_space(FormatterContext *ctx) {
  if (!ctx->at_line_start) {
    fputc(' ', ctx->output);
    ctx->current_column++;
  }
}

void increase_indent(FormatterContext *ctx) { ctx->current_indent++; }
void decrease_indent(FormatterContext *ctx) { ctx->current_indent--; }

void format_node(FormatterContext *ctx, AstNode *node) {
  switch (node->type) {
  case AST_PROGRAM:
    format_program(ctx, node);
    break;
  case AST_PREPROCESSOR_USE:
    format_use_prep(ctx, node);
    break;
  default:
    format_stmt(ctx, node);
  }
}

void format_program(FormatterContext *ctx, Stmt *program) {
  for (size_t i = 0; i < program->stmt.program.module_count; i++) {
    if (i > 0)
      write_newline(ctx);
    format_module_prep(ctx, program->stmt.program.modules[i]);
  }
}

void format_type(FormatterContext *ctx, Type *type) {
  switch (type->type) {
  case AST_TYPE_BASIC:
    write_string(ctx, type->type_data.basic.name);
    break;
  case AST_TYPE_POINTER:
    write_string(ctx, "*");
    format_type(ctx, type->type_data.pointer.pointee_type);
    break;
  case AST_TYPE_ARRAY: {
    // Luma array syntax: [type; size]
    write_string(ctx, "[");
    format_type(ctx, type->type_data.array.element_type);
    write_string(ctx, ";");
    if (ctx->config.space_after_comma) {
      write_space(ctx);
    }
    format_expr(ctx, type->type_data.array.size);
    write_string(ctx, "]");
    break;
  }
  case AST_TYPE_FUNCTION: {
    // Function type syntax: fn(param_types) return_type
    write_string(ctx, "fn(");
    for (size_t i = 0; i < type->type_data.function.param_count; i++) {
      if (i > 0) {
        write_string(ctx, ",");
        if (ctx->config.space_after_comma) {
          write_space(ctx);
        }
      }
      format_type(ctx, type->type_data.function.param_types[i]);
    }
    write_string(ctx, ")");
    if (type->type_data.function.return_type) {
      write_space(ctx);
      format_type(ctx, type->type_data.function.return_type);
    }
    break;
  }
  case AST_TYPE_STRUCT:
    // Just write the struct name for type references
    write_string(ctx, type->type_data.struct_type.name);
    break;
  default:
    return;
  }
}

void format_stmt(FormatterContext *ctx, Stmt *stmt) {
  switch (stmt->type) {
  case AST_STMT_FUNCTION:
    format_function_definition(ctx, stmt);
    break;
  case AST_STMT_VAR_DECL:
    format_variable_declaration(ctx, stmt);
    break;
  case AST_STMT_STRUCT:
    format_struct_definition(ctx, stmt);
    break;
  case AST_STMT_FIELD_DECL:
    format_field_definition(ctx, stmt);
    break;
  case AST_STMT_PRINT:
    format_output_statement(ctx, stmt);
    break;
  case AST_STMT_ENUM:
    format_enum_definition(ctx, stmt);
    break;
  case AST_STMT_IF:
    format_if_statement(ctx, stmt);
    break;
  case AST_STMT_LOOP:
    format_loop_statement(ctx, stmt);
    break;
  case AST_STMT_SWITCH:
    format_switch_statement(ctx, stmt);
    break;
  case AST_STMT_DEFER:
    format_defer_statement(ctx, stmt);
    break;
  case AST_STMT_BLOCK:
    format_block_statement(ctx, stmt);
    break;
  case AST_STMT_RETURN:
    format_return_statement(ctx, stmt);
    break;
  case AST_STMT_BREAK_CONTINUE:
    format_break_continue_statement(ctx, stmt);
    break;
  case AST_STMT_EXPRESSION:
    format_expr_stmt(ctx, stmt);
    break;
  default:
    // Handle unknown statement types
    break;
  }
}

void format_expr(FormatterContext *ctx, Expr *expr) {
  switch (expr->type) {
  case AST_EXPR_BINARY:
    format_binary_expression(ctx, expr);
    break;
  case AST_EXPR_UNARY:
    format_unary_expression(ctx, expr);
    break;
  case AST_EXPR_CALL:
    format_function_call(ctx, expr);
    break;
  case AST_EXPR_LITERAL:
    format_literal_expression(ctx, expr);
    break;
  case AST_EXPR_IDENTIFIER:
    format_identifier_expression(ctx, expr);
    break;
  case AST_EXPR_ARRAY:
    format_array(ctx, expr);
    break;
  case AST_EXPR_MEMBER:
    format_member(ctx, expr);
    break;
  case AST_EXPR_INDEX:
    format_index_expression(ctx, expr);
    break;
  case AST_EXPR_GROUPING:
    format_grouping_expression(ctx, expr);
    break;
  case AST_EXPR_DEREF:
    format_deref_expression(ctx, expr);
    break;
  case AST_EXPR_ADDR:
    format_addr_expression(ctx, expr);
    break;
  case AST_EXPR_ALLOC:
    format_alloc_expression(ctx, expr);
    break;
  case AST_EXPR_MEMCPY:
    format_memcpy_expression(ctx, expr);
    break;
  case AST_EXPR_FREE:
    format_free_expression(ctx, expr);
    break;
  case AST_EXPR_CAST:
    format_cast_expression(ctx, expr);
    break;
  case AST_EXPR_SIZEOF:
    format_sizeof_expression(ctx, expr);
    break;
  case AST_EXPR_ASSIGNMENT: {
    format_expr(ctx, expr->expr.assignment.target);
    if (ctx->config.space_around_operator) {
      write_space(ctx);
    }
    write_string(ctx, "=");
    if (ctx->config.space_around_operator) {
      write_space(ctx);
    }
    format_expr(ctx, expr->expr.assignment.value);
    break;
  }
  case AST_EXPR_TERNARY: {
    format_expr(ctx, expr->expr.ternary.condition);
    write_string(ctx, " ? ");
    format_expr(ctx, expr->expr.ternary.then_expr);
    write_string(ctx, " : ");
    format_expr(ctx, expr->expr.ternary.else_expr);
    break;
  }
  case AST_EXPR_RANGE: {
    // Handle range expressions like 0..10
    format_expr(ctx, expr->expr.binary.left);
    write_string(ctx, "..");
    format_expr(ctx, expr->expr.binary.right);
    break;
  }
  default:
    // Handle unknown expression types
    break;
  }
}

bool format_luma_code(const char *input_path, const char *output_path,
                      FormatterConfig config, ArenaAllocator *allocator) {

  // Parse using your existing function
  BuildConfig build_config = {0};
  build_config.filepath = input_path;

  AstNode *ast = lex_and_parse_file(input_path, allocator, &build_config);
  if (!ast) {
    fprintf(stderr, "Failed to parse input file: %s\n", input_path);
    return false;
  }

  // Check for parse errors
  if (error_report()) {
    return false;
  }

  // Open output file
  FILE *output = stdout;
  if (output_path) {
    output = fopen(output_path, "w");
    if (!output) {
      fprintf(stderr, "Failed to open output file: %s\n", output_path);
      return false;
    }
  }

  // Initialize formatter context
  FormatterContext ctx = {.output = output,
                          .current_indent = 0,
                          .current_column = 0,
                          .at_line_start = true,
                          .config = config,
                          .arena = allocator};

  // Format the AST
  format_node(&ctx, ast);

  // Cleanup
  if (output != stdout) {
    fclose(output);
  }

  return true;
}

static int create_temp_file(char *template_path) {
#ifdef _WIN32
  if (_mktemp_s(template_path, strlen(template_path) + 1) != 0) {
    return -1;
  }
  return _open(template_path, _O_CREAT | _O_EXCL | _O_RDWR, 0600);
#else
  return mkstemp(template_path);
#endif
}

bool check_formatting(const char *filepath, FormatterConfig config,
                      ArenaAllocator *allocator) {

  // Read original file using your existing read_file function
  const char *original = read_file(filepath);
  if (!original) {
    return false;
  }

  // Create temporary file for formatted output
#ifdef _WIN32
  char temp_path[MAX_PATH];
  if (GetTempPath(MAX_PATH, temp_path) == 0) {
    free((void *)original);
    return false;
  }
  strcat_s(temp_path, MAX_PATH, "luma_fmt_XXXXXX");
#else
  char temp_path[] = "/tmp/luma_fmt_XXXXXX";
#endif

  int temp_fd = create_temp_file(temp_path);
  if (temp_fd == -1) {
    free((void *)original);
    return false;
  }
  close(temp_fd);

  // Format to temporary file
  bool success = format_luma_code(filepath, temp_path, config, allocator);
  if (!success) {
    unlink(temp_path);
    free((void *)original);
    return false;
  }

  // Read formatted output using your existing read_file function
  const char *formatted = read_file(temp_path);
  unlink(temp_path);

  if (!formatted) {
    free((void *)original);
    return false;
  }

  // Compare - return true if files differ (need formatting)
  bool files_differ = strcmp(original, formatted) != 0;

  // Free both malloced buffers from read_file
  free((void *)original);
  free((void *)formatted);

  return files_differ;
}

void print_usage(const char *program_name) {
  printf("Usage: %s [OPTIONS] [FILES...]\n\n", program_name);
  printf("Luma Code Formatter - Format your Luma source files\n\n");
  printf("Options:\n");
  printf("  -i, --in-place           Edit files in place\n");
  printf("  -c, --check              Check if input is formatted (exit 1 if "
         "not)\n");
  printf("  -o, --output FILE        Output to FILE (default: stdout)\n");
  printf(
      "  --indent-size N          Number of spaces per indent (default: 4)\n");
  printf("  --use-tabs               Use tabs instead of spaces\n");
  printf("  --max-line-length N      Maximum line length (default: 100)\n");
  printf("  --no-space-operators     Don't add spaces around operators\n");
  printf("  --no-space-comma         Don't add spaces after commas\n");
  printf("  --compact                Use compact block formatting\n");
  printf("  -h, --help               Show this help message\n");
  printf("\nExamples:\n");
  printf("  %s file.luma                   # Format to stdout\n", program_name);
  printf("  %s -i file.luma                # Format in-place\n", program_name);
  printf("  %s -c src/*.luma               # Check formatting\n", program_name);
  printf("  %s -o formatted.luma file.luma # Format to specific file\n",
         program_name);
}