#ifndef HOME_JM_SOFTWARE_CPP_BCHK_SRC_RULES_HPP
#define HOME_JM_SOFTWARE_CPP_BCHK_SRC_RULES_HPP

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/Basic/SourceLocation.h"
#include <datalog.hpp>

#include "intern.hpp"

namespace bchk {

using Sym = uint64_t;
using SrcLoc = clang::SourceLocation;

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

struct ExtractedFactSet {
    Facts facts;
    Interner interner;
    std::unordered_map<Sym, SrcLoc> point_loc;
    std::set<std::string> defined;
};

struct Error {
    std::string relation;
    size_t arity;
    std::array<Sym, 4> columns;
    std::string text;
    SrcLoc loc;
    bool warning;
};

struct Report {
    bool verified;
    std::vector<Error> errors;
};

} // namespace bchk

#endif
