#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "test_utils.hpp"

using namespace test;

TEST(Corpus, SanityCheck) { expectSafe("int f(){ return 0; }"); }

TEST(FnDecls, NoBodyGeneratesUnverified) {
    expectRejected("int f();", "err_unbodied_fn");
}

TEST(FnDecls, PrototypesDontGenUnverified) {
    expectSafe("int f(); int f() { return 0; }");
}

TEST(FnDecls, UnbodiedProtosGenerateErrors) {
    // clang-format off
    expectCases({
        {"int f();",
            {"err_unbodied_fn"}},
        {"int f(); int g();",
            {"err_unbodied_fn", "err_unbodied_fn"}},
        {"int f(); int g(); int h();",
            {"err_unbodied_fn", "err_unbodied_fn", "err_unbodied_fn"}},
    });
    // clang-format on
}

TEST(PtrAnnotations, PointerParamWithoutRoleFails) {
    // clang-format off
    expectCases({
        {"int @(char *ptr) { return 0; }",
            {"err_ptr_missing_role"}},
        {"int @(char OWNER *ptr1, char *ptr2) { return 0; }",
            {"err_ptr_missing_role"}},
        {"int @(char *ptr1, char OWNER *ptr2) { return 0; }",
            {"err_ptr_missing_role"}},
    });
    // clang-format on
}

TEST(PtrAnnotations, PointerParamWithRoleValidates) {
    // Validate no matter what kind of freaky ordering is used
    // clang-format off
    expectCases({
        {"int @(OWNER char *ptr) {}", {}},
        {"int @(char OWNER *ptr) {}", {}},
        {"int @(char *OWNER ptr) {}", {}},
        {"int @(char *ptr OWNER) {}", {}},

        {"int @(BORROW char *ptr) {}", {}},
        {"int @(char BORROW *ptr) {}", {}},
        {"int @(char *BORROW ptr) {}", {}},
        {"int @(char *ptr BORROW) {}", {}},

        {"int @(MUT_BORROW char *ptr) {}", {}},
        {"int @(char MUT_BORROW *ptr) {}", {}},
        {"int @(char *MUT_BORROW ptr) {}", {}},
        {"int @(char *ptr MUT_BORROW) {}", {}},
    });
    // clang-format on
}

TEST(BannedDecl, GlobalAtomicsError) {
    // clang-format off
    expectCases({
        {"_Atomic int @;",
            {"err_banned_decl"}},
        {"_Atomic int *@;",
            {"err_banned_decl", "err_ptr_missing_role"}},
        {"_Atomic int @ = 0;",
            {"err_banned_decl"}},
        {"_Atomic int *@ = 0;",
            {"err_banned_decl", "err_ptr_missing_role"}},
        {"extern _Atomic int @;",
            {"err_banned_decl"}},
        {"extern _Atomic int *@;",
            {"err_banned_decl", "err_ptr_missing_role"}},
        {"static _Atomic int @;",
            {"err_banned_decl"}},
        {"static _Atomic int *@;",
            {"err_banned_decl", "err_ptr_missing_role"}},
        {"static _Atomic int @;",
            {"err_banned_decl"}},
        {"static _Atomic int *@;",
            {"err_banned_decl", "err_ptr_missing_role"}},
        {"static _Atomic int @ = 0;",
            {"err_banned_decl"}},
        {"static _Atomic int *@ = 0;",
            {"err_banned_decl", "err_ptr_missing_role"}}
    });
    // clang-format on
}

TEST(BannedDecl, LocalAtomicsError) {
    // clang-format off
    expectCases({
        {"void @0() { _Atomic int @1; }",
            {"err_banned_decl"}},
        {"void @0() { _Atomic int *@1; }",
            {"err_banned_decl", "err_ptr_missing_role"}},
        {"void @0() { _Atomic int @1 = 0; }",
            {"err_banned_decl"}},
        {"void @0() { _Atomic int *@1 = 0; }",
            {"err_banned_decl", "err_ptr_missing_role"}},
        {"void @0() { static _Atomic int @1; }",
            {"err_banned_decl"}},
        {"void @0() { static _Atomic int *@1; }",
            {"err_banned_decl", "err_ptr_missing_role"}},
        {"void @0() { static _Atomic int @1; }",
            {"err_banned_decl"}},
        {"void @0() { static _Atomic int *@1; }",
            {"err_banned_decl", "err_ptr_missing_role"}},
        {"void @0() { static _Atomic int @1 = 0; }",
            {"err_banned_decl"}},
        {"void @0() { static _Atomic int *@1 = 0; }",
            {"err_banned_decl", "err_ptr_missing_role"}}
    });
    // clang-format on
}

TEST(BannedDecl, UnionDeclsError) {
    // clang-format off
    expectCases({
        {"union @ { int x; };",
            {"err_banned_decl"}},
        {"union @;",
            {"err_banned_decl"}},
        {"extern union @ { int x; };",
            {"err_banned_decl"}},
        {"extern union @;",
            {"err_banned_decl"}},
        {"static union @ { int x; };",
            {"err_banned_decl"}},
        {"static union @;",
            {"err_banned_decl"}},

        {"void @() { union U { int x; }; }",
            {"err_banned_decl"}},
        {"void @() { union U; }",
            {"err_banned_decl"}},
        {"void @() { extern union U { int x; }; }",
            {"err_banned_decl"}},
        {"void @() { extern union U; }",
            {"err_banned_decl"}},
        {"void @() { static union U { int x; }; }",
            {"err_banned_decl"}},
        {"void @() { static union U; }",
            {"err_banned_decl"}},

        {"void @() { union U { int x; }; }",
            {"err_banned_decl"}},
        {"void @() { union U; }",
            {"err_banned_decl"}},
        {"void @() { extern union U { int x; }; }",
            {"err_banned_decl"}},
        {"void @() { extern union U; }",
            {"err_banned_decl"}},
        {"void @() { static union U { int x; }; }",
            {"err_banned_decl"}},
        {"void @() { static union U; }",
            {"err_banned_decl"}},
    });
    // clang-format on
}

TEST(ReturnAnnotations, PointerReturnRequiresAndAcceptsRoles) {
    expectRejected("int *f(void) { return 0; }", "err_ptr_missing_role");
    expectSafe("OWNER int *owner(void) { return 0; }");
    expectSafe("BORROW int *borrow(void) { return 0; }");
    expectSafe("MUT_BORROW int *mut_borrow(void) { return 0; }");
}

TEST(ReturnAnnotations, ReturnMayBeLifetimeBoundToPointerParameter) {
    const std::string source =
        "BORROW int *get(OWNER int *p) LIFETIME_BOUND { return p; }";
    expectSafe(source);
    const ExtractedFactSet facts = extractFacts(source);
    const Sym fn = facts.interner.id("get").value();
    const Sym borrow = facts.interner.id("borrow").value();
    EXPECT_TRUE(std::any_of(
        facts.facts.fn_return_role.begin(), facts.facts.fn_return_role.end(),
        [=](const auto &fact) { return fact[0] == fn && fact[1] == borrow; }));
    ASSERT_EQ(facts.facts.param_entry.size(), 1);
    const Sym param = facts.facts.param_entry.front()[0];
    EXPECT_TRUE(std::any_of(
        facts.facts.fn_return_lifetime_bound.begin(),
        facts.facts.fn_return_lifetime_bound.end(),
        [=](const auto &fact) { return fact[0] == fn && fact[1] == param; }));
    EXPECT_EQ(facts.facts.return_place.size(), 1);
    EXPECT_EQ(facts.facts.return_value.size(), 1);
    const auto result_type = std::find_if(
        facts.facts.fn_return_type.begin(), facts.facts.fn_return_type.end(),
        [=](const auto &fact) { return fact[0] == fn; });
    ASSERT_NE(result_type, facts.facts.fn_return_type.end());
    EXPECT_TRUE(std::any_of(
        facts.facts.type_pointer.begin(), facts.facts.type_pointer.end(),
        [&](const auto &fact) { return fact[0] == (*result_type)[1]; }));
}

TEST(ExtractorFacts, CapturesCPlacesCallsValuesAndReturns) {
    const ExtractedFactSet facts = extractFacts(R"(
        struct S { BORROW int *field; };
        BORROW int *callee(BORROW int *p) { return p; }
        BORROW int *work(BORROW int *p, int index) {
            BORROW int *q = p;
            struct S s = {.field = q};
            int *(*callback)(int *) = callee;
            q = callee(s.field);
            q = callback(q);
            q = p ? callee(p) : q;
            q = _Generic(q, int *: callee(q), default: p);
            q = (int *)1;
            q = q + index;
            q[0] = *p;
            q[index] = *p;
            for (int n = 0; n < index; ++n) { q += 1; }
            while (index) { --index; break; }
            do { q = p; } while (index);
            switch (index) {
            case 0: q = p; break;
            default: q = callee(q); break;
            }
            BORROW int **qptr = &q;
            return callee(q);
        }
    )");
    std::string unsupported;
    for (const auto &fact : facts.facts.err_internal) {
        unsupported += std::string(facts.interner.lookup(fact[1])) + "; ";
    }
    EXPECT_TRUE(facts.facts.err_internal.empty()) << unsupported;
    EXPECT_FALSE(facts.facts.cfg_edge.empty());
    EXPECT_FALSE(facts.facts.loop_back_edge.empty());
    EXPECT_FALSE(facts.facts.lifetime_end.empty());
    EXPECT_FALSE(facts.facts.local_var.empty());
    EXPECT_FALSE(facts.facts.type_field.empty());
    EXPECT_FALSE(facts.facts.place_field.empty());
    EXPECT_FALSE(facts.facts.place_elem.empty());
    EXPECT_FALSE(facts.facts.place_bucket.empty());
    EXPECT_FALSE(facts.facts.place_deref.empty());
    EXPECT_FALSE(facts.facts.event_bind.empty());
    EXPECT_FALSE(facts.facts.event_copy.empty());
    EXPECT_FALSE(facts.facts.event_load.empty());
    EXPECT_FALSE(facts.facts.event_store.empty());
    EXPECT_FALSE(facts.facts.value_addr.empty());
    EXPECT_FALSE(facts.facts.value_arith.empty());
    EXPECT_FALSE(facts.facts.value_cast.empty());
    EXPECT_FALSE(facts.facts.value_call.empty());
    EXPECT_FALSE(facts.facts.value_choice.empty());
    EXPECT_FALSE(facts.facts.value_literal.empty());
    EXPECT_FALSE(facts.facts.call_site.empty());
    EXPECT_FALSE(facts.facts.call_indirect.empty());
    EXPECT_FALSE(facts.facts.call_arg.empty());
    EXPECT_FALSE(facts.facts.call_arg_value.empty());
    EXPECT_FALSE(facts.facts.call_store_place.empty());
    EXPECT_FALSE(facts.facts.call_result_place.empty());
    EXPECT_FALSE(facts.facts.fn_call.empty());
    EXPECT_FALSE(facts.facts.return_place.empty());
    EXPECT_FALSE(facts.facts.return_value.empty());
    EXPECT_FALSE(facts.facts.return_call.empty());
    EXPECT_FALSE(facts.facts.type_fn_result.empty());
    EXPECT_FALSE(facts.facts.type_fn_param.empty());
}

TEST(FunctionDeclarations, PrototypeContractsMergeWithDefinition) {
    const ExtractedFactSet facts = extractFacts(
        "int *f(int *p); BORROW int *f(BORROW int *p) { return p; }");
    const Sym fn = facts.interner.id("f").value();
    const Sym borrow = facts.interner.id("borrow").value();
    ASSERT_EQ(facts.facts.param_idx.size(), 1);
    const Sym param = facts.facts.param_idx.front()[0];
    EXPECT_EQ(facts.facts.param_idx.front()[1], fn);
    EXPECT_TRUE(std::any_of(
        facts.facts.role.begin(), facts.facts.role.end(),
        [=](const auto &fact) { return fact[0] == param && fact[1] == borrow; }));
    EXPECT_TRUE(std::any_of(
        facts.facts.fn_return_role.begin(), facts.facts.fn_return_role.end(),
        [=](const auto &fact) { return fact[0] == fn && fact[1] == borrow; }));
    EXPECT_TRUE(facts.facts.ptr_missing_role.empty());
    EXPECT_EQ(facts.facts.param_entry.size(), 1);
    EXPECT_EQ(facts.facts.return_place.size(), 1);
}
