#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lsp.h"

char *extract_string(const char *json, const char *key, ArenaAllocator *arena) {
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *found = strstr(json, search);
  if (!found) {
    fprintf(stderr, "[LSP] extract_string: key '%s' not found\n", key);
    return NULL;
  }

  const char *colon = found + strlen(search);

  // Skip whitespace and colon
  while (*colon && (isspace(*colon) || *colon == ':')) {
    colon++;
  }

  const char *start = colon;

  // Skip whitespace before opening quote
  while (*start && isspace(*start))
    start++;

  if (*start != '"') {
    fprintf(stderr, "[LSP] extract_string: expected quote after key '%s'\n",
            key);
    return NULL;
  }
  start++; // Skip opening quote

  // Allocate buffer for the unescaped string
  size_t max_len = strlen(start);
  char *result = arena_alloc(arena, max_len + 1, 1);
  if (!result) {
    fprintf(stderr, "[LSP] extract_string: allocation failed\n");
    return NULL;
  }

  char *dst = result;
  const char *src = start;

  // Parse and unescape the JSON string
  while (*src && *src != '"') {
    if (*src == '\\') {
      src++; // Skip the backslash
      if (!*src)
        break; // Guard against trailing backslash

      switch (*src) {
      case 'n':
        *dst++ = '\n';
        break;
      case 't':
        *dst++ = '\t';
        break;
      case 'r':
        *dst++ = '\r';
        break;
      case '"':
        *dst++ = '"';
        break;
      case '\\':
        *dst++ = '\\';
        break;
      case '/':
        *dst++ = '/';
        break;
      case 'b':
        *dst++ = '\b';
        break;
      case 'f':
        *dst++ = '\f';
        break;
      case 'u': {
        // Unicode escape \uXXXX - simplified handling
        // For full support, you'd need proper UTF-8 encoding
        src++;
        if (strlen(src) >= 4) {
          // Skip the 4 hex digits for now (proper impl needs conversion)
          src += 3; // +3 because we increment at end of loop
        }
        break;
      }
      default:
        // Unknown escape, keep the character
        *dst++ = *src;
        break;
      }
      src++;
    } else {
      *dst++ = *src++;
    }
  }

  *dst = '\0';

  fprintf(stderr, "[LSP] extract_string: key '%s' = '%s'\n", key, result);
  return result;
}

int extract_int(const char *json, const char *key) {
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *found = strstr(json, search);
  if (!found) {
    fprintf(stderr, "[LSP] extract_int: key '%s' not found\n", key);
    return -1;
  }

  // Find the colon after the key
  const char *colon = found + strlen(search);

  // Skip ALL whitespace and the colon character
  while (*colon && (isspace(*colon) || *colon == ':')) {
    colon++;
  }

  // Check if we have a valid number (could be negative)
  if (!*colon) {
    fprintf(stderr, "[LSP] extract_int: no value after key '%s'\n", key);
    return -1;
  }

  if (*colon == '-' || isdigit(*colon)) {
    int value = atoi(colon);
    fprintf(stderr, "[LSP] extract_int: key '%s' = %d\n", key, value);
    return value;
  }

  fprintf(stderr, "[LSP] extract_int: invalid number for key '%s'\n", key);
  return -1;
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

const char *find_json_value(const char *json, const char *key) {
  if (!json || !key) {
    return NULL;
  }

  // Build the search pattern: "key":
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *current = json;
  
  // Search for all occurrences of the key until we find a valid one
  while ((current = strstr(current, search)) != NULL) {
    // Move past the key
    const char *after_key = current + strlen(search);
    
    // Skip whitespace
    while (*after_key && isspace(*after_key)) {
      after_key++;
    }
    
    // Check if followed by colon (indicates this is a key, not a value)
    if (*after_key == ':') {
      // Skip the colon
      after_key++;
      
      // Skip whitespace after colon
      while (*after_key && isspace(*after_key)) {
        after_key++;
      }
      
      return after_key;
    }
    
    // Not a valid key-value pair, continue searching
    current = after_key;
  }

  return NULL;
}

LSPMethod lsp_parse_method(const char *json) {
  if (!json) {
    fprintf(stderr, "[LSP] parse_method: NULL input\n");
    return LSP_METHOD_UNKNOWN;
  }

  // Log the first part of the JSON for debugging
  fprintf(stderr, "[LSP] parse_method: checking message: %.200s\n", json);

  // Find the "method" value - it could be anywhere in the JSON
  const char *method_value = find_json_value(json, "method");
  if (!method_value) {
    fprintf(stderr, "[LSP] parse_method: no 'method' field found\n");
    return LSP_METHOD_UNKNOWN;
  }

  // Extract method string (skip opening quote)
  if (*method_value == '"') {
    method_value++;
  }

  fprintf(stderr, "[LSP] parse_method: found method starting with: %.50s\n",
          method_value);

  // Check methods - LONGEST FIRST to avoid partial matches
  if (strncmp(method_value, "textDocument/documentSymbol", 27) == 0)
    return LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL;
  if (strncmp(method_value, "textDocument/semanticTokens", 27) == 0)
    return LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS;
  if (strncmp(method_value, "textDocument/completion", 23) == 0)
    return LSP_METHOD_TEXT_DOCUMENT_COMPLETION;
  if (strncmp(method_value, "textDocument/definition", 23) == 0)
    return LSP_METHOD_TEXT_DOCUMENT_DEFINITION;
  if (strncmp(method_value, "textDocument/didChange", 22) == 0)
    return LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE;
  if (strncmp(method_value, "textDocument/didClose", 21) == 0)
    return LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE;
  if (strncmp(method_value, "textDocument/didOpen", 20) == 0)
    return LSP_METHOD_TEXT_DOCUMENT_DID_OPEN;
  if (strncmp(method_value, "textDocument/hover", 18) == 0)
    return LSP_METHOD_TEXT_DOCUMENT_HOVER;

  // Check "initialized" BEFORE "initialize" (11 vs 10 chars)
  if (strncmp(method_value, "initialized", 11) == 0)
    return LSP_METHOD_INITIALIZED;
  if (strncmp(method_value, "initialize", 10) == 0)
    return LSP_METHOD_INITIALIZE;

  if (strncmp(method_value, "shutdown", 8) == 0)
    return LSP_METHOD_SHUTDOWN;
  if (strncmp(method_value, "exit", 4) == 0)
    return LSP_METHOD_EXIT;

  fprintf(stderr, "[LSP] parse_method: unknown method\n");
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

void serialize_diagnostics_to_json(const char *uri, LSPDiagnostic *diagnostics,
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
