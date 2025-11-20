#include "type.h"

bool validate_array_type(AstNode *array_type, Scope *scope,
                         ArenaAllocator *arena) {
  if (!array_type || array_type->type != AST_TYPE_ARRAY) {
    return false;
  }

  AstNode *element_type = array_type->type_data.array.element_type;
  AstNode *size_expr = array_type->type_data.array.size;

  // Validate element type exists and is valid
  if (!element_type || element_type->category != Node_Category_TYPE) {
    tc_error(array_type, "Array Type Error", "Array has invalid element type");
    return false;
  }

  // Validate size expression if present
  if (size_expr) {
    AstNode *size_type = typecheck_expression(size_expr, scope, arena);
    if (!size_type) {
      tc_error(array_type, "Array Size Error",
               "Failed to determine type of array size expression");
      return false;
    }

    if (!is_numeric_type(size_type)) {
      tc_error_help(array_type, "Array Size Type Error",
                    "Array size must be a numeric type (typically int)",
                    "Array size has type '%s', expected numeric type",
                    type_to_string(size_type, arena));
      return false;
    }

    // Additional validation for constant integer sizes
    if (size_expr->type == AST_EXPR_LITERAL &&
        size_expr->expr.literal.lit_type == LITERAL_INT) {
      long long size_val = size_expr->expr.literal.value.int_val;

      if (size_val < 0) {
        tc_error_help(array_type, "Array Size Error",
                      "Array size must be non-negative",
                      "Array size %lld is negative", size_val);
        return false;
      }

      if (size_val == 0) {
        tc_error_help(array_type, "Array Size Warning",
                      "Zero-sized arrays are not recommended",
                      "Array declared with size 0");
        // Don't return false - this might be intentional
      }

      // Warn about very large arrays (arbitrary limit)
      if (size_val > 1000000) {
        tc_error_help(array_type, "Array Size Warning",
                      "Very large arrays may cause memory issues",
                      "Array size %lld is very large", size_val);
      }
    }
  }

  return true;
}

// Enhanced array bounds checking for constant indices
bool check_array_bounds(AstNode *array_type, AstNode *index_expr,
                        ArenaAllocator *arena) {
  (void)arena;
  if (!array_type || array_type->type != AST_TYPE_ARRAY || !index_expr) {
    return true; // Can't check - assume valid
  }

  AstNode *size_expr = array_type->type_data.array.size;

  // Only check if both array size and index are constant integers
  if (size_expr && size_expr->type == AST_EXPR_LITERAL &&
      size_expr->expr.literal.lit_type == LITERAL_INT &&
      index_expr->type == AST_EXPR_LITERAL &&
      index_expr->expr.literal.lit_type == LITERAL_INT) {

    long long array_size = size_expr->expr.literal.value.int_val;
    long long index_val = index_expr->expr.literal.value.int_val;

    if (index_val < 0) {
      tc_error_help(index_expr, "Array Bounds Error",
                    "Array indices must be non-negative",
                    "Index value %lld is negative", index_val);
      return false;
    }

    if (index_val >= array_size) {
      tc_error_help(index_expr, "Array Bounds Error",
                    "Array index out of bounds",
                    "Index %lld is out of bounds for array of size %lld",
                    index_val, array_size);
      return false;
    }
  }

  return true;
}

// Multi-dimensional array type checking
AstNode *typecheck_multidim_array_access(AstNode *base_type, AstNode **indices,
                                         size_t index_count,
                                         ArenaAllocator *arena) {
  AstNode *current_type = base_type;

  for (size_t i = 0; i < index_count; i++) {
    if (!current_type) {
      tc_error(indices[i], "Array Access Error",
               "Cannot index beyond array dimensions");
      return NULL;
    }

    if (current_type->type == AST_TYPE_ARRAY) {
      // Check bounds if possible
      if (!check_array_bounds(current_type, indices[i], arena)) {
        return NULL;
      }

      // Move to element type
      current_type = current_type->type_data.array.element_type;

    } else if (current_type->type == AST_TYPE_POINTER) {
      // Pointer arithmetic - move to pointee type
      current_type = current_type->type_data.pointer.pointee_type;

    } else {
      tc_error_help(indices[i], "Array Access Error",
                    "Only arrays and pointers can be indexed",
                    "Cannot index expression of type '%s' at dimension %zu",
                    type_to_string(current_type, arena), i + 1);
      return NULL;
    }
  }

  return current_type;
}

// Array type compatibility checking (for assignments, comparisons, etc.)
TypeMatchResult check_array_compatibility(AstNode *type1, AstNode *type2) {
  if (!type1 || !type2 || type1->type != AST_TYPE_ARRAY ||
      type2->type != AST_TYPE_ARRAY) {
    return TYPE_MATCH_NONE;
  }

  // Check element types first
  TypeMatchResult element_match = types_match(
      type1->type_data.array.element_type, type2->type_data.array.element_type);

  if (element_match == TYPE_MATCH_NONE) {
    return TYPE_MATCH_NONE;
  }

  // Check array sizes
  AstNode *size1 = type1->type_data.array.size;
  AstNode *size2 = type2->type_data.array.size;

  // If both have explicit sizes, they must match exactly
  if (size1 && size2) {
    if (size1->type == AST_EXPR_LITERAL && size2->type == AST_EXPR_LITERAL &&
        size1->expr.literal.lit_type == LITERAL_INT &&
        size2->expr.literal.lit_type == LITERAL_INT) {

      long long val1 = size1->expr.literal.value.int_val;
      long long val2 = size2->expr.literal.value.int_val;

      if (val1 != val2) {
        return TYPE_MATCH_NONE; // Same element type but different sizes
      }
    } else if (size1 != size2) {
      // Different size expressions - assume incompatible
      return TYPE_MATCH_NONE;
    }
  }

  // Arrays with unspecified sizes are compatible if element types match
  return element_match;
}

// Add array type validation to existing variable declaration checking
bool validate_array_initializer(AstNode *declared_type, AstNode *initializer,
                                Scope *scope, ArenaAllocator *arena) {
  if (!declared_type || declared_type->type != AST_TYPE_ARRAY || !initializer) {
    return true; // Not an array or no initializer
  }

  if (initializer->type != AST_EXPR_ARRAY) {
    // Non-array initializer for array variable
    tc_error_help(initializer, "Array Initialization Error",
                  "Array variables must be initialized with array literals",
                  "Cannot initialize array with non-array expression");
    return false;
  }

  // Check that array literal type matches declared array type
  AstNode *init_type = typecheck_array_expr(initializer, scope, arena);
  if (!init_type) {
    return false;
  }

  TypeMatchResult match = check_array_compatibility(declared_type, init_type);
  if (match == TYPE_MATCH_NONE) {
    tc_error_help(initializer, "Array Type Mismatch",
                  "Array initializer type must match declared array type",
                  "Declared type '%s', initializer type '%s'",
                  type_to_string(declared_type, arena),
                  type_to_string(init_type, arena));
    return false;
  }

  return true;
}
