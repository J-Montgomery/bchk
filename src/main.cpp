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

#include "checker.hpp"
#include "extractor.hpp"
#include "rules.hpp"

using namespace bchk;

const char *bchk_version = "0.0.1";

static llvm::cl::OptionCategory bchk_category("bchk options");
static llvm::cl::list<std::string>
    compiler_flags(llvm::cl::Sink, llvm::cl::desc("Compiler flags"));
static llvm::cl::list<std::string>
    source_paths(llvm::cl::Positional, llvm::cl::desc("<input source files>"),
                 llvm::cl::OneOrMore, llvm::cl::cat(bchk_category));
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
    llvm::cl::ParseCommandLineOptions(argc, argv, overview);

    if (compiler_flags.empty()) {
        llvm::errs() << "ERROR: no input files or arguments\n";
        return 1;
    }

    std::vector<std::string> args = compiler_flags;
    clang::tooling::FixedCompilationDatabase compilations(".", args);

    // TODO: Create library model
    // TODO: Load builtin decls

    bchk::ExtractedFactSet facts = bchk::runExtract(compilations);
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
