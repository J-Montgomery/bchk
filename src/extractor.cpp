#include "clang/Tooling/CompilationDatabase.h"
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Analysis/CFG.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cfg_abstractions.hpp"
#include "extractor.hpp"
#include "rules.hpp"

namespace bchk {

// enum class BorrowKind { None, Owner, Borrow, MutBorrow };

// static BorrowKind getBorrowKind(const clang::Decl *decl) {
//     if (!decl)
//         return BorrowKind::None;
//     for (const auto *attr : decl->specific_attrs<clang::AnnotateAttr>()) {
//         llvm::StringRef ann = attr->getAnnotation();
//         if (ann == "bc_owner")
//             return BorrowKind::Owner;
//         if (ann == "bc_borrow")
//             return BorrowKind::Borrow;
//         if (ann == "bc_mut_borrow")
//             return BorrowKind::MutBorrow;
//     }
//     return BorrowKind::None;
// }

// static bool hasAnnotation(const clang::Decl *decl, llvm::StringRef target) {
//     if (!decl)
//         return false;
//     for (const auto *attr : decl->specific_attrs<clang::AnnotateAttr>()) {
//         if (attr->getAnnotation() == target)
//             return true;
//     }
//     return false;
// }

class FactExtractorVisitor
    : public clang::RecursiveASTVisitor<FactExtractorVisitor> {
    clang::ASTContext &context;
    Interner &interner;
    Facts &facts;
    PointSourceMap &point_loc;

    std::string current_fn;
    Sym fn_exit = -1;
    Sym current_point = -1;
    int alloc_seq = 0;
    ControlBuilder control;
    PlaceBuilder places;

    std::unordered_map<const clang::Stmt *, Sym> stmt_pt;
    std::unordered_map<const clang::ValueDecl *, Sym> var_pt;
    std::unordered_map<Sym, Sym> current_alloc;

  public:
    FactExtractorVisitor(clang::ASTContext &context, Interner &interner,
                         Facts &facts, PointSourceMap &point_loc)
        : context(context), interner(interner), facts(facts),
          point_loc(point_loc), control(interner, facts),
          places(interner, facts) {}
};

class FactExtractorConsumer final : public clang::ASTConsumer {
    clang::ASTContext *context = nullptr;
    Interner &interner;
    Facts &facts;
    PointSourceMap &point_loc;

  public:
    FactExtractorConsumer(Interner &interner, Facts &facts,
                          PointSourceMap &point_loc)
        : interner(interner), facts(facts), point_loc(point_loc) {}

    void Initialize(clang::ASTContext &ctx) override { context = &ctx; }

    void HandleTranslationUnit(clang::ASTContext &ctx) override {
        FactExtractorVisitor visitor(ctx, interner, facts, point_loc);
        visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    }
};

class FactExtractorAction final : public clang::ASTFrontendAction {
    Interner &interner;
    Facts &facts;
    PointSourceMap &point_loc;

  public:
    FactExtractorAction(Interner &interner, Facts &facts,
                        PointSourceMap &point_loc)
        : interner(interner), facts(facts), point_loc(point_loc) {}

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef) {
        return std::make_unique<FactExtractorConsumer>(interner, facts,
                                                       point_loc);
    }
};

class FactExtractorFactory final
    : public clang::tooling::FrontendActionFactory {
    Interner &interner;
    Facts &facts;
    PointSourceMap &point_loc;

  public:
    FactExtractorFactory(Interner &interner, Facts &facts,
                         PointSourceMap &point_loc)
        : interner(interner), facts(facts), point_loc(point_loc) {}

    std::unique_ptr<clang::FrontendAction> create() {
        return std::make_unique<FactExtractorAction>(interner, facts,
                                                     point_loc);
    }
};

std::vector<std::string>
getSourcePaths(const clang::tooling::CompilationDatabase &compilations) {
    std::vector<std::string> source_paths;
    for (const auto &cmd : compilations.getAllCompileCommands()) {
        source_paths.push_back(cmd.Filename);
    }

    return source_paths;
}

// NOLINTBEGIN(clang-diagnostic-unused-parameter)
ExtractedFactSet
runExtract(const clang::tooling::CompilationDatabase &compilations) {
    // NOLINTEND(clang-diagnostic-unused-parameter)
    ExtractedFactSet result = {};
    const auto source_paths = getSourcePaths(compilations);

    clang::tooling::ClangTool tool(compilations, source_paths);
    tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
        {"-fsyntax-only", "-DBCHK_ANNOTATIONS=1"},
        clang::tooling::ArgumentInsertPosition::BEGIN));

    FactExtractorFactory factory(result.interner, result.facts,
                                 result.point_loc);
    tool.run(&factory);
    return result;
}

} // namespace bchk