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

TEST(PtrAnnotations, PointerWithRoleValidates) {
    expectSafe("int f(char OWNER *ptr) {}");
}

TEST(BannedDecl, GlobalAtomicsError) {
    expectRejected("_Atomic int a;", "err_banned_decl");
    expectRejected("_Atomic int *a;", "err_banned_decl");

    expectRejected("_Atomic int a = 0;", "err_banned_decl");
    expectRejected("_Atomic int *a = 0;", "err_banned_decl");

    expectRejected("extern _Atomic int a;", "err_banned_decl");
    expectRejected("extern _Atomic int *a;", "err_banned_decl");
}
