#pragma once

#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "intern.hpp"
#include "rules.hpp"

namespace bchk {

void build_rules(Database &db, Interner &interner);
Report CollectReport(const Database &db, const Interner &interner,
                     const std::unordered_map<Sym, SrcLoc> &point_loc,
                     const std::set<std::string> &disabled = {});

// Unit test entry point
Report run_check(const std::string &c_source);

// CLI entry point
Report run_check(Database &db, const ExtractedFactSet &fact_set,
                 const std::set<std::string> &disabled = {});

} // namespace bchk
