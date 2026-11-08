#pragma once

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <interface.hpp>

using namespace bchk;

namespace test {

struct TestCase {
    std::string source;
    std::vector<std::string> expected_errs;
};

struct BatchReport {
    Report report;
    std::map<size_t, std::vector<Error>> by_case;
};

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

[[maybe_unused]] BatchReport analyzeBatch(std::vector<TestCase> testcases) {
    std::string src = include_str;
    BatchReport out;
    std::map<size_t, size_t> line_case_map;

    for (size_t i = 0; i < testcases.size(); i++) {
        const size_t start_line = std::count(src.begin(), src.end(), '\n') + 1;

        std::string code = testcases[i].source;
        std::string varname = "_" + std::to_string(i) + "_";

        size_t pos = 0;
        while ((pos = code.find('@', pos)) != std::string::npos) {
            code.replace(pos, 1, varname);
            pos += varname.length();
        }

        src += code + "\n\n";
        line_case_map[start_line] = i;
    }

    out.report = analyzeSrcString(src, args);

    for (const Error &err : out.report.errors) {
        size_t err_line = err.loc.begin_line;
        auto it = line_case_map.upper_bound(err_line);
        if (it != line_case_map.begin()) {
            --it;
            out.by_case[it->second].push_back(err);
        }
    }

    return out;
}

/* It's vastly more efficient to run the analyzer over a whole batch of tests
 * simultaneously than to spin up a new analyzer for each individual test case.
 * This functions does that, and maps any generated diagnostics back to the
 * offending testcase.
 *
 * USAGE:
 *  expectCases({
 *      {"good_src", {}}, // Check if "src" validates
 *      {"bad_src", {"err1"}},
 *      {"badder_src", {"err1", "err2"}},
 * });
 *
 * Unique identifiers will be substituted for occurences of "@" in each
 * test case. Multiple identifiers can be generated per each test case by
 * suffixing each with a unique string, e.g. "@1", "@2", etc.
 */
[[maybe_unused]] BatchReport expectCases(const std::vector<TestCase> &cases) {
    BatchReport batch = analyzeBatch(cases);

    for (size_t i = 0; i < cases.size(); ++i) {
        const auto &test_case = cases[i];
        const auto &errors = batch.by_case[i];

        for (const auto &expected_rel : test_case.expected_errs) {
            bool found = false;
            for (const auto &err : errors) {
                if (err.relation == expected_rel) {
                    found = true;
                    break;
                }
            }
            std::string actual_errors = "";
            if (batch.report.errors.empty()) {
                actual_errors = "    (no errors reported)\n";
            } else {
                for (const auto &err : batch.report.errors) {
                    actual_errors += "    - " + err.relation + "\n";
                }
            }

            EXPECT_TRUE(found)
                << "Failed on test case index " << i << "\n"
                << "Expected relation '" << expected_rel << "' not found.\n"
                << "  code:\n"
                << test_case.source << "\n"
                << "  Actual errors produced:\n"
                << actual_errors;
        }
    }

    return batch;
}

} // namespace test
