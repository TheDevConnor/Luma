# Luma Language Compiler TODO

## ✅ Implemented (Up to Codegen)

These AST node types are fully implemented in code generation:

### Expressions

- [x] `AST_EXPR_LITERAL`
- [x] `AST_EXPR_IDENTIFIER`
- [x] `AST_EXPR_BINARY`
- [x] `AST_EXPR_UNARY`
- [x] `AST_EXPR_CALL`
- [x] `AST_EXPR_ASSIGNMENT`
- [x] `AST_EXPR_GROUPING`
- [x] `AST_EXPR_CAST`
- [x] `AST_EXPR_SIZEOF`
- [x] `AST_EXPR_ALLOC`
- [x] `AST_EXPR_FREE`
- [x] `AST_EXPR_DEREF`
- [x] `AST_EXPR_ADDR`
- [x] `AST_EXPR_MEMBER`
- [x] `AST_EXPR_INC`
- [x] `AST_EXPR_DEC`

### Statements

- [x] `AST_PROGRAM` (multi-module support)
- [x] `AST_PREPROCESSOR_MODULE`
- [x] `AST_PREPROCESSOR_USE`
- [x] `AST_STMT_EXPRESSION`
- [x] `AST_STMT_VAR_DECL`
- [x] `AST_STMT_FUNCTION`
- [x] `AST_STMT_RETURN`
- [x] `AST_STMT_BLOCK`
- [x] `AST_STMT_IF`
- [x] `AST_STMT_PRINT`
- [x] `AST_STMT_DEFER`
- [x] `AST_STMT_LOOP` (this is for, while, and while-true)
- [x] `AST_STMT_IMPL`

### Types

- [x] `AST_TYPE_BASIC`
- [x] `AST_TYPE_POINTER`
- [x] `AST_TYPE_ARRAY`
- [x] `AST_TYPE_FUNCTION`
- [ ] `AST_TYPE_SET`
- [ ] `AST_TYPE_SOME`

---

## 🧠 Static Memory Analysis

### ✅ Currently Implemented

- [x] Basic allocation/free tracking by variable name
- [x] Memory leak detection (allocated but never freed)
- [x] Double-free detection with count tracking
- [x] Integration with `defer` statements
- [x] Detailed error reporting with source locations
- [x] Anonymous allocation filtering

### 🔧 Memory Analysis Improvements Needed

#### High Priority

- [ ] **Pointer aliasing detection**
- [ ] Track when `ptr2 = ptr1` creates aliases
- [ ] Warn when analyzer can't track aliased pointers
- [ ] Consider ownership transfer semantics
- [ ] Allowing structs to point to itself -- name struct {some: *name};

#### Control Flow Analysis  

- [ ] **Conditional path tracking**
- [ ] Detect leaks in conditional branches (`if/else` without free in all paths)
- [ ] Handle early returns and breaks
- [ ] Track memory across loop iterations

#### Function Call Analysis

- [ ] **Cross-function tracking**
- [ ] Track pointers passed to functions as parameters
- [ ] Handle functions that free parameters
- [ ] Return value allocation tracking
- [ ] Support for ownership transfer through function calls

#### Advanced Pointer Operations

- [ ] **Complex pointer arithmetic**
- [ ] Handle `ptr + offset` operations
- [ ] Track array element allocations
- [ ] Detect out-of-bounds access potential

#### Memory Operation Extensions

- [ ] **Additional memory functions**
- [ ] Track `realloc()` operations
- [ ] Handle `calloc()` and `malloc()` variants
- [ ] Monitor `memcpy()` for potential use-after-free

#### Data Structure Tracking

- [ ] **Struct/array memory management**
- [ ] Track allocations within struct members
- [ ] Handle nested pointer structures
- [ ] Monitor array of pointers

#### Use-After-Free Detection

- [ ] **Access after free tracking**
- [ ] Detect reads/writes to freed pointers
- [ ] Track freed pointer usage across scopes
- [ ] Integration with dereference operations

#### Scope and Lifetime Analysis

- [ ] **Advanced scope tracking**
- [ ] Detect pointers escaping local scope
- [ ] Handle static/global pointer lifetimes
- [ ] Stack vs heap allocation analysis

---

## 📝 Next Steps

### Parsing

- [ ] Add parsing for templates (`fn[T]`, `struct[T]`)  
- [ ] Add parsing for type aliases using `type` keyword  
- [ ] Add parsing for modules and imports refinements  
- [ ] Design and implement **union syntax**
- [ ] Consider Go/Odin-style loop syntax improvements

### Semantic Analysis

- [ ] Type inference for generics  
- [ ] Detect unused imports and symbols  

### Codegen

- [ ] Implement codegen for `switch` or `match` constructs  
- [ ] Support more LLVM optimizations  
- [ ] **Add structs and enums support** in codegen  
- [ ] **Add unions support** in codegen
- [ ] **Add in memcpy and streq** streq === strcmp

### Lexer & Parser

- [ ] Add tokens and grammar for unions  

### Type Checker

- [ ] Implement type checking for structs  
- [ ] Implement type checking for unions

---

## 🚀 Future Features Ideas (Maybe)

- [ ] Investigate pattern matching  
- [ ] Build minimal standard library
- [ ] Consider ownership/borrowing system for advanced memory safety
- [ ] Explore compile-time memory layout optimization
