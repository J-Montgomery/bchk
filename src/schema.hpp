#pragma once

// X-macro of all possible fact relations and their arity
#define BCHK_RELATIONS(X)                                                      \
    X(point, 1)                                                                \
    X(cfg_edge, 2)                                                             \
    X(call_site, 2)                                                            \
    X(fn_entry, 2)                                                             \
    X(fn_exit, 2)                                                              \
    X(role, 2)                                                                 \
    X(pointer_decl, 1)                                                         \
    X(var_init, 1)                                                             \
    X(global_decl, 1)                                                          \
    X(banned_decl, 2)                                                          \
    X(unbodied_fn, 2)                                                          \
    X(ptr_missing_role, 2)

#define BCHK_ERR_RELATIONS(X)                                                  \
    X(err_unverified, 2)                                                       \
    X(err_unbodied_fn, 2)                                                      \
    X(err_banned_decl, 2)                                                      \
    X(err_ptr_missing_role, 2)                                                 \
    X(err_internal, 2)                                                         \
    X(err_uaf, 2)                                                              \
    X(err_leak, 2)                                                             \
    X(err_double_free, 2)                                                      \
    X(err_double_handoff, 2)                                                   \
    X(err_dangling_borrow, 2)                                                  \
    X(err_dangling_ret, 2)                                                     \
    X(err_use_after_move, 2)                                                   \
    X(err_borrow_moved, 2)                                                     \
    X(err_mut_conflict, 2)                                                     \
    X(err_freed_borrow, 2)                                                     \
    X(err_mutated_borrow, 2)                                                   \
    X(err_static_borrow_esc, 2)                                                \
    X(err_contract_mismatch, 2)                                                \
    X(err_unbound_ptr, 2)
