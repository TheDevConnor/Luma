#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lsp.h"

// ============================================================================
// JSON Parsing Utilities
// ============================================================================

char *extract_string(const char *json, const char *key,
                            ArenaAllocator *arena) {
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *found = strstr(json, search);
  if (!found)
    return NULL;

  const char *colon = strchr(found + strlen(search), ':');
  if (!colon)
    return NULL;

  const char *start = colon + 1;
  while (*start && isspace(*start))
    start++;
  if (*start != '"')
    return NULL;
  start++;

  // Allocate a buffer for the unescaped string (worst case: same size)
  size_t max_len = strlen(start);
  char *result = arena_alloc(arena, max_len + 1, 1);
  char *dst = result;

  // Parse and unescape the JSON string
  const char *src = start;
  while (*src && *src != '"') {
    if (*src == '\\') {
      src++; // Skip the backslash
      if (*src == 'n') {
        *dst++ = '\n';
      } else if (*src == 't') {
        *dst++ = '\t';
      } else if (*src == 'r') {
        *dst++ = '\r';
      } else if (*src == '"') {
        *dst++ = '"';
      } else if (*src == '\\') {
        *dst++ = '\\';
      } else {
        // Unknown escape, keep the character
        *dst++ = *src;
      }
      src++;
    } else {
      *dst++ = *src++;
    }
  }

  *dst = '\0';
  return result;
}

int extract_int(const char *json, const char *key) {
  // Build search patterns for both "key": value and "key" : value (with spaces)
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *found = strstr(json, search);
  if (!found) {
    return -1;
  }

  // Find the colon after the key
  const char *colon = found + strlen(search);

  // Skip whitespace and colon
  while (*colon && (isspace(*colon) || *colon == ':')) {
    colon++;
  }

  // Now we should be at the number
  if (!*colon || !isdigit(*colon)) {
    return -1;
  }

  return atoi(colon);
}

LSPPosition extract_position(const char *json) {
  LSPPosition pos = {0, 0};

  const char *position_obj = strstr(json, "\"position\"");
  if (!position_obj)
    return pos;

  pos.line = extract_int(position_obj, "line");
  pos.character = extract_int(position_obj, "character");
  return pos;
}

// ============================================================================
// JSON-RPC Helpers
// ============================================================================

LSPMethod lsp_parse_method(const char *json) {
  if (!json)
    return LSP_METHOD_UNKNOWN;

  // CHECK LONGER STRINGS FIRST!
  if (strstr(json, "textDocument/documentSymbol"))
    return LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL;
  if (strstr(json, "textDocument/semanticTokens"))
    return LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS;
  if (strstr(json, "textDocument/didOpen"))
    return LSP_METHOD_TEXT_DOCUMENT_DID_OPEN;
  if (strstr(json, "textDocument/didChange"))
    return LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE;
  if (strstr(json, "textDocument/didClose"))
    return LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE;
  if (strstr(json, "textDocument/hover"))
    return LSP_METHOD_TEXT_DOCUMENT_HOVER;
  if (strstr(json, "textDocument/definition"))
    return LSP_METHOD_TEXT_DOCUMENT_DEFINITION;
  if (strstr(json, "textDocument/completion"))
    return LSP_METHOD_TEXT_DOCUMENT_COMPLETION;

  // Check "initialized" BEFORE "initialize"!
  if (strstr(json, "initialized"))
    return LSP_METHOD_INITIALIZED;
  if (strstr(json, "initialize"))
    return LSP_METHOD_INITIALIZE;

  if (strstr(json, "shutdown"))
    return LSP_METHOD_SHUTDOWN;
  if (strstr(json, "exit"))
    return LSP_METHOD_EXIT;

  return LSP_METHOD_UNKNOWN;
}

void lsp_send_response(int id, const char *result) {
  char json_msg[8192];
  int msg_len =
      snprintf(json_msg, sizeof(json_msg),
               "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result);

  printf("Content-Length: %d\r\n\r\n%s", msg_len, json_msg);
  fflush(stdout);
}

void lsp_send_notification(const char *method, const char *params) {
  char json_msg[8192];
  int msg_len = snprintf(
      json_msg, sizeof(json_msg),
      "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}", method, params);

  printf("Content-Length: %d\r\n\r\n%s", msg_len, json_msg);
  fflush(stdout);
}

void lsp_send_error(int id, int code, const char *message) {
  char json_msg[8192];
  int msg_len = snprintf(json_msg, sizeof(json_msg),
                         "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%"
                         "d,\"message\":\"%s\"}}",
                         id, code, message);

  printf("Content-Length: %d\r\n\r\n%s", msg_len, json_msg);
  fflush(stdout);
}

void serialize_diagnostics_to_json(const char *uri,
                                          LSPDiagnostic *diagnostics,
                                          size_t diag_count, char *output,
                                          size_t output_size) {
  size_t offset = 0;
  offset += snprintf(output + offset, output_size - offset,
                     "{\"uri\":\"%s\",\"diagnostics\":[", uri);

  for (size_t i = 0; i < diag_count; i++) {
    LSPDiagnostic *diag = &diagnostics[i];
    if (i > 0) {
      offset += snprintf(output + offset, output_size - offset, ",");
    }

    // Escape message text
    char escaped_msg[2048] = {0};
    const char *src = diag->message;
    char *dst = escaped_msg;
    while (*src && (dst - escaped_msg) < (int)sizeof(escaped_msg) - 1) {
      if (*src == '"' || *src == '\\') {
        *dst++ = '\\';
      } else if (*src == '\n') {
        *dst++ = '\\';
        *dst++ = 'n';
        src++;
        continue;
      } else if (*src == '\t') {
        *dst++ = '\\';
        *dst++ = 't';
        src++;
        continue;
      }
      *dst++ = *src++;
    }
    *dst = '\0';

    offset += snprintf(output + offset, output_size - offset,
                       "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                       "\"end\":{\"line\":%d,\"character\":%d}},"
                       "\"severity\":%d,\"message\":\"%s\",\"source\":\"%s\"}",
                       diag->range.start.line, diag->range.start.character,
                       diag->range.end.line, diag->range.end.character,
                       diag->severity, escaped_msg,
                       diag->source ? diag->source : "luma");
  }

  offset += snprintf(output + offset, output_size - offset, "]}");
}

// Helper to serialize completion items to JSON
void serialize_completion_items(LSPCompletionItem *items, size_t count,
                                char *output, size_t output_size) {
  size_t offset = 0;
  offset += snprintf(output + offset, output_size - offset, "{\"items\":[");

  for (size_t i = 0; i < count; i++) {
    LSPCompletionItem *item = &items[i];

    if (i > 0) {
      offset += snprintf(output + offset, output_size - offset, ",");
    }

    offset +=
        snprintf(output + offset, output_size - offset,
                 "{\"label\":\"%s\",\"kind\":%d", item->label, item->kind);

    if (item->insert_text) {
      // Escape special characters in snippet
      char escaped[1024];
      const char *src = item->insert_text;
      char *dst = escaped;
      while (*src && (dst - escaped) < (int)sizeof(escaped) - 1) {
        if (*src == '"' || *src == '\\') {
          *dst++ = '\\';
        } else if (*src == '\n') {
          *dst++ = '\\';
          *dst++ = 'n';
          src++;
          continue;
        } else if (*src == '\t') {
          *dst++ = '\\';
          *dst++ = 't';
          src++;
          continue;
        }
        *dst++ = *src++;
      }
      *dst = '\0';

      offset += snprintf(output + offset, output_size - offset,
                         ",\"insertText\":\"%s\",\"insertTextFormat\":%d",
                         escaped, item->format);
    }

    if (item->detail) {
      offset += snprintf(output + offset, output_size - offset,
                         ",\"detail\":\"%s\"", item->detail);
    }

    offset += snprintf(output + offset, output_size - offset, "}");
  }

  offset += snprintf(output + offset, output_size - offset, "]}");
}