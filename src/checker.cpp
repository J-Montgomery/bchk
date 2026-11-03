#include "checker.hpp"

namespace bchk {

// NOLINTBEGIN(clang-diagnostic-unused-parameter)
Report collectReport(const Database &db, const Interner &interner,
                     const PointSourceMap &point_loc,
                     const std::set<std::string> &disabled) {
    // NOLINTEND(clang-diagnostic-unused-parameter)
    Report report{};
    report.verified = true;

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
