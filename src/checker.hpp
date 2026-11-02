#pragma once

#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/Basic/SourceLocation.h"

#include "intern.hpp"
#include "rules.hpp"

namespace bchk {

typedef clang::SourceLocation SrcLoc;

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

void build_rules(Database &db, Interner &interner);
Report CollectReport(const Database &db, const Interner &interner,
                     const std::unordered_map<Sym, SrcLoc> &point_loc,
                     const std::set<std::string> &disabled = {});
Report run_check(const std::string &c_source);

} // namespace bchk
