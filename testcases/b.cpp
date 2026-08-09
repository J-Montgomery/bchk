#include "borrowck.h"

struct Resource {
    void read() const {}
    void mutate() INVALIDATES {}
    void destroy() DESTROY {}
};

struct Container {
    BORROW const Resource* get_ref() const { return nullptr; }
    void push_back(int) INVALIDATES {}
};

void pass_multiple_shared_borrows() {
    OWNER Resource res;
    BORROW const Resource* r1 = &res;
    BORROW const Resource* r2 = &res;
    r1->read();
    r2->read();
}

void pass_sequential_mut_borrows() {
    OWNER Resource res;
    {
        MUT_BORROW Resource* m1 = &res;
        m1->read();
    } // m1 drops out of lexical scope here

    {
        MUT_BORROW Resource* m2 = &res; // m1 is no longer active
        m2->read();
    }
}

void pass_invalidation_after_borrow_scope() {
    OWNER Resource res;
    {
        BORROW const Resource* r1 = &res;
        r1->read();
    } // end r1 lexical lifetime

    res.mutate();
}