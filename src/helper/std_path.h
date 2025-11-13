/**
 * @file std_path.h
 * @brief Standard library path resolution for Luma compiler
 *
 * Resolves std/ imports by checking multiple locations:
 * 1. System-wide: /usr/local/lib/luma/std/ or C:\Program Files\luma\std\
 * 2. User-local: ~/.luma/std/ or %USERPROFILE%\.luma\std\
 * 3. Current directory: ./std/
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__MINGW32__) || defined(_WIN32)
#include <shlobj.h>
#include <windows.h>
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#else
#include <pwd.h>
#include <unistd.h>
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#endif

/**
 * @brief Resolve a std/ import path to an actual file path
 *
 * @param import_path The import path (e.g., "std/io" or "std/math")
 * @param buffer Buffer to store the resolved path
 * @param buffer_size Size of the buffer
 * @return true if the path was resolved successfully, false otherwise
 */
bool resolve_std_path(const char *import_path, char *buffer,
                      size_t buffer_size);

/**
 * @brief Get the system-wide Luma standard library path
 *
 * @param buffer Buffer to store the path
 * @param buffer_size Size of the buffer
 * @return true if successful, false otherwise
 */
bool get_system_std_path(char *buffer, size_t buffer_size);

/**
 * @brief Get the user-local Luma standard library path
 *
 * @param buffer Buffer to store the path
 * @param buffer_size Size of the buffer
 * @return true if successful, false otherwise
 */
bool get_user_std_path(char *buffer, size_t buffer_size);

/**
 * @brief Check if a file exists at the given path
 *
 * @param path Path to check
 * @return true if file exists, false otherwise
 */
bool file_exists(const char *path);

/**
 * @brief Normalize a path by removing "std/" prefix if present
 *
 * @param path The input path
 * @return Pointer to the normalized path (within the input string)
 */
const char *normalize_std_import(const char *path);
void print_std_search_paths(void);
