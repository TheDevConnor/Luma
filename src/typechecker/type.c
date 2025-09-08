/**
 * @file type.c
 * @brief Implementation of type checking and symbol table management functions
 * 
 * This file contains the implementation of the type checking system declared in type.h.
 * It provides concrete implementations for type comparison, type introspection utilities,
 * and debugging functions for the symbol table and scope management system.
 */

#include <string.h>
#include <stdio.h>

#include "type.h"

TypeMatchResult types_match(AstNode *type1, AstNode *type2) {
    // Null pointer safety check
    if (!type1 || !type2) return TYPE_MATCH_NONE;
    
    // Identity check - same pointer means exact match
    if (type1 == type2) return TYPE_MATCH_EXACT;
    
    // Both must be type nodes to be comparable
    if (type1->category != Node_Category_TYPE || type2->category != Node_Category_TYPE) {
        return TYPE_MATCH_NONE;
    }
    
    // Basic type matching with string comparison
    if (type1->type == AST_TYPE_BASIC && type2->type == AST_TYPE_BASIC) {
        const char *name1 = type1->type_data.basic.name;
        const char *name2 = type2->type_data.basic.name;
        
        if (strcmp(name1, name2) == 0) {
            return TYPE_MATCH_EXACT;
        }
        
        // Check for compatible numeric types (implicit conversions)
        if ((strcmp(name1, "int") == 0 && strcmp(name2, "float") == 0) ||
            (strcmp(name1, "float") == 0 && strcmp(name2, "int") == 0)) {
            return TYPE_MATCH_COMPATIBLE;
        }
        
        // Handle string/char* compatibility - string is compatible with char*
        if ((strcmp(name1, "string") == 0 && strcmp(name2, "char") == 0) ||
            (strcmp(name1, "char") == 0 && strcmp(name2, "string") == 0)) {
            return TYPE_MATCH_COMPATIBLE;
        }
    }
    
    // Handle string and char* pointer compatibility
    if (type1->type == AST_TYPE_BASIC && type2->type == AST_TYPE_POINTER) {
        const char *basic_name = type1->type_data.basic.name;
        AstNode *pointee = type2->type_data.pointer.pointee_type;
        
        // string is compatible with char*
        if (strcmp(basic_name, "string") == 0 && 
            pointee && pointee->category == Node_Category_TYPE && 
            pointee->type == AST_TYPE_BASIC &&
            strcmp(pointee->type_data.basic.name, "char") == 0) {
            return TYPE_MATCH_COMPATIBLE;
        }
    }
    
    if (type1->type == AST_TYPE_POINTER && type2->type == AST_TYPE_BASIC) {
        AstNode *pointee = type1->type_data.pointer.pointee_type;
        const char *basic_name = type2->type_data.basic.name;
        
        // char* is compatible with string
        if (pointee && pointee->category == Node_Category_TYPE && 
            pointee->type == AST_TYPE_BASIC &&
            strcmp(pointee->type_data.basic.name, "char") == 0 &&
            strcmp(basic_name, "string") == 0) {
            return TYPE_MATCH_COMPATIBLE;
        }
    }
    
    // Pointer type matching - recursively check pointee types
    if (type1->type == AST_TYPE_POINTER && type2->type == AST_TYPE_POINTER) {
        return types_match(type1->type_data.pointer.pointee_type, 
                          type2->type_data.pointer.pointee_type);
    }
    
    // Array type matching - recursively check element types
    if (type1->type == AST_TYPE_ARRAY && type2->type == AST_TYPE_ARRAY) {
        return types_match(type1->type_data.array.element_type,
                          type2->type_data.array.element_type);
    }
    
    // Handle array and pointer compatibility (arrays decay to pointers)
    if (type1->type == AST_TYPE_ARRAY && type2->type == AST_TYPE_POINTER) {
        return types_match(type1->type_data.array.element_type,
                          type2->type_data.pointer.pointee_type);
    }
    
    if (type1->type == AST_TYPE_POINTER && type2->type == AST_TYPE_ARRAY) {
        return types_match(type1->type_data.pointer.pointee_type,
                          type2->type_data.array.element_type);
    }
    
    return TYPE_MATCH_NONE;
}

bool is_numeric_type(AstNode *type) {
    // Validate input and ensure it's a basic type node
    if (!type || type->category != Node_Category_TYPE || type->type != AST_TYPE_BASIC) {
        return false;
    }
    
    const char *name = type->type_data.basic.name;
    return strcmp(name, "int") == 0 || 
           strcmp(name, "float") == 0 || 
           strcmp(name, "double") == 0 ||
           strcmp(name, "char") == 0;
}

bool is_pointer_type(AstNode *type) {
    return type && type->category == Node_Category_TYPE && type->type == AST_TYPE_POINTER;
}

bool is_array_type(AstNode *type) {
    return type && type->category == Node_Category_TYPE && type->type == AST_TYPE_ARRAY;
}

const char *type_to_string(AstNode *type, ArenaAllocator *arena) {
    // Handle null or invalid type nodes
    if (!type || type->category != Node_Category_TYPE) {
        return arena_strdup(arena, "unknown");
    }
    
    switch (type->type) {
        case AST_TYPE_BASIC:
            // Simple case - just return the basic type name
            return arena_strdup(arena, type->type_data.basic.name);
        
        case AST_TYPE_POINTER: {
            // Recursively get pointee type and append '*'
            const char *pointee = type_to_string(type->type_data.pointer.pointee_type, arena);
            size_t len = strlen(pointee) + 2;  // +1 for '*', +1 for null terminator
            char *result = arena_alloc(arena, len, len);
            snprintf(result, len, "%s*", pointee);
            return result;
        }
        
        case AST_TYPE_ARRAY: {
            // Recursively get element type and append '[]'
            const char *element = type_to_string(type->type_data.array.element_type, arena);
            size_t len = strlen(element) + 3; // +2 for "[]", +1 for null terminator
            char *result = arena_alloc(arena, len, len);
            snprintf(result, len, "%s[]", element);
            return result;
        }
        
        default:
            // Fallback for unknown or unhandled type categories
            return arena_strdup(arena, "unknown");
    }
}

AstNode *get_enclosing_function_return_type(Scope *scope) {
    while (scope) {
        if (scope->is_function_scope && scope->associated_node) {
            return scope->associated_node->stmt.func_decl.return_type;
        }
        scope = scope->parent;
    }
    return NULL;
}

void debug_print_scope(Scope *scope, int indent_level) {
    // Print current scope header with metadata
    for (int i = 0; i < indent_level; i++) printf("  ");
    printf("Scope '%s' (depth %zu, %zu symbols, %zu children, %zu imports):\n", 
           scope->scope_name, scope->depth, scope->symbols.count, 
           scope->children.count, scope->imported_modules.count);
    
    // Print imported modules if any
    if (scope->imported_modules.count > 0) {
        for (int i = 0; i < indent_level + 1; i++) printf("  ");
        printf("Imported modules:\n");
        
        for (size_t i = 0; i < scope->imported_modules.count; i++) {
            ModuleImport *import = (ModuleImport *)((char*)scope->imported_modules.data + i * sizeof(ModuleImport));
            
            for (int j = 0; j < indent_level + 2; j++) printf("  ");
            printf("- %s as %s (scope: %s)\n", 
                   import->module_name, 
                   import->alias,
                   import->module_scope ? import->module_scope->scope_name : "NULL");
        }
    }
    
    // Print all symbols in the current scope
    for (size_t i = 0; i < scope->symbols.count; i++) {
        // Calculate symbol pointer using array base + offset
        Symbol *s = (Symbol *)((char*)scope->symbols.data + i * sizeof(Symbol));
        
        // Indent for symbol display
        for (int j = 0; j < indent_level + 1; j++) printf("  ");
        
        // Print symbol information with type name (simplified for basic types)
        printf("- %s: %s (public: %d, mutable: %d)\n", 
               s->name, 
               s->type && s->type->category == Node_Category_TYPE && s->type->type == AST_TYPE_BASIC 
                   ? s->type->type_data.basic.name : "complex_type",
               s->is_public, s->is_mutable);
    }
    
    // Recursively print all child scopes if any exist
    if (scope->children.count > 0) {
        for (int i = 0; i < indent_level + 1; i++) printf("  ");
        printf("Child scopes:\n");
        
        for (size_t i = 0; i < scope->children.count; i++) {
            // Calculate child scope pointer using array base + offset
            Scope **child_ptr = (Scope **)((char*)scope->children.data + i * sizeof(Scope*));
            Scope *child = *child_ptr;
            
            // Recursive call with increased indentation
            debug_print_scope(child, indent_level + 2);
        }
    }
}
