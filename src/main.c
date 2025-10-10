/**
 * @file main.c
 * @brief Entry point for the Lux compiler/interpreter build process.
 *
 * This program parses command-line arguments, sets up a memory arena,
 * and runs the build process on a provided source file.
 *
 * ## Responsibilities
 * - Validates command-line arguments.
 * - Parses build configuration options.
 * - Initializes the arena allocator for fast memory management.
 * - Executes the build process for the provided source file.
 *
 * ## Usage
 * ```bash
 * lux build <source_file>
 * ```
 *
 * Example:
 * ```bash
 * lux build hello.lx
 * ```
 */

#include "c_libs/memory/memory.h"
#include "helper/help.h"
#include "lsp/lsp.h"

/**
 * @brief Program entry point.
 *
 * This function:
 * 1. Validates the number of arguments.
 * 2. Parses build configuration from CLI arguments.
 * 3. Initializes an arena allocator for memory management.
 * 4. Executes the build process.
 * 5. Cleans up resources and returns the appropriate exit code.
 *
 * @param argc Argument count from the command line.
 * @param argv Argument vector from the command line.
 * @return 0 if the build succeeded, non-zero on failure.
 */
int main(int argc, char *argv[]) {
  // Step 1: Validate argument count
  if (!check_argc(argc, 1))
    return ARGC_ERROR;

  // Step 2: Initialize build configuration
  BuildConfig config = {0};

  // Step 2.5: Initialize arena allocator (1MB initial size)
  ArenaAllocator allocator;
  arena_allocator_init(&allocator, 1024 * 1024);

  // Step 3: Parse CLI arguments into config
  if (!parse_args(argc, argv, &config, &allocator)) {
    // Commands like --help or --version are handled in parse_args
    return ARGC_ERROR;
  }

  // Step 4: Ensure a source file was provided
  if (!config.filepath) {
    fprintf(stderr, "No source file provided.\n");
    return ARGC_ERROR;
  }

  // Step 6: Run build process
  bool success;

  if (config.format || config.format_check || config.format_in_place) {
    success = run_formatter(config, &allocator);
  } else {
    // Regular build process
    success = run_build(config, &allocator);
  }

  // Step 7: Clean up resources
  arena_destroy(&allocator);

  // Step 8: Return exit status
  return success ? 0 : 1;
}
