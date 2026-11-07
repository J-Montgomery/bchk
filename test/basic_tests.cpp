#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "test_utils.hpp"

TEST(Corpus, SanityCheck) { expectSafe("int f(){ return 0; }"); }

TEST(FnDecls, NoBodyGeneratesUnverified) {
    expectRejected("int f();", "err_unbodied_fn");
}

TEST(FnDecls, PrototypesDontGenUnverified) {
    expectSafe("int f(); int f() { return 0; }");
}

TEST(FnDecls, DuplicateProtosGenerateOneError) {
    const auto report = expectRejected("int f(); int f();", "err_unbodied_fn");
    EXPECT_EQ(report.errors.size(), 1);
}

TEST(FnDecls, MultipleProtosGenerateMultipleErrors) {
    const auto report = expectRejected("int f(); int g();", "err_unbodied_fn");
    EXPECT_EQ(report.errors.size(), 2);
}

TEST(PtrAnnotations, PointerParamWithoutRoleFails) {
    expectRejected("int f(char *ptr) { return 0; }", "err_ptr_missing_role");

    expectRejected("int f(char OWNER *ptr1, char *ptr2) { return 0; }",
                   "err_ptr_missing_role");
    expectRejected("int f(char *ptr1, char OWNER *ptr2) { return 0; }",
                   "err_ptr_missing_role");
}

TEST(PtrAnnotations, PointerParamWithRoleValidates) {
    // Validate no matter what kind of freaky ordering is used
    expectSafe("int f(OWNER char *ptr) {}");
    expectSafe("int f(char OWNER *ptr) {}");
    expectSafe("int f(char *OWNER ptr) {}");
    expectSafe("int f(char *ptr OWNER) {}");

    expectSafe("int f(BORROW char *ptr) {}");
    expectSafe("int f(char BORROW *ptr) {}");
    expectSafe("int f(char *BORROW ptr) {}");
    expectSafe("int f(char *ptr BORROW) {}");

    expectSafe("int f(MUT_BORROW char *ptr) {}");
    expectSafe("int f(char MUT_BORROW *ptr) {}");
    expectSafe("int f(char *MUT_BORROW ptr) {}");
    expectSafe("int f(char *ptr MUT_BORROW) {}");
}

TEST(BannedDecl, GlobalAtomicsError) {
    expectRejected("_Atomic int a;", "err_banned_decl");
    expectRejected("_Atomic int *a;", "err_banned_decl");
    expectRejected("_Atomic int *a;", "err_ptr_missing_role");

    expectRejected("_Atomic int a = 0;", "err_banned_decl");
    expectRejected("_Atomic int *a = 0;", "err_banned_decl");
    expectRejected("_Atomic int *a = 0;", "err_ptr_missing_role");

    expectRejected("extern _Atomic int a;", "err_banned_decl");
    expectRejected("extern _Atomic int *a;", "err_banned_decl");
    expectRejected("extern _Atomic int *a;", "err_ptr_missing_role");

    expectRejected("static _Atomic int a;", "err_banned_decl");
    expectRejected("static _Atomic int *a;", "err_banned_decl");
    expectRejected("static _Atomic int *a;", "err_ptr_missing_role");

    expectRejected("static _Atomic int a;", "err_banned_decl");
    expectRejected("static _Atomic int *a;", "err_banned_decl");
    expectRejected("static _Atomic int *a;", "err_ptr_missing_role");

    expectRejected("static _Atomic int a = 0;", "err_banned_decl");
    expectRejected("static _Atomic int *a = 0;", "err_banned_decl");
    expectRejected("static _Atomic int *a = 0;", "err_ptr_missing_role");
}

TEST(BannedDecl, LocalAtomicsError) {
    expectRejected("void f() { _Atomic int a; }", "err_banned_decl");
    expectRejected("void f() { _Atomic int *a; }", "err_banned_decl");
    expectRejected("void f() { _Atomic int *a; }", "err_ptr_missing_role");

    expectRejected("void f() { _Atomic int a = 0; }", "err_banned_decl");
    expectRejected("void f() { _Atomic int *a = 0; }", "err_banned_decl");
    expectRejected("void f() { _Atomic int *a = 0; }", "err_ptr_missing_role");

    expectRejected("void f() { static _Atomic int a; }", "err_banned_decl");
    expectRejected("void f() { static _Atomic int *a; }", "err_banned_decl");
    expectRejected("void f() { _Atomic int *a = 0; }", "err_ptr_missing_role");

    expectRejected("void f() { static _Atomic int a = 0; }", "err_banned_decl");
    expectRejected("void f() { static _Atomic int *a = 0; }",
                   "err_banned_decl");
    expectRejected("void f() { static _Atomic int *a = 0; }",
                   "err_ptr_missing_role");
}

TEST(BannedDecl, GlobalUnionsError) {
    expectRejected("union U { int x; };", "err_banned_decl");
    expectRejected("union U;", "err_banned_decl");

    expectRejected("extern union U { int x; };", "err_banned_decl");
    expectRejected("extern union U;", "err_banned_decl");

    expectRejected("static union U { int x; };", "err_banned_decl");
    expectRejected("static union U;", "err_banned_decl");
}
