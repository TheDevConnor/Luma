#include "lsp.h"

void lsp_handle_message(LSPServer *server, const char *message) {
  if (!server || !message)
    return;

  // DON'T truncate the log - show more of the message to debug
  fprintf(stderr, "[LSP] Received message: %.500s...\n",
          message); // Changed from 100 to 500

  LSPMethod method = lsp_parse_method(message);
  int request_id = extract_int(message, "id");

  fprintf(stderr, "[LSP] Extracted request_id: %d\n", request_id); // ADD THIS

  ArenaAllocator temp_arena;
  arena_allocator_init(&temp_arena, 64 * 1024);

  switch (method) {
  case LSP_METHOD_INITIALIZE: {
    fprintf(stderr, "[LSP] Handling initialize\n");

    // Only respond if we have a valid ID
    if (request_id >= 0) {
      const char *workspace_uri = extract_string(message, "uri", &temp_arena);
      if (workspace_uri) {
        build_module_registry(server, workspace_uri);
      }

      server->initialized = true;
      const char *capabilities =
          "{"
          "\"capabilities\":{"
          "\"textDocumentSync\":1,"
          "\"hoverProvider\":true,"
          "\"definitionProvider\":true,"
          "\"completionProvider\":{\"triggerCharacters\":[\".\",\":\"]},"
          "\"documentSymbolProvider\":true"
          "},"
          "\"serverInfo\":{\"name\":\"Luma LSP\",\"version\":\"0.1.0\"}"
          "}";
      lsp_send_response(request_id, capabilities);
    } else {
      fprintf(stderr, "[LSP] ERROR: No valid request_id for initialize!\n");
    }
    break;
  }

  case LSP_METHOD_INITIALIZED:
    fprintf(stderr, "[LSP] Client initialized\n");
    // This is a notification, no response needed
    break;

  case LSP_METHOD_TEXT_DOCUMENT_DID_OPEN: {
    fprintf(stderr, "[LSP] Handling didOpen\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    const char *text = extract_string(message, "text", &temp_arena);
    int version = extract_int(message, "version");

    if (uri && text) {
      fprintf(stderr, "[LSP] Opening document: %s (version %d)\n", uri,
              version);
      fprintf(stderr, "[LSP] Document content length: %zu\n", strlen(text));

      LSPDocument *doc = lsp_document_open(server, uri, text, version);
      if (doc) {
        BuildConfig config = {0};
        config.check_mem = true;
        lsp_document_analyze(doc, server, &config); // ADD server parameter

        size_t diag_count;
        LSPDiagnostic *diagnostics =
            lsp_diagnostics(doc, &diag_count, &temp_arena);

        fprintf(stderr, "[LSP] Sending %zu diagnostics\n", diag_count);

        char params[16384];
        serialize_diagnostics_to_json(uri, diagnostics, diag_count, params,
                                      sizeof(params));

        fprintf(stderr, "[LSP] Diagnostic JSON: %s\n", params);

        lsp_send_notification("textDocument/publishDiagnostics", params);
      }
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE: {
    fprintf(stderr, "[LSP] Handling didChange\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    const char *text = extract_string(message, "text", &temp_arena);
    int version = extract_int(message, "version");

    if (uri && text) {
      lsp_document_update(server, uri, text, version);

      LSPDocument *doc = lsp_document_find(server, uri);
      if (doc) {
        BuildConfig config = {0};
        config.check_mem = true;
        lsp_document_analyze(doc, server, &config); // ADD server parameter

        size_t diag_count;
        LSPDiagnostic *diagnostics =
            lsp_diagnostics(doc, &diag_count, &temp_arena);

        char params[16384];
        serialize_diagnostics_to_json(uri, diagnostics, diag_count, params,
                                      sizeof(params));
        lsp_send_notification("textDocument/publishDiagnostics", params);
      }
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE: {
    fprintf(stderr, "[LSP] Handling didClose\n");
    const char *uri = extract_string(message, "uri", &temp_arena);
    if (uri) {
      lsp_document_close(server, uri);
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_HOVER: {
    fprintf(stderr, "[LSP] Handling hover\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    LSPPosition position = extract_position(message);

    if (uri) {
      LSPDocument *doc = lsp_document_find(server, uri);

      if (doc) {
        const char *hover_text = lsp_hover(doc, position, &temp_arena);
        if (hover_text) {
          char result[2048];
          snprintf(result, sizeof(result),
                   "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}",
                   hover_text);
          lsp_send_response(request_id, result);
        } else {
          lsp_send_response(request_id, "null");
        }
      } else {
        lsp_send_response(request_id, "null");
      }
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_DEFINITION: {
    fprintf(stderr, "[LSP] Handling definition\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    LSPPosition position = extract_position(message);

    if (uri) {
      LSPDocument *doc = lsp_document_find(server, uri);

      if (doc) {
        LSPLocation *loc = lsp_definition(doc, position, &temp_arena);
        if (loc) {
          char result[1024];
          snprintf(result, sizeof(result),
                   "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,"
                   "\"character\":%d},"
                   "\"end\":{\"line\":%d,\"character\":%d}}}",
                   loc->uri, loc->range.start.line, loc->range.start.character,
                   loc->range.end.line, loc->range.end.character);
          lsp_send_response(request_id, result);
        } else {
          lsp_send_response(request_id, "null");
        }
      } else {
        lsp_send_response(request_id, "null");
      }
    }
    break;
  }

  case LSP_METHOD_TEXT_DOCUMENT_COMPLETION: {
    fprintf(stderr, "[LSP] Handling completion\n");

    const char *uri = extract_string(message, "uri", &temp_arena);
    LSPPosition position = extract_position(message);

    if (uri) {
      LSPDocument *doc = lsp_document_find(server, uri);

      if (doc) {
        size_t count;
        LSPCompletionItem *items =
            lsp_completion(doc, position, &count, &temp_arena);

        if (items && count > 0) {
          char result[16384];
          serialize_completion_items(items, count, result, sizeof(result));
          lsp_send_response(request_id, result);
        } else {
          lsp_send_response(request_id, "{\"items\":[]}");
        }
      } else {
        lsp_send_response(request_id, "{\"items\":[]}");
      }
    }
    break;
  }

  case LSP_METHOD_SHUTDOWN:
    fprintf(stderr, "[LSP] Handling shutdown\n");
    lsp_send_response(request_id, "null");
    break;

  case LSP_METHOD_EXIT:
    fprintf(stderr, "[LSP] Exiting\n");
    exit(0);
    break;

  default:
    fprintf(stderr, "[LSP] Unhandled method\n");
    break;
  }

  arena_destroy(&temp_arena);
}
