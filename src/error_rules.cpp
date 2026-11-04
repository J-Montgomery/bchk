#include <datalog.hpp>

#include "rules.hpp"
#include "schema.hpp"

namespace bchk {

void buildErrorRules(Database &db, [[maybe_unused]] Interner &interner) {

    {
        DL_VARS(Point, Place, ErrorPoint);
        RULE(
            db.query, db.any_err(Point) <= db.point(Point), db.err_unverified(Place, ErrorPoint));
    }
}
} // namespace bchk
