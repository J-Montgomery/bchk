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

    void begin_fn(const std::string &fn_name, Sym entry, Sym exit);
    void call_site(Sym point, const std::string &callee_text);

    Interner &interner;
    Facts &facts;
};

Sym ControlBuilder::point(const std::string &text) {
    const Sym point = interner.freshen("point:" + text);
    facts.point.push_back({point});
    return point;
}

void ControlBuilder::begin_fn(const std::string &fn_name, Sym entry, Sym exit) {
    facts.fn_entry.push_back({interner.intern(fn_name), entry});
    facts.fn_exit.push_back({interner.intern(fn_name), exit});
}

void ControlBuilder::call_site(Sym point, const std::string &callee_text) {
    facts.call_site.push_back({point, interner.intern(callee_text)});
}

struct PlaceBuilder {
    explicit PlaceBuilder(Interner &interner, Facts &facts)
        : interner(interner), facts(facts) {}

    Sym base(const std::string &text);
    void role(Sym place, const std::string &role_name);

    Interner &interner;
    Facts &facts;
};

Sym PlaceBuilder::base(const std::string &text) {
    const Sym place = interner.freshen(text);
    return place;
}

void PlaceBuilder::role(Sym place, const std::string &role_name) {
    facts.role.push_back({place, interner.intern(role_name)});
}

} // namespace bchk