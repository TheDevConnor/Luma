#include "std_path.h"

bool file_exists(const char *path) {
#if defined(__MINGW32__) || defined(_WIN32)
  DWORD attr = GetFileAttributesA(path);
  return (attr != INVALID_FILE_ATTRIBUTES &&
          !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
  return access(path, F_OK) == 0;
#endif
}

const char *normalize_std_import(const char *path) {
  // Remove "std/" or "std\" prefix if present
  if (strncmp(path, "std/", 4) == 0) {
    return path + 4;
  }
  if (strncmp(path, "std\\", 4) == 0) {
    return path + 4;
  }
  return path;
}

bool get_system_std_path(char *buffer, size_t buffer_size) {
#if defined(__MINGW32__) || defined(_WIN32)
  char program_files[MAX_PATH];
  // Windows: C:\Program Files\luma\std
  if (SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, 0, program_files) != S_OK) {
    return false;
  }
  snprintf(buffer, buffer_size, "%s\\luma\\std", program_files);
#else
  // Unix: /usr/local/lib/luma/std/
  snprintf(buffer, buffer_size, "/usr/local/lib/luma/std");
#endif
  return true;
}

bool get_user_std_path(char *buffer, size_t buffer_size) {
#if defined(__MINGW32__) || defined(_WIN32)
  char *userprofile = getenv("USERPROFILE");
  // Windows: %USERPROFILE%\.luma\std
  if (!userprofile) {
    return false;
  }
  snprintf(buffer, buffer_size, "%s\\.luma\\std", userprofile);
#else
  // Unix: ~/.luma/std/
  const char *home = getenv("HOME");
  if (!home) {
    // Fallback to getpwuid if HOME is not set
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
      home = pw->pw_dir;
    }
  }
  if (!home) {
    return false;
  }
  snprintf(buffer, buffer_size, "%s/.luma/std", home);
#endif
  return true;
}

bool resolve_std_path(const char *import_path, char *buffer,
                      size_t buffer_size) {
  // Normalize the import path (remove "std/" prefix)
  const char *normalized = normalize_std_import(import_path);

  // Check if file already has an extension (.luma or .lx)
  char with_extension[512];
  const char *ext = strrchr(normalized, '.');
  if (ext && (strcmp(ext, ".luma") == 0 || strcmp(ext, ".lx") == 0)) {
    // Already has a valid extension, use as-is
    snprintf(with_extension, sizeof(with_extension), "%s", normalized);
  } else {
    // No extension, try .lx first (your current extension), then .luma
    snprintf(with_extension, sizeof(with_extension), "%s.lx", normalized);
  }

  char test_path[1024];

  // Try both .lx and .luma extensions
  const char *extensions[] = {with_extension, NULL};
  char alt_extension[512];
  // If we used .lx, also try .luma as fallback
  if (ext == NULL || (strcmp(ext, ".lx") != 0 && strcmp(ext, ".luma") != 0)) {
    snprintf(alt_extension, sizeof(alt_extension), "%s.luma", normalized);
    extensions[1] = alt_extension;
  }

  for (int ext_idx = 0; ext_idx < 2 && extensions[ext_idx]; ext_idx++) {
    const char *try_ext = extensions[ext_idx];

    // 1. Check system-wide installation
    if (get_system_std_path(test_path, sizeof(test_path))) {
      snprintf(buffer, buffer_size, "%s%c%s", test_path, PATH_SEPARATOR,
               try_ext);
      if (file_exists(buffer)) {
        return true;
      }
    }

    // 2. Check user-local installation
    if (get_user_std_path(test_path, sizeof(test_path))) {
      snprintf(buffer, buffer_size, "%s%c%s", test_path, PATH_SEPARATOR,
               try_ext);
      if (file_exists(buffer)) {
        return true;
      }
    }

    // 3. Check current directory (development fallback)
    snprintf(buffer, buffer_size, ".%cstd%c%s", PATH_SEPARATOR, PATH_SEPARATOR,
             try_ext);
    if (file_exists(buffer)) {
      return true;
    }
  }

  // 4. Check if the import path itself is valid (absolute or relative path)
  if (file_exists(import_path)) {
    snprintf(buffer, buffer_size, "%s", import_path);
    return true;
  }

  // Not found in any location
  return false;
}

// Helper function to print search paths for debugging
void print_std_search_paths(void) {
  char path[1024];
  fprintf(stderr, "Standard library search paths:\n");

  if (get_system_std_path(path, sizeof(path))) {
    fprintf(stderr, "  1. System: %s\n", path);
  }

  if (get_user_std_path(path, sizeof(path))) {
    fprintf(stderr, "  2. User:   %s\n", path);
  }

  fprintf(stderr, "  3. Local:  ./std/\n");
}