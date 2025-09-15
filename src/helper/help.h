/**
 * @file help.h
 * @brief Declarations for command-line parsing, file reading, and build
 * configuration.
 *
 * Provides functions to parse arguments, read files, print
 * help/version/license, and print token debug info.
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../ast/ast.h"
#include "../c_libs/memory/memory.h"
#include "../lexer/lexer.h"
#include "../llvm/llvm.h"

/** Macro to access token at index in a token growable array */
#define TOKEN_AT(i) (((Token *)tokens.data)[(i)])
#define MAX_TOKENS 100

#define BAR_WIDTH 40

/** Enable debug logs for arena allocator (comment to disable) */
#define DEBUG_ARENA_ALLOC 1

/** Error codes returned by the compiler */
typedef enum {
  ARGC_ERROR = 1,
  FILE_ERROR = 2,
  MEMORY_ERROR = 3,
  LEXER_ERROR = 4,
  PARSER_ERROR = 5,
  RUNTIME_ERROR = 6,
  UNKNOWN_ERROR = 99
} ErrorCode;

/**
 * @brief Configuration structure to hold build options parsed from CLI.
 */

typedef struct {
  const char *filepath;
  const char *name;
  bool save;
  bool clean;
  bool check_mem;
  GrowableArray files; // Change from char** to GrowableArray
  size_t file_count;   // Keep for convenience, or remove and use files.count

  Token *tokens;
  size_t token_count;
} BuildConfig;

bool check_argc(int argc, int expected);
const char *read_file(const char *filename);

int print_help();
int print_version();
int print_license();

AstNode *lex_and_parse_file(const char *path, ArenaAllocator *allocator,
                            BuildConfig *config);

bool parse_args(int argc, char *argv[], BuildConfig *config,
                ArenaAllocator *arena);
bool run_build(BuildConfig config, ArenaAllocator *allocator);

void print_token(const Token *t);

void print_progress(int step, int total, const char *stage);
void ensure_clean_line();

bool link_with_ld(const char *obj_filename, const char *exe_filename);
bool get_gcc_file_path(const char *filename, char *buffer, size_t buffer_size);
bool get_lib_paths(char *buffer, size_t buffer_size);
bool link_with_ld_simple(const char *obj_filename, const char *exe_filename);
bool link_object_files(const char *output_dir, const char *executable_name);
bool validate_module_system(CodeGenContext *ctx);
void save_module_output_files(CodeGenContext *ctx, const char *output_dir);
