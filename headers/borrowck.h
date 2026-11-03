#ifndef HOME_JM_SOFTWARE_CPP_BCHK_HEADERS_BORROWCK_H
#define HOME_JM_SOFTWARE_CPP_BCHK_HEADERS_BORROWCK_H

#pragma once

#ifdef BCHK_ANNOTATIONS

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

#else

#define OWNER
#define BORROW
#define MUT_BORROW
#define LIFETIME_BOUND
#define INVALIDATES
#define ASSUME_INIT

#endif

#endif
