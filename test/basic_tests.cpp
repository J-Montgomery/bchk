#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "interface.hpp"
namespace bchk {

namespace {

bool relationPresent(const Report &report, const std::string &relation) {
    for (const Error &error : report.errors) {
        if (error.relation == relation) {
            return true;
        }
    }

    return false;
}

[[maybe_unused]] void expectSafe(const std::string &source) {
    const Report report = analyzeSrcString(source);
    EXPECT_TRUE(report.verified);
    EXPECT_TRUE(report.errors.empty());
}

[[maybe_unused]] void expectRejected(const std::string &source,
                                     const std::string &relation) {
    const Report report = analyzeSrcString(source);
    EXPECT_FALSE(report.verified);
    EXPECT_TRUE(relationPresent(report, relation));
}

[[maybe_unused]] void expectWarned(const std::string &source,
                                   const std::string &relation) {
    const Report report = analyzeSrcString(source);
    EXPECT_TRUE(report.verified);
    EXPECT_TRUE(relationPresent(report, relation));
}

} // namespace

TEST(Corpus, SanityCheck) { expectSafe("int f(){ return 0; }"); }

TEST(Corpus, NoBodyGeneratesUnverified) {
    expectRejected("int f();", "err_unbodied_fn");
}

} // namespace bchk