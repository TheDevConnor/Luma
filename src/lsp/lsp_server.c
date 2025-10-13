/**
 * @file lsp_server.c
 * @brief Language Server Protocol implementation with type error diagnostics
 */

#include "lsp.h"
#include <dirent.h> // For Unix
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *lsp_uri_to_path(const char *uri, ArenaAllocator *arena) {
  if (!uri)
    return NULL;

  if (strncmp(uri, "file://", 7) == 0) {
    const char *path = uri + 7;
#ifdef _WIN32
    if (path[0] == '/' && path[2] == ':') {
      path++;
    }
#endif
    return arena_strdup(arena, path);
  }

  return arena_strdup(arena, uri);
}

const char *lsp_path_to_uri(const char *path, ArenaAllocator *arena) {
  if (!path)
    return NULL;

  size_t len = strlen(path) + 8;
  char *uri = arena_alloc(arena, len, 1);

#ifdef _WIN32
  snprintf(uri, len, "file:///%s", path);
#else
  snprintf(uri, len, "file://%s", path);
#endif

  return uri;
}

bool lsp_server_init(LSPServer *server, ArenaAllocator *arena) {
  if (!server || !arena)
    return false;

  server->arena = arena;
  server->initialized = false;
  server->client_process_id = -1;
  server->document_count = 0;
  server->document_capacity = 16;

  server->documents =
      arena_alloc(arena, server->document_capacity * sizeof(LSPDocument *),
                  alignof(LSPDocument *));

  // Initialize module registry
  server->module_registry.entries = NULL;
  server->module_registry.count = 0;
  server->module_registry.capacity = 0;

  return server->documents != NULL;
}

void lsp_server_run(LSPServer *server) {
  if (!server)
    return;

  fprintf(stderr, "[LSP] Server started, waiting for messages...\n");

  char buffer[65536];

  while (1) {
    // Read Content-Length header
    if (!fgets(buffer, sizeof(buffer), stdin)) {
      break;
    }

    if (strncmp(buffer, "Content-Length:", 15) != 0) {
      continue;
    }

    int content_length = atoi(buffer + 15);
    fprintf(stderr, "[LSP] Content-Length: %d\n", content_length);

    // Skip empty line
    fgets(buffer, sizeof(buffer), stdin);

    // Read message
    if (content_length > 0 && content_length < (int)sizeof(buffer)) {
      size_t read = fread(buffer, 1, content_length, stdin);
      buffer[read] = '\0';

      lsp_handle_message(server, buffer);
    }
  }
}

void lsp_server_shutdown(LSPServer *server) {
  if (!server)
    return;

  // Clean up all documents
  for (size_t i = 0; i < server->document_count; i++) {
    if (server->documents[i]->arena) {
      arena_destroy(server->documents[i]->arena);
    }
  }

  server->initialized = false;
  server->document_count = 0;
}
