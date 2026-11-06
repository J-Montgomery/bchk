#pragma once

#include <set>
#include <string>
#include <vector>

#include <clang/Tooling/Tooling.h>

#include "rules.hpp"

namespace bchk {

Report analyzeSrcString(const std::string &c_source,
                        const std::vector<std::string> &args = {});
Report analyzeFacts(Database &db, ExtractedFactSet &fact_set,
                    const std::set<std::string> &disabled = {});

ExtractedFactSet generateFacts(clang::tooling::ClangTool &tool);
} // namespace bchk
