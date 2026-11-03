#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <format>

#include "extractor.hpp"

using namespace bchk;

static llvm::cl::OptionCategory BchkCategory("bchk Options");

static llvm::cl::list<std::string>
    CompilerFlags(llvm::cl::Sink, llvm::cl::desc("Compiler flags"));

static llvm::cl::list<std::string>
    SourcePaths(llvm::cl::Positional, llvm::cl::desc("<input source files>"),
                llvm::cl::OneOrMore, llvm::cl::cat(BchkCategory));

const char *BchkVersion = "0.0.1";

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
    clang::tooling::ClangTool Tool(Compilations, SourcePaths);

    Tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
        "-DBCHK_ANNOTATIONS=1", clang::tooling::ArgumentInsertPosition::BEGIN));
    return Tool.run(
        clang::tooling::newFrontendActionFactory<CFGAction>().get());
}
