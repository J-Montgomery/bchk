#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "checker.hpp"
namespace bchk {

namespace {

bool relation_present(const Report &report, const std::string &relation) {
    for (const Error &error : report.errors) {
        if (error.relation == relation) {
            return true;
        }
    }
}

void expect_safe(const std::string &source) {
    const Report report = run_check(source);
    EXPECT_TRUE(report.verified);
    EXPECT_TRUE(report.errors.empty());
}

void expect_rejected(const std::string &source, const std::string &relation) {
    const Report report = run_check(source);
    EXPECT_FALSE(report.verified);
    EXPECT_TRUE(relation_present(report, relation));
}

void expect_warned(const std::string &source, const std::string &relation) {
    const Report report = run_check(source);
    EXPECT_TRUE(report.verified);
    EXPECT_TRUE(relation_present(report, relation));
}

} // namespace

TEST(Corpus, SanityCheck) { expect_safe("int f(){ return 0; }"); }

} // namespace bchk