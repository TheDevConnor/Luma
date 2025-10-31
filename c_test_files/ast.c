#include <stdio.h>
#include <stdlib.h>

// Define an enum to distinguish node types
typedef enum {
  NODE_TYPE_NUMBER,
  NODE_TYPE_BINARY_OP,
  NODE_TYPE_UNARY_OP
} NodeType;

// Base struct for all AST nodes
typedef struct ASTNode {
  NodeType type;
  // Common fields like line number, column number can go here
} ASTNode;

// Specific struct for number literals
typedef struct NumberNode {
  ASTNode base; // Embed the base struct
  int value;
} NumberNode;

// Specific struct for binary operations (e.g., +, -, *, /)
typedef struct BinaryOpNode {
  ASTNode base; // Embed the base struct
  char operator;
  struct ASTNode *left;
  struct ASTNode *right;
} BinaryOpNode;

// Specific struct for unary operations (e.g., negation)
typedef struct UnaryOpNode {
  ASTNode base; // Embed the base struct
  char operator;
  struct ASTNode *operand;
} UnaryOpNode;

// Function to create a number node
ASTNode *create_number_node(int value) {
  NumberNode *node = (NumberNode *)malloc(sizeof(NumberNode));
  node->base.type = NODE_TYPE_NUMBER;
  node->value = value;
  return (ASTNode *)node;
}

// Function to create a binary operation node
ASTNode *create_binary_op_node(char op, ASTNode *left, ASTNode *right) {
  BinaryOpNode *node = (BinaryOpNode *)malloc(sizeof(BinaryOpNode));
  node->base.type = NODE_TYPE_BINARY_OP;
  node->operator = op;
  node->left = left;
  node->right = right;
  return (ASTNode *)node;
}

// Function to create a unary operation node
ASTNode *create_unary_op_node(char op, ASTNode *operand) {
  UnaryOpNode *node = (UnaryOpNode *)malloc(sizeof(UnaryOpNode));
  node->base.type = NODE_TYPE_UNARY_OP;
  node->operator = op;
  node->operand = operand;
  return (ASTNode *)node;
}

// Function to free AST nodes (recursive)
void free_ast(ASTNode *node) {
  if (node == NULL) {
    return;
  }

  switch (node->type) {
  case NODE_TYPE_NUMBER:
    // No child nodes to free
    break;
  case NODE_TYPE_BINARY_OP: {
    BinaryOpNode *bin_node = (BinaryOpNode *)node;
    free_ast(bin_node->left);
    free_ast(bin_node->right);
    break;
  }
  case NODE_TYPE_UNARY_OP: {
    UnaryOpNode *un_node = (UnaryOpNode *)node;
    free_ast(un_node->operand);
    break;
  }
  }
  free(node);
}

// Example usage:
int main() {
  // Represents (5 + 3) * 2
  ASTNode *five = create_number_node(5);
  ASTNode *three = create_number_node(3);
  ASTNode *add = create_binary_op_node('+', five, three);
  ASTNode *two = create_number_node(2);
  ASTNode *multiply = create_binary_op_node('*', add, two);

  // You would typically have a function to traverse and interpret the AST here
  // For demonstration, we'll just free it.
  printf("AST created successfully.\n");

  free_ast(multiply);
  printf("AST freed.\n");

  return 0;
}