#pragma once

#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <clang/Tooling/Tooling.h>

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

ExtractedFactSet generateFacts(clang::tooling::ClangTool &tool);
} // namespace bchk
