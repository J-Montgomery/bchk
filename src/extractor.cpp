
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Analysis/CFG.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <format>
#include <iostream>

#include "extractor.hpp"

namespace borrowchk {

struct Point {
    size_t block;
    size_t statement;
};

bool CFGVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *rd) {
    if (!rd->isCompleteDefinition())
        return true;

    for (const auto *field : rd->fields()) {
        // TODO: Update this to emit facts that will allow checking send/sync
        // violations
    }
    rd->print(llvm::outs(), this->ctx.getLangOpts());

    llvm::outs() << "\n";
    return true;
}

bool CFGVisitor::VisitFunctionDecl(clang::FunctionDecl *fd) {
    if (!fd || !fd->hasBody())
        return true;

    llvm::outs() << std::format("CFG for function {}\n",
                                fd->getQualifiedNameAsString());

    clang::CFG::BuildOptions options;
    options.AddInitializers = true;
    options.AddEHEdges = true;
    options.AddLifetime = true;
    options.AddScopes = true;
    options.AddStaticInitBranches = true;
    auto cfg = clang::CFG::buildCFG(fd, fd->getBody(), &this->ctx, options);
    if (!cfg) {
        llvm::errs() << std::format("Failed to build CFG for {}\n",
                                    fd->getQualifiedNameAsString());
        return false;
    }

    cfg->print(llvm::outs(), this->ctx.getLangOpts(), true);

    llvm::outs() << "\n";
    return true;

    // for (const clang::CFGBlock *block : cfg) {
    //     const auto block_id = block->getBlockID();

    //     for(size_t idx = 0; idx < block->size(); idx++) {
    //         Point p{block_id, idx};

    //         // Emit a fact?
    //     }

    //     for(const clang::CFGBlock::AdjacentBlock &succ : block->succs()) {
    //         if (succ.Reachable()) {
    //             const auto successor_id = succ->getBlockID();

    //             // Emit a CFG edge
    //             std::format("CFG Edge {} -> {}\n", block_id, successor_id);
    //         }
    //     }
    // }
}

} // namespace borrowchk