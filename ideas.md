# Fixing C

- context free grammar
- no preprocessor
- strong typing
- no declaration order
- defined fixed sized primitives
- replace unsigned with natural that can overflow
- no integral promotion
- checked integer arithmetic
- bit array primitive, don't conflate bit operations with integers
- texts literals not null terminated
- remove varargs
- named function parameters
- defined expression evaluation order
- remove comma operator
- product type with undefined layout
- object sizes can be 0
- remove pointer arithmetic
- arrays have value semantics, remove array implicit conversion to pointer
- modules, including modulemap
- (probably many more, maybe starting with C not a great idea)

## NOTE: for runtime constants on switch cases

```txt
LLVM IRError: Case values must be compile-time constants
Error: Case value must be a compile-time constant
Basic Block in function 'main' does not have terminator!
label %switch_end
LLVM ERROR: Broken module found, compilation aborted!
```

## Linked List Ideas

```luma
;; Syntax will change on somethins
const Link = struct {
    tag: int,
    value = union {
        .nil = struct {},
        .node = struct {
            value: int,
            next: *Link
        }
    }
};

const link_list_length_rec = fn (list: *Link) int {
    if (list.tag == nil) {
        return 0;
    } else {
        return 1 + link_list_length_rec(list.value.node.next);
    }
};

const link_list_length_tail = fn (list: *Link, acc: int) int {
    loop [acc: int = 0, lt: *Link = list, i:int = 0](true) {
        if (lt.tag == nil) return acc;
        else {
            continue[lt.value.node.next, acc + 1, _]; ;;Underscore does not change the value
        }
    }
};

const link_list_length = fn (list: *Link) int {
    return link_list_length_tail(list, 0);
};
```

## C Interoperability in Luma

## Overview

This proposal outlines a clean and integrated approach for C interoperability in Luma that leverages the existing module system. C bindings are treated as first-class Luma modules, maintaining consistency with the language's design while providing seamless access to C libraries.

## Core Concept

C headers are converted into Luma modules through automatic binding generation, allowing developers to use C libraries through Luma's standard module import system.

## Generated Binding Modules

```luma
// stdio.lx - Generated binding module
@module "stdio"

extern "C" {
    const FILE: type = opaque;
    const printf = fn (format: *char, ...) int;
    const fopen = fn (filename: *char, mode: *char) *FILE;
    const fclose = fn (stream: *FILE) int;
    const fprintf = fn (stream: *FILE, format: *char, ...) int;
}
```

## Usage in Application Code

```luma
// main.lx
@module "main"
@use "stdio" as io

pub const main = fn () int {
    io.printf("Hello from C!\n");
    
    let file: *io.FILE = io.fopen("test.txt", "w");
    defer io.fclose(file);
    
    io.fprintf(file, "Writing from Luma!\n");
    return 0;
}
```

## impl and its unique features

## simple example

impl [func list...] -> [struct list...] {
    if (1 > 0) {
       define a function one way
    } else {
      define the same function another way
    }
}

## The goals of the impl is to implement functions for structs

## It should have the ability to conditionally make functions

## The functions are `injected` into the struct and can reference

## `self` which is a pointer to the struct AstNode that should already be there

## There should be two types of functions for this instance. Runtime ready, and compile time optional

## with a tag of #runtime, the function will be compiled and optionally ran during runtime

## two instances of the same function can only run one at a time, and must be derived from an expression

## a #compile tag from a function within a conditional will be optionally compiled, and so only one available at runtime

## Why the two? One use case for @run_time is to allow dynamic function assignment. Lets say you must work with an api

## This api does not respond with the same data, same type of data and so on. This means you can write multiple capture() functions

## Yes this is function overloading, but conditionally, and can be programmed for the potential context the appliction will be in

## For @compile_time, it optionally compiles one of the implementations of the function. Say you need portability, you can use the same

## source code and target specific architectures. This can be thought of #IF_WINDOWS bullshit from C, you can conditionaly compile

## one function or another, but in a nice and effecient way

## the ? and None type

someType: ?; # is a None, or a real type.

## Thats what it does. Gives a way to init without a type. Can also be used to identify things. if (someType? == None) {return "Nothing found";}

## It is our solution to NULL, but it provides a more meaning. Because it can also be a value if (someType?) {return "value found";}

## This is very straigh forward in its concept. someType? returns the value inside, or it returns the None type

## the set type

## A set is a fixed sized array of types

## a, b, c : (int, float, char)

## const func1 = fn () (int, float, int) {}

## The same thing

## a, b, c : () = func1

## changes the the position for the returning set

## a, b, c : (float, int, int) = func1

## The () is here to be explicit that we are expecting a set from func1

## Luma Post-Processing and Build System (LPBS)

## LPBS is made in Luma and uses a subset (vars, funcs, if (expr))

// -V2 : verbose level 2 for strictness?
// -OB : object files
cc_o = luma-1 -OB -V2;

// -x86_64 could be used to verify the executable and compatability for translate()
cc = luma-1 -O3 -x86_64;

// the output for artefacts and exe
output = "bin/";

src_files = {
    if (check_dir_exists("src/")) {
       -> ("main.lx", "src/*lx");
    }
    -> ("main.lx");
}

compile:
output("Getting internal and external libraries\n");
get_libs -> () {
        path_ex = find_external("raylib", "opengl");
        path_in = find_internal("std::math", "std::stl::stack", "std::net::network");
        return path_ex, path_in;
}

libs, clibs = get_libs();

get_obj -> (cc, src_files, libs, clibs) {
    output("Getting luma obj files\n");
    obj_files = cc_o src_files -l libs;
    output("Getting external library obj files\Combining all obj files\n");
    return obj_files + translate(clibs);
}
// compiler, obj files, to where
output("Compiling program\n");
compile(cc, get_obj(), output);
output("Compiled, output: ", output);

clean:
clean_all -> (where) {
    remove(output);
    output("Removed all artefacts\n");
}

## compile: and clean: are labels, there are the external commands a user can run (lpbs compile, lpbs clean)

## Lets break it down. Post-Processing is about understanding end context from the src code and a solution

## The LPB System should generate bindings for the end result

## It should manage ffi for C and providing that compatability

## It should provide a way to create, manage, and work with shared libraries or static libraries

## Then it should build the program, it should read in a simp

### Manual Binding Generation

```bash
## Generate bindings from C header
./luma bindgen stdio.h -o std/stdio.lx

## Build with explicit linking
./luma build main.lx -l std/stdio.lx -name main
```

### Automatic Header Detection

```bash
## Build system detects and handles C dependencies
./luma build main.lx -l std/stdio.lx -link-c stdio -name main
```

## Enhanced Import Syntax

For even more streamlined development, direct C header imports could be supported:

```luma
// main.lx
@module "main"
@use "stdio" as io        // Regular Luma module
@use_c "math.h" as math   // Direct C header import (auto-generates bindings)

pub const main = fn () int {
    io.printf("Square root of 16 is: %f\n", math.sqrt(16.0));
    return 0;
}
```

## Comprehensive Standard Library Generation

```bash
## Generate all standard C library bindings at once
./luma bindgen --std-headers -o std/

## This creates:
## std/stdio.lx
## std/stdlib.lx  
## std/math.lx
## std/string.lx
## etc.

## Build with multiple C libraries
./luma build main.lx -l std/stdio.lx -l std/math.lx -name main
```

## Module Metadata for Automatic Linking

Binding modules can include metadata to automate the linking process:

```luma
// std/stdio.lx
@module "stdio"
@c_header "stdio.h"
@link_lib "c"        // Automatically link libc

extern "C" {
    const printf = fn (format: *char, ...) int;
    // ... other declarations
}
```

Build system automatically detects linking requirements:

```bash
## Build system sees @link_lib "c" and automatically adds -lc
./luma build main.lx -l std/stdio.lx -name main
```

## Key Benefits

### 1. **Module System Consistency**

C bindings are treated as regular Luma modules, maintaining language design coherence.

### 2. **Explicit Dependency Management**

Developers maintain full control over what gets linked and in what order, respecting the build system's link order requirements.

### 3. **Namespace Protection**

The `@use "stdio" as io` pattern prevents namespace pollution while clearly indicating C library usage.

### 4. **Tooling Integration**

Automatic binding generation creates proper Luma modules that integrate seamlessly with existing tools.

### 5. **Build System Compatibility**

Works within the existing build system architecture without requiring fundamental changes.

## Implementation Strategy

1. **Bindgen Tool**: Create a robust C header parser that generates clean Luma binding modules
2. **Module System Extension**: Extend the module system to handle C-specific metadata
3. **Build System Enhancement**: Add automatic linking detection based on module metadata
4. **Standard Library Package**: Pre-generate bindings for common C standard library headers

This approach provides a foundation for C interoperability that feels natural to Luma developers while maintaining the language's design principles.

## Look into adding in Multithreading

## Add in multiple return types. ``const createStack = fn (stackCeiling: int) <*Stack, *void>``
