#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <datalog.hpp>

namespace borrowchk {

using Sym = uint64_t;

#define EXTRACT_RELATION(X)                                                    \
    X(init, 2)    /* (Region, Pt) */                                           \
    X(destroy, 2) /* (Region, Pt) */

struct Database {
    df::datalog::Datalog query;
};

struct Facts {
#define COLUMN_DECL(name, arity) std::vector<std::array<Sym, arity>> name;
    EXTRACT_RELATION(COLUMN_DECL)
#undef COLUMN_DECL
};

struct Program {
    Facts facts;
};
} // namespace borrowchk