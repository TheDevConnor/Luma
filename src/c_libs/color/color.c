/**
 * @file color.c
 * @brief Implementation of terminal color support detection.
 *
 * This module provides the function `terminal_supports_color()` which detects
 * if the current terminal supports ANSI color escape sequences. It caches
 * the result to avoid redundant system calls.
 *
 * On Windows, it attempts to enable ANSI support on modern terminals (Windows Terminal,
 * VS Code terminal, ConEmu, Cmder) by enabling virtual terminal processing.
 * On Unix-like systems, it checks environment variables and terminal capabilities
 * to determine color support.
 *
 * @note The function uses platform-specific APIs and environment variables.
 *
 * @author Connor Harris
 * @date 2025
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "color.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define isatty _isatty
#else
#include <unistd.h>
#endif

/**
 * @brief Checks if the current terminal supports ANSI color escape sequences.
 *
 * This function determines whether the terminal connected to standard output
 * supports colored output using ANSI escape sequences.
 * 
 * The result is cached after the first call for efficiency.
 *
 * On Windows:
 * - Detects Windows Terminal, VS Code terminal, ConEmu, Cmder via environment variables.
 * - Attempts to enable ANSI escape code processing via the console API.
 * - Checks Windows version for fallback support.
 *
 * On Unix/Linux:
 * - Checks TERM environment variable for common terminal types supporting color.
 * - Uses `tput colors` command to verify the number of colors supported.
 *
 * @return true if ANSI color output is supported, false otherwise.
 */
bool terminal_supports_color(void) {
    static int cached_result = -1;
    
    /* Cache the result to avoid repeated system calls */
    if (cached_result != -1) {
        return cached_result == 1;
    }
    
    /* Check if we're connected to a terminal */
    if (!isatty(STDOUT_FILENO)) {
        cached_result = 0;
        return false;
    }
    
#ifdef _WIN32
    /* Windows-specific color support detection */
    
    /* Check for Windows Terminal, VS Code terminal, or other modern terminals */
    const char* wt_session = getenv("WT_SESSION");
    const char* term_program = getenv("TERM_PROGRAM");
    const char* vscode_term = getenv("VSCODE_PID");
    
    if (wt_session || 
        (term_program && (strstr(term_program, "vscode") || 
                         strstr(term_program, "Windows Terminal") ||
                         strstr(term_program, "ConEmu") ||
                         strstr(term_program, "Cmder"))) ||
        vscode_term) {
        cached_result = 1;
        return true;
    }
    
    /* Try to enable ANSI color support on Windows 10+ */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        cached_result = 0;
        return false;
    }
    
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        cached_result = 0;
        return false;
    }
    
    /* Enable ANSI escape sequences if supported */
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(hOut, dwMode)) {
        cached_result = 1;
        return true;
    }
    
    /* Fallback: Check Windows version for basic support */
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    
    if (GetVersionEx(&osvi)) {
        /* Windows 10 and later have better ANSI support */
        if (osvi.dwMajorVersion >= 10) {
            cached_result = 1;
            return true;
        }
    }
    
    /* Check ANSICON or ConEmu environment variables */
    if (getenv("ANSICON") || getenv("ConEmuANSI")) {
        cached_result = 1;
        return true;
    }
    
    cached_result = 0;
    return false;
    
#else
    /* Unix/Linux color support detection */
    
    /* Check TERM environment variable */
    const char* term = getenv("TERM");
    if (!term || strcmp(term, "dumb") == 0) {
        cached_result = 0;
        return false;
    }
    
    /* Check for common terminals that support color */
    if (strstr(term, "color") || 
        strstr(term, "xterm") || 
        strstr(term, "screen") ||
        strstr(term, "tmux") ||
        strcmp(term, "linux") == 0) {
        cached_result = 1;
        return true;
    }
    
    /* Try to get number of colors supported */
    FILE *pipe = fopen("tput colors 2>/dev/null", "r");
    if (!pipe) {
        /* If tput fails, assume basic color support for common terminals */
        cached_result = 1;
        return true;
    }
    
    int colors = 0;
    if (fscanf(pipe, "%d", &colors) == 1 && colors > 0) {
        cached_result = 1;
    } else {
        cached_result = 0;
    }
    
    fclose(pipe);
    return cached_result == 1;
#endif
}

#ifdef _WIN32
/**
 * @brief Enables ANSI color support on Windows consoles if possible.
 *
 * Attempts to enable virtual terminal processing on the standard output
 * console handle to allow ANSI escape sequences.
 *
 * @return true if ANSI escape sequences were successfully enabled, false otherwise.
 */
bool enable_windows_color_support(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return false;
    }
    
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, dwMode);
}
#endif
