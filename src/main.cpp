#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

// #include <filesystem>
// #include <format>
#include <algorithm>
#include <iostream>
#include <set>
#include <string>

#include "interface.hpp"
#include "rules.hpp"

using namespace bchk;

const char *bchk_version = "0.0.1";

static llvm::cl::OptionCategory bchk_category("bchk options");
llvm::cl::list<std::string>
    summary_specs("summary", llvm::cl::cat(bchk_category),
                  llvm::cl::desc("declare a function effect"));
llvm::cl::list<std::string>
    summary_files("summaries", llvm::cl::cat(bchk_category),
                  llvm::cl::desc("read effect declarations from a file, "
                                 "one per line"));
llvm::cl::list<std::string> disable_classes(
    "bchk-disable", llvm::cl::cat(bchk_category),
    llvm::cl::desc("suppress comma-separated errors and warnings"),
    llvm::cl::CommaSeparated);
llvm::cl::opt<bool> quiet("quiet", llvm::cl::cat(bchk_category),
                          llvm::cl::desc("disable summary"));

int main(int argc, const char **argv) {
    llvm::cl::HideUnrelatedOptions(bchk_category);
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream &os) {
        os << "bchk v" << bchk_version << " (LLVM v" << LLVM_VERSION << ")\n";
    });

    const char *overview = "C borrowchecker\n";
    auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc, argv, bchk_category, llvm::cl::OneOrMore, overview);
    if (!expected_parser) {
        llvm::errs() << expected_parser.takeError();
        return 1;
    }
    clang::tooling::CommonOptionsParser &options_parser = expected_parser.get();

    clang::tooling::ClangTool tool(options_parser.getCompilations(),
                                   options_parser.getSourcePathList());

    // TODO: Create library model
    // TODO: Load builtin decls

    bchk::ExtractedFactSet facts = bchk::generateFacts(tool);
    bchk::Database db;
    std::set<std::string> disabled(disable_classes.begin(),
                                   disable_classes.end());
    const bchk::Report report = bchk::analyzeFacts(db, facts, disabled);

    // TODO: initialize db
    // TODO: Extract and sort the errors/diags
    // TODO: emit the diags and errors, skipping the disabled ones

    std::vector<bchk::Error> errors = report.errors;
    std::stable_sort(errors.begin(), errors.end(),
                     [](const bchk::Error &a, const bchk::Error &b) {
                         if (!(a.loc == b.loc)) {
                             return a.loc < b.loc;
                         }
                         return a.text < b.text;
                     });

    // NOLINTNEXTLINE(clang-diagnostic-unused-variable)
    for (const bchk::Error &error : errors) {
        // TODO: Emit clang diags
    }
    if (!quiet) {
        if (report.verified) {
            llvm::outs() << "verified\n";
        } else {
            llvm::errs() << "ERROR: verification failed\n";
        }
    }
    return report.verified ? 0 : 1;
}
