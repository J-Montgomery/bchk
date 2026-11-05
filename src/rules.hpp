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

namespace role {
const std::string owner = "owner";
const std::string borrow = "borrow";
const std::string mut_borrow = "mut_borrow";
} // namespace role

struct SrcLoc {
    std::string file;
    unsigned line = 0;
    unsigned col = 0;

    bool valid() const { return !file.empty(); }
    std::string str() const {
        return file + ":" + std::to_string(line) + ":" + std::to_string(col);
    }
    auto tie() const { return std::tie(file, line, col); }
    bool operator<(const SrcLoc &other) const { return tie() < other.tie(); }
    bool operator==(const SrcLoc &other) const { return tie() == other.tie(); }
};

using PointSourceMap = std::unordered_map<Sym, SrcLoc>;
struct Database {
    df::datalog::Datalog query;
#define DB_PREDICATE(name, arity) df::datalog::Predicate<Sym, arity> name;
    BCHK_RELATIONS(DB_PREDICATE)
#define ERR_DB_PREDICATE(name, arity, text) DB_PREDICATE(name, arity)
    BCHK_ERR_RELATIONS(ERR_DB_PREDICATE)
#undef DB_PREDICATE
#undef ERR_DB_PREDICATE
    Database() {
#define DB_REGISTER(name, arity) query.register_predicate(&name);
        BCHK_RELATIONS(DB_REGISTER)
#define DB_ERR_REGISTER(name, arity, text) DB_REGISTER(name, arity)
        BCHK_ERR_RELATIONS(DB_ERR_REGISTER)
#undef DB_REGISTER
#undef DB_ERR_REGISTER
    }
};

constexpr size_t max_arity = 4;
struct Facts {
#define COLUMN_DECL(name, arity) std::vector<std::array<Sym, arity>> name;
    BCHK_RELATIONS(COLUMN_DECL)
#define ERR_COLUMN_DECL(name, arity, text) COLUMN_DECL(name, arity)
    BCHK_ERR_RELATIONS(ERR_COLUMN_DECL)
#undef COLUMN_DECL
#undef ERR_COLUMN_DECL
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
    std::array<Sym, max_arity> columns;
    std::string text;
    SrcLoc loc;
    bool warning;
};

struct Report {
    bool verified;
    std::vector<Error> errors;
};

void loadRelations(Database &db, Facts facts);
void buildErrorRules(Database &db, Interner &);
} // namespace bchk

#endif
