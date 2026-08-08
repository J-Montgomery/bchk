#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <format>

#include "extractor.hpp"

using namespace borrowchk;

static llvm::cl::OptionCategory BorrowckCategory("borrowck options");
static llvm::cl::list<std::string>
    SourcePaths(llvm::cl::Positional, llvm::cl::desc("<input source files>"),
                llvm::cl::OneOrMore, llvm::cl::cat(BorrowckCategory));

int main(int argc, const char **argv) {
    llvm::cl::HideUnrelatedOptions(BorrowckCategory);
    llvm::cl::ParseCommandLineOptions(argc, argv, "Borrow checker tool\n");

    std::vector<std::string> DefaultArgs = {"-std=c++20"};
    clang::tooling::FixedCompilationDatabase Compilations(".", DefaultArgs);

    for (llvm::StringRef SourcePath : SourcePaths) {
        llvm::outs() << "Found input source: " << SourcePath << "\n";
    }

    clang::tooling::ClangTool Tool(Compilations, SourcePaths);
    return Tool.run(
        clang::tooling::newFrontendActionFactory<CFGAction>().get());
}
