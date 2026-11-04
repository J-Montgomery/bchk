#ifndef HOME_JM_SOFTWARE_CPP_BCHK_SRC_CHECKER_HPP
#define HOME_JM_SOFTWARE_CPP_BCHK_SRC_CHECKER_HPP

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

void buildRules(Database &db, Interner &interner);
Report collectReport(const Database &db, const Interner &interner,
                     const std::unordered_map<Sym, SrcLoc> &point_loc,
                     const std::set<std::string> &disabled = {});

Report analyzeSrcString(const std::string &c_source);
Report analyzeFacts(Database &db, ExtractedFactSet &fact_set,
                    const std::set<std::string> &disabled = {});

} // namespace bchk

#endif
