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
    X(ptr_missing_role, 2)                                                     \
    X(any_err, 1)

#define BCHK_ERR_RELATIONS(X)                                                  \
    X(err_unverified, 2, "Could not verify")                                   \
    X(err_internal, 2, "Internal tool error")

namespace bchk::err {
#define BCHK_ERR_CLASS_VARS(name, arity, text)                                 \
    inline constexpr const char *k_##name = #text;
BCHK_ERR_RELATIONS(BCHK_ERR_CLASS_VARS)
#undef BCHK_ERR_CLASS_VARS

namespace verification {
inline constexpr const char *k_banned_decl =
    "Declaration {} is unsupported type {}";
inline constexpr const char *k_fn_missing_body = "Function {} lacks a body";
inline constexpr const char *k_role_conflict =
    "Variable {} has conflicting roles";
inline constexpr const char *k_ptr_missing_role = "Pointer {} lacks a role";
} // namespace verification
} // namespace bchk::err
