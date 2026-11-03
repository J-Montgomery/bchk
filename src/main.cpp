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

const char *BchkVersion = "0.0.1";

static llvm::cl::OptionCategory BchkCategory("bchk options");
static llvm::cl::list<std::string>
    CompilerFlags(llvm::cl::Sink, llvm::cl::desc("Compiler flags"));
static llvm::cl::list<std::string>
    SourcePaths(llvm::cl::Positional, llvm::cl::desc("<input source files>"),
                llvm::cl::OneOrMore, llvm::cl::cat(BchkCategory));
llvm::cl::list<std::string>
    SummarySpecs("summary", llvm::cl::cat(BchkCategory),
                 llvm::cl::desc("declare a function effect"));
llvm::cl::list<std::string>
    SummaryFiles("summaries", llvm::cl::cat(BchkCategory),
                 llvm::cl::desc("read effect declarations from a file, "
                                "one per line"));
llvm::cl::list<std::string> DisableClasses(
    "bchk-disable", llvm::cl::cat(BchkCategory),
    llvm::cl::desc("suppress comma-separated errors and warnings"),
    llvm::cl::CommaSeparated);
llvm::cl::opt<bool> Quiet("quiet", llvm::cl::cat(BchkCategory),
                          llvm::cl::desc("disable summary"));

int main(int argc, const char **argv) {
    llvm::cl::HideUnrelatedOptions(BchkCategory);
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream &OS) {
        OS << "bchk v" << BchkVersion << " (LLVM v" << LLVM_VERSION << ")\n";
    });

    const char *Overview = "C borrowchecker\n";
    llvm::cl::ParseCommandLineOptions(argc, argv, Overview);

    if (CompilerFlags.empty()) {
        llvm::errs() << "ERROR: no input files or arguments\n";
        return 1;
    }

    std::vector<std::string> Args = CompilerFlags;
    clang::tooling::FixedCompilationDatabase Compilations(".", Args);

    // TODO: Create library model
    // TODO: Load builtin decls

    bchk::ExtractedFactSet facts = bchk::run_extract(Compilations);
    bchk::Database db;
    std::set<std::string> disabled(DisableClasses.begin(),
                                   DisableClasses.end());
    const bchk::Report report = bchk::run_check(db, facts, disabled);

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

    for (const bchk::Error &error : errors) {
        // TODO: Emit clang diags
    }
    if (!Quiet) {
        if (report.verified) {
            llvm::outs() << "verified\n";
        } else {
            llvm::errs() << "ERROR: verification failed\n";
        }
    }
    return report.verified ? 0 : 1;

    // clang::tooling::ClangTool Tool(Compilations, SourcePaths);

    // Tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
    //     {"-fsyntax-only", "-DBCHK_ANNOTATIONS=1"},
    //     clang::tooling::ArgumentInsertPosition::BEGIN));
    // return Tool.run(
    //     clang::tooling::newFrontendActionFactory<CFGAction>().get());
}
