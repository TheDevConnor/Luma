#include <stdio.h>
#include <string.h>

#include "../c_libs/color/color.h"
#include "ast.h"
#include "ast_utils.h"

static void indent(int level) {
  for (int i = 0; i < level; ++i)
    printf("  ");
}

const char *node_type_to_string(NodeType type) {
  switch (type) {
  case AST_PROGRAM:
    return "Program";
  case AST_PREPROCESSOR_MODULE:
    return "Module";
  case AST_PREPROCESSOR_USE:
    return "Use";
  case AST_EXPR_LITERAL:
    return "Literal";
  case AST_EXPR_IDENTIFIER:
    return "Identifier";
  case AST_EXPR_BINARY:
    return "Binary";
  case AST_EXPR_UNARY:
    return "Unary";
  case AST_EXPR_CALL:
    return "Call";
  case AST_EXPR_ASSIGNMENT:
    return "Assignment";
  case AST_EXPR_TERNARY:
    return "Ternary";
  case AST_EXPR_MEMBER:
    return "Member";
  case AST_EXPR_INDEX:
    return "Index";
  case AST_EXPR_GROUPING:
    return "Grouping";
  case AST_EXPR_ARRAY:
    return "Array";
  case AST_EXPR_ADDR:
    return "ADDR";
  case AST_EXPR_DEREF:
    return "DEREF";
  case AST_EXPR_CAST:
    return "CAST";
  case AST_EXPR_SIZEOF:
    return "SIZEOF";
  case AST_EXPR_ALLOC:
    return "ALLOC";
  case AST_EXPR_FREE:
    return "FREE";
  case AST_STMT_EXPRESSION:
    return "ExprStmt";
  case AST_STMT_VAR_DECL:
    return "VarDecl";
  case AST_STMT_CONST_DECL:
    return "ConstDecl";
  case AST_STMT_FUNCTION:
    return "Function";
  case AST_STMT_IF:
    return "If";
  case AST_STMT_LOOP:
    return "Loop";
  case AST_STMT_RETURN:
    return "Return";
  case AST_STMT_BREAK_CONTINUE:
    return "BreakContinue";
  case AST_STMT_BLOCK:
    return "Block";
  case AST_STMT_PRINT:
    return "Print";
  case AST_STMT_MODULE:
    return "Module";
  case AST_STMT_ENUM:
    return "Enum";
  case AST_STMT_STRUCT:
    return "Struct";
  case AST_STMT_DEFER:
    return "Defer";
  case AST_STMT_FIELD_DECL:
    return "FieldDecl";
  case AST_TYPE_BASIC:
    return "TypeBasic";
  case AST_TYPE_POINTER:
    return "TypePointer";
  case AST_TYPE_ARRAY:
    return "TypeArray";
  case AST_TYPE_FUNCTION:
    return "TypeFunction";
  case AST_TYPE_STRUCT:
    return "TypeStruct";
  case AST_TYPE_ENUM:
    return "TypeEnum";
  default:
    return "Unknown";
  }
}

const char *binop_to_string(BinaryOp op) {
  switch (op) {
  case BINOP_ADD:
    return "+";
  case BINOP_SUB:
    return "-";
  case BINOP_MUL:
    return "*";
  case BINOP_DIV:
    return "/";
  case BINOP_MOD:
    return "%";
  case BINOP_POW:
    return "**";
  case BINOP_EQ:
    return "==";
  case BINOP_NE:
    return "!=";
  case BINOP_LT:
    return "<";
  case BINOP_LE:
    return "<=";
  case BINOP_GT:
    return ">";
  case BINOP_GE:
    return ">=";
  case BINOP_AND:
    return "&&";
  case BINOP_OR:
    return "||";
  case BINOP_BIT_AND:
    return "&";
  case BINOP_BIT_OR:
    return "|";
  case BINOP_BIT_XOR:
    return "^";
  case BINOP_SHL:
    return "<<";
  case BINOP_SHR:
    return ">>";
  case BINOP_RANGE:
    return "..";
  default:
    return "??";
  }
}

const char *unop_to_string(UnaryOp op) {
  switch (op) {
  case UNOP_NOT:
    return "!";
  case UNOP_NEG:
    return "-";
  case UNOP_POS:
    return "+";
  case UNOP_BIT_NOT:
    return "~";
  case UNOP_PRE_INC:
    return "++";
  case UNOP_PRE_DEC:
    return "--";
  case UNOP_POST_INC:
    return "x++";
  case UNOP_POST_DEC:
    return "x--";
  case UNOP_DEREF:
    return "*";
  case UNOP_ADDR:
    return "&";
  default:
    return "??";
  }
}

const char *literal_type_to_string(LiteralType type) {
  switch (type) {
  case LITERAL_INT:
    return "int";
  case LITERAL_FLOAT:
    return "float";
  case LITERAL_STRING:
    return "string";
  case LITERAL_CHAR:
    return "char";
  case LITERAL_BOOL:
    return "bool";
  case LITERAL_NULL:
    return "null";
  default:
    return "unknown";
  }
}

void print_prefix(const char *prefix, bool is_last) {
  if (is_last) {
    printf("%s└── ", prefix);
  } else {
    printf("%s├── ", prefix);
  }
}

void print_ast(const AstNode *node, const char *prefix, bool is_last,
               bool is_root) {
  if (!node) {
    print_prefix(prefix, is_last);
    printf(GRAY("<null>\n"));
    return;
  }

  if (!is_root) {
    print_prefix(prefix, is_last);
  }
  printf(BOLD_MAGENTA("%s\n"), node_type_to_string(node->type));

  char next_prefix[512];
  snprintf(next_prefix, sizeof(next_prefix), "%s%s", prefix,
           is_last ? "    " : "│   ");

  if (node->line > 0 || node->column > 0) {
    print_prefix(next_prefix, true);
    printf(GRAY("Line: %zu, Column: %zu\n"), node->line, node->column);
  }

  switch (node->type) {
  case AST_PROGRAM:
    indent(0);
    for (size_t i = 0; i < node->stmt.program.module_count; ++i) {
      bool last = (i == node->stmt.program.module_count - 1);
      print_ast(node->stmt.program.modules[i], next_prefix, last, false);
    }
    break;

  case AST_PREPROCESSOR_MODULE:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Module Name: "));
    if (node->preprocessor.module.name) {
      printf(YELLOW("%s"), node->preprocessor.module.name);
    } else {
      printf(YELLOW("<unnamed>\n"));
    }
    printf(BOLD_CYAN(" (Potions: %d)\n"), node->preprocessor.module.potions);
    if (node->preprocessor.module.body) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Body:\n"));
      for (size_t i = 0; node->preprocessor.module.body[i] != NULL; ++i) {
        bool last = (node->preprocessor.module.body[i + 1] == NULL);
        print_ast(node->preprocessor.module.body[i], next_prefix, last, false);
      }
    }
    break;

  case AST_PREPROCESSOR_USE:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Use Module: "));
    if (node->preprocessor.use.module_name) {
      printf(YELLOW("%s"), node->preprocessor.use.module_name);
    } else {
      printf(YELLOW("<unnamed>\n"));
    }
    if (node->preprocessor.use.alias) {
      printf(BOLD_CYAN(" as "));
      printf(YELLOW("%s\n"), node->preprocessor.use.alias);
    } else {
      printf("\n");
    }
    break;

  case AST_TYPE_BASIC:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Basic Type: "));
    if (node->type_data.basic.name) {
      printf(YELLOW("%s\n"), node->type_data.basic.name);
    } else {
      printf(YELLOW("<unnamed>\n"));
    }
    break;

  case AST_TYPE_POINTER:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Pointer Type: \n"));
    if (node->type_data.pointer.pointee_type) {
      print_ast(node->type_data.pointer.pointee_type, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<void>\n"));
    }
    break;

  case AST_TYPE_ARRAY:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Array Type: \n"));
    print_ast(node->type_data.array.element_type, next_prefix, true, false);
    if (node->type_data.array.size) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Size: "));
      print_ast(node->type_data.array.size, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<unsized>\n"));
    }
    break;

  case AST_TYPE_FUNCTION:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Function Type: \n"));
    if (node->type_data.function.return_type) {
      print_ast(node->type_data.function.return_type, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<void>\n"));
    }
    if (node->type_data.function.param_count > 0) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Parameters: %zu\n"),
             node->type_data.function.param_count);
      for (size_t i = 0; i < node->type_data.function.param_count; ++i) {
        print_ast(node->type_data.function.param_types[i], next_prefix, true,
                  false);
      }
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no parameters>\n"));
    }
    break;

  case AST_EXPR_LITERAL:
    print_prefix(next_prefix, true);
    printf(GREEN(" (%s): "),
           literal_type_to_string(node->expr.literal.lit_type));
    switch (node->expr.literal.lit_type) {
    case LITERAL_INT:
      printf(GREEN("%lld\n"), node->expr.literal.value.int_val);
      break;
    case LITERAL_FLOAT:
      printf(GREEN("%f\n"), node->expr.literal.value.float_val);
      break;
    case LITERAL_STRING:
      printf(GREEN("\"%s\"\n"), node->expr.literal.value.string_val);
      break;
    case LITERAL_CHAR:
      printf(GREEN("'%c'\n"), node->expr.literal.value.char_val);
      break;
    case LITERAL_BOOL:
      printf(GREEN("%s\n"),
             node->expr.literal.value.bool_val ? "true" : "false");
      break;
    case LITERAL_NULL:
      printf(GREEN("null\n"));
      break;
    default:
      print_prefix(next_prefix, true);
      printf(GRAY("<unknown literal type>\n"));
      break;
    }
    break;

  case AST_EXPR_IDENTIFIER:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Identifier: "));
    if (node->expr.identifier.name) {
      printf(YELLOW("%s\n"), node->expr.identifier.name);
    } else {
      printf(YELLOW("<unnamed>\n"));
    }
    break;

  case AST_EXPR_BINARY:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Binary Expression: "));
    printf(YELLOW(" (%s)\n"), binop_to_string(node->expr.binary.op));

    print_ast(node->expr.binary.left, next_prefix, false, false);
    print_ast(node->expr.binary.right, next_prefix, true, false);
    break;

  case AST_EXPR_GROUPING:
    print_ast(node->expr.grouping.expr, next_prefix, true, false);
    break;

  case AST_EXPR_UNARY:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Unary Operator: "));
    printf(YELLOW(" (%s)\n"), node->expr.unary.op
                                  ? unop_to_string(node->expr.unary.op)
                                  : "unknown");
    print_ast(node->expr.unary.operand, next_prefix, true, false);
    break;

  case AST_EXPR_ARRAY:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Array Expression: \n"));
    for (size_t i = 0; i < node->expr.array.element_count; ++i) {
      print_ast(node->expr.array.elements[i], next_prefix, true, false);
    }
    break;

  case AST_EXPR_CALL:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Function Call: "));
    if (node->expr.call.callee) {
      print_ast(node->expr.call.callee, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no callee>\n"));
    }
    if (node->expr.call.arg_count > 0) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Arguments: %zu\n"), node->expr.call.arg_count);
      for (size_t i = 0; i < node->expr.call.arg_count; ++i) {
        print_ast(node->expr.call.args[i], next_prefix, true, false);
      }
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no arguments>\n"));
    }
    break;

  case AST_EXPR_ASSIGNMENT:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Assignment: \n"));
    if (node->expr.assignment.target) {
      print_ast(node->expr.assignment.target, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no target>\n"));
    }
    if (node->expr.assignment.value) {
      print_ast(node->expr.assignment.value, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no value>\n"));
    }
    break;

  case AST_EXPR_MEMBER:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Member Access: \n"));
    if (node->expr.member.object) {
      print_ast(node->expr.member.object, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no object>\n"));
    }
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Member Name: %s\n"),
           node->expr.member.member ? node->expr.member.member : "<unnamed>");
    break;

  case AST_EXPR_INDEX:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Index Expression: \n"));
    if (node->expr.index.object) {
      print_ast(node->expr.index.object, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no object>\n"));
    }
    if (node->expr.index.index) {
      print_ast(node->expr.index.index, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no index>\n"));
    }
    break;

  case AST_EXPR_TERNARY:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Ternary Expression: \n"));
    if (node->expr.ternary.condition) {
      print_ast(node->expr.ternary.condition, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no condition>\n"));
    }
    if (node->expr.ternary.then_expr) {
      print_ast(node->expr.ternary.then_expr, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no then expression>\n"));
    }
    if (node->expr.ternary.else_expr) {
      print_ast(node->expr.ternary.else_expr, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no else expression>\n"));
    }
    break;

  case AST_EXPR_DEREF:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Deref Expression: \n"));
    if (node->expr.deref.object) {
      print_ast(node->expr.deref.object, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no condition>\n"));
    }
    break;

  case AST_EXPR_ADDR:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Addr Expression: \n"));
    if (node->expr.addr.object) {
      print_ast(node->expr.addr.object, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no condition>\n"));
    }
    break;

  case AST_EXPR_ALLOC:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Alloc Expression: \n"));
    if (node->expr.alloc.size) {
      print_ast(node->expr.alloc.size, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no size>\n"));
    }
    break;

  case AST_EXPR_SIZEOF:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Sizeof Expression: \n"));
    if (node->expr.size_of.object) {
      print_ast(node->expr.size_of.object, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no object>\n"));
    }
    break;

  case AST_EXPR_MEMCPY:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Memcpy Expression: \n"));
    if (node->expr.memcpy.to) {
      print_prefix(next_prefix, false);
      printf(BOLD_CYAN("To: \n"));
      print_ast(node->expr.memcpy.to, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no destination>\n"));
    }
    if (node->expr.memcpy.from) {
      print_prefix(next_prefix, false);
      printf(BOLD_CYAN("From: \n"));
      print_ast(node->expr.memcpy.from, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no source>\n"));
    }
    if (node->expr.memcpy.size) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Size: \n"));
      print_ast(node->expr.memcpy.size, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no size>\n"));
    }
    break;

  case AST_EXPR_FREE:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Free Expression: \n"));
    if (node->expr.free.ptr) {
      print_ast(node->expr.free.ptr, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no pointer>\n"));
    }
    break;

  case AST_EXPR_CAST:
    print_prefix(next_prefix, false);
    printf(BOLD_CYAN("Cast Expression: \n"));
    if (node->expr.cast.type) {
      print_prefix(next_prefix, false);
      printf(BOLD_CYAN("Type: \n"));
      print_ast(node->expr.cast.type, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no type>\n"));
    }
    if (node->expr.cast.castee) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Castee: \n"));
      print_ast(node->expr.cast.castee, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no castee>\n"));
    }
    break;

  case AST_STMT_EXPRESSION:
    print_ast(node->stmt.expr_stmt.expression, next_prefix, true, false);
    break;

  case AST_STMT_PRINT:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Print Statement: \n"));
    if (node->stmt.print_stmt.expr_count > 0) {
      for (size_t i = 0; i < node->stmt.print_stmt.expr_count; ++i) {
        print_ast(node->stmt.print_stmt.expressions[i], next_prefix,
                  (i == node->stmt.print_stmt.expr_count - 1), false);
      }
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no expressions>\n"));
    }
    if (node->stmt.print_stmt.ln) {
      print_prefix(next_prefix, true);
      printf(GRAY("Print with newline\n"));
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("Print without newline\n"));
    }
    break;

  case AST_STMT_VAR_DECL:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Variable Declaration: "));
    if (node->stmt.var_decl.name) {
      printf(YELLOW("%s\n"), node->stmt.var_decl.name);
    } else {
      printf(YELLOW("<unnamed>\n"));
    }
    print_ast(node->stmt.var_decl.var_type, next_prefix, true, false);
    print_ast(node->stmt.var_decl.initializer, next_prefix, true, false);
    print_prefix(next_prefix, true);
    printf(GRAY("Mutable: %s\n"),
           node->stmt.var_decl.is_mutable ? "true" : "false");
    print_prefix(next_prefix, true);
    printf(GRAY("Is Public: %s\n"),
           node->stmt.var_decl.is_public ? "true" : "false");
    break;

  case AST_STMT_FUNCTION:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Function Declaration: "));
    if (node->stmt.func_decl.name) {
      printf(YELLOW("%s\n"), node->stmt.func_decl.name);
    } else {
      printf(YELLOW("<unnamed>\n"));
    }
    print_prefix(next_prefix, true);
    printf(GRAY("Is Public: %s\n"),
           node->stmt.func_decl.is_public ? "true" : "false");
    if (node->stmt.func_decl.param_count > 0) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Parameters: %zu\n"), node->stmt.func_decl.param_count);
      for (size_t i = 0; i < node->stmt.func_decl.param_count; ++i) {
        print_prefix(next_prefix, false);
        printf(GREEN("Parameter %zu: %s\n"), i + 1,
               node->stmt.func_decl.param_names[i]);
        print_ast(node->stmt.func_decl.param_types[i], next_prefix, true,
                  false);
      }
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no parameters>\n"));
    }
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Return Type: \n"));
    if (node->stmt.func_decl.return_type) {
      print_ast(node->stmt.func_decl.return_type, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no return type>\n"));
    }
    print_ast(node->stmt.func_decl.body, next_prefix, true, false);
    break;

  case AST_STMT_ENUM:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Enum Declaration: "));
    if (node->stmt.enum_decl.name) {
      printf(YELLOW("%s\n"), node->stmt.enum_decl.name);
    } else {
      printf(YELLOW("<unnamed>\n"));
    }
    print_prefix(next_prefix, true);
    printf(GRAY("Is Public: %s\n"),
           node->stmt.enum_decl.is_public ? "true" : "false");
    if (node->stmt.enum_decl.member_count > 0) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Members: %zu\n"), node->stmt.enum_decl.member_count);
      for (size_t i = 0; i < node->stmt.enum_decl.member_count; ++i) {
        print_prefix(next_prefix, false);
        printf(GREEN("Member %zu: %s\n"), i + 1,
               node->stmt.enum_decl.members[i]);
      }
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no members>\n"));
    }
    break;

  case AST_STMT_STRUCT:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Struct Declaration: "));
    if (node->stmt.struct_decl.name) {
      printf(YELLOW("%s\n"), node->stmt.struct_decl.name);
    } else {
      printf(YELLOW("<unnamed>\n"));
    }
    print_prefix(next_prefix, true);
    printf(GRAY("Is Public: %s\n"),
           node->stmt.struct_decl.is_public ? "true" : "false");

    if (node->stmt.struct_decl.public_count > 0) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Public Members: %zu\n"),
             node->stmt.struct_decl.public_count);
      for (size_t i = 0; i < node->stmt.struct_decl.public_count; ++i) {
        print_ast(node->stmt.struct_decl.public_members[i], next_prefix, true,
                  false);
      }
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no public members>\n"));
    }

    if (node->stmt.struct_decl.private_count > 0) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Private Members: %zu\n"),
             node->stmt.struct_decl.private_count);
      for (size_t i = 0; i < node->stmt.struct_decl.private_count; ++i) {
        print_ast(node->stmt.struct_decl.private_members[i], next_prefix, true,
                  false);
      }
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no private members>\n"));
    }
    break;

  case AST_STMT_FIELD_DECL:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Field Declaration: "));
    if (node->stmt.field_decl.name) {
      printf(YELLOW("%s\n"), node->stmt.field_decl.name);
    } else {
      printf(YELLOW("<unnamed>\n"));
    }
    print_ast(node->stmt.field_decl.type, next_prefix, true, false);
    print_prefix(next_prefix, true);
    printf(GRAY("Is Public: %s\n"),
           node->stmt.field_decl.is_public ? "true" : "false");
    if (node->stmt.field_decl.function) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Function: \n"));
      print_ast(node->stmt.field_decl.function, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no function>\n"));
    }
    break;

  case AST_STMT_BLOCK:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Block Statement\n"));
    for (size_t i = 0; i < node->stmt.block.stmt_count; ++i) {
      bool last = (i == node->stmt.block.stmt_count - 1);
      print_ast(node->stmt.block.statements[i], next_prefix, last, false);
    }
    break;

  case AST_STMT_RETURN:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Return Statement\n"));
    if (node->stmt.return_stmt.value) {
      print_ast(node->stmt.return_stmt.value, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no return value>\n"));
    }
    break;

  case AST_STMT_IF:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("If Statement\n"));
    print_ast(node->stmt.if_stmt.condition, next_prefix, false, false);
    print_ast(node->stmt.if_stmt.then_stmt, next_prefix, true, false);
    if (node->stmt.if_stmt.elif_stmts) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Elif Statements\n"));
      for (int i = 0; i < node->stmt.if_stmt.elif_count; ++i) {
        print_ast(node->stmt.if_stmt.elif_stmts[i], next_prefix,
                  (i == node->stmt.if_stmt.elif_count - 1), false);
      }
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no elif statements>\n"));
    }
    if (node->stmt.if_stmt.else_stmt) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Else Statement\n"));
      print_ast(node->stmt.if_stmt.else_stmt, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no else statement>\n"));
    }
    break;

  case AST_STMT_LOOP:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Loop Statement\n"));
    if (node->stmt.loop_stmt.condition) {
      print_prefix(next_prefix, false);
      printf(BOLD_CYAN("Condition: \n"));
      print_ast(node->stmt.loop_stmt.condition, next_prefix, false, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no condition>\n"));
    }
    if (node->stmt.loop_stmt.optional) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Optional Expression: \n"));
      print_ast(node->stmt.loop_stmt.optional, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, false);
      printf(GRAY("<no optional expression>\n"));
    }
    if (node->stmt.loop_stmt.initializer) {
      print_prefix(next_prefix, true);
      printf(BOLD_CYAN("Initializers: %zu\n"), node->stmt.loop_stmt.init_count);
      for (size_t i = 0; i < node->stmt.loop_stmt.init_count; ++i) {
        print_ast(node->stmt.loop_stmt.initializer[i], next_prefix,
                  (i == node->stmt.loop_stmt.init_count - 1), false);
      }
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no initializers>\n"));
    }
    print_ast(node->stmt.loop_stmt.body, next_prefix, true, false);
    break;

  case AST_STMT_BREAK_CONTINUE:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("%s Statement | "),
           node->stmt.break_continue.is_continue ? "Continue" : "Break");
    if (node->stmt.break_continue.is_continue) {
      printf(GRAY("This is a continue statement.\n"));
    } else {
      printf(GRAY("This is a break statement.\n"));
    }
    printf("\n");
    break;

  case AST_STMT_DEFER:
    print_prefix(next_prefix, true);
    printf(BOLD_CYAN("Defer Statement\n"));
    if (node->stmt.defer_stmt.statement) {
      print_ast(node->stmt.defer_stmt.statement, next_prefix, true, false);
    } else {
      print_prefix(next_prefix, true);
      printf(GRAY("<no deferred statement>\n"));
    }
    break;

  default:
    print_prefix(next_prefix, true);
    printf(GRAY("No specific print logic for this node type.\n"));
    break;
  }
}
