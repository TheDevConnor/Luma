/**
 * @file memory.c
 * @brief Implementation of arena allocator and growable array utilities.
 *
 * This module implements a memory arena allocator that reduces overhead by
 * allocating large buffers and serving sequential allocations from them.
 * It also provides a growable array type that uses the arena for backing
 * storage.
 *
 * Features:
 * - Dynamic buffer sizing with growth factor.
 * - Aligned allocations.
 * - Reset and free all buffers at once.
 * - Debug print support for allocations and frees.
 *
 * @see memory.h
 */

#include "memory.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <malloc.h>
#define ALIGNED_ALLOC(sz, align) _aligned_malloc((sz), (align))
#define ALIGNED_FREE(ptr) _aligned_free(ptr)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdlib.h>
#define ALIGNED_ALLOC(sz, align) aligned_alloc((align), (sz))
#define ALIGNED_FREE(ptr) free(ptr)
#else
#error "aligned_alloc or platform equivalent not available"
#endif

#ifndef DEBUG_PRINT
#define DEBUG_PRINT(...)                                                       \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
  } while (0)
#endif

/**
 * @brief Returns the maximum of two size_t values.
 * @param a First value.
 * @param b Second value.
 * @return The larger of a and b.
 */
static inline size_t max_size(size_t a, size_t b) { return (a > b) ? a : b; }

/**
 * @brief Returns the minimum of two size_t values.
 * @param a First value.
 * @param b Second value.
 * @return The smaller of a and b.
 */
static inline size_t min_size(size_t a, size_t b) { return (a < b) ? a : b; }

/**
 * @brief Calculates the next buffer size for arena allocation using growth
 * factors.
 *
 * Ensures the new size respects minimum and maximum buffer size constraints.
 *
 * @param current_size Current buffer size.
 * @param requested_size Minimum requested size for the new buffer.
 * @return The computed next buffer size.
 */
static size_t calculate_next_buffer_size(size_t current_size,
                                         size_t requested_size) {
  size_t next_size =
      max_size(current_size * ARENA_GROWTH_FACTOR, requested_size);
  next_size = max_size(next_size, ARENA_MIN_BUFFER_SIZE);
  next_size = min_size(next_size, ARENA_MAX_BUFFER_SIZE);
  return next_size;
}

/**
 * @brief Creates a new aligned buffer for the arena allocator.
 *
 * Allocates memory for a Buffer struct plus usable space aligned according to
 * the alignment.
 *
 * @param s Size of usable memory requested.s
 * @param alignment Alignment requirement for the usable memory.
 * @return Pointer to the new Buffer on success, or NULL on failure.
 */
Buffer *buffer_create(size_t s, size_t alignment) {
  if (alignment == 0)
    alignment = 1024;

  alignment = max_size(alignment, alignof(Buffer));

  size_t total_size = sizeof(Buffer) + s + alignment;
  size_t aligned_total_size = (total_size + alignment - 1) & ~(alignment - 1);

#if defined(_WIN32)
  void *raw = _aligned_malloc(aligned_total_size, alignment);
#else
  void *raw = aligned_alloc(alignment, aligned_total_size);
#endif

  TRACK_ALLOC(raw);
  if (!raw) {
    DEBUG_PRINT("buffer_create: FAILED to allocate %zu bytes (align %zu)\n",
                aligned_total_size, alignment);
    return NULL;
  }

  memset(raw, 0, aligned_total_size);

  Buffer *buf = (Buffer *)raw;
  buf->size = s;
  buf->next = NULL;

  char *raw_bytes = (char *)raw;
  size_t struct_end = (size_t)(raw_bytes + sizeof(Buffer));
  size_t aligned_data = (struct_end + alignment - 1) & ~(alignment - 1);
  buf->ptr = (char *)aligned_data;

  DEBUG_PRINT("buffer_create: allocated buffer %p (%zu bytes aligned to %zu), "
              "usable ptr %p\n",
              (void *)buf, aligned_total_size, alignment, (void *)buf->ptr);

  return buf;
}

/**
 * @brief Initializes an ArenaAllocator with a specified initial buffer size.
 *
 * Allocates the first buffer and sets initial parameters.
 *
 * @param arena Pointer to the ArenaAllocator to initialize.
 * @param initial_size Initial buffer size to allocate (will be clamped to
 * minimum size).
 * @return 0 on success, -1 on failure.
 */
int arena_allocator_init(ArenaAllocator *arena, size_t initial_size) {
  if (!arena)
    return -1;

  initial_size = max_size(initial_size, ARENA_MIN_BUFFER_SIZE);

  arena->buffer = buffer_create(initial_size, 1024);
  if (!arena->buffer)
    return -1;

  arena->head = arena->buffer;
  arena->offset = 0;
  arena->next_buffer_size = calculate_next_buffer_size(initial_size, 0);
  arena->total_allocated = initial_size;

  DEBUG_PRINT("arena_allocator_init: initialized with %zu bytes, next buffer "
              "will be %zu bytes\n",
              initial_size, arena->next_buffer_size);

  return 0;
}

/**
 * @brief Adds a new buffer to the arena's linked list with at least min_size.
 *
 * Allocates a new buffer, adds it to the chain, and updates the allocator
 * metadata.
 *
 * @param arena Pointer to the ArenaAllocator.
 * @param min_size Minimum size of the buffer to allocate.
 * @return Pointer to the new Buffer, or NULL on allocation failure.
 */
static Buffer *arena_add_buffer(ArenaAllocator *arena, size_t min_size) {
  size_t buffer_size =
      calculate_next_buffer_size(arena->next_buffer_size, min_size);

  Buffer *new_buffer = buffer_create(buffer_size, 1024);
  if (!new_buffer) {
    DEBUG_PRINT("arena_add_buffer: FAILED to create new buffer of size %zu\n",
                buffer_size);
    return NULL;
  }

  Buffer *current = arena->head;
  while (current->next) {
    current = current->next;
  }
  current->next = new_buffer;

  arena->total_allocated += buffer_size;
  arena->next_buffer_size = calculate_next_buffer_size(buffer_size, 0);

  DEBUG_PRINT("arena_add_buffer: added new buffer %p (size %zu), total "
              "allocated: %zu bytes\n",
              (void *)new_buffer, buffer_size, arena->total_allocated);

  return new_buffer;
}

/**
 * @brief Allocates memory from the arena allocator with automatic buffer
 * growth.
 *
 * Attempts to allocate aligned memory from current buffers. If insufficient
 * space, it moves to the next buffer or allocates a new one.
 *
 * Large allocations create dedicated buffers.
 *
 * @param arena Pointer to the ArenaAllocator.
 * @param size Number of bytes to allocate.
 * @param alignment Memory alignment requirement.
 * @return Pointer to allocated memory or NULL on failure.
 */
void *arena_alloc(ArenaAllocator *arena, size_t size, size_t alignment) {
  if (!arena || !arena->buffer)
    return NULL;

  if (alignment == 0)
    alignment = alignof(max_align_t);

  if (size > ARENA_MAX_BUFFER_SIZE / 4) {
    Buffer *dedicated_buffer = arena_add_buffer(arena, size);
    if (!dedicated_buffer)
      return NULL;

    // Zero out the allocated memory
    memset(dedicated_buffer->ptr, 0, size);

    DEBUG_PRINT(
        "arena_alloc (dedicated): %zu bytes (align %zu) at %p in buffer %p\n",
        size, alignment, dedicated_buffer->ptr, (void *)dedicated_buffer);
    return dedicated_buffer->ptr;
  }

  while (1) {
    size_t aligned_offset =
        (arena->offset + (alignment - 1)) & ~(alignment - 1);

    if (aligned_offset + size <= arena->buffer->size) {
      void *ptr = arena->buffer->ptr + aligned_offset;
      arena->offset = aligned_offset + size;

      DEBUG_PRINT("arena_alloc: %zu bytes (align %zu) at %p in buffer %p "
                  "(offset now %zu/%zu)\n",
                  size, alignment, ptr, (void *)arena->buffer, arena->offset,
                  arena->buffer->size);
      return ptr;
    }

    if (arena->buffer->next) {
      arena->buffer = arena->buffer->next;
      arena->offset = 0;
      DEBUG_PRINT("arena_alloc: switched to existing buffer %p (size %zu)\n",
                  (void *)arena->buffer, arena->buffer->size);
      continue;
    }

    Buffer *new_buffer = arena_add_buffer(arena, size);
    if (!new_buffer) {
      DEBUG_PRINT("arena_alloc: FAILED to add new buffer for %zu bytes\n",
                  size);
      return NULL;
    }

    arena->buffer = new_buffer;
    arena->offset = 0;
    DEBUG_PRINT("arena_alloc: switched to new buffer %p (size %zu)\n",
                (void *)arena->buffer, arena->buffer->size);
  }
}

void *arena_realloc(ArenaAllocator *arena, void *ptr, size_t old_size,
                    size_t new_size, size_t alignment) {
  if (!arena || !arena->buffer)
    return NULL;

  // Handle realloc(NULL, size) case
  if (!ptr) {
    return arena_alloc(arena, new_size, alignment);
  }

  // Handle realloc(ptr, 0) case - equivalent to free (but we can't actually
  // free in arena)
  if (new_size == 0) {
    DEBUG_PRINT("arena_realloc: size 0 requested, returning NULL (cannot free "
                "in arena)\n");
    return NULL;
  }

  if (alignment == 0)
    alignment = alignof(max_align_t);

  // Check if this is the most recent allocation in the current buffer
  size_t aligned_old_size = (old_size + (alignment - 1)) & ~(alignment - 1);
  char *expected_end = (char *)ptr + aligned_old_size;
  char *current_end = arena->buffer->ptr + arena->offset;

  bool is_last_allocation = (expected_end == current_end);

  if (is_last_allocation) {
    // We can potentially resize in place
    size_t aligned_new_size = (new_size + (alignment - 1)) & ~(alignment - 1);
    char *new_end = (char *)ptr + aligned_new_size;
    size_t buffer_end = (size_t)(arena->buffer->ptr + arena->buffer->size);

    if ((size_t)new_end <= buffer_end) {
      // Can resize in place
      arena->offset = (char *)ptr + aligned_new_size - arena->buffer->ptr;

      // Zero any newly allocated space
      if (new_size > old_size) {
        memset((char *)ptr + old_size, 0, new_size - old_size);
      }

      DEBUG_PRINT(
          "arena_realloc: resized in place from %zu to %zu bytes at %p\n",
          old_size, new_size, ptr);
      return ptr;
    }
  }

  // Cannot resize in place, allocate new block and copy
  void *new_ptr = arena_alloc(arena, new_size, alignment);
  if (!new_ptr) {
    DEBUG_PRINT("arena_realloc: failed to allocate new block of size %zu\n",
                new_size);
    return NULL;
  }

  // Copy data (copy the smaller of old_size or new_size)
  size_t copy_size = (old_size < new_size) ? old_size : new_size;
  memcpy(new_ptr, ptr, copy_size);

  DEBUG_PRINT(
      "arena_realloc: allocated new block, copied %zu bytes from %p to %p\n",
      copy_size, ptr, new_ptr);

  // Note: Cannot free old block in arena allocator
  return new_ptr;
}

/**
 * @brief Resets the arena allocator, making all buffers reusable.
 *
 * Does not free memory; resets the current allocation offset and buffer
 * pointer.
 *
 * @param arena Pointer to the ArenaAllocator to reset.
 */
void arena_reset(ArenaAllocator *arena) {
  if (!arena)
    return;

  arena->offset = 0;
  arena->buffer = arena->head;
  DEBUG_PRINT("arena_reset: reset to first buffer %p\n", (void *)arena->head);
}

/**
 * @brief Frees all buffers and resets the arena allocator.
 *
 * @param arena Pointer to the ArenaAllocator to destroy.
 */
void arena_destroy(ArenaAllocator *arena) {
  if (!arena)
    return;

  Buffer *current = arena->head;
  while (current) {
    DEBUG_PRINT("arena_destroy: freeing buffer %p (size %zu, ptr %p)\n",
                (void *)current, current->size, (void *)current->ptr);
    Buffer *next = current->next;
#if defined(_WIN32)
    _aligned_free(current);
#else
    free(current);
#endif
    TRACK_FREE(current);
    current = next;
  }

  arena->head = NULL;
  arena->buffer = NULL;
  arena->offset = 0;
  arena->next_buffer_size = 0;
  arena->total_allocated = 0;
}

/**
 * @brief Returns the total number of bytes allocated across all buffers.
 *
 * @param arena Pointer to the ArenaAllocator.
 * @return Total allocated bytes, or 0 if arena is NULL.
 */
size_t arena_get_total_allocated(ArenaAllocator *arena) {
  if (!arena)
    return 0;
  return arena->total_allocated;
}

/**
 * @brief Prints statistics about the arena allocator to stderr.
 *
 * Includes number of buffers, total allocated memory, current buffer and
 * offset, and the planned size for the next buffer.
 *
 * @param arena Pointer to the ArenaAllocator.
 */
void arena_print_stats(ArenaAllocator *arena) {
  if (!arena) {
    fprintf(stderr, "Arena is NULL\n");
    return;
  }

  size_t buffer_count = 0;
  size_t total_size = 0;
  Buffer *current = arena->head;

  while (current) {
    buffer_count++;
    total_size += current->size;
    current = current->next;
  }

  fprintf(stderr, "Arena Statistics:\n");
  fprintf(stderr, "  Total buffers: %zu\n", buffer_count);
  fprintf(stderr, "  Total allocated: %zu bytes (%.2f MB)\n", total_size,
          (double)total_size / (1024 * 1024));
  fprintf(stderr, "  Current buffer: %p (offset: %zu/%zu)\n",
          (void *)arena->buffer, arena->offset,
          arena->buffer ? arena->buffer->size : 0);
  fprintf(stderr, "  Next buffer size: %zu bytes\n", arena->next_buffer_size);
}

/**
 * @brief Duplicates a null-terminated string into arena-allocated memory.
 *
 * @param arena Pointer to the ArenaAllocator.
 * @param src Null-terminated source string.
 * @return Pointer to the duplicated string in arena memory, or NULL on failure.
 */
char *arena_strdup(ArenaAllocator *arena, const char *src) {
  size_t len = strlen(src) + 1;
  char *dst = arena_alloc(arena, len, alignof(char));
  if (dst)
    memcpy(dst, src, len);
  return dst;
}

/**
 * @brief Initializes a GrowableArray backed by an arena allocator.
 *
 * Allocates initial storage for the array and sets metadata.
 *
 * @param arr Pointer to the GrowableArray to initialize.
 * @param arena Pointer to the ArenaAllocator for backing storage.
 * @param initial_capacity Initial number of elements to allocate space for.
 * @param item_size Size in bytes of each element.
 * @return true on success, false on failure.
 */
bool growable_array_init(GrowableArray *arr, ArenaAllocator *arena,
                         size_t initial_capacity, size_t item_size) {
  if (initial_capacity == 0)
    initial_capacity = 4;

  size_t alignment = (item_size == sizeof(void *)) ? alignof(void *)
                     : (item_size >= alignof(max_align_t))
                         ? alignof(max_align_t)
                         : item_size;

  arr->data = arena_alloc(arena, initial_capacity * item_size, alignment);
  if (!arr->data) {
    DEBUG_PRINT("growable_array_init: FAILED to allocate initial array "
                "(cap=%zu, size=%zu, align=%zu)\n",
                initial_capacity, item_size, alignment);
    return false;
  }

  arr->arena = arena;
  arr->count = 0;
  arr->capacity = initial_capacity;
  arr->item_size = item_size;

  DEBUG_PRINT("growable_array_init: created array at %p (cap=%zu, "
              "item_size=%zu, alignment=%zu, total=%zu bytes)\n",
              arr->data, arr->capacity, arr->item_size, alignment,
              arr->capacity * arr->item_size);

  return true;
}

/**
 * @brief Pushes a new element onto the growable array, resizing if necessary.
 *
 * Automatically doubles the array capacity and copies existing elements
 * into new arena memory when full.
 *
 * @param arr Pointer to the GrowableArray.
 * @return Pointer to the newly allocated element slot, or NULL on failure.
 */
void *growable_array_push(GrowableArray *arr) {
  if (arr->count >= arr->capacity) {
    size_t new_capacity = arr->capacity * 2;
    size_t alignment = (arr->item_size == sizeof(void *)) ? alignof(void *)
                       : (arr->item_size >= alignof(max_align_t))
                           ? alignof(max_align_t)
                           : arr->item_size;

    void *new_block =
        arena_alloc(arr->arena, new_capacity * arr->item_size, alignment);
    if (!new_block) {
      DEBUG_PRINT("growable_array_push: FAILED to grow array (new cap=%zu, "
                  "total size=%zu)\n",
                  new_capacity, new_capacity * arr->item_size);
      return NULL;
    }

    DEBUG_PRINT("growable_array_push: growing array from %p to %p "
                "(old cap=%zu → new cap=%zu, item_size=%zu)\n",
                arr->data, new_block, arr->capacity, new_capacity,
                arr->item_size);

    memcpy(new_block, arr->data, arr->count * arr->item_size);
    arr->data = new_block;
    arr->capacity = new_capacity;
  }

  void *slot = (char *)arr->data + (arr->count * arr->item_size);
  arr->count++;

  DEBUG_PRINT("growable_array_push: pushed item at index %zu (address %p)\n",
              arr->count - 1, slot);

  return slot;
}
