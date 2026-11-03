#ifndef HOME_JM_SOFTWARE_CPP_BCHK_SRC_EXTRACTOR_HPP
#define HOME_JM_SOFTWARE_CPP_BCHK_SRC_EXTRACTOR_HPP

#pragma once

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Analysis/CFG.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "intern.hpp"
#include "rules.hpp"

namespace bchk {

class CFGVisitor : public clang::RecursiveASTVisitor<CFGVisitor> {
  public:
    explicit CFGVisitor(clang::ASTContext &ctx) : ctx(ctx) {}

    bool visitCxxRecordDecl(clang::CXXRecordDecl *rd);
    bool visitFunctionDecl(clang::FunctionDecl *fd);

  private:
    clang::ASTContext &ctx;
};

class CFGConsumer : public clang::ASTConsumer {
  public:
    void HandleTranslationUnit(clang::ASTContext &ctx) override {
        CFGVisitor visitor(ctx);

        visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    }
};

class CFGAction : public clang::ASTFrontendAction {
  public:
    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance &inst,
                      clang::StringRef file) override {
        return std::make_unique<CFGConsumer>();
    }
};

ExtractedFactSet
runExtract(const clang::tooling::CompilationDatabase &compilations);
} // namespace bchk

#endif
