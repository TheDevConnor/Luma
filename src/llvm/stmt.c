#include "llvm.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdlib.h>

// Legacy program handler (now redirects to multi-module handler)
LLVMValueRef codegen_stmt_program(CodeGenContext *ctx, AstNode *node) {
  return codegen_stmt_program_multi_module(ctx, node);
}

LLVMValueRef codegen_stmt_expression(CodeGenContext *ctx, AstNode *node) {
  return codegen_expr(ctx, node->stmt.expr_stmt.expression);
}

LLVMValueRef codegen_stmt_var_decl(CodeGenContext *ctx, AstNode *node) {
  LLVMTypeRef var_type = codegen_type(ctx, node->stmt.var_decl.var_type);
  if (!var_type)
    return NULL;

  LLVMValueRef var_ref;

  // Use current module instead of legacy ctx->module
  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;

  if (ctx->current_function == NULL) {
    var_ref =
        LLVMAddGlobal(current_llvm_module, var_type, node->stmt.var_decl.name);

    // Set linkage based on whether this is a public declaration
    if (node->stmt.var_decl.is_public) {
      LLVMSetLinkage(var_ref, LLVMExternalLinkage);
    } else {
      LLVMSetLinkage(var_ref, LLVMInternalLinkage);
    }
  } else {
    var_ref = LLVMBuildAlloca(ctx->builder, var_type, node->stmt.var_decl.name);
  }

  if (node->stmt.var_decl.initializer) {
    LLVMValueRef init_val = codegen_expr(ctx, node->stmt.var_decl.initializer);
    if (init_val && ctx->current_function == NULL) {
      LLVMSetInitializer(var_ref, init_val);
    } else if (!init_val && ctx->current_function == NULL) {
      LLVMSetInitializer(var_ref, LLVMConstNull(var_type));
    } else {
      LLVMBuildStore(ctx->builder, init_val, var_ref);
    }
  }

  add_symbol(ctx, node->stmt.var_decl.name, var_ref, var_type, false);
  return var_ref;
}

LLVMValueRef codegen_stmt_function(CodeGenContext *ctx, AstNode *node) {
  LLVMTypeRef *param_types = (LLVMTypeRef *)arena_alloc(
      ctx->arena, sizeof(LLVMTypeRef) * node->stmt.func_decl.param_count,
      alignof(LLVMTypeRef));

  for (size_t i = 0; i < node->stmt.func_decl.param_count; i++) {
    param_types[i] = codegen_type(ctx, node->stmt.func_decl.param_types[i]);
    if (!param_types[i])
      return NULL;
  }

  LLVMTypeRef return_type = codegen_type(ctx, node->stmt.func_decl.return_type);
  if (!return_type)
    return NULL;

  LLVMTypeRef func_type = LLVMFunctionType(
      return_type, param_types, node->stmt.func_decl.param_count, false);

  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;

  LLVMValueRef function = LLVMAddFunction(current_llvm_module,
                                          node->stmt.func_decl.name, func_type);

  LLVMSetLinkage(function, get_function_linkage(node));
  add_symbol(ctx, node->stmt.func_decl.name, function, func_type, true);

  // Set parameter names
  for (size_t i = 0; i < node->stmt.func_decl.param_count; i++) {
    LLVMValueRef param = LLVMGetParam(function, i);
    LLVMSetValueName2(param, node->stmt.func_decl.param_names[i],
                      strlen(node->stmt.func_decl.param_names[i]));
  }

  // Create entry block
  LLVMBasicBlockRef entry_block =
      LLVMAppendBasicBlockInContext(ctx->context, function, "entry");
  LLVMPositionBuilderAtEnd(ctx->builder, entry_block);

  // Save old function context
  LLVMValueRef old_function = ctx->current_function;
  DeferredStatement *old_deferred = ctx->deferred_statements;
  size_t old_defer_count = ctx->deferred_count;

  // Set new function context
  ctx->current_function = function;
  init_defer_stack(ctx);

  // Add parameters to symbol table as allocas
  for (size_t i = 0; i < node->stmt.func_decl.param_count; i++) {
    LLVMValueRef param = LLVMGetParam(function, i);
    LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, param_types[i],
                                          node->stmt.func_decl.param_names[i]);
    LLVMBuildStore(ctx->builder, param, alloca);
    add_symbol(ctx, node->stmt.func_decl.param_names[i], alloca, param_types[i],
               false);
  }

  // Create blocks for normal return and cleanup
  LLVMBasicBlockRef normal_return =
      LLVMAppendBasicBlockInContext(ctx->context, function, "normal_return");
  LLVMBasicBlockRef cleanup_entry =
      LLVMAppendBasicBlockInContext(ctx->context, function, "cleanup_entry");

  // Generate function body
  codegen_stmt(ctx, node->stmt.func_decl.body);

  // If we reach the end without an explicit return, branch to cleanup
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
    LLVMBuildBr(ctx->builder, cleanup_entry);
  }

  // Generate cleanup blocks for deferred statements
  generate_cleanup_blocks(ctx);

  // Set up cleanup entry point
  LLVMPositionBuilderAtEnd(ctx->builder, cleanup_entry);

  if (ctx->deferred_statements) {
    // Branch to the first cleanup block
    LLVMBuildBr(ctx->builder, ctx->deferred_statements->cleanup_block);
  } else {
    // No deferred statements, go straight to normal return
    LLVMBuildBr(ctx->builder, normal_return);
  }

  // Set up normal return block
  LLVMPositionBuilderAtEnd(ctx->builder, normal_return);

  if (LLVMGetTypeKind(return_type) == LLVMVoidTypeKind) {
    LLVMBuildRetVoid(ctx->builder);
  } else {
    // Return default value for the type
    LLVMValueRef default_val = LLVMConstNull(return_type);
    LLVMBuildRet(ctx->builder, default_val);
  }

  // Restore old function context
  ctx->current_function = old_function;
  ctx->deferred_statements = old_deferred;
  ctx->deferred_count = old_defer_count;

  return function;
}

LLVMValueRef codegen_stmt_return(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef ret_val = NULL;

  if (node->stmt.return_stmt.value) {
    ret_val = codegen_expr(ctx, node->stmt.return_stmt.value);
    if (!ret_val)
      return NULL;
  }

  // If we have deferred statements at function level, we need to branch to
  // cleanup blocks But first, execute any local scope deferred statements
  // inline
  if (ctx->deferred_statements) {
    LLVMValueRef return_val_storage = NULL;

    if (ret_val) {
      // Allocate storage for return value
      LLVMTypeRef ret_type = LLVMTypeOf(ret_val);
      return_val_storage =
          LLVMBuildAlloca(ctx->builder, ret_type, "return_val_storage");
      LLVMBuildStore(ctx->builder, ret_val, return_val_storage);
    }

    // Execute deferred statements inline (these will be from local scopes)
    execute_deferred_statements_inline(ctx, ctx->deferred_statements);

    // Return the stored value or void
    if (return_val_storage) {
      LLVMValueRef final_ret_val =
          LLVMBuildLoad2(ctx->builder, LLVMTypeOf(ret_val), return_val_storage,
                         "final_return_val");
      return LLVMBuildRet(ctx->builder, final_ret_val);
    } else {
      return LLVMBuildRetVoid(ctx->builder);
    }
  } else {
    // No deferred statements, return normally
    if (ret_val) {
      return LLVMBuildRet(ctx->builder, ret_val);
    } else {
      return LLVMBuildRetVoid(ctx->builder);
    }
  }
}

LLVMValueRef codegen_stmt_block(CodeGenContext *ctx, AstNode *node) {
  // Save the current defer context
  DeferredStatement *saved_defers = ctx->deferred_statements;
  size_t saved_count = ctx->deferred_count;

  // Create new defer scope for this block
  ctx->deferred_statements = NULL;
  ctx->deferred_count = 0;

  // Process all statements in the block
  for (size_t i = 0; i < node->stmt.block.stmt_count; i++) {
    // Stop processing if we hit a terminator
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      break;
    }
    codegen_stmt(ctx, node->stmt.block.statements[i]);
  }

  // Execute any deferred statements from this block scope (in reverse order)
  if (ctx->deferred_statements) {
    execute_deferred_statements_inline(ctx, ctx->deferred_statements);
  }

  // Restore the previous defer context
  ctx->deferred_statements = saved_defers;
  ctx->deferred_count = saved_count;

  return NULL;
}

LLVMValueRef codegen_stmt_if(CodeGenContext *ctx, AstNode *node) {
  LLVMValueRef condition = codegen_expr(ctx, node->stmt.if_stmt.condition);
  if (!condition)
    return NULL;

  LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "then");
  LLVMBasicBlockRef else_block =
      node->stmt.if_stmt.else_stmt
          ? LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function,
                                          "else")
          : NULL;
  LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "merge");

  LLVMBuildCondBr(ctx->builder, condition, then_block,
                  else_block ? else_block : merge_block);

  // Generate then block
  LLVMPositionBuilderAtEnd(ctx->builder, then_block);
  codegen_stmt(ctx, node->stmt.if_stmt.then_stmt);

  // Only add branch if block isn't already terminated
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
    LLVMBuildBr(ctx->builder, merge_block);
  }

  // Generate else block if it exists
  if (else_block) {
    LLVMPositionBuilderAtEnd(ctx->builder, else_block);
    codegen_stmt(ctx, node->stmt.if_stmt.else_stmt);

    // Only add branch if block isn't already terminated
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildBr(ctx->builder, merge_block);
    }
  }

  // Continue with merge block
  LLVMPositionBuilderAtEnd(ctx->builder, merge_block);
  return NULL;
}

LLVMValueRef codegen_stmt_print(CodeGenContext *ctx, AstNode *node) {
  // Use current module instead of legacy ctx->module
  LLVMModuleRef current_llvm_module =
      ctx->current_module ? ctx->current_module->module : ctx->module;

  // Declare printf once (check if it already exists first)
  LLVMValueRef printf_func =
      LLVMGetNamedFunction(current_llvm_module, "printf");
  LLVMTypeRef printf_type = NULL;

  if (!printf_func) {
    LLVMTypeRef printf_arg_types[] = {
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0)};
    printf_type = LLVMFunctionType(LLVMInt32TypeInContext(ctx->context),
                                   printf_arg_types, 1, true);
    printf_func = LLVMAddFunction(current_llvm_module, "printf", printf_type);
    add_symbol(ctx, "printf", printf_func, printf_type, true);
  } else {
    printf_type = LLVMGlobalGetValueType(printf_func);
  }

  for (size_t i = 0; i < node->stmt.print_stmt.expr_count; i++) {
    AstNode *expr = node->stmt.print_stmt.expressions[i];
    LLVMValueRef value = NULL;
    const char *format_str = NULL;

    if (expr->type == AST_EXPR_LITERAL &&
        expr->expr.literal.lit_type == LITERAL_STRING) {

      // Process escape sequences in the string literal
      char *processed_str =
          process_escape_sequences(expr->expr.literal.value.string_val);

      // Create the string value directly (not using %s format)
      value = LLVMBuildGlobalStringPtr(ctx->builder, processed_str, "str");
      format_str = "%s";

      free(processed_str);
    } else {
      // Handle non-string expressions as before
      value = codegen_expr(ctx, expr);
      if (!value)
        return NULL;

      LLVMTypeRef value_type = LLVMTypeOf(value);
      if (LLVMGetTypeKind(value_type) == LLVMIntegerTypeKind) {
        unsigned int bits = LLVMGetIntTypeWidth(value_type);
        if (bits == 8 || bits == 16 || bits == 32) {
          format_str = "%d";
        } else if (bits == 64) {
          format_str = "%lld";
        } else {
          format_str = "%d";
        }
      } else if (LLVMGetTypeKind(value_type) == LLVMDoubleTypeKind) {
        format_str = "%f";
      } else {
        format_str = "%p";
      }
    }

    // Create format string and call printf
    LLVMValueRef format_str_val =
        LLVMBuildGlobalStringPtr(ctx->builder, format_str, "fmt");
    LLVMValueRef args[] = {format_str_val, value};
    LLVMBuildCall2(ctx->builder, printf_type, printf_func, args, 2, "");
  }

  return LLVMConstNull(LLVMVoidTypeInContext(ctx->context));
}

LLVMValueRef codegen_stmt_defer(CodeGenContext *ctx, AstNode *node) {
  // Push the deferred statement onto the context's deferred stack
  push_defer_statement(ctx, node->stmt.defer_stmt.statement);
  return NULL;
}

LLVMValueRef codegen_stmt_break_continue(CodeGenContext *ctx, AstNode *node) {
  if (node->stmt.break_continue.is_continue) {
    if (ctx->loop_continue_block) {
      LLVMBuildBr(ctx->builder, ctx->loop_continue_block);
    } else {
      fprintf(stderr, "Error: 'continue' used outside of a loop\n");
    }
  } else {
    if (ctx->loop_break_block) {
      LLVMBuildBr(ctx->builder, ctx->loop_break_block);
    } else {
      fprintf(stderr, "Error: 'break' used outside of a loop\n");
    }
  }
  return NULL;
}

// loop { ... }
LLVMValueRef codegen_infinite_loop(CodeGenContext *ctx, AstNode *node) {
  LLVMBasicBlockRef loop_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "infinite_loop");
  LLVMBasicBlockRef after_loop_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "after_infinite_loop");

  // Branch to loop block
  LLVMBuildBr(ctx->builder, loop_block);

  // Generate loop block
  LLVMPositionBuilderAtEnd(ctx->builder, loop_block);

  // Save old loop context
  LLVMBasicBlockRef old_continue = ctx->loop_continue_block;
  LLVMBasicBlockRef old_break = ctx->loop_break_block;

  // Set new loop context
  ctx->loop_continue_block = loop_block;
  ctx->loop_break_block = after_loop_block;

  // Generate loop body
  codegen_stmt(ctx, node->stmt.loop_stmt.body);

  // If we reach the end of the loop body without a terminator, branch back to
  // the start
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
    LLVMBuildBr(ctx->builder, loop_block);
  }

  // Restore old loop context
  ctx->loop_continue_block = old_continue;
  ctx->loop_break_block = old_break;

  // Continue with after loop block
  LLVMPositionBuilderAtEnd(ctx->builder, after_loop_block);
  return NULL;
}

// loop (i < 10) { ... }
// loop (i < 10) : (i++) { ... }
LLVMValueRef codegen_while_loop(CodeGenContext *ctx, AstNode *node) {
  // Create basic blocks for the loop structure
  LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "while_cond");
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "while_body");
  LLVMBasicBlockRef after_block = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "while_end");

  // Jump to condition check
  LLVMBuildBr(ctx->builder, cond_block);

  // Generate condition block
  LLVMPositionBuilderAtEnd(ctx->builder, cond_block);
  if (node->stmt.loop_stmt.condition) {
    LLVMValueRef cond = codegen_expr(ctx, node->stmt.loop_stmt.condition);
    if (!cond) {
      fprintf(stderr, "Error: Failed to generate condition for while loop at line %zu\n",
              node->line);
      return NULL;
    }
    LLVMBuildCondBr(ctx->builder, cond, body_block, after_block);
  } else {
    // Infinite loop if no condition
    LLVMBuildBr(ctx->builder, body_block);
  }

  // Generate loop body
  LLVMPositionBuilderAtEnd(ctx->builder, body_block);
  codegen_stmt(ctx, node->stmt.loop_stmt.body);

  // Generate increment expressions if they exist
  if (node->stmt.loop_stmt.optional) {
      codegen_expr(ctx, node->stmt.loop_stmt.optional);
  }

  // If body doesn't have a terminator, jump back to condition check
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildBr(ctx->builder, cond_block);  // Jump back to condition, not after_block
  }

  // Continue with after loop block
  LLVMPositionBuilderAtEnd(ctx->builder, after_block);
  return NULL;
}

// loop [i: int = 0](i < 10) { ... }
// loop [i: int = 0](i < 10) : (i++) { ... }
// loop [i: int = 0, j: int = 0](i < 10 && j < 20) { ... }
LLVMValueRef codegen_for_loop(CodeGenContext *ctx, AstNode *node) {
    // Create basic blocks for the loop structure
    LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "for_cond");
    LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "for_body");
    LLVMBasicBlockRef increment_block = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "for_inc");
    LLVMBasicBlockRef after_block = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "for_end");

    // Store old loop blocks for nested loop support
    LLVMBasicBlockRef old_continue = ctx->loop_continue_block;
    LLVMBasicBlockRef old_break = ctx->loop_break_block;
    ctx->loop_continue_block = increment_block;
    ctx->loop_break_block = after_block;

    // Generate initializers in current block
    for (int i = 0; i < node->stmt.loop_stmt.init_count; i++) {
        if (!codegen_stmt(ctx, node->stmt.loop_stmt.initializer[i])) {
            fprintf(stderr, "Error: Failed to generate initializer for for loop at line %zu\n",
                    node->line);
            // Restore old blocks
            ctx->loop_continue_block = old_continue;
            ctx->loop_break_block = old_break;
            return NULL;
        }
    }

    // Jump to condition check
    LLVMBuildBr(ctx->builder, cond_block);

    // Generate condition block
    LLVMPositionBuilderAtEnd(ctx->builder, cond_block);
    if (node->stmt.loop_stmt.condition) {
        LLVMValueRef cond = codegen_expr(ctx, node->stmt.loop_stmt.condition);
        if (!cond) {
            // Restore old blocks
            ctx->loop_continue_block = old_continue;
            ctx->loop_break_block = old_break;
            return NULL;
        }
        LLVMBuildCondBr(ctx->builder, cond, body_block, after_block);
    } else {
        // Infinite loop if no condition
        LLVMBuildBr(ctx->builder, body_block);
    }

    // Generate loop body
    LLVMPositionBuilderAtEnd(ctx->builder, body_block);
    codegen_stmt(ctx, node->stmt.loop_stmt.body);

    // If body doesn't have a terminator, jump to increment
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
        LLVMBuildBr(ctx->builder, increment_block);
    }

    // Generate increment block
    LLVMPositionBuilderAtEnd(ctx->builder, increment_block);
    
    // Generate increment expressions if they exist
    if (node->stmt.loop_stmt.optional) {
        codegen_expr(ctx, node->stmt.loop_stmt.optional);
    }
    
    // Jump back to condition check
    LLVMBuildBr(ctx->builder, cond_block);

    // Restore old loop blocks
    ctx->loop_continue_block = old_continue;
    ctx->loop_break_block = old_break;

    // Continue with after loop block
    LLVMPositionBuilderAtEnd(ctx->builder, after_block);
    return NULL;
}

LLVMValueRef codegen_loop(CodeGenContext *ctx, AstNode *node) {
  if (node->stmt.loop_stmt.condition == NULL && node->stmt.loop_stmt.initializer == NULL)
    return codegen_infinite_loop(ctx, node);
  else if (node->stmt.loop_stmt.condition != NULL && node->stmt.loop_stmt.initializer == NULL)
    return codegen_while_loop(ctx, node);
  else
    return codegen_for_loop(ctx, node);
}
