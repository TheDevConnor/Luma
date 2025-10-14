#include "../c_libs/error/error.h"
#include "lsp.h"

LSPDiagnostic *convert_errors_to_diagnostics(size_t *diagnostic_count,
                                             ArenaAllocator *arena) {
  if (!diagnostic_count)
    return NULL;

  size_t error_count = error_get_count();

  fprintf(stderr, "[LSP] Converting %zu errors to diagnostics\n", error_count);

  if (error_count == 0) {
    *diagnostic_count = 0;
    return NULL;
  }

  LSPDiagnostic *diagnostics = arena_alloc(
      arena, error_count * sizeof(LSPDiagnostic), alignof(LSPDiagnostic));
  if (!diagnostics) {
    fprintf(stderr, "[LSP] Failed to allocate diagnostics array\n");
    *diagnostic_count = 0;
    return NULL;
  }

  for (size_t i = 0; i < error_count; i++) {
    ErrorInformation *error = error_get_at_index(i);
    if (!error) {
      fprintf(stderr, "[LSP] Warning: error at index %zu is NULL\n", i);
      continue;
    }

    fprintf(stderr, "[LSP] Error %zu: %s at line %d, col %d: %s\n", i,
            error->error_type, error->line, error->col, error->message);

    LSPDiagnostic *diag = &diagnostics[i];

    diag->range.start.line = error->line > 0 ? error->line - 1 : 0;
    diag->range.start.character = error->col > 0 ? error->col - 1 : 0;
    diag->range.end.line = diag->range.start.line;
    diag->range.end.character =
        diag->range.start.character +
        (error->token_length > 0 ? error->token_length : 1);

    if (strstr(error->error_type, "Error")) {
      diag->severity = LSP_DIAGNOSTIC_ERROR;
    } else if (strstr(error->error_type, "Warning")) {
      diag->severity = LSP_DIAGNOSTIC_WARNING;
    } else {
      diag->severity = LSP_DIAGNOSTIC_INFORMATION;
    }

    if (error->help) {
      size_t msg_len = strlen(error->message) + strlen(error->help) + 20;
      char *full_msg = arena_alloc(arena, msg_len, 1);
      snprintf(full_msg, msg_len, "%s\n\nHelp: %s", error->message,
               error->help);
      diag->message = full_msg;
    } else {
      diag->message = arena_strdup(arena, error->message);
    }

    diag->source = "luma-lsp";

    fprintf(stderr, "[LSP] Diagnostic %zu: severity=%d, line=%d, col=%d\n", i,
            diag->severity, diag->range.start.line,
            diag->range.start.character);
  }

  *diagnostic_count = error_count;
  return diagnostics;
}

LSPDiagnostic *lsp_diagnostics(LSPDocument *doc, size_t *diagnostic_count,
                               ArenaAllocator *arena) {
  if (!doc || !diagnostic_count)
    return NULL;

  // Return cached diagnostics if available
  if (doc->diagnostics && doc->diagnostic_count > 0) {
    *diagnostic_count = doc->diagnostic_count;
    return doc->diagnostics;
  }

  // Convert current errors to diagnostics
  return convert_errors_to_diagnostics(diagnostic_count, arena);
}
