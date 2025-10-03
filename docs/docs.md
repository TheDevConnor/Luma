# Luma Language Documentation

Luma is a statically typed, compiled programming language designed for systems programming. It combines the low-level control of C with a strong type system and modern safety features that eliminate many common runtime errors.

## Table of Contents

- [Language Philosophy](#language-philosophy)
- [Quick Start](#quick-start)
- [Type System](#type-system)
- [Generics](#generics)
- [Top-Level Bindings with `const`](#top-level-bindings-with-const)
- [Name Resolution](#name-resolution)
- [Control Flow](#control-flow)
- [Switch Statements](#switch-statements)
- [Module System](#module-system)
- [Memory Management](#memory-management)
- [Error Handling](#error-handling)
- [Performance](#performance)
- [Safety Features](#safety-features)

## Language Philosophy

Luma is built on three core principles:

- **Simplicity**: Minimal syntax with consistent patterns
- **Safety**: Strong typing and memory safety features
- **Performance**: Zero-cost abstractions and predictable performance

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
        return sqrt(cast<float>(dx * dx + dy * dy));
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
        Status::Active: outputln("System is running");
        Status::Inactive: outputln("System is stopped");
        Status::Pending: outputln("System is starting");
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
    if a > b {
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
    let int_box: Box<int> = Box<int> { value: 42 };
    outputln("Box contains: ", int_box.get());
    
    // Box holding a float
    let float_box: Box<float> = Box<float> { value: 3.14 };
    
    // Pair with different types
    let pair: Pair<int, str> = Pair<int, str> { 
        first: 1, 
        second: "hello" 
    };
    outputln("Pair: (", pair.first, ", ", pair.second, ")");
    
    return 0;
}
```

### Multiple Type Parameters

Generic functions and structs can have multiple type parameters:

```luma
const convert = fn<From, To>(value: From) To {
    return cast<To>(value);
}

const Tuple = struct<T, U, V> {
    first: T,
    second: U,
    third: V
};
```

### Generic Arrays and Collections

Generics are particularly useful for building collection types:

```luma
const DynamicArray = struct<T> {
    data: *T,
    size: uint,
    capacity: uint,
    
    push = fn(item: T) void {
        // Implementation for adding items
    },
    
    get = fn(index: uint) T {
        return data[index];
    }
};

const main = fn() int {
    let numbers: DynamicArray<int> = DynamicArray<int> {
        data: null,
        size: 0,
        capacity: 0
    };
    
    numbers.push(10);
    numbers.push(20);
    numbers.push(30);
    
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

### Design Considerations

**Explicit type arguments**: Luma requires explicit type arguments at call sites (`add<int>(1, 2)`) rather than type inference. This makes code more readable and predictable, avoiding "magic" type deduction.

**Angle bracket syntax**: Type parameters use `<>` brackets, consistent with type casting syntax (`cast<T>`) and familiar from other systems languages.

**No partial specialization**: Currently, Luma generics do not support partial specialization or constraints (traits/concepts). This keeps the system simple while still covering most use cases.

**Compile-time only**: All generic instantiations happen at compile time. There is no runtime polymorphism or dynamic dispatch with generics.

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
let box: Box<int> = Box<int> { value: 42 };
outputln(box.value);
```

### Benefits of This Distinction

- **Semantic clarity**: `::` means "resolved at compile time", `.` means "accessed at runtime"
- **Easier parsing**: The compiler immediately knows the access type
- **Consistent with systems languages**: Similar to C++ and Rust conventions
- **Future-proof**: Supports advanced features like associated functions

## Control Flow

Luma provides clean, flexible control flow constructs that handle most programming patterns without unnecessary complexity.

### Conditional Statements

Use `if`, `elif`, and `else` for branching logic:

```luma
const x: int = 7;

if x > 10 {
    print("Large number");
} elif x > 5 {
    print("Medium number");  // This will execute
} else {
    print("Small number");
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
    counter++;
}

// While loop with post-action
let j: int = 0;
loop (j < 10) : (j++) {
    outputln("Processing: ", j);
}
```

#### Infinite Loops

```luma
loop {
    // Runs forever until `break` is encountered
    if should_exit() {
        break;
    }
    do_work();
}
```

## Switch Statements

Luma provides powerful pattern matching through `switch` statements that work with enums, integers, and other types. Switch statements must be exhaustive and all cases must be compile-time constants.

### Basic Switch Syntax

```luma
const WeekDay = enum {
    Sunday,
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
};

const classify_day = fn (day: WeekDay) {
    switch (day) {
        WeekDay::Monday, WeekDay::Tuesday, WeekDay::Wednesday, 
        WeekDay::Thursday, WeekDay::Friday: 
            outputln("Weekday");
        WeekDay::Saturday, WeekDay::Sunday: 
            outputln("Weekend");
    }
};
```

### Switch with Default Case

When you need to handle unexpected values or want a catch-all case, use the default wildcard pattern `_`:

```luma
const handle_status_code = fn (code: int) {
    switch (code) {
        200: outputln("OK");
        404: outputln("Not Found");
        500: outputln("Internal Server Error");
        _: outputln("Unknown status code");
    }
};
```

### Switch Features

- **Multiple values per case**: Combine multiple values using commas
- **Exhaustiveness**: All possible values must be covered (or use `_` for default)
- **Compile-time constants**: All case values must be compile-time constants
- **No fallthrough**: Each case is automatically contained (no `break` needed)

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
@use "fib" as fibonacci

const main = fn () int {
    // Access imported functions with namespace
    let result: float = math::sqrt(25.0);
    let fib_num: int = fibonacci::fib(10);
    
    outputln("Square root: ", result);
    outputln("Fibonacci: ", fib_num);
    return 0;
}
```

### Module Features

- **Explicit imports**: All dependencies must be explicitly declared
- **Namespace isolation**: Imported modules are accessed through their aliases
- **Static resolution**: All module access is resolved at compile time using `::`
- **Clean syntax**: Simple `@use "module" as alias` pattern

### Example: Math Module

```luma
// File: math.lx
@module "math"

pub const PI: float = 3.14159265359;

pub const sqrt = fn (x: float) float {
    // Implementation here
    return x;  // Placeholder
};

pub const pow = fn (base: float, exp: float) float {
    // Implementation here
    return base;  // Placeholder
};
```

```luma
// File: main.lx
@module "main"
@use "math" as math

const main = fn () int {
    let radius: float = 5.0;
    let area: float = math::PI * math::pow(radius, 2.0);
    outputln("Circle area: ", area);
    return 0;
}
```

## Memory Management

Luma provides explicit memory management with safety-oriented features. While manual, it includes compile-time static analysis and runtime tools to prevent common memory errors like leaks, double-frees, and use-after-free bugs.

### Basic Memory Operations

```luma
alloc(size: uint) -> *void    // Allocate memory
free(ptr: *void)              // Deallocate memory
cast<T>(ptr: *void) -> *T     // Type casting
sizeof(type) -> uint          // Size of type in bytes
memcpy(dest: *void, src: *void, size: uint)  // Memory copy
```

### Example Usage

```luma
const main = fn () int {
    // Allocate memory for an integer
    let ptr: *int = cast<*int>(alloc(sizeof(int)));
    
    // Use the memory
    *ptr = 42;
    outputln("Value: ", *ptr);
    
    // Clean up
    free(ptr);
    return 0;
}
```

### The `defer` Statement

To prevent memory leaks and ensure cleanup, Luma provides `defer` statements that execute when leaving the current scope:

```luma
const process_data = fn () {
    let buffer: *int = cast<*int>(alloc(sizeof(int) * 100));
    defer free(buffer);  // Guaranteed to run when function exits
    
    let file: *File = open_file("data.txt");
    defer close_file(file);  // Will run even if early return
    
    // Complex processing...
    if error_condition {
        return; // defer statements still execute
    }
    
    // More processing...
    // defer statements execute here automatically
}
```

You can also defer multiple statements:

```luma
defer {
    close_file(file);
    cleanup_resources();
    log("Operation completed");
}
```

**Key Benefits of `defer`:**
- Ensures cleanup code runs regardless of how the function exits
- Keeps allocation and deallocation code close together
- Prevents resource leaks from early returns or error conditions
- Executes in reverse order (LIFO - Last In, First Out)

### Size Queries

```luma
const check_sizes = fn () {
    outputln("int size: ", sizeof(int));        // 8 bytes
    outputln("Point size: ", sizeof(Point));    // 16 bytes
    outputln("Direction size: ", sizeof(Direction)); // 4 bytes
    outputln("Box<int> size: ", sizeof(Box<int>));   // Size of int
}
```

### Static Memory Analysis

Luma's compiler includes a powerful static analyzer that tracks memory allocations and deallocations at compile time. This helps catch memory management errors before your program runs.

#### What the Analyzer Tracks

- **Allocation/Deallocation Pairs**: Ensures every `alloc()` has a corresponding `free()`
- **Double-Free Detection**: Prevents freeing the same pointer multiple times
- **Memory Leaks**: Identifies allocated memory that's never freed
- **Variable-Level Tracking**: Associates allocations with specific variable names
- **Cross-Function Analysis**: Tracks memory through function calls and returns

#### Example: Static Analysis in Action

```luma
const good_memory_usage = fn () {
    let ptr: *int = cast<*int>(alloc(sizeof(int)));
    defer free(ptr);  // ✅ Analyzer sees this will always execute
    
    *ptr = 42;
    outputln("Value: ", *ptr);
    // ✅ Analyzer confirms ptr is freed on function exit
}

const problematic_memory_usage = fn () {
    let buffer: *int = cast<*int>(alloc(sizeof(int) * 100));
    
    if some_condition {
        free(buffer);
        return;  // ✅ Early return is fine, buffer was freed
    }
    
    process_data(buffer);
    // ❌ Compiler error: potential memory leak
    //     buffer not freed on all code paths
}

const double_free_example = fn () {
    let data: *int = cast<*int>(alloc(sizeof(int)));
    *data = 42;
    
    free(data);
    if error_condition {
        free(data);  // ❌ Compiler error: double free detected
                     //     Variable 'data' already freed at line X
    }
}
```

#### Memory Analysis Features

**Variable-level tracking**: The analyzer associates each allocation with the specific variable that holds the pointer, enabling precise error reporting.

**Flow-sensitive analysis**: The analyzer understands control flow and can track memory state through branches, loops, and function calls.

**Integration with `defer`**: The analyzer recognizes that `defer` statements provide guaranteed cleanup, making them the preferred pattern for memory management.

**Precise error reporting**: Memory issues are reported with exact line numbers, variable names, and helpful suggestions for fixes.

#### Compiler Messages

The analyzer provides clear, actionable error messages with precise source locations:

```
Error: Memory leak detected
  --> main.lx:15:5
   |
15 |     let buffer: *int = cast<*int>(alloc(sizeof(int) * 100));
   |         ^^^^^^ allocated here
...
23 |     return;
   |     ^^^^^^ buffer not freed on this path
   |
Note: This allocation has no corresponding free()
Help: Add a free() call before the variable goes out of scope

Error: Double free detected
  --> main.lx:18:5
   |
18 |     free(data);
   |     ^^^^^^^^^^ double free detected here
   |
Note: Memory was already freed previously
Help: Remove the duplicate free() call or check your control flow
```

#### Best Practices for Static Analysis

1. **Use `defer` for cleanup**: This guarantees the analyzer can verify proper cleanup
2. **Keep allocation scope small**: Allocate and free in the same function when possible  
3. **Explicit null checks**: Use explicit null checks rather than assuming validity
4. **Document ownership**: Use clear variable names and comments about ownership transfer

```luma
const recommended_pattern = fn () {
    let buffer: *int = cast<*int>(alloc(sizeof(int) * 100));
    defer free(buffer);  // Guaranteed cleanup - analyzer approves ✅
    
    if buffer == null {  // Explicit null check
        outputln("Allocation failed");
        return;  // defer still runs, no leak
    }
    
    // Use buffer safely...
    process_data(buffer);
    // Analyzer tracks that buffer will be freed via defer
}

const tracking_example = fn () {
    // Analyzer associates allocation with variable 'data'
    let data: *int = cast<*int>(alloc(sizeof(int)));
    *data = 42;
    
    if should_cleanup {
        free(data);  // Analyzer marks 'data' as freed
        data = null; // Good practice: nullify freed pointer
    }
    // If should_cleanup is false, analyzer will report leak for 'data'
}
```

This static analysis system makes manual memory management in Luma much safer than traditional C, while still giving you full control over allocation patterns and performance characteristics.

## Error Handling

*[This section would contain information about Luma's error handling - to be documented]*

## Performance

*[This section would contain information about Luma's performance characteristics - to be documented]*

## Safety Features

*[This section would contain information about Luma's safety features - to be documented]*
