#pragma once

// X-macro of all possible fact relations and their arity
#define BCHK_RELATIONS(X)                                                      \
    X(point, 1)                                                                \
    X(cfg_edge, 2)                                                             \
    X(call_site, 2)                                                            \
    X(fn_entry, 2)                                                             \
    X(fn_exit, 2)                                                              \
    X(role, 2)
