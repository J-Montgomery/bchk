#include <format>

#include "checker.hpp"
#include "rules.hpp"

namespace bchk {

template <size_t N>
void collect_errors(Report &report, const std::string &rel_name,
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
            collect_errors(report, #name, interner, db.name.peek(),            \
                           point_loc);                                         \
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

// NOLINTNEXTLINE(clang-diagnostic-unused-parameter)
Report runCheck(const std::string &c_source) {
    Database db;
    Interner interner;
    PointSourceMap point_loc{};
    std::set<std::string> disabled_relations{};

    return collectReport(db, interner, point_loc, disabled_relations);
}

Report runCheck(Database &db, const ExtractedFactSet &fact_set,
                const std::set<std::string> &disabled) {
    // load_facts(db, std::move(fact_set.facts));
    // build_rules(db, fact_set.interner);
    // db.program.solve();

    return collectReport(db, fact_set.interner, fact_set.point_loc, disabled);
}

} // namespace bchk
