#pragma once

#include <stdio.h>

#include "../../c_libs/memory/memory.h"
#include "../../ast/ast.h"

typedef struct  {
    int indent_size;
    bool use_tabs;
    int max_line_length;
    bool space_around_operator;
    bool space_after_comma;
    bool compact_blocks;
    bool check_only;        // --check flag
    bool write_in_place;    // -i flag
    const char *output_file;
} FormatterConfig;

typedef struct {
    FILE *output;
    int current_indent;
    int current_column;
    bool at_line_start;
    FormatterConfig config;
    ArenaAllocator *arena;
} FormatterContext;

// Core formatting functions
void write_indent(FormatterContext *ctx);
void write_string(FormatterContext *ctx, const char *str);
void write_newline(FormatterContext *ctx);
void write_space(FormatterContext *ctx);

void increase_indent(FormatterContext *ctx);
void decrease_indent(FormatterContext *ctx);

void format_node(FormatterContext *ctx, AstNode *node);
void format_stmt(FormatterContext *ctx, Stmt *stmt);
void format_expr(FormatterContext *ctx, Expr *expr);
void format_type(FormatterContext *ctx, Type *type);

void format_program(FormatterContext *ctx, Stmt *stmt);

// Preprocessor formatting
void format_module_prep(FormatterContext* ctx, Stmt* module);
void format_use_prep(FormatterContext* ctx, Stmt* use);

// Statement formatting
void format_function_definition(FormatterContext* ctx, Stmt* stmt);
void format_variable_declaration(FormatterContext* ctx, Stmt* stmt);
void format_struct_definition(FormatterContext* ctx, Stmt* stmt);
void format_field_definition(FormatterContext* ctx, Stmt* stmt);
void format_enum_definition(FormatterContext* ctx, Stmt* stmt);
void format_if_statement(FormatterContext* ctx, Stmt* stmt);
void format_loop_statement(FormatterContext* ctx, Stmt* stmt);
void format_switch_statement(FormatterContext* ctx, Stmt* stmt);
void format_defer_statement(FormatterContext* ctx, Stmt* stmt);
void format_block_statement(FormatterContext* ctx, Stmt* stmt);
void format_return_statement(FormatterContext* ctx, Stmt* stmt);
void format_break_continue_statement(FormatterContext* ctx, Stmt* stmt);
void format_output_statement(FormatterContext *ctx, Stmt *stmt);
void format_expr_stmt(FormatterContext* ctx, Stmt* stmt);

// Expression formatting
void format_binary_expression(FormatterContext* ctx, Expr* expr);
void format_unary_expression(FormatterContext* ctx, Expr* expr);
void format_function_call(FormatterContext* ctx, Expr* expr);
void format_literal_expression(FormatterContext* ctx, Expr* expr);
void format_identifier_expression(FormatterContext* ctx, Expr* expr);
void format_index_expression(FormatterContext* ctx, Expr* expr);
void format_grouping_expression(FormatterContext* ctx, Expr* expr);
void format_deref_expression(FormatterContext* ctx, Expr* expr);
void format_addr_expression(FormatterContext* ctx, Expr* expr);
void format_alloc_expression(FormatterContext* ctx, Expr* expr);
void format_memcpy_expression(FormatterContext* ctx, Expr* expr);
void format_free_expression(FormatterContext* ctx, Expr* expr);
void format_cast_expression(FormatterContext* ctx, Expr* expr);
void format_sizeof_expression(FormatterContext* ctx, Expr* expr);
void format_array(FormatterContext* ctx, Expr* expr);
void format_member(FormatterContext* ctx, Expr* expr);

// Main formatter functions
bool format_luma_code(const char* input_path, const char* output_path, 
                      FormatterConfig config, ArenaAllocator* allocator);
bool check_formatting(const char* filepath, FormatterConfig config, 
                      ArenaAllocator* allocator);
void print_usage(const char* program_name);