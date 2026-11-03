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
#include "schema.hpp"

namespace bchk {

using Sym = int32_t;
using SrcLoc = clang::SourceLocation;
using PointSourceMap = std::unordered_map<Sym, SrcLoc>;

struct Database {
    df::datalog::Datalog query;
#define DB_PREDICATE(name, arity) df::datalog::Predicate<Sym, arity> name;
    BCHK_RELATIONS(DB_PREDICATE)
#undef DB_PREDICATE

    Database() {
#define DB_REGISTER(name, arity) query.register_predicate(&name);
        BCHK_RELATIONS(DB_REGISTER)
#undef DB_REGISTER
    }
};

struct Facts {
#define COLUMN_DECL(name, arity) std::vector<std::array<Sym, arity>> name;
    BCHK_RELATIONS(COLUMN_DECL)
#undef COLUMN_DECL
};

struct Program {
    Facts facts;
};

struct ExtractedFactSet {
    Facts facts;
    Interner interner;
    PointSourceMap point_loc;
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
