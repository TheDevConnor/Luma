/**
 * @file help.c
 * @brief Implements command-line argument parsing, file reading,
 * help/version/license printing, and token printing with color-highlighted
 * token types.
 */

#include <string.h>

// Platform-specific includes
#if defined(__MINGW32__) || defined(_WIN32)
    #include <windows.h>
    #include <process.h>  // For _spawnvp on Windows
#else
    #include <sys/wait.h>
    #include <unistd.h>
    #include <sys/stat.h>
#endif

#include "../c_libs/color/color.h"
#include "help.h"

/**
 * @brief Checks if the number of command-line arguments is at least expected.
 *
 * Prints usage info if not enough arguments.
 *
 * @param argc Actual argument count.
 * @param expected Minimum expected argument count.
 * @return true if argc >= expected, false otherwise.
 */
bool check_argc(int argc, int expected) {
  if (argc < expected) {
    fprintf(stderr, "Usage: %s <source_file>\n", "lux");
    return false;
  }
  return true;
}

/**
 * @brief Reads entire file content into a newly allocated buffer.
 *
 * The caller must free the returned buffer.
 *
 * @param filename Path to the file to read.
 * @return Pointer to null-terminated file content, or NULL on failure.
 */
const char *read_file(const char *filename) {
  FILE *file = fopen(filename, "rb");  // Use "rb" instead of "r"
  if (!file) {
    perror("Failed to open file");
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(size + 1);
  if (!buffer) {
    perror("Failed to allocate memory");
    fclose(file);
    return NULL;
  }

  size_t bytes_read = fread(buffer, 1, size, file);
  buffer[bytes_read] = '\0';  // Use bytes_read instead of size

  fclose(file);
  return buffer;
}

/**
 * @brief Prints help message describing usage and options.
 *
 * @return Always returns 0.
 */
int print_help() {
  printf("Usage: lux [options] <source_file>\n");
  printf("Options:\n");
  printf("  -v, --version      Show version information\n");
  printf("  -h, --help         Show this help message\n");
  printf("  -lc, --license     Show license information\n");
  printf("Crust Compiler Options:\n");
  printf("  -name <name>       Set the name of the build target\n");
  printf("  -save              Save the outputed llvm file\n");
  printf("  build <target>     Build the specified target\n");
  printf("  clean              Clean the build artifacts\n");
  printf("  -debug             builds a debug version and shows the allocators "
         "trace\n");
  printf("  -l, -link          Link lux files so that they can be used in "
         "other lux files\n");
  printf(
      "  --no-sanitize      Disable memory safety checks during compilation\n");
  return 0;
}

/**
 * @brief Prints version information.
 *
 * @return Always returns 0.
 */
int print_version() {
  printf("Lux Compiler v1.0\n");
  return 0;
}

/**
 * @brief Prints license information.
 *
 * @return Always returns 0.
 */
int print_license() {
  printf("Lux Compiler is licensed under the MIT License.\n");
  return 0;
}

// Platform-specific file system functions
#if defined(__MINGW32__) || defined(_WIN32)
    bool PathExist(const char* p){
        DWORD attr = GetFileAttributes(p);
        if (attr == INVALID_FILE_ATTRIBUTES) return false;
        return true;
    };
    bool PathIsDir(const char* p){
        DWORD attr = GetFileAttributes(p);
        return attr & FILE_ATTRIBUTE_DIRECTORY;
    };
#else
    bool PathExist(const char* p){
        struct stat FileAttrstat;
        if(stat(p, &FileAttrstat) != 0) return false;
        return S_ISREG(FileAttrstat.st_mode) || S_ISDIR(FileAttrstat.st_mode);
    };
    bool PathIsDir(const char* p){
        struct stat FileAttrstat;
        if(stat(p, &FileAttrstat) != 0) return false;
        return S_ISDIR(FileAttrstat.st_mode);
    };
#endif

/**
 * @brief Parses command-line arguments and configures the build.
 *
 * Supports options for version, help, license, build, save, clean, and debug.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param config Pointer to BuildConfig struct to fill.
 * @return false if help/version/license was printed or error occurred, true
 * otherwise.
 */
bool parse_args(int argc, char *argv[], BuildConfig *config,
                ArenaAllocator *arena) {
  if (!growable_array_init(&config->files, arena, 4, sizeof(char *))) {
    fprintf(stderr, "Failed to initialize files array\n");
    return false;
  }

  // set check_mem to true by default
  config->check_mem = true;
  config->filepath = false;
  for (int i = 1; i < argc; i++) {
    if(!PathExist(argv[i]) || argv[i][0] == '-'){
      if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
        return print_version(), false;
      else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        return print_help(), false;
      else if (strcmp(argv[i], "-lc") == 0 || strcmp(argv[i], "--license") == 0)
        return print_license(), false;
      else if (strcmp(argv[i], "-name") == 0 && i + 1 < argc)
        config->name = argv[++i];
      else if (strcmp(argv[i], "-save") == 0)
        config->save = true;
      else if (strcmp(argv[i], "-clean") == 0)
        config->clean = true;
      else if (strcmp(argv[i], "--no-sanitize") == 0 ||
               strcmp(argv[i], "-no-sanitize") == 0) {
        config->check_mem = false;
      } else if (strcmp(argv[i], "-debug") == 0) {
            // Placeholder for debug flag
      } else if (strcmp(argv[i], "-l") == 0 ||
                strcmp(argv[i], "-link") == 0) {
            // Collect files until next flag or end of args
        int start = i + 1;
        while (start < argc && argv[start][0] != '-') {
          char **slot = (char **)growable_array_push(&config->files);
          if (!slot) {
            fprintf(stderr, "Failed to add file to array\n");
            return false;
          }
          *slot = argv[start];
          config->file_count++;
          start++;
        }
        i = start - 1;
      } else {
        if(argv[i][0] == '-'){
          fprintf(stderr, "Unknown build option: %s\n", argv[i]);
          return false;
        }else{
          fprintf(stderr, "%s: No such file or directory\n", argv[i]);
          return false;
        }
      }  
    }else if (PathIsDir(argv[i])) {
      fprintf(stderr, "%s: Is a directory\n", argv[i]);
      return false;
    }else{
      config->filepath = argv[i];
    }
  }
  return true;
}

/**
 * @brief Prints a token's text and its token type with color formatting.
 *
 * @param t Pointer to the Token to print.
 */
void print_token(const Token *t) {
  printf("%.*s -> ", t->length, t->value);

  switch (t->type_) {
  case TOK_EOF:
    puts("EOF");
    break;
  case TOK_IDENTIFIER:
    printf(BOLD_GREEN("IDENTIFIER"));
    break;
  // ... other token types similarly formatted ...
  default:
    puts("UNKNOWN");
    break;
  }

  printf(" at line ");
  printf(COLORIZE(COLOR_RED, "%d"), t->line);
  printf(", column ");
  printf(COLORIZE(COLOR_RED, "%d"), t->col);
  printf("\n");
}

#if defined(__MINGW32__) || defined(_WIN32)
/**
 * @brief Links object file using system() on Windows
 *
 * @param obj_filename Path to the object file
 * @param exe_filename Path for the output executable
 * @return true if linking succeeded, false otherwise
 */
bool link_with_ld(const char *obj_filename, const char *exe_filename) {
  char command[1024];
  
  // On Windows, we'll use gcc for linking as it's simpler and more reliable
  snprintf(command, sizeof(command), "gcc \"%s\" -o \"%s\"", obj_filename, exe_filename);
  
  printf("Linking with: %s\n", command);
  return system(command) == 0;
}

bool link_with_ld_simple(const char *obj_filename, const char *exe_filename) {
  // On Windows, just use the same approach as link_with_ld
  return link_with_ld(obj_filename, exe_filename);
}

bool get_gcc_file_path(const char *filename, char *buffer, size_t buffer_size) {
  char command[256];
  snprintf(command, sizeof(command), "gcc -print-file-name=%s", filename);

  FILE *fp = _popen(command, "r");  // Use _popen on Windows
  if (!fp)
    return false;

  if (fgets(buffer, buffer_size, fp) != NULL) {
    // Remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }
    _pclose(fp);  // Use _pclose on Windows

    // Check if gcc actually found the file
    return strcmp(buffer, filename) != 0;
  }

  _pclose(fp);
  return false;
}

bool get_lib_paths(char *buffer, size_t buffer_size) {
  // This is more complex on Windows, for now just return false
  // You might want to implement Windows-specific library path discovery
  return false;
}

#else
/**
 * @brief Links object file using ld to create an executable (Unix/Linux)
 *
 * @param obj_filename Path to the object file
 * @param exe_filename Path for the output executable
 * @return true if linking succeeded, false otherwise
 */
bool link_with_ld(const char *obj_filename, const char *exe_filename) {
  // Check if we're on a 64-bit system
  bool is_64bit = sizeof(void *) == 8;

  // Build the ld command
  char *ld_args[16];
  int arg_count = 0;

  ld_args[arg_count++] = "ld";

  // Add architecture-specific arguments
  if (is_64bit) {
    ld_args[arg_count++] = "-m";
    ld_args[arg_count++] = "elf_x86_64";
  } else {
    ld_args[arg_count++] = "-m";
    ld_args[arg_count++] = "elf_i386";
  }

  // Add dynamic linker (for shared libraries)
  if (is_64bit) {
    ld_args[arg_count++] = "--dynamic-linker";
    ld_args[arg_count++] = "/lib64/ld-linux-x86-64.so.2";
  } else {
    ld_args[arg_count++] = "--dynamic-linker";
    ld_args[arg_count++] = "/lib/ld-linux.so.2";
  }

  // Add standard library paths and startup files
  ld_args[arg_count++] = "/usr/lib/x86_64-linux-gnu/crt1.o"; // Entry point
  ld_args[arg_count++] = "/usr/lib/x86_64-linux-gnu/crti.o"; // Init
  ld_args[arg_count++] =
      "/usr/lib/gcc/x86_64-linux-gnu/11/crtbegin.o"; // GCC runtime begin

  // Add our object file
  ld_args[arg_count++] = (char *)obj_filename;

  // Add standard libraries
  ld_args[arg_count++] = "-lc"; // Standard C library

  // Add GCC runtime end
  ld_args[arg_count++] = "/usr/lib/gcc/x86_64-linux-gnu/11/crtend.o";
  ld_args[arg_count++] = "/usr/lib/x86_64-linux-gnu/crtn.o";

  // Output file
  ld_args[arg_count++] = "-o";
  ld_args[arg_count++] = (char *)exe_filename;

  // Null terminate
  ld_args[arg_count] = NULL;

  printf("Linking with: ");
  for (int i = 0; ld_args[i] != NULL; i++) {
    printf("%s ", ld_args[i]);
  }
  printf("\n");

  // Fork and execute ld
  pid_t pid = fork();
  if (pid == 0) {
    // Child process - execute ld
    execvp("ld", ld_args);
    perror("execvp failed");
    exit(1);
  } else if (pid > 0) {
    // Parent process - wait for child
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
  } else {
    perror("fork failed");
    return false;
  }
}

/**
 * @brief Get a file path from gcc
 *
 * @param filename The filename to search for
 * @param buffer Buffer to store the result
 * @param buffer_size Size of the buffer
 * @return true if path was found, false otherwise
 */
bool get_gcc_file_path(const char *filename, char *buffer, size_t buffer_size) {
  char command[256];
  snprintf(command, sizeof(command), "gcc -print-file-name=%s", filename);

  FILE *fp = popen(command, "r");
  if (!fp)
    return false;

  if (fgets(buffer, buffer_size, fp) != NULL) {
    // Remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }
    pclose(fp);

    // Check if gcc actually found the file (it returns the filename unchanged
    // if not found)
    return strcmp(buffer, filename) != 0;
  }

  pclose(fp);
  return false;
}

/**
 * @brief Get the system's library search paths
 *
 * @param buffer Buffer to store library paths
 * @param buffer_size Size of the buffer
 * @return true if paths were found
 */
bool get_lib_paths(char *buffer, size_t buffer_size) {
  FILE *fp =
      popen("gcc -print-search-dirs | grep '^libraries:' | cut -d'=' -f2", "r");
  if (!fp)
    return false;

  if (fgets(buffer, buffer_size, fp) != NULL) {
    // Remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }
    pclose(fp);
    return true;
  }

  pclose(fp);
  return false;
}

/**
 * @brief Alternative simpler linking approach using system()
 *
 * @param obj_filename Path to the object file
 * @param exe_filename Path for the output executable
 * @return true if linking succeeded, false otherwise
 */
bool link_with_ld_simple(const char *obj_filename, const char *exe_filename) {
  char crt1_path[512], crti_path[512], crtn_path[512];
  char crtbegin_path[512], crtend_path[512];
  char command[2048];

  // Try to get paths from gcc
  bool found_crt1 = get_gcc_file_path("crt1.o", crt1_path, sizeof(crt1_path));
  bool found_crti = get_gcc_file_path("crti.o", crti_path, sizeof(crti_path));
  bool found_crtn = get_gcc_file_path("crtn.o", crtn_path, sizeof(crtn_path));
  bool found_crtbegin =
      get_gcc_file_path("crtbegin.o", crtbegin_path, sizeof(crtbegin_path));
  bool found_crtend =
      get_gcc_file_path("crtend.o", crtend_path, sizeof(crtend_path));

  if (!found_crt1 || !found_crti || !found_crtn || !found_crtbegin ||
      !found_crtend) {
    printf("Warning: Could not locate all startup files. Trying common "
           "paths...\n");

    // Fallback to common paths
    const char *common_paths[] = {
        "/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/usr/lib",
        "/lib/x86_64-linux-gnu",     "/lib64",     "/lib"};

    // Try to find crt1.o in common locations
    for (int i = 0; i < 6 && !found_crt1; i++) {
      snprintf(crt1_path, sizeof(crt1_path), "%s/crt1.o", common_paths[i]);
      if (access(crt1_path, F_OK) == 0) {
        found_crt1 = true;
        // Also try to find crti.o and crtn.o in the same location
        snprintf(crti_path, sizeof(crti_path), "%s/crti.o", common_paths[i]);
        snprintf(crtn_path, sizeof(crtn_path), "%s/crtn.o", common_paths[i]);
        found_crti = (access(crti_path, F_OK) == 0);
        found_crtn = (access(crtn_path, F_OK) == 0);
      }
    }
  }

  if (!found_crt1 || !found_crti || !found_crtn) {
    printf("✗ Could not locate startup files. Using gcc as fallback:\n");
    snprintf(command, sizeof(command), "gcc %s -o %s", obj_filename,
             exe_filename);
    printf("Executing: %s\n", command);
    return system(command) == 0;
  }

  // Build the ld command
  if (found_crtbegin && found_crtend) {
    snprintf(command, sizeof(command),
             "ld -dynamic-linker /lib64/ld-linux-x86-64.so.2 "
             "%s %s %s %s -lc %s %s -o %s",
             crt1_path, crti_path, crtbegin_path, obj_filename, crtend_path,
             crtn_path, exe_filename);
  } else {
    // Simpler version without crtbegin/crtend
    snprintf(command, sizeof(command),
             "ld -dynamic-linker /lib64/ld-linux-x86-64.so.2 "
             "%s %s %s -lc %s -o %s",
             crt1_path, crti_path, obj_filename, crtn_path, exe_filename);
  }

  return system(command) == 0;
}
#endif

void print_progress(int step, int total, const char *stage) {
  float ratio = (float)step / total;
  int filled = (int)(ratio * BAR_WIDTH);

  printf("\r["); // carriage return for in-place update
  for (int i = 0; i < BAR_WIDTH; i++) {
    if (i < filled)
      putchar('=');
    else if (i == filled)
      putchar('>');
    else
      putchar(' ');
  }
  printf("] %d%% - %s", (int)(ratio * 100), stage);
  // clear out the rest of the line
  printf("\033[K");
  fflush(stdout);

  // Always add newline after progress bar to prevent interference
  if (step == total) {
    printf("\n");
  }
}

// Add a helper function to ensure clean output after progress bar
void ensure_clean_line() {
  printf("\n");
  fflush(stdout);
}
