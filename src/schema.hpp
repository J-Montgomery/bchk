#pragma once

// X-macro of all possible fact relations and their arity
#define BCHK_RELATIONS(X)                                                      \
    X(point, 1)                                                                \
    X(cfg_edge, 2)                                                             \
    X(call_site, 2)                                                            \
    X(fn_entry, 2)                                                             \
    X(fn_exit, 2)                                                              \
    X(role, 2)                                                                 \
    X(any_err, 1)

#define BCHK_ERR_RELATIONS(X) X(err_unverified, 2, "Could not verify")

namespace bchk::err {
#define BCHK_ERR_CLASS_VARS(name, arity, text)                                 \
    inline constexpr const char *k_##name = #text;
BCHK_ERR_RELATIONS(BCHK_ERR_CLASS_VARS)
#undef BCHK_ERR_CLASS_VARS
} // namespace bchk::err
