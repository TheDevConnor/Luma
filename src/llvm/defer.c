#include "llvm.h"

void init_defer_stack(CodeGenContext *ctx) {
  ctx->deferred_statements = NULL;
  ctx->deferred_count = 0;
  ctx->deferred_capacity = 0;
}

void push_defer_statement(CodeGenContext *ctx, AstNode *statement) {
  DeferredStatement *defer_stmt = (DeferredStatement *)arena_alloc(
      ctx->arena, sizeof(DeferredStatement), alignof(DeferredStatement));

  defer_stmt->statement = statement;
  defer_stmt->cleanup_block = NULL;
  defer_stmt->next = ctx->deferred_statements;
  ctx->deferred_statements = defer_stmt;
  ctx->deferred_count++;
}

// Execute deferred statements inline (for nested contexts)
void execute_deferred_statements_inline(CodeGenContext *ctx,
                                        DeferredStatement *defers) {
  if (!defers)
    return;

  // Execute in reverse order (LIFO)
  DeferredStatement *current = defers;
  DeferredStatement *reversed_list = NULL;

  // Reverse the list to execute in LIFO order
  while (current) {
    DeferredStatement *next = current->next;
    current->next = reversed_list;
    reversed_list = current;
    current = next;
  }

  // Execute all deferred statements
  current = reversed_list;
  while (current) {
    // Save the current defer context
    DeferredStatement *saved_defers = ctx->deferred_statements;
    size_t saved_count = ctx->deferred_count;

    // Create new defer context for this statement
    ctx->deferred_statements = NULL;
    ctx->deferred_count = 0;

    // Execute the deferred statement
    codegen_stmt(ctx, current->statement);

    // Execute any nested deferred statements that were created
    if (ctx->deferred_statements) {
      execute_deferred_statements_inline(ctx, ctx->deferred_statements);
    }

    // Restore the defer context
    ctx->deferred_statements = saved_defers;
    ctx->deferred_count = saved_count;

    current = current->next;
  }
}

void generate_cleanup_blocks(CodeGenContext *ctx) {
  if (!ctx->deferred_statements || !ctx->current_function) {
    return;
  }

  // Create a "normal_return" block that the last cleanup will branch to
  LLVMBasicBlockRef normal_return = LLVMAppendBasicBlockInContext(
      ctx->context, ctx->current_function, "normal_return");

  // Create cleanup blocks for each deferred statement (in reverse order)
  DeferredStatement *current = ctx->deferred_statements;
  LLVMBasicBlockRef next_cleanup = normal_return; // Start with normal return

  while (current) {
    // Create a cleanup block for this deferred statement
    char block_name[64];
    snprintf(block_name, sizeof(block_name), "defer_cleanup_%p",
             (void *)current);

    current->cleanup_block = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, block_name);

    // Set up the block
    LLVMBasicBlockRef old_block = LLVMGetInsertBlock(ctx->builder);
    LLVMPositionBuilderAtEnd(ctx->builder, current->cleanup_block);

    // Save the current defer context before executing
    DeferredStatement *saved_defers = ctx->deferred_statements;
    size_t saved_count = ctx->deferred_count;

    // Create new defer context for this cleanup block
    ctx->deferred_statements = NULL;
    ctx->deferred_count = 0;

    // Generate the deferred statement
    codegen_stmt(ctx, current->statement);

    // Execute any nested deferred statements that were created
    if (ctx->deferred_statements) {
      execute_deferred_statements_inline(ctx, ctx->deferred_statements);
    }

    // Restore the defer context
    ctx->deferred_statements = saved_defers;
    ctx->deferred_count = saved_count;

    // Always branch to next cleanup block (or normal return)
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
      LLVMBuildBr(ctx->builder, next_cleanup);
    }

    // Restore builder position
    LLVMPositionBuilderAtEnd(ctx->builder, old_block);

    next_cleanup = current->cleanup_block;
    current = current->next;
  }

  // Set up the normal return block
  LLVMPositionBuilderAtEnd(ctx->builder, normal_return);

  // Get the function's return type to determine what to return
  LLVMTypeRef func_type = LLVMGlobalGetValueType(ctx->current_function);
  LLVMTypeRef return_type = LLVMGetReturnType(func_type);

  if (LLVMGetTypeKind(return_type) == LLVMVoidTypeKind) {
    LLVMBuildRetVoid(ctx->builder);
  } else {
    // Return default value for the type
    LLVMValueRef default_val = LLVMConstNull(return_type);
    LLVMBuildRet(ctx->builder, default_val);
  }
}

void clear_defer_stack(CodeGenContext *ctx) {
  ctx->deferred_statements = NULL;
  ctx->deferred_count = 0;
}
