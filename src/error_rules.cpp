#include <datalog.hpp>

#include "rules.hpp"
#include "schema.hpp"

namespace bchk {

void buildErrorRules(Database &db, [[maybe_unused]] Interner &interner) {

    {
        DL_VARS(Point, Reason);
        RULE(db.query, db.err_ptr_missing_role(Point, Reason) <=
                           db.ptr_missing_role(Point, Reason));
    }
    {
        DL_VARS(Point, Reason);
        RULE(db.query, db.err_unbodied_fn(Point, Reason) <=
                           db.unbodied_fn(Point, Reason));
    }
    {
        DL_VARS(Point, Reason);
        RULE(db.query, db.err_banned_decl(Point, Reason) <=
                           db.banned_decl(Point, Reason));
    }

    /* The below rules collect the various error types into the generic
     * err_unverified collection */
    {
        DL_VARS(Point, Reason);
        RULE(db.query, db.err_unverified(Point, Reason) <=
                           db.err_banned_decl(Point, Reason));
    }
    {
        DL_VARS(Point, Reason);
        RULE(db.query, db.err_unverified(Point, Reason) <=
                           db.err_ptr_missing_role(Point, Reason));
    }
    {
        DL_VARS(Point, Reason);
        RULE(db.query, db.err_unbodied_fn(Point, Reason) <=
                           db.err_ptr_missing_role(Point, Reason));
    }
}
} // namespace bchk
