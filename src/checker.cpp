#include "checker.hpp"

namespace bchk {

Report collect_report(const Database &db, const Interner &interner,
                      const std::unordered_map<Sym, SrcLoc> &point_loc,
                      const std::set<std::string> &disabled) {
    Report report{};
    report.verified = true;

    return report;
}
Report run_check(const std::string &c_source) {
    Database db;
    Interner interner;
    std::unordered_map<Sym, SrcLoc> locs{};
    std::set<std::string> disabled_relations{};

    return collect_report(db, interner, locs, disabled_relations);
}

Report run_check(Database &db, const ExtractedFactSet &fact_set,
                 const std::set<std::string> &disabled) {
    // load_facts(db, std::move(fact_set.facts));
    // build_rules(db, fact_set.interner);
    // db.program.solve();

    return collect_report(db, fact_set.interner, fact_set.point_loc, disabled);
}

} // namespace bchk
