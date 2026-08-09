#include "borrowck.h"

struct Resource {
    void read() const {}
    void mutate() INVALIDATES {}
    void destroy() DESTROY {}
};

void fail_double_mut_borrow() {
    OWNER Resource res;
    MUT_BORROW Resource* m1 = &res;
    MUT_BORROW Resource* m2 = &res; // mut borrow conflict
}

void fail_shared_and_mut_borrow() {
    OWNER Resource res;
    BORROW const Resource* r1 = &res;
    MUT_BORROW Resource* m1 = &res; // mut borrow conflict
}

void fail_invalidation_during_borrow() {
    OWNER Resource res;
    BORROW const Resource* r1 = &res;

    res.mutate(); // mutate invalidates res while active borrow exists

    r1->read();
}

void fail_container_invalidation_during_borrow() {
    OWNER Container vec;
    BORROW const Resource* elem = vec.get_ref();

    vec.push_back(42); // push_back invalidates vec while active borrow exists
}