// struct_helpers.c - Additional utility functions for struct support
#include "llvm.h"

// =============================================================================
// STRUCT TYPE SYSTEM INTEGRATION
// =============================================================================

// Check if an LLVM type is a struct type we've defined
bool is_struct_type(CodeGenContext *ctx, LLVMTypeRef type) {
    if (LLVMGetTypeKind(type) != LLVMStructTypeKind) {
        return false;
    }
    
    // Search through our registered struct types
    for (StructInfo *info = ctx->struct_types; info; info = info->next) {
        if (info->llvm_type == type) {
            return true;
        }
    }
    return false;
}

// Get struct name from LLVM type
const char *get_struct_name_from_type(CodeGenContext *ctx, LLVMTypeRef type) {
    for (StructInfo *info = ctx->struct_types; info; info = info->next) {
        if (info->llvm_type == type) {
            return info->name;
        }
    }
    return NULL;
}

// =============================================================================
// STRUCT CONSTRUCTION HELPERS
// =============================================================================

// Create a zero-initialized struct
LLVMValueRef create_struct_zero_initializer(CodeGenContext *ctx, const char *struct_name) {
    StructInfo *struct_info = find_struct_type(ctx, struct_name);
    if (!struct_info) {
        fprintf(stderr, "Error: Struct type '%s' not found for zero initialization\n", struct_name);
        return NULL;
    }
    
    return LLVMConstNull(struct_info->llvm_type);
}

// Create a copy of a struct
LLVMValueRef create_struct_copy(CodeGenContext *ctx, LLVMValueRef src_struct, 
                               StructInfo *struct_info) {
    if (!struct_info) {
        fprintf(stderr, "Error: No struct info provided for copy\n");
        return NULL;
    }
    
    // Allocate space for the new struct
    LLVMValueRef dest_alloca = LLVMBuildAlloca(ctx->builder, struct_info->llvm_type, "struct_copy");
    
    // If source is a constant, we can use it directly
    if (LLVMIsConstant(src_struct)) {
        LLVMBuildStore(ctx->builder, src_struct, dest_alloca);
        return LLVMBuildLoad2(ctx->builder, struct_info->llvm_type, dest_alloca, "copied_struct");
    }
    
    // Otherwise, copy field by field
    LLVMValueRef src_alloca = LLVMBuildAlloca(ctx->builder, struct_info->llvm_type, "src_temp");
    LLVMBuildStore(ctx->builder, src_struct, src_alloca);
    
    for (size_t i = 0; i < struct_info->field_count; i++) {
        // Get source field
        LLVMValueRef src_field_ptr = LLVMBuildStructGEP2(
            ctx->builder, struct_info->llvm_type, src_alloca, i, "src_field_ptr");
        LLVMValueRef field_value = LLVMBuildLoad2(
            ctx->builder, struct_info->field_types[i], src_field_ptr, "field_value");
        
        // Store in destination
        LLVMValueRef dest_field_ptr = LLVMBuildStructGEP2(
            ctx->builder, struct_info->llvm_type, dest_alloca, i, "dest_field_ptr");
        LLVMBuildStore(ctx->builder, field_value, dest_field_ptr);
    }
    
    return LLVMBuildLoad2(ctx->builder, struct_info->llvm_type, dest_alloca, "copied_struct");
}

// =============================================================================
// STRUCT DEBUGGING AND INTROSPECTION
// =============================================================================

// Print information about a struct type
void print_struct_info(CodeGenContext *ctx, const char *struct_name) {
    StructInfo *struct_info = find_struct_type(ctx, struct_name);
    if (!struct_info) {
        printf("Struct '%s' not found.\n", struct_name);
        return;
    }
    
    printf("=== Struct Info: %s ===\n", struct_name);
    printf("Visibility: %s\n", struct_info->is_public ? "public" : "private");
    printf("Field count: %zu\n", struct_info->field_count);
    printf("Fields:\n");
    
    for (size_t i = 0; i < struct_info->field_count; i++) {
        const char *visibility = struct_info->field_is_public[i] ? "public" : "private";
        
        // Get type name (simplified - you might want more detailed type info)
        LLVMTypeKind kind = LLVMGetTypeKind(struct_info->field_types[i]);
        const char *type_name = "unknown";
        
        switch (kind) {
            case LLVMIntegerTypeKind: {
                unsigned width = LLVMGetIntTypeWidth(struct_info->field_types[i]);
                static char int_type[32];
                snprintf(int_type, sizeof(int_type), "i%u", width);
                type_name = int_type;
                break;
            }
            case LLVMFloatTypeKind:
                type_name = "float";
                break;
            case LLVMDoubleTypeKind:
                type_name = "double";
                break;
            case LLVMPointerTypeKind:
                type_name = "pointer";
                break;
            case LLVMStructTypeKind:
                type_name = "struct";
                break;
            default:
                type_name = "other";
                break;
        }
        
        printf("  [%zu] %s: %s (%s)\n", i, struct_info->field_names[i], type_name, visibility);
    }
    printf("========================\n");
}

// Debug struct layout (shows memory layout)
void debug_struct_layout(CodeGenContext *ctx, const char *struct_name) {
    StructInfo *struct_info = find_struct_type(ctx, struct_name);
    if (!struct_info) {
        printf("Struct '%s' not found for layout debug.\n", struct_name);
        return;
    }
    
    printf("=== Struct Layout Debug: %s ===\n", struct_name);
    
    // Get data layout for size calculations
    LLVMModuleRef current_module = ctx->current_module ? ctx->current_module->module : ctx->module;
    char *target_triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(current_module, target_triple);
    
    // This is simplified - in a real implementation you'd get the actual target data
    printf("Struct size: %llu bytes (estimated)\n", 
           (unsigned long long)LLVMStoreSizeOfType(NULL, struct_info->llvm_type));
    
    for (size_t i = 0; i < struct_info->field_count; i++) {
        // Get offset (simplified calculation)
        printf("  Field %zu (%s): offset ~%zu bytes\n", 
               i, struct_info->field_names[i], i * 8); // Rough estimate
    }
    
    LLVMDisposeMessage(target_triple);
    printf("==============================\n");
}

// =============================================================================
// ENHANCED STRUCT OPERATIONS
// =============================================================================

// Compare two structs for equality (field by field)
LLVMValueRef compare_structs_equal(CodeGenContext *ctx, LLVMValueRef struct1, 
                                  LLVMValueRef struct2, StructInfo *struct_info) {
    if (!struct_info) {
        fprintf(stderr, "Error: No struct info provided for comparison\n");
        return NULL;
    }
    
    // Start with true, AND with each field comparison
    LLVMValueRef result = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, false);
    
    // Allocate temporaries for struct access
    LLVMValueRef temp1 = LLVMBuildAlloca(ctx->builder, struct_info->llvm_type, "temp1");
    LLVMValueRef temp2 = LLVMBuildAlloca(ctx->builder, struct_info->llvm_type, "temp2");
    LLVMBuildStore(ctx->builder, struct1, temp1);
    LLVMBuildStore(ctx->builder, struct2, temp2);
    
    for (size_t i = 0; i < struct_info->field_count; i++) {
        // Get field values
        LLVMValueRef field1_ptr = LLVMBuildStructGEP2(
            ctx->builder, struct_info->llvm_type, temp1, i, "field1_ptr");
        LLVMValueRef field2_ptr = LLVMBuildStructGEP2(
            ctx->builder, struct_info->llvm_type, temp2, i, "field2_ptr");
        
        LLVMValueRef field1 = LLVMBuildLoad2(
            ctx->builder, struct_info->field_types[i], field1_ptr, "field1");
        LLVMValueRef field2 = LLVMBuildLoad2(
            ctx->builder, struct_info->field_types[i], field2_ptr, "field2");
        
        // Compare fields (this is simplified - you'd want type-specific comparison)
        LLVMValueRef field_equal;
        LLVMTypeKind kind = LLVMGetTypeKind(struct_info->field_types[i]);
        
        if (kind == LLVMIntegerTypeKind) {
            field_equal = LLVMBuildICmp(ctx->builder, LLVMIntEQ, field1, field2, "field_eq");
        } else if (kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind) {
            field_equal = LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, field1, field2, "field_eq");
        } else {
            // For other types, use pointer comparison or implement specific logic
            field_equal = LLVMBuildICmp(ctx->builder, LLVMIntEQ, field1, field2, "field_eq");
        }
        
        // AND with result
        result = LLVMBuildAnd(ctx->builder, result, field_equal, "and_result");
    }
    
    return result;
}

// Initialize struct with default values
LLVMValueRef initialize_struct_with_defaults(CodeGenContext *ctx, const char *struct_name) {
    StructInfo *struct_info = find_struct_type(ctx, struct_name);
    if (!struct_info) {
        fprintf(stderr, "Error: Struct type '%s' not found for default initialization\n", struct_name);
        return NULL;
    }
    
    // Allocate struct
    LLVMValueRef struct_alloca = LLVMBuildAlloca(ctx->builder, struct_info->llvm_type, "default_struct");
    
    // Initialize each field with appropriate default
    for (size_t i = 0; i < struct_info->field_count; i++) {
        LLVMValueRef default_value;
        LLVMTypeKind kind = LLVMGetTypeKind(struct_info->field_types[i]);
        
        switch (kind) {
            case LLVMIntegerTypeKind:
                default_value = LLVMConstInt(struct_info->field_types[i], 0, false);
                break;
            case LLVMFloatTypeKind:
                default_value = LLVMConstReal(struct_info->field_types[i], 0.0);
                break;
            case LLVMDoubleTypeKind:
                default_value = LLVMConstReal(struct_info->field_types[i], 0.0);
                break;
            case LLVMPointerTypeKind:
                default_value = LLVMConstNull(struct_info->field_types[i]);
                break;
            default:
                default_value = LLVMConstNull(struct_info->field_types[i]);
                break;
        }
        
        LLVMValueRef field_ptr = LLVMBuildStructGEP2(
            ctx->builder, struct_info->llvm_type, struct_alloca, i, "field_ptr");
        LLVMBuildStore(ctx->builder, default_value, field_ptr);
    }
    
    return LLVMBuildLoad2(ctx->builder, struct_info->llvm_type, struct_alloca, "initialized_struct");
}