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

Start with lexical lifetimes first I guess before introducing dartfrog?