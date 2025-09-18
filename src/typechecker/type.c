/**
 * @file type.c
 * @brief Implementation of type checking and symbol table management functions
 *
 * This file contains the implementation of the type checking system declared in
 * type.h. It provides concrete implementations for type comparison, type
 * introspection utilities, and debugging functions for the symbol table and
 * scope management system.
 */

#include <stdio.h>
#include <string.h>

#include "type.h"

TypeMatchResult types_match(AstNode *type1, AstNode *type2) {
  // Null pointer safety check
  if (!type1 || !type2)
    return TYPE_MATCH_NONE;

  // Identity check - same pointer means exact match
  if (type1 == type2)
    return TYPE_MATCH_EXACT;

  // Both must be type nodes to be comparable
  if (type1->category != Node_Category_TYPE ||
      type2->category != Node_Category_TYPE) {
    return TYPE_MATCH_NONE;
  }

  if (type1->type == AST_TYPE_STRUCT && type2->type == AST_TYPE_STRUCT) {
    // Structs match if they have the same name
    // (assuming nominal typing - structs with same structure but different
    // names are different)
    const char *name1 = type1->type_data.struct_type.name;
    const char *name2 = type2->type_data.struct_type.name;

    if (strcmp(name1, name2) == 0) {
      return TYPE_MATCH_EXACT;
    }

    return TYPE_MATCH_NONE;
  }

  // Basic type matching with string comparison
  if (type1->type == AST_TYPE_BASIC && type2->type == AST_TYPE_BASIC) {
    const char *name1 = type1->type_data.basic.name;
    const char *name2 = type2->type_data.basic.name;

    if (strcmp(name1, name2) == 0) {
      return TYPE_MATCH_EXACT;
    }

    // Check if one is an enum and the other is int (allow enum <-> int
    // conversion) This assumes enum names are not "int", "float", etc.
    bool type1_is_builtin =
        (strcmp(name1, "int") == 0 || strcmp(name1, "float") == 0 ||
         strcmp(name1, "double") == 0 || strcmp(name1, "bool") == 0 ||
         strcmp(name1, "string") == 0 || strcmp(name1, "char") == 0 ||
         strcmp(name1, "void") == 0);
    bool type2_is_builtin =
        (strcmp(name2, "int") == 0 || strcmp(name2, "float") == 0 ||
         strcmp(name2, "double") == 0 || strcmp(name2, "bool") == 0 ||
         strcmp(name2, "string") == 0 || strcmp(name2, "char") == 0 ||
         strcmp(name2, "void") == 0);

    // Allow enum to int conversion (one is enum, other is int)
    if (!type1_is_builtin && strcmp(name2, "int") == 0) {
      return TYPE_MATCH_COMPATIBLE; // enum -> int
    }
    if (!type2_is_builtin && strcmp(name1, "int") == 0) {
      return TYPE_MATCH_COMPATIBLE; // int -> enum
    }

    // Standard numeric conversions
    if ((strcmp(name1, "int") == 0 && strcmp(name2, "float") == 0) ||
        (strcmp(name1, "float") == 0 && strcmp(name2, "int") == 0)) {
      return TYPE_MATCH_COMPATIBLE;
    }
  }

  // Handle string and char* pointer compatibility
  if (type1->type == AST_TYPE_BASIC && type2->type == AST_TYPE_POINTER) {
    const char *basic_name = type1->type_data.basic.name;
    AstNode *pointee = type2->type_data.pointer.pointee_type;

    // string is compatible with char*
    if (strcmp(basic_name, "string") == 0 && pointee &&
        pointee->category == Node_Category_TYPE &&
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

  // Enhanced array type matching - check both element types AND sizes
  // Add this debug version to your types_match function in the array section:

  // Enhanced array type matching - check both element types AND sizes
  if (type1->type == AST_TYPE_ARRAY && type2->type == AST_TYPE_ARRAY) {
    printf("DEBUG: Comparing array types\n");

    // First check if element types match
    TypeMatchResult element_match =
        types_match(type1->type_data.array.element_type,
                    type2->type_data.array.element_type);

    if (element_match == TYPE_MATCH_NONE) {
      return TYPE_MATCH_NONE;
    }

    // Now check array sizes if both have size expressions
    AstNode *size1 = type1->type_data.array.size;
    AstNode *size2 = type2->type_data.array.size;

    // If both arrays have explicit sizes, compare them
    if (size1 && size2) {
      // For literal integer sizes, we can do direct comparison
      if (size1->type == AST_EXPR_LITERAL && size2->type == AST_EXPR_LITERAL &&
          size1->expr.literal.lit_type == LITERAL_INT &&
          size2->expr.literal.lit_type == LITERAL_INT) {

        long long val1 = size1->expr.literal.value.int_val;
        long long val2 = size2->expr.literal.value.int_val;

        if (val1 == val2) {
          return element_match; // Return the element match result (EXACT or
                                // COMPATIBLE)
        } else {
          return TYPE_MATCH_NONE; // Same element type but different sizes
        }
      }

      // For more complex size expressions, we could add more sophisticated
      // comparison logic here if needed. For now, assume they don't match
      // unless they're the same node (pointer equality)
      if (size1 == size2) {
        return element_match;
      }

      // Different size expressions - assume incompatible for safety
      return TYPE_MATCH_NONE;
    }

    // If one or both arrays have no explicit size (e.g., int[]),
    // they're compatible if element types match
    if (!size1 || !size2) {
      return element_match;
    }

    return element_match;
  }

  // Handle array and pointer compatibility (arrays decay to pointers)
  // Note: When arrays decay to pointers, size information is lost,
  // so we only check element/pointee type compatibility
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
  if (!type || type->category != Node_Category_TYPE ||
      type->type != AST_TYPE_BASIC) {
    return false;
  }

  const char *name = type->type_data.basic.name;
  return strcmp(name, "int") == 0 || strcmp(name, "float") == 0 ||
         strcmp(name, "double") == 0 || strcmp(name, "char") == 0;
}

bool is_pointer_type(AstNode *type) {
  return type && type->category == Node_Category_TYPE &&
         type->type == AST_TYPE_POINTER;
}

bool is_array_type(AstNode *type) {
  return type && type->category == Node_Category_TYPE &&
         type->type == AST_TYPE_ARRAY;
}

const char *type_to_string(AstNode *type, ArenaAllocator *arena) {
  // Handle null or invalid type nodes
  if (!type) {
    return arena_strdup(arena, "<null_type>");
  }

  if (type->category != Node_Category_TYPE) {
    return arena_strdup(arena, "<invalid_type>");
  }

  switch (type->type) {
  case AST_TYPE_BASIC: {
    const char *name = type->type_data.basic.name;
    if (!name) {
      return arena_strdup(arena, "<unnamed_basic_type>");
    }
    return arena_strdup(arena, name);
  }

  case AST_TYPE_STRUCT: {
    const char *struct_name = type->type_data.struct_type.name;
    if (!struct_name) {
      return arena_strdup(arena, "<unnamed_struct>");
    }
    size_t len = strlen("struct ") + strlen(struct_name) + 1;
    char *result = arena_alloc(arena, len, len);
    snprintf(result, len, "struct %s", struct_name);
    return result;
  }

  case AST_TYPE_POINTER: {
    AstNode *pointee = type->type_data.pointer.pointee_type;
    if (!pointee) {
      return arena_strdup(arena, "<null>*");
    }
    const char *pointee_str = type_to_string(pointee, arena);
    size_t len = strlen(pointee_str) + 2; // +1 for '*', +1 for null terminator
    char *result = arena_alloc(arena, len, len);
    snprintf(result, len, "%s*", pointee_str);
    return result;
  }

  case AST_TYPE_ARRAY: {
    AstNode *element = type->type_data.array.element_type;
    if (!element) {
      return arena_strdup(arena, "<null>[]");
    }
    const char *element_str = type_to_string(element, arena);
    size_t len = strlen(element_str) + 3; // +2 for "[]", +1 for null terminator
    char *result = arena_alloc(arena, len, len);
    snprintf(result, len, "%s[]", element_str);
    return result;
  }

  case AST_TYPE_FUNCTION: {
    // For function types, show return type and parameter count
    AstNode *return_type = type->type_data.function.return_type;
    size_t param_count = type->type_data.function.param_count;

    const char *return_str =
        return_type ? type_to_string(return_type, arena) : "<null>";

    size_t len =
        strlen("fn(") + strlen(return_str) + 20; // extra space for param count
    char *result = arena_alloc(arena, len, len);
    snprintf(result, len, "fn(%zu params) -> %s", param_count, return_str);
    return result;
  }

  default:
    // Fallback for unknown or unhandled type categories
    return arena_strdup(arena, "<unknown_type>");
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

AstNode *create_struct_type(ArenaAllocator *arena, const char *name,
                            AstNode **member_types, const char **member_names,
                            size_t member_count, size_t line, size_t column) {
  AstNode *struct_type = arena_alloc(arena, sizeof(AstNode), alignof(AstNode));
  struct_type->type = AST_TYPE_STRUCT;
  struct_type->category = Node_Category_TYPE;
  struct_type->line = line;
  struct_type->column = column;

  struct_type->type_data.struct_type.name = arena_strdup(arena, name);
  struct_type->type_data.struct_type.member_count = member_count;

  // Allocate and copy member types
  struct_type->type_data.struct_type.member_types =
      arena_alloc(arena, member_count * sizeof(AstNode *), alignof(AstNode *));
  for (size_t i = 0; i < member_count; i++) {
    struct_type->type_data.struct_type.member_types[i] = member_types[i];
  }

  // Allocate and copy member names
  struct_type->type_data.struct_type.member_names =
      arena_alloc(arena, member_count * sizeof(char *), alignof(char *));
  for (size_t i = 0; i < member_count; i++) {
    struct_type->type_data.struct_type.member_names[i] =
        arena_strdup(arena, member_names[i]);
  }

  return struct_type;
}

AstNode *get_struct_member_type(AstNode *struct_type, const char *member_name) {
  if (!struct_type || struct_type->type != AST_TYPE_STRUCT || !member_name) {
    return NULL;
  }

  for (size_t i = 0; i < struct_type->type_data.struct_type.member_count; i++) {
    if (strcmp(struct_type->type_data.struct_type.member_names[i],
               member_name) == 0) {
      return struct_type->type_data.struct_type.member_types[i];
    }
  }

  return NULL;
}

bool struct_has_member(AstNode *struct_type, const char *member_name) {
  return get_struct_member_type(struct_type, member_name) != NULL;
}

void debug_print_struct_type(AstNode *struct_type, int indent) {
  if (!struct_type || struct_type->type != AST_TYPE_STRUCT) {
    printf("Not a struct type\n");
    return;
  }

  for (int i = 0; i < indent; i++)
    printf("  ");
  printf("Struct '%s' (%zu members):\n",
         struct_type->type_data.struct_type.name,
         struct_type->type_data.struct_type.member_count);

  for (size_t i = 0; i < struct_type->type_data.struct_type.member_count; i++) {
    for (int j = 0; j < indent + 1; j++)
      printf("  ");

    AstNode *member_type = struct_type->type_data.struct_type.member_types[i];
    const char *member_name =
        struct_type->type_data.struct_type.member_names[i];

    // Simple type name extraction without arena allocation
    const char *type_name = "unknown";
    if (member_type && member_type->category == Node_Category_TYPE) {
      switch (member_type->type) {
      case AST_TYPE_BASIC:
        if (member_type->type_data.basic.name) {
          type_name = member_type->type_data.basic.name;
        }
        break;
      case AST_TYPE_POINTER:
        type_name = "pointer";
        break;
      case AST_TYPE_FUNCTION:
        type_name = "function";
        break;
      case AST_TYPE_STRUCT:
        type_name = "struct";
        break;
      default:
        type_name = "complex_type";
        break;
      }
    }

    printf("- %s: %s\n", member_name, type_name);
  }
}

void debug_print_scope(Scope *scope, int indent_level) {
  // Print current scope header with metadata
  for (int i = 0; i < indent_level; i++)
    printf("  ");
  printf("Scope '%s' (depth %zu, %zu symbols, %zu children, %zu imports):\n",
         scope->scope_name, scope->depth, scope->symbols.count,
         scope->children.count, scope->imported_modules.count);

  // Print imported modules if any
  if (scope->imported_modules.count > 0) {
    for (int i = 0; i < indent_level + 1; i++)
      printf("  ");
    printf("Imported modules:\n");

    for (size_t i = 0; i < scope->imported_modules.count; i++) {
      ModuleImport *import =
          (ModuleImport *)((char *)scope->imported_modules.data +
                           i * sizeof(ModuleImport));

      for (int j = 0; j < indent_level + 2; j++)
        printf("  ");
      printf("- %s as %s (scope: %s)\n", import->module_name, import->alias,
             import->module_scope ? import->module_scope->scope_name : "NULL");
    }
  }

  // Print all symbols in the current scope
  for (size_t i = 0; i < scope->symbols.count; i++) {
    // Calculate symbol pointer using array base + offset
    Symbol *s = (Symbol *)((char *)scope->symbols.data + i * sizeof(Symbol));

    // Indent for symbol display
    for (int j = 0; j < indent_level + 1; j++)
      printf("  ");

    // Print symbol information with type name (simplified for basic types)
    printf("- %s: %s (public: %d, mutable: %d)\n", s->name,
           s->type && s->type->category == Node_Category_TYPE &&
                   s->type->type == AST_TYPE_BASIC
               ? s->type->type_data.basic.name
               : "complex_type",
           s->is_public, s->is_mutable);
  }

  // Recursively print all child scopes if any exist
  if (scope->children.count > 0) {
    for (int i = 0; i < indent_level + 1; i++)
      printf("  ");
    printf("Child scopes:\n");

    for (size_t i = 0; i < scope->children.count; i++) {
      // Calculate child scope pointer using array base + offset
      Scope **child_ptr =
          (Scope **)((char *)scope->children.data + i * sizeof(Scope *));
      Scope *child = *child_ptr;

      // Recursive call with increased indentation
      debug_print_scope(child, indent_level + 2);
    }
  }
}
