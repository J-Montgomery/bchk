#include <datalog.hpp>

#include "rules.hpp"
#include "schema.hpp"

namespace bchk {

void buildErrorRules(Database &db, [[maybe_unused]] Interner &interner) {
    const Sym atomic_decl = interner.intern("atomic");
    const Sym tls_decl = interner.intern("tls");

    DL_RULE(db.query, (Point, Reason),
            db.err_ptr_missing_role(Point, Reason) <=
                db.ptr_missing_role(Point, Reason));
    DL_RULE(db.query, (Point, Fn),
            db.err_unbodied_fn(Point, Fn) <= db.fn_decl(Fn, Point),
            !db.fn_defined(Fn));
    DL_RULE(db.query, (Point, Reason),
            db.err_banned_decl(Point, Reason) <= db.union_decl(Point, Reason));
    DL_RULE(db.query, (Point, Kind, Name),
            db.err_banned_decl(Point, Name) <= db.decl_kind(Point, Kind, Name),
            df::datalog::where<Kind>(
                [atomic_decl](const Sym kind) { return kind == atomic_decl; }));
    DL_RULE(db.query, (Point, Kind, Name),
            db.err_banned_decl(Point, Name) <= db.decl_kind(Point, Kind, Name),
            df::datalog::where<Kind>(
                [tls_decl](const Sym kind) { return kind == tls_decl; }));

    /* The below rules collect the various error types into the generic
     * err_unverified collection */
    DL_RULE(db.query, (Point, Reason),
            db.err_unverified(Point, Reason) <=
                db.err_banned_decl(Point, Reason));
    DL_RULE(db.query, (Point, Reason),
            db.err_unverified(Point, Reason) <=
                db.err_ptr_missing_role(Point, Reason));
    DL_RULE(db.query, (Point, Reason),
            db.err_unverified(Point, Reason) <=
                db.err_unbodied_fn(Point, Reason));
    DL_RULE(db.query, (Point, Reason),
            db.err_unverified(Point, Reason) <= db.err_internal(Point, Reason));
}
} // namespace bchk
