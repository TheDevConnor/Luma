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
#include <time.h>

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
  bool format;          // Add format flag
  bool format_check;    // Add format check flag
  bool format_in_place; // Add in-place formatting flag
  GrowableArray files;  // Change from char** to GrowableArray
  size_t file_count;    // Keep for convenience, or remove and use files.count

  GrowableArray tokens;
  size_t token_count;
} BuildConfig;

typedef struct {
  clock_t start_time;
  clock_t end_time;
  double elapsed_ms;
} CompileTimer;

bool check_argc(int argc, int expected);
const char *read_file(const char *filename);

int print_help();
int print_version();
int print_license();

AstNode *lex_and_parse_file(const char *path, ArenaAllocator *allocator,
                            BuildConfig *config);

bool PathExist(const char *p);
bool PathIsDir(const char *p);
bool parse_args(int argc, char *argv[], BuildConfig *config,
                ArenaAllocator *arena);
bool run_build(BuildConfig config, ArenaAllocator *allocator);
bool run_formatter(BuildConfig config,
                   ArenaAllocator *allocator); // Add formatter function

void print_token(const Token *t);

void print_progress(int step, int total, const char *stage);
void ensure_clean_line();

void timer_start(CompileTimer *timer);
void timer_stop(CompileTimer *timer);
double timer_get_elapsed_ms(CompileTimer *timer);
void print_progress_with_time(int step, int total, const char *stage,
                              CompileTimer *timer);

bool link_with_ld(const char *obj_filename, const char *exe_filename);
bool get_gcc_file_path(const char *filename, char *buffer, size_t buffer_size);
bool get_lib_paths(char *buffer, size_t buffer_size);
bool link_with_ld_simple(const char *obj_filename, const char *exe_filename);
bool link_object_files(const char *output_dir, const char *executable_name);
bool validate_module_system(CodeGenContext *ctx);
void save_module_output_files(CodeGenContext *ctx, const char *output_dir);
