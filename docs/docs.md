# Luma Language Documentation

Luma is a statically typed, compiled programming language designed for systems programming. It combines the low-level control of C with a strong type system and modern safety features that eliminate many common runtime errors.

## Table of Contents

- [Language Philosophy](#language-philosophy)
- [Quick Start](#quick-start)
- [Type System](#type-system)
- [Generics](#generics)
- [Top-Level Bindings with `const`](#top-level-bindings-with-const)
- [Variables and Mutability](#variables-and-mutability)
- [Functions](#functions)
- [Name Resolution](#name-resolution)
- [Control Flow](#control-flow)
- [Switch Statements](#switch-statements)
- [Module System](#module-system)
- [Built-in Functions](#built-in-functions)
- [Type Casting System](#type-casting-system)
- [Array Types](#array-types)
- [String Literals and String Types](#string-literals-and-string-types)
- [Pointer Arithmetic](#pointer-arithmetic)
- [Visibility and Access Control](#visibility-and-access-control)
- [Memory Management](#memory-management)
- [Error Handling](#error-handling)
- [Performance](#performance)
- [Standard Library](#standard-library)
- [Safety Features](#safety-features)

---

## Language Philosophy

Luma is built on three core principles:

- **Simplicity**: Minimal syntax with consistent patterns
- **Safety**: Strong typing and memory safety features
- **Performance**: Zero-cost abstractions and predictable performance

---

## Quick Start

Here's a complete Luma program that demonstrates the core language features:

```luma
@module "main"

const Point = struct {
    x: int,
    y: int,
    
    distance_to = fn (other: Point) float {
        let dx: int = other.x - x;
        let dy: int = other.y - y;
        return sqrt(cast(dx * dx + dy * dy));
    }
};

const Status = enum {
    Active,
    Inactive,
    Pending,
};

const main = fn () int {
    let origin: Point = Point { x: 0, y: 0 };
    let destination: Point = Point { x: 3, y: 4 };
    let current_status: Status = Status::Active;
    
    outputln("Distance: ", origin.distance_to(destination));
    
    switch (current_status) {
        Status::Active => outputln("System is running");
        Status::Inactive => outputln("System is stopped");
        Status::Pending => outputln("System is starting");
    }
    
    return 0;
}
```

This example shows:
- Module declaration with `@module`
- Struct definitions with methods
- Enum definitions
- Static access with `::` for enum variants
- Runtime access with `.` for struct members
- Function definitions and calls
- Switch statements with pattern matching

---

## Type System

Luma provides a straightforward type system with both primitive and compound types.

### Primitive Types

| Type | Description | Size |
|------|-------------|------|
| `int` | Signed integer | 64-bit |
| `uint` | Unsigned integer | 64-bit |
| `float` | Floating point | 32-bit |
| `double` | Floating point | 64-bit |
| `bool` | Boolean | 1 byte |
| `char` | Unicode Character| 1 byte |
| `str` | String | Variable |

### Enumerations

Enums provide type-safe constants with clean syntax:

```luma
const Direction = enum {
    North,
    South,
    East,
    West
};

const current_direction: Direction = Direction::North;
```

### Structures

Structures group related data with optional access control:

```luma
const Point = struct {
    x: int,
    y: int
};

// With explicit access modifiers
const Player = struct {
pub:
    name: str,
    score: int,
priv:
    internal_id: uint,
    
    // Methods can be defined inside structs
    get_display_name = fn () str {
        return name + " (" + str(score) + " pts)";
    }
};
```

### Using Types

```luma
const origin: Point = Point { x: 0, y: 0 };
const player: Player = Player { 
    name: "Alice", 
    score: 100,
    internal_id: 12345 
};
```

---

## Generics

Luma supports generic programming through templates, enabling you to write code that works with multiple types while maintaining type safety and zero-cost abstractions.

### Generic Functions

Generic functions are declared with type parameters in angle brackets `<>` after the `fn` keyword:

```luma
const add = fn<T>(a: T, b: T) T { 
    return a + b; 
}

const swap = fn<T>(a: *T, b: *T) void {
    let temp: T = *a;
    *a = *b;
    *b = temp;
}

const max = fn<T>(a: T, b: T) T {
    if (a > b) {
        return a;
    }
    return b;
}
```

### Using Generic Functions

Generic functions require **explicit type arguments** at the call site:

```luma
const main = fn() int {
    // Integer arithmetic
    outputln("add(1, 2) = ", add<int>(1, 2));
    
    // Floating-point arithmetic
    outputln("add(1.5, 2.5) = ", add<float>(1.5, 2.5));

    // Swapping integers
    let x: int = 5; 
    let y: int = 10;
    swap<int>(&x, &y);
    outputln("After swap: x = ", x, ", y = ", y);
    
    // Finding maximum
    let largest: int = max<int>(42, 17);
    outputln("Max: ", largest);
    
    return 0;
}
```

### Generic Structs

Structs can also be generic, allowing you to create container types and data structures that work with any type:

```luma
const Box = struct<T> {
    value: T,
    
    get = fn() T {
        return value;
    },
    
    set = fn(new_value: T) void {
        value = new_value;
    }
};

const Pair = struct<T, U> {
    first: T,
    second: U
};

// Usage
const main = fn() int {
    // Box holding an integer
    let int_box: Box<int> = Box { value: 42 };
    outputln("Box contains: ", int_box.get());
    
    // Box holding a float
    let float_box: Box<float> = Box { value: 3.14 };
    
    // Pair with different types
    let pair: Pair<int, str> = Pair { 
        first: 1, 
        second: "hello" 
    };
    outputln("Pair: (", pair.first, ", ", pair.second, ")");
    
    return 0;
}
```

### Monomorphization

Luma uses **monomorphization** for generic code generation. This means:

- The compiler generates **separate machine code** for each concrete type used
- Generic code has **zero runtime overhead** compared to hand-written type-specific code
- Each instantiation (e.g., `add<int>`, `add<float>`) produces its own optimized assembly
- Similar to C++ templates and Rust generics, not Java's type erasure

**Example:**

```luma
const identity = fn<T>(x: T) T {
    return x;
}

// These calls generate separate functions in the compiled binary:
let a: int = identity<int>(42);        // Generates identity_int
let b: float = identity<float>(3.14);  // Generates identity_float
let c: str = identity<str>("hello");   // Generates identity_str
```

---

## Top-Level Bindings with `const`

Luma uses the `const` keyword as a **unified declaration mechanism** for all top-level bindings. Whether you're declaring variables, functions, types, or enums, `const` provides a consistent syntax that enforces immutability at the binding level.

### Basic Syntax

```luma
const NUM: int = 42;                                  // Immutable variable
const Direction = enum { North, South, East, West };  // Enum definition
const Point = struct { x: int, y: int };              // Struct definition
const Box = struct<T> { value: T };                   // Generic struct
const add = fn (a: int, b: int) int {                 // Function definition
    return a + b; 
};
const max = fn<T>(a: T, b: T) T {                     // Generic function
    if (a > b) { return a; }
    return b;
};
```

### Why This Design?

**Unified syntax**: One parsing rule handles all top-level declarations, simplifying both the compiler and developer experience.

**Semantic clarity**: The binding itself is immutable—you cannot reassign or shadow a top-level `const`. This prevents accidental redefinition bugs.

**Compiler optimization**: Immutable bindings enable better optimization opportunities.

**Future extensibility**: This approach naturally supports compile-time metaprogramming and uniform import behavior.

### Important Notes

```luma
const x: int = 5;
x = 10; // ❌ Error: `x` is immutable

const add = fn (a: int, b: int) int { return a + b; };
add = something_else; // ❌ Error: cannot reassign function binding
```

---

## Variables and Mutability

Inside functions, use `let` to declare local variables:

```luma
const main = fn () int {
    let x: int = 10;        // Mutable local variable
    x = 20;                 // ✅ Can be reassigned
    
    let y: int = 5;
    y = y + 1;              // ✅ Can be modified
    
    let counter: int = 0;
    loop (counter < 10) {
        counter = counter + 1;  // ✅ Mutating in loop
    }
    
    return 0;
}
```

**Key difference:**
- `const` at top-level = immutable binding (cannot reassign)
- `let` in functions = mutable variable (can reassign and modify)

---

## Functions

Functions are first-class values in Luma.

### Function Declaration

```luma
// Basic function
const add = fn (a: int, b: int) int {
    return a + b;
};

// Function with no parameters
const greet = fn () void {
    outputln("Hello!");
};

// Function with no return value
const print_number = fn (n: int) void {
    outputln("Number: ", n);
};
```

### Function Calls

```luma
const main = fn () int {
    let result: int = add(5, 3);
    outputln("5 + 3 = ", result);
    
    greet();
    print_number(42);
    
    return 0;
}
```

### Function Parameters

Parameters are passed by value by default:

```luma
const modify = fn (x: int) void {
    x = 100;  // Modifies local copy only
};

const main = fn () int {
    let num: int = 10;
    modify(num);
    outputln(num);  // Still 10
    return 0;
}
```

To modify the caller's variable, use pointers:

```luma
const modify_ptr = fn (x: *int) void {
    *x = 100;  // Modifies original value
};

const main = fn () int {
    let num: int = 10;
    modify_ptr(&num);  // Pass address
    outputln(num);     // Now 100
    return 0;
}
```

### Return Values

```luma
// Single return value
const square = fn (x: int) int {
    return x * x;
};

// Multiple return values via struct
const DivResult = struct {
    quotient: int,
    remainder: int
};

const divide = fn (a: int, b: int) DivResult {
    return DivResult {
        quotient: a / b,
        remainder: a % b
    };
};

const main = fn () int {
    let result: DivResult = divide(17, 5);
    outputln("17 / 5 = ", result.quotient, " R ", result.remainder);
    return 0;
}
```

### Early Returns

```luma
const find_positive = fn (numbers: *int, size: int) int {
    loop [i: int = 0](i < size) : (++i) {
        if (numbers[i] > 0) {
            return numbers[i];  // Early return
        }
    }
    return -1;  // Not found
};
```

---

## Name Resolution

Luma uses two distinct operators for name resolution to provide semantic clarity:

### Static Access with `::`

The `::` operator is used for **compile-time static access**:

```luma
// Enum variants
const day: WeekDay = WeekDay::Monday;

// Module/namespace access
math::sqrt(16.0)

// Associated functions (if added later)
Point::new(10, 20)
```

### Runtime Member Access with `.`

The `.` operator is used for **runtime member access**:

```luma
// Struct field access
let point: Point = Point { x: 10, y: 20 };
outputln(point.x);  // Access field at runtime

// Method calls on instances
let distance: float = origin.distance_to(destination);

// Generic struct field access
let box: Box<int> = Box { value: 42 };
outputln(box.value);
```

### Benefits of This Distinction

- **Semantic clarity**: `::` means "resolved at compile time", `.` means "accessed at runtime"
- **Easier parsing**: The compiler immediately knows the access type
- **Consistent with systems languages**: Similar to C++ and Rust conventions
- **Future-proof**: Supports advanced features like associated functions

---

## Control Flow

Luma provides clean, flexible control flow constructs that handle most programming patterns without unnecessary complexity.

### Conditional Statements

Use `if`, `elif`, and `else` for branching logic:

```luma
const x: int = 7;

if (x > 10) {
    outputln("Large number");
} elif (x > 5) {
    outputln("Medium number");  // This will execute
} else {
    outputln("Small number");
}
```

### Loop Constructs

The `loop` keyword provides several iteration patterns:

#### For-Style Loops

```luma
// Basic for loop
loop [i: int = 0](i < 10) {
    outputln("Iteration: ", i);
    ++i;
}

// For loop with post-increment
loop [i: int = 0](i < 10) : (++i) {
    outputln("i = ", i);
}

// Multiple loop variables
loop [i: int = 0, j: int = 0](i < 10) : (++i) {
    outputln("i = ", i, ", j = ", j);
    ++j;
}
```

#### While-Style Loops

```luma
// Condition-only loop
let counter: int = 0;
loop (counter < 5) {
    outputln("Count: ", counter);
    counter = counter + 1;
}

// While loop with post-action
let j: int = 0;
loop (j < 10) : (++j) {
    outputln("Processing: ", j);
}
```

#### Infinite Loops

```luma
loop {
    // Runs forever until `break` is encountered
    if (should_exit()) {
        break;
    }
    do_work();
}
```

#### Loop Control

```luma
// Break: exit loop early
loop [i: int = 0](i < 100) : (++i) {
    if (i == 50) {
        break;  // Exit loop
    }
    process(i);
}

// Continue: skip to next iteration
loop [i: int = 0](i < 10) : (++i) {
    if (i % 2 == 0) {
        continue;  // Skip even numbers
    }
    outputln("Odd: ", i);
}
```

---

## Switch Statements

Luma provides powerful pattern matching through `switch` statements that work with enums, integers, and other types. Switch statements must be exhaustive and all cases must be compile-time constants.

### Basic Switch Syntax

```luma
@module "main"

const WeekDay = enum {
    Sunday,
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
};

const classify_day = fn (day: WeekDay) void {
    switch (day) {
        WeekDay::Monday, WeekDay::Tuesday, WeekDay::Wednesday, 
        WeekDay::Thursday, WeekDay::Friday =>
            outputln("Weekday => ", day);
        WeekDay::Saturday, WeekDay::Sunday =>
            outputln("Weekend => ", day);
    }
}

pub const main = fn () int {
    classify_day(WeekDay::Monday);   // Output: Weekday => 1
    classify_day(WeekDay::Saturday); // Output: Weekend => 6
    return 0;
}
```

### Switch with Default Case

When you need to handle unexpected values or want a catch-all case, use the default wildcard pattern `_`:

```luma
const handle_status_code = fn (code: int) void {
    switch (code) {
        200 => outputln("OK");
        404 => outputln("Not Found");
        500 => outputln("Internal Server Error");
        _   => outputln("Unknown status code");
    }
};
```

### Switch Features

- **Multiple values per case**: Combine multiple values using commas
- **Exhaustiveness**: All possible values must be covered (or use `_` for default)
- **Compile-time constants**: All case values must be compile-time constants
- **No fallthrough**: Each case is automatically contained (no `break` needed)

---

## Module System

Luma provides a simple module system for code organization and namespace management.

### Module Declaration

Every Luma source file must declare its module name:

```luma
@module "main"

// Your code here...
```

### Importing Modules

Use the `@use` directive to import other modules:

```luma
@module "main"

@use "math" as math
@use "string" as str

const main = fn () int {
    // Access imported functions with namespace
    let result: double = math::sqrt(25.0);
    let len: int = str::strlen("hello");
    
    outputln("Square root: ", result);
    outputln("Length: ", len);
    return 0;
}
```

### Module Features

- **Explicit imports**: All dependencies must be explicitly declared
- **Namespace isolation**: Imported modules are accessed through their aliases
- **Static resolution**: All module access is resolved at compile time using `::`
- **Clean syntax**: Simple `@use "module" as alias` pattern

---

## Built-in Functions

Luma provides several built-in functions that are always available without imports.

### Output Functions

```luma
output(...)      // Print values without newline
outputln(...)    // Print values with newline
```

Both functions are **variadic** - they accept any number of arguments of any type:

```luma
const main = fn () int {
    output("Hello", " ", "World");           // Hello World
    outputln("The answer is:", 42);          // The answer is: 42\n
    
    let x: int = 10;
    let y: float = 3.14;
    outputln("x = ", x, ", y = ", y);       // x = 10, y = 3.14\n
    
    return 0;
}
```

### Input Functions

```luma
input<T>(prompt: *char) -> T    // Read typed input
```

The `input` function is generic and reads a value of the specified type:

```luma
const main = fn () int {
    let name: *char = input<*char>("Enter your name: ");
    let age: int = input<int>("Enter your age: ");
    let height: double = input<double>("Enter height (meters): ");
    
    outputln("Name: ", name);
    outputln("Age: ", age);
    outputln("Height: ", height);
    
    return 0;
}
```

### System Commands

```luma
system(command: *char) -> int    // Execute system command
```

Execute shell commands from your program:

```luma
const main = fn () int {
    system("clear");  // Clear terminal (Linux/Mac)
    system("stty -icanon -echo");  // Configure terminal
    return 0;
}
```

### Type Information

```luma
sizeof<T> -> int    // Size of type in bytes
```

Get the size of any type at compile time:

```luma
const main = fn () int {
    outputln("int: ", sizeof<int>);           // 8
    outputln("char: ", sizeof<char>);         // 1
    outputln("double: ", sizeof<double>);     // 8
    
    // Use in allocations
    let buffer: *int = cast<*int>(alloc(100 * sizeof<int>));
    defer free(buffer);
    
    return 0;
}
```

---

## Type Casting System

Luma uses explicit casting with the `cast<T>()` function for all type conversions.

### Basic Syntax

```luma
cast<TargetType>(expression)
```

### Numeric Conversions

```luma
const main = fn () int {
    // Integer to float
    let i: int = 42;
    let f: float = cast<float>(i);        // 42.0
    
    // Float to integer (truncates)
    let pi: double = 3.14159;
    let rounded: int = cast<int>(pi);     // 3
    
    // Between integer types
    let small: char = cast<char>(65);     // 'A'
    let large: int = cast<int>(small);    // 65
    
    return 0;
}
```

### Pointer Casting

```luma
const main = fn () int {
    // void* to typed pointer
    let raw: *void = alloc(sizeof<int>);
    let typed: *int = cast<*int>(raw);
    *typed = 42;
    free(raw);
    
    // Between pointer types
    let int_ptr: *int = cast<*int>(alloc(sizeof<int>));
    let void_ptr: *void = cast<*void>(int_ptr);
    free(int_ptr);
    
    return 0;
}
```

### Pointer to Integer (and back)

```luma
const main = fn () int {
    let ptr: *char = cast<*char>(alloc(10));
    defer free(ptr);
    
    // Pointer to integer
    let addr: int = cast<int>(ptr);
    
    // Add offset (pointer arithmetic)
    let offset_addr: int = addr + 5;
    
    // Back to pointer
    let offset_ptr: *char = cast<*char>(offset_addr);
    
    return 0;
}
```

---

## Array Types

Luma supports fixed-size arrays with compile-time known sizes.

### Array Declaration

```luma
// Syntax: [Type; Size]
let numbers: [int; 10];           // Array of 10 integers
let chars: [char; 256];           // Array of 256 characters
let buffer: [double; 100];        // Array of 100 doubles

// Constants can be arrays too
const PRIMES: [int; 5] = [2, 3, 5, 7, 11];
```

### Array Initialization

```luma
const main = fn () int {
    // Uninitialized (contains garbage)
    let data: [int; 5];
    
    // Initialize with literal
    let primes: [int; 5] = [2, 3, 5, 7, 11];
    
    // Initialize element by element
    let scores: [int; 3];
    scores[0] = 95;
    scores[1] = 87;
    scores[2] = 92;
    
    return 0;
}
```

### Array Access

```luma
const main = fn () int {
    let numbers: [int; 5] = [10, 20, 30, 40, 50];
    
    // Read elements
    let first: int = numbers[0];    // 10
    let last: int = numbers[4];     // 50
    
    // Write elements
    numbers[2] = 99;
    
    // Loop through array
    loop [i: int = 0](i < 5) : (++i) {
        outputln("numbers[", i, "] = ", numbers[i]);
    }
    
    return 0;
}
```

---

## String Literals and String Types

### String Literals

String literals are null-terminated character arrays:

```luma
const main = fn () int {
    // String literal - type is *char
    let message: *char = "Hello, World!";
    outputln(message);
    
    return 0;
}
```

### Character Literals

Single characters use single quotes:

```luma
const main = fn () int {
    let letter: char = 'A';           // Character literal
    let newline: char = '\n';         // Escape sequence
    let tab: char = '\t';             // Tab character
    
    return 0;
}
```

### Escape Sequences

```luma
'\n'   // Newline
'\r'   // Carriage return
'\t'   // Horizontal tab
'\\'   // Backslash
'\''   // Single quote
'\"'   // Double quote
'\0'   // Null character
'\xHH' // Hexadecimal byte (e.g., '\x1b' for ESC)
```

---

## Pointer Arithmetic

Luma supports pointer arithmetic for low-level memory manipulation.

### Basic Pattern

```luma
const main = fn () int {
    let arr: *int = cast<*int>(alloc(5 * sizeof<int>));
    defer free(arr);
    
    // Initialize
    loop [i: int = 0](i < 5) : (++i) {
        arr[i] = i * 10;
    }
    
    // Pointer arithmetic: convert to int, add offset, convert back
    let addr: int = cast<int>(arr);
    let new_addr: int = addr + (2 * sizeof<int>);
    let new_ptr: *int = cast<*int>(new_addr);
    
    outputln(*new_ptr);  // arr[2] = 20
    
    return 0;
}
```

---

## Visibility and Access Control

### Public Module Members

Use `pub` to export items from a module:

```luma
@module "math"

// Public - accessible from other modules
pub const PI: double = 3.14159265359;

pub const sqrt = fn (x: double) double {
    return x;
}

// Private - only within this module
const INTERNAL_CONSTANT: int = 42;
```

### Struct Access Control

```luma
const Person = struct {
pub:
    name: *char,
    age: int,
    
priv:
    ssn: *char,
    internal_id: int
};
```

---

## Memory Management

Luma provides explicit memory management with safety-oriented features.

### Basic Memory Operations

```luma
alloc(size: int) -> *void    // Allocate memory
free(ptr: *void)             // Deallocate memory
sizeof<T> -> int             // Size of type
```

### Example Usage

```luma
const main = fn () int {
    // Allocate memory
    let ptr: *int = cast<*int>(alloc(sizeof<int>));
    
    // Use the memory
    *ptr = 42;
    outputln("Value: ", *ptr);
    
    // Clean up
    free(ptr);
    return 0;
}
```

### The `defer` Statement

Ensure cleanup with `defer` statements that execute when leaving scope:

```luma
const process_data = fn () void {
    let buffer: *int = cast<*int>(alloc(100 * sizeof<int>));
    defer free(buffer);  // Guaranteed to run when function exits
    
    let file: *File = open_file("data.txt");
    defer close_file(file);  // Will run even if early return
    
    // Complex processing...
    if (error_condition) {
        return; // defer statements still execute
    }
    
    // More processing...
    // defer statements execute here automatically
}
```

Multiple statements can be deferred:

```luma
defer {
    close_file(file);
    cleanup_resources();
    log("Operation completed");
}
```

**Key Benefits:**
- Ensures cleanup code runs regardless of how the function exits
- Keeps allocation and deallocation code close together
- Prevents resource leaks from early returns
- Executes in reverse order (LIFO - Last In, First Out)

### Ownership Transfer Attributes

Luma provides function attributes that document and enforce ownership semantics.

#### `#returns_ownership`

Marks functions that allocate and return pointers:

```luma
#returns_ownership
const create_buffer = fn (size: int) *int {
    let buffer: *int = cast<*int>(alloc(size * sizeof<int>));
    return buffer;  // Caller now owns this memory
}

const main = fn () int {
    let data: *int = create_buffer(100);
    defer free(data);  // Caller must free
    return 0;
}
```

#### `#takes_ownership`

Marks functions that take ownership of pointer arguments:

```luma
#takes_ownership
const consume_buffer = fn (buffer: *int) void {
    outputln("Processing: ", *buffer);
    free(buffer);  // Function owns and frees the buffer
}

const main = fn () int {
    let data: *int = cast<*int>(alloc(sizeof<int>));
    *data = 42;
    
    consume_buffer(data);  // Ownership transferred
    // ❌ Error: data was freed inside consume_buffer
    
    return 0;
}
```

### Static Memory Analysis

Luma's compiler includes a static analyzer that tracks memory at compile time:

- **Allocation/Deallocation Pairs**: Ensures every `alloc()` has a corresponding `free()`
- **Double-Free Detection**: Prevents freeing the same pointer twice
- **Memory Leaks**: Identifies allocated memory that's never freed
- **Use-After-Free**: Detects access to freed memory
- **Ownership Transfer**: Monitors ownership through attributes

**Example:**

```luma
const good_memory_usage = fn () void {
    let ptr: *int = cast<*int>(alloc(sizeof<int>));
    defer free(ptr);  // ✅ Analyzer confirms cleanup
    *ptr = 42;
}

const bad_memory_usage = fn () void {
    let ptr: *int = cast<*int>(alloc(sizeof<int>));
    *ptr = 42;
    // ❌ Compiler error: memory leak - ptr never freed
}
```

---

## Error Handling

Luma doesn't have exceptions. Use these patterns:

### Pattern 1: Error Return Codes

```luma
const SUCCESS: int = 0;
const ERROR_NULL_PTR: int = -1;
const ERROR_OUT_OF_BOUNDS: int = -2;

const safe_divide = fn (a: int, b: int, result: *int) int {
    if (b == 0) {
        return ERROR_OUT_OF_BOUNDS;
    }
    
    if (result == cast<*int>(0)) {
        return ERROR_NULL_PTR;
    }
    
    *result = a / b;
    return SUCCESS;
}

const main = fn () int {
    let quotient: int;
    let status: int = safe_divide(10, 2, &quotient);
    
    if (status != SUCCESS) {
        outputln("Error: ", status);
        return 1;
    }
    
    outputln("Result: ", quotient);
    return 0;
}
```

### Pattern 2: Nullable Pointers

```luma
const find_element = fn (arr: *int, size: int, value: int) *int {
    loop [i: int = 0](i < size) : (++i) {
        if (arr[i] == value) {
            let addr: int = cast<int>(arr) + (i * sizeof<int>);
            return cast<*int>(addr);
        }
    }
    return cast<*int>(0);  // Not found - return null
}

const main = fn () int {
    let numbers: [int; 5] = [10, 20, 30, 40, 50];
    let found: *int = find_element(cast<*int>(&numbers), 5, 30);
    
    if (found == cast<*int>(0)) {
        outputln("Not found");
    } else {
        outputln("Found: ", *found);
    }
    
    return 0;
}
```

### Pattern 3: Result Struct

```luma
const Result = struct {
    success: bool,
    value: int,
    error_code: int
};

const safe_operation = fn (x: int) Result {
    if (x < 0) {
        return Result {
            success: false,
            value: 0,
            error_code: -1
        };
    }
    
    return Result {
        success: true,
        value: x * 2,
        error_code: 0
    };
}

const main = fn () int {
    let result: Result = safe_operation(-5);
    
    if (!result.success) {
        outputln("Error: ", result.error_code);
        return 1;
    }
    
    outputln("Result: ", result.value);
    return 0;
}
```

### Best Practices

**Always check return values:**
```luma
// Bad
let ptr: *void = alloc(size);
*cast<*int>(ptr) = 42;  // Might crash if allocation failed

// Good
let ptr: *void = alloc(size);
if (ptr == cast<*void>(0)) {
    return ERROR_ALLOCATION;
}
defer free(ptr);
*cast<*int>(ptr) = 42;
```

**Use consistent error codes:**
```luma
const SUCCESS: int = 0;
const ERR_NULL_PTR: int = -1;
const ERR_OUT_OF_BOUNDS: int = -2;
const ERR_ALLOCATION: int = -3;
```

---

## Performance

Understanding performance is crucial for systems programming.

### Zero-Cost Abstractions

Luma follows the "zero-cost abstraction" principle: abstractions should have no runtime overhead.

**Generics are zero-cost:**
```luma
const add = fn<T>(a: T, b: T) T {
    return a + b;
}

// These calls compile to separate, optimized functions:
let x: int = add<int>(1, 2);        // Same as: x = 1 + 2
let y: float = add<float>(1.0, 2.0); // Same as: y = 1.0 + 2.0
```

**No runtime dispatch** - all generic instantiations are resolved at compile time through monomorphization.

### Monomorphization

Luma generates specialized code for each type:

```luma
const max = fn<T>(a: T, b: T) T {
    if (a > b) { return a; }
    return b;
}

// Compiler generates:
// max_int(a: int, b: int) -> int { ... }
// max_float(a: float, b: float) -> float { ... }
```

**Benefits:**
- No runtime overhead
- Full optimization per type
- No vtables or dynamic dispatch

**Trade-offs:**
- Larger binary size (one copy per type)
- Longer compilation time

### Memory Layout

**Struct layout is predictable:**
```luma
const Point = struct {
    x: int,    // Offset 0, 8 bytes
    y: int     // Offset 8, 8 bytes
};  // Total: 16 bytes
```

**Array layout is contiguous:**
```luma
let arr: [int; 10];  // 80 contiguous bytes
// arr[0] at offset 0, arr[1] at offset 8, arr[2] at offset 16...
```

### Memory Allocation Performance

**Stack allocation is fast:**
```luma
const fast_function = fn () void {
    let buffer: [int; 1024];  // Stack allocated - instant
    // Use buffer...
}  // Automatically cleaned up
```

**Heap allocation has overhead:**
```luma
const slower_function = fn () void {
    let buffer: *int = cast<*int>(alloc(1024 * sizeof<int>));
    defer free(buffer);
    // Use buffer...
}
```

### Optimization Guidelines

**1. Prefer stack allocation when possible:**
```luma
let temp: [int; 100];  // Good for small, fixed-size data
```

**2. Minimize pointer indirection:**
```luma
// Better: direct access
let ptr: *int;
let value: int = *ptr;   // One memory load

// Best: value directly
let value2: int = 42;     // No memory load
```

**3. Batch operations:**
```luma
// Good: one large allocation
let buffer: *int = cast<*int>(alloc(1000 * sizeof<int>));
loop [i: int = 0](i < 1000) : (++i) {
    // Use buffer[i]...
}
free(buffer);
```

**4. Avoid unnecessary copying:**
```luma
// Bad: pass large struct by value
const process = fn (data: LargeStruct) void { }

// Good: pass by pointer
const process_fast = fn (data: *LargeStruct) void { }
```

### Performance Summary

| Operation | Cost | Notes |
|-----------|------|-------|
| Stack variable | ~0 | Instant |
| Heap allocation | High | System call |
| Pointer dereference | Low | One memory access |
| Array index | Low | Bounds check + access |
| Function call | Low-Medium | Depends on size |
| Generic instantiation | 0 | Compile-time only |
| Struct field access | Low | Offset calculation |
| Enum comparison | ~0 | Integer comparison |

---

## Standard Library

Luma's standard library provides essential functionality for systems programming.

### Module: `math`

Mathematical operations and constants.

```luma
@use "math" as math

// Constants
math::PI           // 3.14159...
math::TWO_PI       // 6.28318...
math::HALF_PI      // 1.57079...

// Arithmetic
math::add(x, y)
math::subtract(x, y)
math::multiply(x, y)
math::divide(x, y)
math::mod(x, y)
math::power(base, exponent)

// Min/Max
math::max_size(a, b)
math::min_size(a, b)

// Trigonometry
math::sin(angle)
math::cos(angle)
math::tan(angle)
math::sec(angle)
math::csc(angle)
math::cot(angle)

// Other
math::fib(n, a, b)  // Fibonacci
```

**Example:**
```luma
@use "math" as math

const main = fn () int {
    let angle: double = math::PI / 4.0;
    let sine: double = math::sin(angle);
    outputln("sin(π/4) = ", sine);
    return 0;
}
```

### Module: `memory`

Low-level memory operations.

```luma
@use "memory" as mem

// Basic operations
mem::memcpy(dest, src, n)       // Copy memory
mem::memcmp(a, b, n)            // Compare memory
mem::memset(dest, value, n)     // Fill memory
mem::memmove(dest, src, n)      // Move (handles overlap)
mem::memzero(dest, n)           // Zero memory

// Search
mem::memchr(ptr, value, n)      // Find byte

// Allocation helpers
mem::calloc(count, size)        // Allocate + zero
mem::realloc(ptr, new_size)     // Reallocate

// Utilities
mem::memswap(a, b, n)           // Swap regions
mem::memrev(ptr, n)             // Reverse bytes
mem::memcount(ptr, value, n)    // Count occurrences
mem::memdup(src, n)             // Duplicate region
mem::memeq(a, b, n)             // Check equality
```

**Example:**
```luma
@use "memory" as mem

const main = fn () int {
    let buffer: *void = mem::calloc(10, sizeof<char>);
    defer free(buffer);
    
    mem::memset(buffer, 65, 10);  // Fill with 'A'
    outputln("Buffer filled");
    
    return 0;
}
```

### Module: `string`

String manipulation functions.

```luma
@use "string" as string

// Creation
string::from_char(c)            // Create string from char
string::from_int(n)             // Convert int to string

// Measurement
string::strlen(s)               // Get length

// Comparison
string::strcmp(s1, s2)          // Compare strings

// Search
string::s_char(s, c)            // Find character

// Manipulation
string::copy(dest, src)         // Copy string
string::n_copy(dest, src, n)    // Copy n characters
string::cat(dest, s1, s2)       // Concatenate
```

**Example:**
```luma
@use "string" as string

const main = fn () int {
    let name: *char = "Alice";
    let len: int = string::strlen(name);
    outputln("Length: ", len);
    
    let num_str: *char = string::from_int(42);
    defer free(num_str);
    outputln("Number: ", num_str);
    
    return 0;
}
```

### Module: `termfx`

Terminal formatting and colors (ANSI escape codes).

```luma
@use "termfx" as fx

// Basic colors
fx::RED, fx::GREEN, fx::BLUE, fx::YELLOW
fx::MAGENTA, fx::CYAN, fx::WHITE, fx::BLACK

// Bright colors
fx::BRIGHT_RED, fx::BRIGHT_GREEN, fx::BRIGHT_BLUE

// Background colors
fx::BG_RED, fx::BG_GREEN, fx::BG_BLUE

// Text styles
fx::BOLD, fx::UNDERLINE, fx::ITALIC

// Screen control
fx::CLEAR_SCREEN
fx::CLEAR_LINE
fx::CURSOR_HOME
fx::CURSOR_HIDE
fx::CURSOR_SHOW

// Functions
fx::fg_rgb(r, g, b)          // Custom foreground color
fx::bg_rgb(r, g, b)          // Custom background color
fx::move_cursor(row, col)    // Move cursor

// Reset
fx::RESET
```

**Example:**
```luma
@use "termfx" as fx

const main = fn () int {
    output(fx::CLEAR_SCREEN, fx::CURSOR_HOME);
    output(fx::BOLD, fx::RED, "ERROR: ", fx::RESET);
    outputln("Something went wrong!");
    output(fx::GREEN, "✓ Success", fx::RESET, "\n");
    return 0;
}
```

### Module: `terminal`

Interactive terminal input functions.

```luma
@use "terminal" as term

term::getch()              // Get single char (no echo, no enter)
term::getch_silent()       // Get char silently
term::getche()             // Get char with echo
term::kbhit()              // Check if key pressed
term::wait_for_key()       // Wait for any key
term::clear_input_buffer() // Clear input buffer
term::getpass(prompt)      // Get password (hidden input)
```

**Example:**
```luma
@use "terminal" as term
@use "string" as string

const main = fn () int {
    outputln("Press any key...");
    let key: char = term::getch();
    outputln("You pressed: ", string::from_char(key));
    
    let password: *char = term::getpass("Enter password: ");
    defer free(password);
    outputln("Password entered");
    
    return 0;
}
```

### Creating Your Own Modules

**Module structure:**
```luma
// File: mymodule.lx
@module "mymodule"

// Private helper
const helper = fn (x: int) int {
    return x * 2;
}

// Public function
pub const process = fn (data: *int, size: int) void {
    loop [i: int = 0](i < size) : (++i) {
        data[i] = helper(data[i]);
    }
}

// Public constant
pub const VERSION: int = 1;
```

**Using your module:**
```luma
// File: main.lx
@module "main"

@use "mymodule" as mm

const main = fn () int {
    let data: [int; 5] = [1, 2, 3, 4, 5];
    mm::process(cast<*int>(&data), 5);
    outputln("Version: ", mm::VERSION);
    return 0;
}
```

---

## Safety Features

Luma provides several safety features to prevent common bugs:

### 1. Static Memory Analysis

- Tracks allocations and deallocations at compile time
- Detects memory leaks before runtime
- Prevents double-free errors
- Identifies use-after-free bugs

### 2. Ownership Attributes

- `#returns_ownership` documents memory transfers
- `#takes_ownership` clarifies responsibility
- Compiler enforces ownership rules

### 3. Defer Statements

- Guarantees cleanup code execution
- Prevents resource leaks
- Works with early returns and errors

### 4. Strong Type System

- No implicit conversions (except safe ones)
- Explicit casting required
- Type-safe generics
- Enum exhaustiveness checking

### 5. Explicit Error Handling

- No hidden exceptions
- Clear error propagation
- Predictable control flow

---

## Quick Reference

### Keywords

```
const      let        if         elif       else
loop       break      continue   return     defer
struct     enum       pub        priv       cast
sizeof     alloc      free       switch     fn
```

### Operators

```
Arithmetic:  +  -  *  /  %  ++  --
Comparison:  ==  !=  <  >  <=  >=
Logical:     &&  ||  !
Bitwise:     &  |  ^  ~  <<  >>
Assignment:  =
Access:      .   ::  []  *  &
```

### Primitive Types

```
int     double    bool    *T      [T; N]
uint    float     char    str     void
```

### Common Patterns

```luma
// Allocation with cleanup
let ptr: *T = cast<*T>(alloc(sizeof<T>));
defer free(ptr);

// Array iteration
loop [i: int = 0](i < size) : (++i) {
    array[i] = value;
}

// Error checking
if (ptr == cast<*T>(0)) {
    return ERROR_CODE;
}

// Module usage
@use "module" as m
let x: int = m::function();

// String operations
let s: *char = string::from_int(42);
defer free(s);
```

---

## Conclusion

Luma is a modern systems programming language that provides:

- **Simplicity**: Clean, consistent syntax
- **Safety**: Static analysis and ownership tracking
- **Performance**: Zero-cost abstractions and predictable behavior
- **Control**: Manual memory management with safety nets

The language is designed for programmers who want the performance and control of C with modern safety features and ergonomics.

For more examples, see the standard library modules and test files included with the language distribution.