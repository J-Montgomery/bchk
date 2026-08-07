#pragma once

#define OWNER __attribute__((annotate("bc_owner")))
#define BORROW __attribute__((annotate("bc_borrow")))
#define MUT_BORROW __attribute__((annotate("bc_mut_borrow")))
#define INVALIDATES __attribute__((annotate("bc_invalidates")))
#define DESTROY __attribute__((annotate("bc_destroy")))

#if defined(__cplusplus)
#define LIFETIME_BOUND [[gsl::lifetimebound]]
#else
#define LIFETIME_BOUND __attribute((lifetimebound))
#endif
