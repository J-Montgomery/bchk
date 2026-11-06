
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <llvm/Support/raw_ostream.h>

#include "dump.hpp"
#include "rules.hpp"
#include "schema.hpp"

namespace bchk_cli {

using namespace bchk;

namespace {

template <size_t N, size_t... Is>
void writeRow(std::ostream &out, const Interner &interner,
              const std::array<Sym, N> &row, std::index_sequence<Is...>) {
    ((out << (Is == 0 ? "" : "\t") << interner.lookup(row[Is])), ...);
    out << "\n";
}

template <size_t N>
void writeRelation(std::ostream &out, const Interner &interner,
                   const std::vector<std::array<Sym, N>> &rows) {
    for (const auto &row : rows) {
        writeRow(out, interner, row, std::make_index_sequence<N>{});
    }
}
} // namespace

void dumpRelations(const bchk::Database &db, const Interner &interner,
                   const std::filesystem::path &dir) {
    try {
        std::filesystem::create_directories(dir);
    } catch (const std::filesystem::filesystem_error &e) {
        llvm::errs() << "ERROR: Failed to create dir " << dir << "\n";
    }

    auto dumpFile = [&](std::string_view name, auto &&rows) {
        std::filesystem::path filepath = dir / (std::string(name) + ".tsv");
        std::ofstream f(filepath);
        if (!f) {
            llvm::errs() << "ERROR: Failed to open file " + filepath.string()
                         << "\n";
        }

        writeRelation(f, interner, rows);
    };

#define DUMP_RELATIONS(name, arity) dumpFile(#name, db.name.peek());
    BCHK_RELATIONS(DUMP_RELATIONS)
#define DUMP_ERROR_RELATIONS(name, arity) DUMP_RELATIONS(name, arity)
#undef DUMP_ERROR_RELATIONS
#undef DUMP_RELATIONS
}

} // namespace bchk_cli
