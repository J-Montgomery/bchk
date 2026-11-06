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

static std::string format_err_unbodied_fn(const std::array<Sym, 2> &args,
                                          const Interner &in) {
    return std::format("function '{}' has no body", in.lookup(args[1]));
}

static std::string format_err_ptr_missing_role(const std::array<Sym, 2> &args,
                                               const Interner &in) {
    return std::format("pointer '{}' has no borrow role", in.lookup(args[1]));
}

static std::string format_err_banned_decl(const std::array<Sym, 2> &args,
                                          const Interner &in) {
    return std::format("unsupported declaration '{}'", in.lookup(args[1]));
}

static std::string format_err_internal(const std::array<Sym, 2> &args,
                                       const Interner &in) {
    return std::format("internal error: {}", in.lookup(args[1]));
}

static std::string format_err_uaf(const std::array<Sym, 2> &args,
                                  const Interner &in) {
    return std::format("uaf: {}", in.lookup(args[1]));
}

static std::string format_err_leak(const std::array<Sym, 2> &args,
                                   const Interner &in) {
    return std::format("leak: {}", in.lookup(args[1]));
}

static std::string format_err_double_free(const std::array<Sym, 2> &args,
                                          const Interner &in) {
    return std::format("double free: {}", in.lookup(args[1]));
}

static std::string format_err_double_handoff(const std::array<Sym, 2> &args,
                                             const Interner &in) {
    return std::format("double handoff: {}", in.lookup(args[1]));
}

static std::string format_err_dangling_borrow(const std::array<Sym, 2> &args,
                                              const Interner &in) {
    return std::format("dangling borrow: {}", in.lookup(args[1]));
}

static std::string format_err_dangling_ret(const std::array<Sym, 2> &args,
                                           const Interner &in) {
    return std::format("dangling ret: {}", in.lookup(args[1]));
}

static std::string format_err_use_after_move(const std::array<Sym, 2> &args,
                                             const Interner &in) {
    return std::format("use after move: {}", in.lookup(args[1]));
}

static std::string format_err_borrow_moved(const std::array<Sym, 2> &args,
                                           const Interner &in) {
    return std::format("borrow_moved {}", in.lookup(args[1]));
}

static std::string format_err_mut_conflict(const std::array<Sym, 2> &args,
                                           const Interner &in) {
    return std::format("mut conflict: {}", in.lookup(args[1]));
}

static std::string format_err_freed_borrow(const std::array<Sym, 2> &args,
                                           const Interner &in) {
    return std::format("freed borrow: {}", in.lookup(args[1]));
}

static std::string format_err_mutated_borrow(const std::array<Sym, 2> &args,
                                             const Interner &in) {
    return std::format("mutated borrow: {}", in.lookup(args[1]));
}

static std::string format_err_static_borrow_esc(const std::array<Sym, 2> &args,
                                                const Interner &in) {
    return std::format("Static borrow esc: {}", in.lookup(args[1]));
}

static std::string format_err_contract_mismatch(const std::array<Sym, 2> &args,
                                                const Interner &in) {
    return std::format("contract mismatch: {}", in.lookup(args[1]));
}

static std::string format_err_unbound_ptr(const std::array<Sym, 2> &args,
                                          const Interner &in) {
    return std::format("unbound ptr: {}", in.lookup(args[1]));
}

// This formatter is never called because collectReport strips out the
// err_unverified relations before calling collectErrors. However, the compiler
// still sees the function because macros and C++ don't mix well and expects the
// symbol to exist.
[[maybe_unused]] static std::string
format_err_unverified(const std::array<Sym, 2> &, const Interner &) {
    return "";
}

template <size_t N, typename Formatter>
void collectErrors(Report &report, const std::string &rel_name,
                   const Interner &interner,
                   const std::vector<std::array<Sym, N>> &facts,
                   const PointSourceMap &point_loc, Formatter format) {
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
        err.text = format(f, interner);

        constexpr size_t place_idx = 0;
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
        constexpr bool is_unverified = std::string(#name) == "err_unverified"; \
        if constexpr (!is_unverified) {                                        \
            if (disabled.find(#name) == disabled.end()) {                      \
                collectErrors(report, #name, interner, db.name.peek(),         \
                              point_loc, format_##name);                       \
                if (!db.name.peek().empty()) {                                 \
                    report.verified = false;                                   \
                }                                                              \
            }                                                                  \
        }                                                                      \
    }

#define COLLECTED_ERRS(name, arity) COLLECT_NAMED_ERR(name)
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
Report analyzeSrcString(const std::string &c_source,
                        const std::vector<std::string> &args) {
    Database db;
    Interner interner;
    PointSourceMap point_loc{};
    std::set<std::string> disabled_relations{};

    std::vector<std::string> clang_args = {"clang", "input.c"};
    clang_args.insert(clang_args.end(), args.begin(), args.end());

    clang::tooling::FixedCompilationDatabase compilations(".", args);
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
