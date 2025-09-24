#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "../c_libs/memory/memory.h"

// Forward declaration
typedef struct AstNode AstNode;

// Node type enumeration
typedef enum {
  // Preprocessor nodes
  AST_PREPROCESSOR_MODULE, // Module declaration
  AST_PREPROCESSOR_USE,    // Use/import statement

  // Expression nodes
  AST_EXPR_LITERAL,    // Literal values (numbers, strings, booleans)
  AST_EXPR_IDENTIFIER, // Variable/function names
  AST_EXPR_BINARY,     // Binary operations (+, -, *, /, etc.)
  AST_EXPR_UNARY,      // Unary operations (!, -, ++, --)
  AST_EXPR_CALL,       // Function calls
  AST_EXPR_ASSIGNMENT, // Assignment expressions
  AST_EXPR_TERNARY,    // Conditional expressions (? :)
  AST_EXPR_MEMBER,     // Member access (obj.field)
  AST_EXPR_INDEX,      // Array/object indexing (obj[index])
  AST_EXPR_GROUPING,   // Parenthesized expressions
  AST_EXPR_RANGE,      // range expressions '..'
  AST_EXPR_ARRAY,      // [ ... ] array expressions
  AST_EXPR_DEREF,      // *object
  AST_EXPR_ADDR,       // &object
  AST_EXPR_ALLOC,
  AST_EXPR_MEMCPY,
  AST_EXPR_FREE,
  AST_EXPR_CAST,
  AST_EXPR_SIZEOF,

  // Statement nodes
  AST_PROGRAM,             // Program root node
  AST_STMT_EXPRESSION,     // Expression statements
  AST_STMT_VAR_DECL,       // Variable declarations
  AST_STMT_CONST_DECL,     // Constant declarations
  AST_STMT_FUNCTION,       // Function definitions
  AST_STMT_IF,             // If statements
  AST_STMT_LOOP,           // Loop statements (while, for)
  AST_STMT_BREAK_CONTINUE, // Break and continue statements
  AST_STMT_RETURN,         // Return statements
  AST_STMT_BLOCK,          // Block statements
  AST_STMT_PRINT,          // Print statements
  AST_STMT_MODULE,         // Module declarations
  AST_STMT_ENUM,           // Enum declarations
  AST_STMT_STRUCT,         // Struct declarations
  AST_STMT_FIELD_DECL,     // Field declarations (for structs)
  AST_STMT_DEFER,          // Defer statements
  AST_STMT_SWITCH,         // Switch statement
  AST_STMT_CASE,
  AST_STMT_DEFAULT,

  // Type nodes
  AST_TYPE_BASIC,    // Basic types (int, float, string, etc.)
  AST_TYPE_POINTER,  // Pointer types
  AST_TYPE_ARRAY,    // Array types
  AST_TYPE_FUNCTION, // Function types
  AST_TYPE_STRUCT,   // Struct types
  AST_TYPE_ENUM,     // Enum types
} NodeType;

// Literal types
typedef enum {
  LITERAL_IDENT,
  LITERAL_INT,
  LITERAL_FLOAT,
  LITERAL_DOUBLE,
  LITERAL_STRING,
  LITERAL_CHAR,
  LITERAL_BOOL,
  LITERAL_NULL
} LiteralType;

// Binary operators
typedef enum {
  BINOP_ADD,     // +
  BINOP_SUB,     // -
  BINOP_MUL,     // *
  BINOP_DIV,     // /
  BINOP_MOD,     // %
  BINOP_POW,     // **
  BINOP_EQ,      // ==
  BINOP_NE,      // !=
  BINOP_LT,      // <
  BINOP_LE,      // <=
  BINOP_GT,      // >
  BINOP_GE,      // >=
  BINOP_AND,     // &&
  BINOP_OR,      // ||
  BINOP_BIT_AND, // &
  BINOP_BIT_OR,  // |
  BINOP_BIT_XOR, // ^
  BINOP_SHL,     // <<
  BINOP_SHR,     // >>
  BINOP_RANGE,   // ..
} BinaryOp;

// Unary operators
typedef enum {
  UNOP_NOT,      // !
  UNOP_NEG,      // -
  UNOP_POS,      // +
  UNOP_BIT_NOT,  // ~
  UNOP_PRE_INC,  // ++x
  UNOP_PRE_DEC,  // --x
  UNOP_POST_INC, // x++
  UNOP_POST_DEC, // x--
  UNOP_DEREF,    // *x
  UNOP_ADDR,     // &x
} UnaryOp;

typedef enum {
  Node_Category_EXPR,
  Node_Category_STMT,
  Node_Category_TYPE,
  Node_Category_PREPROCESSOR
} NodeCategory;

// Base AST node structure
struct AstNode {
  NodeType type;
  size_t line;
  size_t column;
  NodeCategory category; // Category of the node (expression, statement, type)

  union {
    struct {
      union {
        // Preprocessor-specific data
        struct {
          char *name;
          int potions;
          AstNode **body;
          size_t body_count;
        } module;

        // @use "module_name" as module;
        struct {
          const char *module_name;
          const char *alias;
        } use;
      };
    } preprocessor;

    struct {
      // Expression-specific data
      union {
        // Literal expression
        struct {
          LiteralType lit_type;
          union {
            long long int_val;
            double float_val;
            char *string_val;
            char char_val;
            bool bool_val;
          } value;
        } literal;

        // Identifier expression
        struct {
          char *name;
        } identifier;

        // Binary expression
        struct {
          BinaryOp op;
          AstNode *left;  // Changed from Expr* to AstNode*
          AstNode *right; // Changed from Expr* to AstNode*
        } binary;

        // Unary expression
        struct {
          UnaryOp op;
          AstNode *operand; // Changed from Expr* to AstNode*
        } unary;

        // Function call expression
        struct {
          AstNode *callee; // Changed from Expr* to AstNode*
          AstNode **args;  // Changed from Expr** to AstNode**
          size_t arg_count;
        } call;

        // Assignment expression
        struct {
          AstNode *target; // Changed from Expr* to AstNode*
          AstNode *value;  // Changed from Expr* to AstNode*
        } assignment;

        // Ternary expression
        struct {
          AstNode *condition; // Changed from Expr* to AstNode*
          AstNode *then_expr; // Changed from Expr* to AstNode*
          AstNode *else_expr; // Changed from Expr* to AstNode*
        } ternary;

        // Member access expression
        struct {
          bool is_compiletime;
          AstNode *object; // Changed from Expr* to AstNode*
          char *member;
        } member;

        // Index expression
        struct {
          AstNode *object; // Changed from Expr* to AstNode*
          AstNode *index;  // Changed from Expr* to AstNode*
        } index;

        // Grouping expression
        struct {
          AstNode *expr; // Changed from Expr* to AstNode*
        } grouping;

        // Array expression
        struct {
          AstNode **elements; // Changed from Expr** to AstNode**
          size_t element_count;
        } array;
        
        // Deref expression
        struct {
          AstNode *object;
        } deref;

        // Address experssion
        struct {
          AstNode *object;
        } addr;

        // alloc expression
        struct {
          AstNode *size;
        } alloc;

        // memcpy expression
        struct {
          AstNode *to;
          AstNode *from;
          AstNode *size;
        } memcpy;

        // free expression
        struct {
          AstNode *ptr;
        } free;

        // cast expression
        struct {
          AstNode *type;
          AstNode *castee;
        } cast;

        // sizeof expression
        struct {
          AstNode *object;
          bool is_type;
        } size_of;
      };
    } expr;

    struct {
      // Statement-specific data
      union {
        // Program root node
        struct {
          AstNode **modules; // Changed from Stmt** to AstNode**
          size_t module_count;
        } program;

        // Expression statement
        struct {
          AstNode *expression; // Changed from Expr* to AstNode*
        } expr_stmt;

        // Variable declaration
        struct {
          const char *name;
          AstNode *var_type;    // Changed from Type* to AstNode*
          AstNode *initializer; // Changed from Expr* to AstNode*
          bool is_mutable;      // Whether the variable is mutable
          bool is_public;
        } var_decl;

        // const Persion = struct {
        // pub:
        //   name: str;
        //   age: int;
        // priv:
        //   ssn: str;
        // };
        // Struct declaration
        struct {
          const char *name;
          AstNode **public_members; // Changed from Stmt** to AstNode**
          size_t public_count;
          AstNode **private_members; // Changed from Stmt** to AstNode**
          size_t private_count;
          bool is_public; // Whether the struct is public (which is true by
                          // default)
        } struct_decl;

        struct {
          const char *name;
          AstNode *type;     // Changed from Type* to AstNode*
          AstNode *function; // Changed from Stmt* to AstNode*
          bool is_public;    // Whether the field is public
        } field_decl;

        // Enumeration declaration
        struct {
          const char *name;
          char **members; // Changed from char** to AstNode**
          size_t member_count;
          bool is_public; // same as struct, enums are public by default
        } enum_decl;

        // Function declaration
        struct {
          const char *name;
          char **param_names;
          AstNode **param_types; // Changed from Type** to AstNode**
          size_t param_count;
          AstNode *return_type; // Changed from Type* to AstNode*
          bool is_public;
          AstNode *body; // Changed from Stmt* to AstNode*
        } func_decl;

        // If statement
        struct {
          AstNode *condition;   // Changed from Expr* to AstNode*
          AstNode *then_stmt;   // Changed from Stmt* to AstNode*
          AstNode **elif_stmts; // Changed from Stmt* to AstNode*
          int elif_count;       // Number of elif statements
          AstNode *else_stmt;   // Changed from Stmt* to AstNode*
        } if_stmt;

        // Loop statement (Combined while and for)
        struct {
          AstNode *condition; // Changed from Expr* to AstNode*
          AstNode *optional; // Optional expression (e.g., for loop initializer)
          AstNode *body;     // Changed from Stmt* to AstNode*
          // For loops can be represented as a while loop with an initializer
          // and increment
          AstNode **initializer; // Optional initializer for for loops (Changed
                                 // from Stmt* to AstNode*)
          size_t init_count;     // Number of initializers
        } loop_stmt;

        // Return statement
        struct {
          AstNode *value; // Changed from Expr* to AstNode*
        } return_stmt;

        // Block statement
        struct {
          AstNode **statements; // Changed from Stmt** to AstNode**
          size_t stmt_count;
        } block;

        // Print statement
        struct {
          AstNode **expressions; // Changed from Expr** to AstNode**
          size_t expr_count;
          bool ln; // Whether to print with a newline
        } print_stmt;

        struct {
          bool is_continue; // true for continue, false for break
        } break_continue;

        struct {
          AstNode *statement;
        } defer_stmt;

        struct {
          AstNode *condition;     // The switch expression
          struct AstNode **cases; // Array of case clauses
          size_t case_count;
          struct AstNode *default_case; // Optional default case
        } switch_stmt;

        // Case clause node
        struct {
          AstNode **values;   // Array of expressions (0, 1, 2, 3, ...)
          size_t value_count; // Number of values in this case
          AstNode *body;      // Block statement for this case
        } case_clause;

        struct {
          AstNode *body; // Block statement for default case
        } default_clause;
      };
    } stmt;

    struct {
      // Type-specific data
      union {
        // Basic type
        struct {
          const char *name;
        } basic;

        // Pointer type
        struct {
          AstNode *pointee_type; // Changed from Type* to AstNode*
        } pointer;

        // Array type
        struct {
          AstNode *element_type; // Changed from Type* to AstNode*
          AstNode *size;         // Changed from Expr* to AstNode*
        } array;

        // Function type
        struct {
          AstNode **param_types; // Changed from Type** to AstNode**
          size_t param_count;
          AstNode *return_type; // Changed from Type* to AstNode*
        } function;

        // AST_TYPE_STRUCT - ADD THIS STRUCT
        struct {
          const char *name;          // Name of the struct type
          AstNode **member_types;    // Array of member types
          const char **member_names; // Array of member names
          size_t member_count;       // Number of members
        } struct_type;
      };
    } type_data;
  };
};

// Type aliases for cleaner code (defined AFTER the struct)
typedef AstNode Preprocessor;
typedef AstNode Expr;
typedef AstNode Stmt;
typedef AstNode Type;

AstNode *create_preprocessor_node(ArenaAllocator *arena, NodeType type,
                                  NodeCategory category, size_t line,
                                  size_t column);
AstNode *create_expr_node(ArenaAllocator *arena, NodeType type, size_t line,
                          size_t column);
AstNode *create_stmt_node(ArenaAllocator *arena, NodeType type, size_t line,
                          size_t column);
AstNode *create_type_node(ArenaAllocator *arena, NodeType type, size_t line,
                          size_t column);

// Helper macros for creating nodes
#define create_preprocessor(arena, type, line, column)                         \
  create_preprocessor_node(arena, type, Node_Category_PREPROCESSOR, line,      \
                           column)
#define create_expr(arena, type, line, column)                                 \
  create_expr_node(arena, type, line, column)
#define create_stmt(arena, type, line, column)                                 \
  create_stmt_node(arena, type, line, column)
#define create_type(arena, type, line, column)                                 \
  create_type_node(arena, type, line, column)

// Create the AstNode
AstNode *create_ast_node(ArenaAllocator *arena, NodeType type,
                         NodeCategory category, size_t line, size_t column);

// Preprocessor creation macros
AstNode *create_module_node(ArenaAllocator *arena, const char *name,
                            int potions, AstNode **body, size_t body_count,
                            size_t line, size_t column);
AstNode *create_use_node(ArenaAllocator *arena, const char *module_name,
                         const char *alias, size_t line, size_t column);

// Expression creation macros
AstNode *create_literal_expr(ArenaAllocator *arena, LiteralType lit_type,
                             void *value, size_t line, size_t column);
AstNode *create_identifier_expr(ArenaAllocator *arena, const char *name,
                                size_t line, size_t column);
AstNode *create_binary_expr(ArenaAllocator *arena, BinaryOp op, Expr *left,
                            Expr *right, size_t line, size_t column);
AstNode *create_unary_expr(ArenaAllocator *arena, UnaryOp op, Expr *operand,
                           size_t line, size_t column);
AstNode *create_call_expr(ArenaAllocator *arena, Expr *callee, Expr **args,
                          size_t arg_count, size_t line, size_t column);
AstNode *create_assignment_expr(ArenaAllocator *arena, Expr *target,
                                Expr *value, size_t line, size_t column);
AstNode *create_ternary_expr(ArenaAllocator *arena, Expr *condition,
                             Expr *then_expr, Expr *else_expr, size_t line,
                             size_t column);
AstNode *create_member_expr(ArenaAllocator *arena, Expr *object,
                            bool is_compiletime, const char *member,
                            size_t line, size_t column);
AstNode *create_index_expr(ArenaAllocator *arena, Expr *object, Expr *index,
                           size_t line, size_t column);
AstNode *create_grouping_expr(ArenaAllocator *arena, Expr *expr, size_t line,
                              size_t column);
AstNode *create_array_expr(ArenaAllocator *arena, Expr **elements,
                           size_t element_count, size_t line, size_t column);
AstNode *create_deref_expr(ArenaAllocator *arena, Expr *object, size_t line,
                           size_t col);
AstNode *create_addr_expr(ArenaAllocator *arena, Expr *object, size_t line,
                          size_t col);
AstNode *create_alloc_expr(ArenaAllocator *arena, Expr *size, size_t line,
                           size_t col);
AstNode *create_memcpy_expr(ArenaAllocator *arena, Expr *to, Expr *from,
                            Expr *size, size_t line, size_t col);
AstNode *create_free_expr(ArenaAllocator *arena, Expr *ptr, size_t line,
                          size_t col);
AstNode *create_cast_expr(ArenaAllocator *arena, Expr *type, Expr *castee,
                          size_t line, size_t col);
AstNode *create_sizeof_expr(ArenaAllocator *arena, Expr *object, bool is_type,
                            size_t line, size_t col);

// Statement creation macros
AstNode *create_program_node(ArenaAllocator *arena, AstNode **statements,
                             size_t stmt_count, size_t line, size_t column);
AstNode *create_expr_stmt(ArenaAllocator *arena, Expr *expression, size_t line,
                          size_t column);
AstNode *create_var_decl_stmt(ArenaAllocator *arena, const char *name,
                              AstNode *var_type, Expr *initializer,
                              bool is_mutable, bool is_public, size_t line,
                              size_t column);
AstNode *create_func_decl_stmt(ArenaAllocator *arena, const char *name,
                               char **param_names, AstNode **param_types,
                               size_t param_count, AstNode *return_type,
                               bool is_public, AstNode *body, size_t line,
                               size_t column);
AstNode *create_struct_decl_stmt(ArenaAllocator *arena, const char *name,
                                 AstNode **public_members, size_t public_count,
                                 AstNode **private_members,
                                 size_t private_count, bool is_piblic,
                                 size_t line, size_t column);
AstNode *create_field_decl_stmt(ArenaAllocator *arena, const char *name,
                                AstNode *type, AstNode *function,
                                bool is_public, size_t line, size_t column);
AstNode *create_enum_decl_stmt(ArenaAllocator *arena, const char *name,
                               char **members, size_t member_count,
                               bool is_public, size_t line, size_t column);
AstNode *create_if_stmt(ArenaAllocator *arena, Expr *condition,
                        AstNode *then_stmt, AstNode **elif_stmts,
                        int elif_count, AstNode *else_stmt, size_t line,
                        size_t column);

AstNode *create_infinite_loop_stmt(ArenaAllocator *arena, AstNode *body,
                                   size_t line, size_t column);
AstNode *create_for_loop_stmt(ArenaAllocator *arena, AstNode **initializers,
                              size_t init_count, Expr *condition,
                              Expr *optional, AstNode *body, size_t line,
                              size_t column);
AstNode *create_loop_stmt(ArenaAllocator *arena, Expr *condition,
                          Expr *optional, AstNode *body, size_t line,
                          size_t column);

AstNode *create_return_stmt(ArenaAllocator *arena, Expr *value, size_t line,
                            size_t column);
AstNode *create_block_stmt(ArenaAllocator *arena, AstNode **statements,
                           size_t stmt_count, size_t line, size_t column);
AstNode *create_print_stmt(ArenaAllocator *arena, Expr **expressions,
                           size_t expr_count, bool ln, size_t line,
                           size_t column);
AstNode *create_break_continue_stmt(ArenaAllocator *arena, bool is_continue,
                                    size_t line, size_t column);
AstNode *create_defer_stmt(ArenaAllocator *arena, AstNode *statement,
                           size_t line, size_t column);
AstNode *create_switch_stmt(ArenaAllocator *arena, AstNode *condition,
                            AstNode **cases, size_t case_count,
                            AstNode *default_case, size_t line, size_t column);
AstNode *create_case_stmt(ArenaAllocator *arena, AstNode **values,
                          size_t value_count, AstNode *body, size_t line,
                          size_t column);
AstNode *create_default_stmt(ArenaAllocator *arena, AstNode *body, size_t line,
                             size_t column);

// Type creation macros
AstNode *create_basic_type(ArenaAllocator *arena, const char *name, size_t line,
                           size_t column);
AstNode *create_pointer_type(ArenaAllocator *arena, AstNode *pointee_type,
                             size_t line, size_t column);
AstNode *create_array_type(ArenaAllocator *arena, AstNode *element_type,
                           Expr *size, size_t line, size_t column);
AstNode *create_function_type(ArenaAllocator *arena, AstNode **param_types,
                              size_t param_count, AstNode *return_type,
                              size_t line, size_t column);
