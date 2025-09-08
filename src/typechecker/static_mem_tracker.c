#include "type.h"
#include <stdio.h>
#include <string.h>

void static_memory_analyzer_init(StaticMemoryAnalyzer *analyzer, ArenaAllocator *arena) {
    analyzer->arena = arena;
    growable_array_init(&analyzer->allocations, arena, 32, sizeof(StaticAllocation));
}

void static_memory_track_alloc(StaticMemoryAnalyzer *analyzer, size_t line, size_t column, const char *var_name) {
    // Don't track anonymous allocations - let variable declarations handle it
    if (!var_name || strcmp(var_name, "anonymous") == 0) {
        return;
    }
    
    StaticAllocation *alloc = (StaticAllocation *)growable_array_push(&analyzer->allocations);
    if (alloc) {
        alloc->line = line;
        alloc->column = column;
        alloc->variable_name = arena_strdup(analyzer->arena, var_name);
        alloc->has_matching_free = false;
        alloc->free_count = 0;  // Track number of times freed
        // printf("STATIC: Tracked alloc() for variable '%s' at line %zu:%zu\n", var_name, line, column);
    }
}

void static_memory_track_free(StaticMemoryAnalyzer *analyzer, const char *var_name) {
    if (!var_name || strcmp(var_name, "unknown") == 0) {
        printf("Warning: free() called on unknown variable\n");
        return;
    }
    
    bool found = false;
    StaticAllocation *target_alloc = NULL;
    
    // Find the allocation for this variable
    for (size_t i = 0; i < analyzer->allocations.count; i++) {
        StaticAllocation *alloc = (StaticAllocation *)((char *)analyzer->allocations.data + 
                                                       i * sizeof(StaticAllocation));
        if (alloc->variable_name && strcmp(alloc->variable_name, var_name) == 0) {
            target_alloc = alloc;
            found = true;
            break;
        }
    }
    
    if (!found) {
        printf("Warning: free() called on variable '%s' without matching alloc()\n", var_name);
        return;
    }
    
    // Check for double free
    if (target_alloc->has_matching_free) {
        target_alloc->free_count++;
        printf("ERROR: Double free detected! Variable '%s' freed %d times (originally allocated at line %zu:%zu)\n", 
               var_name, target_alloc->free_count + 1, target_alloc->line, target_alloc->column);
        return;
    }
    
    // Mark as freed for the first time
    target_alloc->has_matching_free = true;
    target_alloc->free_count = 1;
    // printf("STATIC: Tracked free(%s) - matches allocation at line %zu:%zu\n", 
    //        var_name, target_alloc->line, target_alloc->column);
}

void static_memory_report_leaks(StaticMemoryAnalyzer *analyzer) {
    size_t leak_count = 0;
    size_t double_free_count = 0;
    
    for (size_t i = 0; i < analyzer->allocations.count; i++) {
        StaticAllocation *alloc = (StaticAllocation *)((char *)analyzer->allocations.data + 
                                                       i * sizeof(StaticAllocation));
        
        if (alloc->free_count > 1) {
            printf("DOUBLE FREE ERROR: Variable '%s' allocated at line %zu:%zu was freed %d times\n",
                   alloc->variable_name, alloc->line, alloc->column, alloc->free_count);
            double_free_count++;
        } else if (!alloc->has_matching_free) {
            printf("POTENTIAL LEAK: Variable '%s' allocated at line %zu:%zu but never freed\n",
                   alloc->variable_name, alloc->line, alloc->column);
            leak_count++;
        }
    }
    
    if (leak_count == 0 && double_free_count == 0) {
        // printf("No memory issues detected!\n");
    } else {
        if (leak_count > 0) {
            printf("Found %zu potential memory leak(s)\n", leak_count);
        }
        if (double_free_count > 0) {
            printf("Found %zu double free error(s)\n", double_free_count);
        }
    }
}

StaticMemoryAnalyzer *get_static_analyzer(Scope *scope) {
    Scope *current = scope;
    while (current) {
        if (current->memory_analyzer) {
            return current->memory_analyzer;
        }
        current = current->parent;
    }
    return NULL;
}

const char *extract_variable_name_from_free(AstNode *free_expr) {
    AstNode *ptr_expr = free_expr->expr.free.ptr;
    
    if (ptr_expr && ptr_expr->type == AST_EXPR_IDENTIFIER) {
        return ptr_expr->expr.identifier.name;
    }
    
    return "unknown";
}