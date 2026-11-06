#include <array>
#include <cstdint>
#include <datalog.hpp>
#include <vector>

#include "rules.hpp"
#include "schema.hpp"

namespace bchk {

template <size_t N>
void loadPred(df::datalog::Predicate<Sym, N> &predicate,
              std::vector<std::array<Sym, N>> facts) {
    predicate.insert(
        df::Relation<std::array<Sym, N>>::from_vec(std::move(facts)));
}

void loadRelations(Database &db, Facts facts) {
#define LOAD_PRED(name, arity) loadPred(db.name, std::move(facts.name));
    BCHK_RELATIONS(LOAD_PRED)
#define LOAD_ERR_PRED(name, arity) LOAD_PRED(name, arity)
    BCHK_ERR_RELATIONS(LOAD_ERR_PRED)
#undef LOAD_ERR_PRED
#undef LOAD_PRED
}

} // namespace bchk
