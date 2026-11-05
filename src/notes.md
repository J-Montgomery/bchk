# Development notes
----

## Goals

* Annotated C++ that passes is guaranteed to be temporally memory safe
    * Requires soundness
    * Requires a subset of C++
        * No bare pointers
        * Objects must have owners
            * Owner can create, read, update, move, and drop
        * You can make const/immutable references to objects
            * `&obj` in rust, can only read  
        * Only one mutable reference at a time
            * `&mut obj`
            * Can update or read obj
    * Able to enforce Send/Sync at multi-threading entrypoints
        * Custom annotations for send/sync
        * What should annotations for the functions look like?
* If the TU fails, it must indicate where you've violated the borrowcheck rules or failed to annotate
* Tool to help "rustify" C++ and guarantee temporal memory safety
* Users are responsible for bounds checking


## bchk rules

* Any number of immutable references to an obj
* A single mutable reference to an obj
* Only one or the other of these may be true at any point

~~Start with lexical lifetimes first I guess before introducing dartfrog?~~

## summary DSL thoughts

Since C doesn't have mangling, only one function name can exist at a time in a TU, and between
TUs duplicates (e.g. static locals) only need to be disambiguated to TU level (except nested functions). That means parameters are irrelevant, so we can limit consideration to just param indices, and then a set list of effects for each.

possible syntax?

```
fn malloc 1
    returns_owned
fn free 1
    takes_ownership 0
    frees 0
fn realloc 2
    returns_owned
    takes_ownership 0
    frees 0
fn set 2 @ src/foo.c
    writes 0
    aliases 0 1
```
