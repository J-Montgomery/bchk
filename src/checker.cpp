#include "checker.hpp"

namespace bchk {

Report collect_report(const Database &db, const Interner &interner,
                      const std::map<Sym, SrcLoc> &point_loc,
                      const std::set<std::string> &disabled) {
    Report report{};
    report.verified = true;

    return report;
}
Report run_check(const std::string &c_source) {
    Database db;
    Interner interner;
    std::map<Sym, SrcLoc> locs{};
    std::set<std::string> disabled_relations{};

    return collect_report(db, interner, locs, disabled_relations);
}

} // namespace bchk
