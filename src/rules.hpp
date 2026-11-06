#ifndef HOME_JM_SOFTWARE_CPP_BCHK_SRC_RULES_HPP
#define HOME_JM_SOFTWARE_CPP_BCHK_SRC_RULES_HPP

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

    unsigned begin_line = 0;
    unsigned begin_col = 0;

    unsigned end_line = 0;
    unsigned end_col = 0;

    bool valid() const { return !file.empty(); }
    std::string str() const {
        auto begin_str =
            std::to_string(begin_line) + ":" + std::to_string(begin_col);
        auto end_str = std::to_string(end_line) + ":" + std::to_string(end_col);
        return file + ":" + begin_str + "->" + end_str;
    }
    auto tie() const { return std::tie(file, begin_line, begin_col); }
    bool operator<(const SrcLoc &other) const { return tie() < other.tie(); }
    bool operator==(const SrcLoc &other) const { return tie() == other.tie(); }
};

using PointSourceMap = std::unordered_map<Sym, SrcLoc>;
struct Database {
    df::datalog::Datalog query;
#define DB_PREDICATE(name, arity) df::datalog::Predicate<Sym, arity> name;
    BCHK_RELATIONS(DB_PREDICATE)
#define ERR_DB_PREDICATE(name, arity) DB_PREDICATE(name, arity)
    BCHK_ERR_RELATIONS(ERR_DB_PREDICATE)
#undef DB_PREDICATE
#undef ERR_DB_PREDICATE
    Database() {
#define DB_REGISTER(name, arity) query.register_predicate(&name);
        BCHK_RELATIONS(DB_REGISTER)
#define DB_ERR_REGISTER(name, arity) DB_REGISTER(name, arity)
        BCHK_ERR_RELATIONS(DB_ERR_REGISTER)
#undef DB_REGISTER
#undef DB_ERR_REGISTER
    }
};

constexpr size_t max_arity = 4;
struct Facts {
#define COLUMN_DECL(name, arity) std::vector<std::array<Sym, arity>> name;
    BCHK_RELATIONS(COLUMN_DECL)
#define ERR_COLUMN_DECL(name, arity) COLUMN_DECL(name, arity)
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
    std::unordered_set<std::string> defined;
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
