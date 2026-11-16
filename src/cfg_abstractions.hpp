#pragma once

#include "rules.hpp"

namespace bchk {

// Each builders holds setters to track the program points where events
// we care about happen in the program CFG graph
// Each setter generates a new row in the db table

struct ControlBuilder {
    explicit ControlBuilder(Interner &interner, Facts &facts)
        : interner(interner), facts(facts) {}

    Sym point(const std::string &text);


    Interner &interner;
    Facts &facts;
};

Sym ControlBuilder::point(const std::string &text) {
    const Sym point = interner.freshen("point:" + text);
    facts.point.push_back({point});
    return point;
}



} // namespace bchk