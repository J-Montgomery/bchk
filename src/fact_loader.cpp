#include <array>
#include <cstdint>
#include <datalog.hpp>
#include <vector>

#include "rules.hpp"
#include "schema.hpp"

namespace bchk {

template <size_t N>
void load_pred(df::datalog::Predicate<Sym, N> &predicate,
               std::vector<std::array<Sym, N>> facts) {
    predicate.insert(
        df::Relation<std::array<Sym, N>>::from_vec(std::move(facts)));
}

void load_facts(Database &db, Facts facts) {
#define LOAD_PRED(name, arity) load_pred(db.name, std::move(facts.name));
    BCHK_RELATIONS(LOAD_PRED)
#undef BCHK_LOAD_ONE
}

} // namespace bchk