#pragma once

#include <filesystem>

#include "rules.hpp"

namespace bchk_cli {
void dumpRelations(const bchk::Database &db, const Interner &interner,
                   const std::filesystem::path &dir);
} // namespace bchk_cli