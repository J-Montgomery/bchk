#pragma once

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include <interface.hpp>

using namespace bchk;

bool relationPresent(const Report &report, const std::string &relation) {
    for (const Error &error : report.errors) {
        if (error.relation == relation) {
            return true;
        }
    }

    return false;
}

const std::string include_str = "#include <borrowck.h>\n";
std::vector<std::string> args = {std::string("-I") + BCHK_TEST_INCLUDE_DIR};

[[maybe_unused]] Report expectSafe(const std::string &source) {
    const Report report = analyzeSrcString(include_str + source, args);
    EXPECT_TRUE(report.verified)
        << "Expected code to be accepted, but it failed.\n"
        << "  code: " << source;
    EXPECT_TRUE(report.errors.empty());

    return report;
}

[[maybe_unused]] Report expectRejected(const std::string &source,
                                       const std::string &relation) {
    const Report report = analyzeSrcString(include_str + source, args);
    EXPECT_FALSE(report.verified) << "  code: " << source;
    bool found = relationPresent(report, relation);
    if (!found) {
        std::string actual_errors = "";
        if (report.errors.empty()) {
            actual_errors = "    (no errors reported)\n";
        } else {
            for (const auto &err : report.errors) {
                actual_errors += "    - " + err.relation + "\n";
            }
        }

        ADD_FAILURE() << "Expected relation '" << relation << "' not found.\n"
                      << "  code: " << source << "\n"
                      << "  Actual errors produced:\n"
                      << actual_errors;
    }
    return report;
}
