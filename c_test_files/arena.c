#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t *buffer;   // memory buffer
    size_t capacity;   // total size of buffer
    size_t offset;     // current allocation offset
} Arena;

// Create an arena with a given capacity
Arena *arena_create(size_t capacity) {
    Arena *arena = malloc(sizeof(Arena));
    if (!arena) return NULL;
    arena->buffer = malloc(capacity);
    if (!arena->buffer) {
        free(arena);
        return NULL;
    }
    arena->capacity = capacity;
    arena->offset = 0;
    return arena;
}

// Allocate memory from the arena
void *arena_alloc(Arena *arena, size_t size) {
    if (arena->offset + size > arena->capacity) {
        return NULL; // out of memory
    }
    void *ptr = arena->buffer + arena->offset;
    arena->offset += size;
    return ptr;
}

// Reset arena (frees all allocations at once)
void arena_reset(Arena *arena) {
    arena->offset = 0;
}

// Free arena completely
void arena_destroy(Arena *arena) {
    if (arena) {
        free(arena->buffer);
        free(arena);
    }
}

// Example usage
int main(void) {
    Arena *arena = arena_create(1024); // 1 KB arena

    int *arr = (int *)arena_alloc(arena, 10 * sizeof(int));
    if (arr) {
        for (int i = 0; i < 10; i++) arr[i] = i * 2;
        for (int i = 0; i < 10; i++) printf("%d ", arr[i]);
        printf("\n");
    } else {
        printf("Allocation failed\n");
    }

    arena_reset(arena); // all allocations invalid now

    arena_destroy(arena);
    return 0;
}
