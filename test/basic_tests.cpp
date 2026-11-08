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
        {"int f(char *ptr) { return 0; }",
            {"err_ptr_missing_role"}},
        {"int f(char OWNER *ptr1, char *ptr2) { return 0; }",
            {"err_ptr_missing_role"}},
        {"int f(char *ptr1, char OWNER *ptr2) { return 0; }",
            {"err_ptr_missing_role"}},
    });
    // clang-format on
}

TEST(PtrAnnotations, PointerParamWithRoleValidates) {
    // Validate no matter what kind of freaky ordering is used
    // clang-format off
    expectCases({
        {"int f(OWNER char *ptr) {}", {}},
        {"int f(char OWNER *ptr) {}", {}},
        {"int f(char *OWNER ptr) {}", {}},
        {"int f(char *ptr OWNER) {}", {}},

        {"int f(BORROW char *ptr) {}", {}},
        {"int f(char BORROW *ptr) {}", {}},
        {"int f(char *BORROW ptr) {}", {}},
        {"int f(char *ptr BORROW) {}", {}},

        {"int f(MUT_BORROW char *ptr) {}", {}},
        {"int f(char MUT_BORROW *ptr) {}", {}},
        {"int f(char *MUT_BORROW ptr) {}", {}},
        {"int f(char *ptr MUT_BORROW) {}", {}},
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
