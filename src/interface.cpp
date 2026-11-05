#include <array>
#include <cstddef>
#include <format>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include "extractor.hpp"
#include "interface.hpp"
#include "intern.hpp"
#include "rules.hpp"

namespace bchk {

namespace {
template <size_t N>
void collectErrors(Report &report, const std::string &rel_name,
                   const Interner &interner,
                   const std::vector<std::array<Sym, N>> &facts,
                   const PointSourceMap &point_loc) {
    for (const std::array<Sym, N> &f : facts) {
        Error err;

        err.relation = rel_name;
        err.arity = N;
        err.warning = false;
        std::copy(f.begin(), f.end(), err.columns.begin());

        std::string args;
        bool first = true;
        for (size_t i = 0; i < N; i++) {
            if (!first) {
                args += ", ";
            }
            args += interner.lookup(f[i]);
            first = false;
        }
        err.text = std::format("{}({})", rel_name, args);

        constexpr size_t place_idx = 1;
        const auto found = point_loc.find(f[place_idx]);
        if (found != point_loc.end()) {
            err.loc = found->second;
        }
        report.errors.push_back(std::move(err));
    }
}

// NOLINTBEGIN(clang-diagnostic-unused-parameter)
Report collectReport(const Database &db, const Interner &interner,
                     const PointSourceMap &point_loc,
                     const std::set<std::string> &disabled) {
    // NOLINTEND(clang-diagnostic-unused-parameter)
    Report report{};
    report.verified = true;
#define COLLECT_NAMED_ERR(name)                                                \
    {                                                                          \
        const bool verify_err = std::string(#name) == "err_unverified";        \
        const bool enabled =                                                   \
            verify_err || disabled.find(#name) == disabled.end();              \
        if (enabled) {                                                         \
            collectErrors(report, #name, interner, db.name.peek(), point_loc); \
            if (!db.name.peek().empty()) {                                     \
                report.verified = false;                                       \
            }                                                                  \
        }                                                                      \
    }

#define COLLECTED_ERRS(name, arity, text) COLLECT_NAMED_ERR(name)
    BCHK_ERR_RELATIONS(COLLECTED_ERRS)
#undef COLLECT_ERRS
#undef COLLECT_NAMED_ERR

    return report;
}

void buildRules(Database &db, Interner &interner) {
    buildErrorRules(db, interner);
}

} // anonymous namespace

bchk::Report analyzeFacts(Database &db, bchk::ExtractedFactSet &fact_set,
                          const std::set<std::string> &disabled) {
    loadRelations(db, std::move(fact_set.facts));
    buildRules(db, fact_set.interner);
    db.query.solve();

    return collectReport(db, fact_set.interner, fact_set.point_loc, disabled);
}

// NOLINTNEXTLINE(clang-diagnostic-unused-parameter)
Report analyzeSrcString(const std::string &c_source) {
    Database db;
    Interner interner;
    PointSourceMap point_loc{};
    std::set<std::string> disabled_relations{};

    clang::tooling::FixedCompilationDatabase compilations(".", {});
    clang::tooling::ClangTool tool(compilations, {"input.c"});
    tool.mapVirtualFile("input.c", c_source);

    bchk::ExtractedFactSet facts = bchk::generateFacts(tool);
    return analyzeFacts(db, facts, {});
}

// NOLINTBEGIN(clang-diagnostic-unused-parameter)
ExtractedFactSet generateFacts(clang::tooling::ClangTool &tool) {
    // NOLINTEND(clang-diagnostic-unused-parameter)
    ExtractedFactSet result = {};

    tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
        {"-fsyntax-only", "-DBCHK_ANNOTATIONS=1"},
        clang::tooling::ArgumentInsertPosition::BEGIN));

    FactExtractorFactory factory(result.interner, result.facts,
                                 result.point_loc);
    tool.run(&factory);
    return result;
}

} // namespace bchk
