#include <datalog.hpp>

#include "rules.hpp"
#include "schema.hpp"

namespace bchk {

void buildErrorRules(Database &db, [[maybe_unused]] Interner &interner) {

    {
        DL_VARS(Point, Reason);
        RULE(db.query, db.any_err(Point) <= db.err_unverified(Point, Reason));
    }
}
} // namespace bchk
