# bchk

**bchk** is an annotations-based borrowchecker for C.

`bchk [options] <source0> [... <sourceN>] -- [clang options]` 

## Table of Contents

* [Project Goals](#project-goals)

* [Using bchk](#using-bchk)

* [Borrowchecking model](#borrowchecking-model)

* [Build Guide](#building-bchk)

## Project Goals

> [!WARNING]
> **bchk** is in extremely early development and doesn't completely implement the contents of this document. It is also buggy.

**bchk** is a sound temporal borrowchecker for C. Correctly annotated programs accepted by **bchk** are temporally memory safe without compiler-inserted checks or value-dependent analysis. As a direct consequence of this, the tool only accepts a subset of valid C .

It does not require using a new compiler or a special runtime, like more powerful tools such as [Fil-c](https://fil-c.org/) do.

### Limitations

* **bchk** requires user annotations on pointers and only accepts a subset of valid C. 

* The tool doesn't do value-dependent analysis, and so cannot check *spatial* memory safety. The user must ensure pointers are bounds and null-checked, for instance with the `-fbounds-safety` flag.

* The tool doesn't check value initialization. Compilers with sanitizers are extremely good at detecting this already and it would make **bchk** significantly more complex.

* The tool only analyzes one translation unit at a time, like a compiler. As much analysis is function-local as possible.

* I haven't figured out how to implement concurrency in **bchk**, so it currently assumes the program is single-threaded.

## Using **bchk**

**bchk** implements a borrowchecking model that is similar to [Rust's NLL](https://rust-lang.github.io/rfcs/2094-nll.html) and actually uses a datalog backend similar to polonius. However, **bchk** doesn't currently support `Send` or `Sync` equivalents in C. 

There are 3 macros that must be used to annotate all of the pointers in your program:

*  `OWNER` designates a reference that owns an object
  * Only `OWNER` references can be freed (dropped)
* `MUT_BORROW` designates a reference that mutably borrows an object
  * Only one mutable borrow for each distinct object may exist at a given program point  
* `BORROW` designates a reference that immutably borrows an owned value
  * An unlimited number of borrows may exist at any given program point, as long as no mutable borrows exist at that same program point

The `LIFETIME_BOUND()` macro can also be applied to functions where the lifetime of an object returned from a function must exceed the lifetime of the function parameters passed to it. It takes the indices of the parameters it must outlive.

All of these macros evaluate to nothing outside the tool, so there's no overhead or codegen changes. Invoke **bchk** the same way you as clang and it will list everywhere your program violates the model.

## Borrowchecking model

All objects are required to have owners. Only owners may allocate or free their objects. Objects fall into one of the following categories:

1. Arithmetic types

2. Pointer types

3. Array types

4. Struct and union types

Arithmetic types don't interact with borrowchecking except via the other categories (or concurrent operations). There should only ever be one owner of an object at any given point in the program, though this isn't always true for array and struct types. 

Pointers are required to either own the object they point to, borrow it mutably, or borrow it immutably. Pointers lacking an annotation describing their role will generate an error. Because **bchk** does not do value analysis, this includes null pointers and other illegal pointers. Converting integers to pointers is prohibited. Converting an owned pointer to an integer is allowed, but terminates the lifetime of the underlying object since it can no longer be referred to. 

Owning pointers are move-only in the **bchk** model and will generate an error if used after assignment. Bitwise copies via memcpy (or other functions that implement the `copies()` summary attribute) are also treated as moves. **bchk** requires the user to manually implement rust's `Drop` trait for allocated objects by calling `free()` (or another function with the `drops()` summary attribute). A leak error is generated if an owner's lifetime ends without the object being dropped and it was originally created by malloc (or another function with the `alloc` summary attribute). Owning pointers must use the `OWNER` annotation, like this:

```C
int cleanup(int *OWNER p) {
    free(p);
}
```

or

```C
int *OWNER p = malloc(4);
```

Owning pointers may only allocate, initialize, or drop their objects. Mutation must be done with mutable borrows.

Mutable borrows may write to, or read from the objects they point to, and only one mutable borrow may exist for a given object at a particular program point. Mutable borrows use the `MUT_BORROW` annotation. Assignments, bitwise copies, and passing to a function will all move the pointer. **bchk** will error if the pointer is used after a move.

Immutable borrows may only derefence the object and cannot exist at the same program point as a live mutable borrow for the same object. Immutable borrows may be copied, assigned, or passed freely and use the `BORROW` annotation.

Structs may contain any of the other object types, each of which exists as a distinct field within a struct. Distinct fields must not alias, and are allowed to have distinct mutable borrows taken at a given program point. Mutably borrowing the parent struct prevents borrowing child fields, recursively, and vice versa. Immutable borrows to parents can coexist with immutable borrows of child fields. A struct field that owns another object must be freed or moved-from before the owner of the base struct may free it.

```c
typedef struct {
 int a;
 int b;
} s;

s example = {0};
OWNER s* own = &example;
MUT_BORROW int *a_mut = own.a;
MUT_BORROW int *b_mut = own.b;
```

Unions are currently prohibited because union fields inherently alias. **bchk** may eventually support them.

Array types may contain any of the other object types, each with distinct indices. Pointers to distinct static indices are allowed to coexist, subject to the same recursive rules as struct fields. Dynamic indices resolve to the special value `Array[...]`, which conflicts with all possible indices. References to distinct static indices promote to `Array[...]` after arithmetic. Owning pointers always imply `Array[...]` for allocation and dropping.

### Prohibited syntax

`setjmp`, `longjmp`, thread-local storage, atomics, varargs, and unions are all prohibited. **bchk** does not analyze functions for which it has a function summary, so code that needs to use these can do so inside a summarized function. Function summaries also allow you to define the behavior of functions outside the translation unit.

### Lifetimes

Globals (including static) have `'static` lifetimes and live from program start to end.Function local stack allocations cannot outlive their declaring function, unless they are returned from it as a copy. Returned pointer types referencing a stack allocated function local value dangle and will produce an error.

Heap-allocated values *may* outlive the function they're allocated in, but will produce an error if they outlive their owning pointer.

Static local values should have `'static` lifetime, but handling that properly would require **bchk** to see all translation units in the general case and make it much slower. Static locals are treated as local stack allocations instead. It's unhygenic to return pointers to static local variables anyway, so don't do it.

## Building **bchk**

### Dependencies

```
CMake versions >= 3.20
LLVM versions >= 18.x.x
    Clang           (required)
    LibTooling      (required)
    clang-format    (optional)
    clang-tidy      (optional)
Dartfrog >= any
```

**bchk** depends on [Dartfrog](https://github.com/J-Montgomery/dartfrog), LLVM, Clang, and LibTooling. Development has been done with LLVM versions >= 18.x.x, but earlier versions may also work. **bchk** uses clang-format and clang-tidy of the same version to enforce formatting and lint rules. 

**bchk** is slow to compile because Dartfrog abuses the compiler to do compile time query planning.

### Build Guide

```bash
cmake -G "Unix Makefiles" -B build/
cmake --build build/
ctest --test-dir build/
```


