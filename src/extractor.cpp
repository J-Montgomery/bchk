#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
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
#include <string>
#include <vector>

#include "extractor.hpp"

namespace bchk {

struct Point {
    size_t block;
    size_t statement;
};

enum class BorrowKind { None, Owner, Borrow, MutBorrow };

struct ActiveBorrow {
    std::string borrow_var;
    std::string origin_var;
    BorrowKind kind;
    size_t scope_depth;
    clang::SourceLocation decl_loc;
};

static BorrowKind getBorrowKind(const clang::Decl *decl) {
    if (!decl)
        return BorrowKind::None;
    for (const auto *attr : decl->specific_attrs<clang::AnnotateAttr>()) {
        llvm::StringRef ann = attr->getAnnotation();
        if (ann == "bc_owner")
            return BorrowKind::Owner;
        if (ann == "bc_borrow")
            return BorrowKind::Borrow;
        if (ann == "bc_mut_borrow")
            return BorrowKind::MutBorrow;
    }
    return BorrowKind::None;
}

static bool hasAnnotation(const clang::Decl *decl, llvm::StringRef target) {
    if (!decl)
        return false;
    for (const auto *attr : decl->specific_attrs<clang::AnnotateAttr>()) {
        if (attr->getAnnotation() == target)
            return true;
    }
    return false;
}

static const clang::VarDecl *extractOriginVar(const clang::Expr *expr) {
    if (!expr)
        return nullptr;
    expr = expr->IgnoreParenImpCasts();

    if (const auto *op = llvm::dyn_cast<clang::UnaryOperator>(expr)) {
        if (op->getOpcode() == clang::UO_AddrOf) {
            return extractOriginVar(op->getSubExpr());
        }
    }
    if (const auto *ref = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
        return llvm::dyn_cast<clang::VarDecl>(ref->getDecl());
    }
    if (const auto *call = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
        if (const auto *obj = call->getImplicitObjectArgument()) {
            return extractOriginVar(obj);
        }
    }
    return nullptr;
}

class LexicalASTVisitor : public clang::RecursiveASTVisitor<LexicalASTVisitor> {
    clang::ASTContext &ctx;
    size_t scope_depth = 0;
    std::vector<ActiveBorrow> active_borrows;

    std::string formatLoc(clang::SourceLocation loc) const {
        if (loc.isInvalid())
            return "<unknown location>";

        const auto &sm = ctx.getSourceManager();

        clang::SourceLocation fileLoc = sm.getFileLoc(loc);

        clang::PresumedLoc pLoc = sm.getPresumedLoc(fileLoc);
        if (pLoc.isInvalid())
            return "<invalid location>";

        return std::format("{}:{}:{}", pLoc.getFilename(), pLoc.getLine(),
                           pLoc.getColumn());
    }

  public:
    explicit LexicalASTVisitor(clang::ASTContext &ctx) : ctx(ctx) {}

    bool TraverseCompoundStmt(clang::CompoundStmt *cs) {
        scope_depth++;
        bool result =
            clang::RecursiveASTVisitor<LexicalASTVisitor>::TraverseCompoundStmt(
                cs);

        std::erase_if(active_borrows, [this](const ActiveBorrow &b) {
            return b.scope_depth == scope_depth;
        });

        scope_depth--;
        return result;
    }

    bool VisitVarDecl(clang::VarDecl *vd) {
        BorrowKind kind = getBorrowKind(vd);
        if (kind == BorrowKind::None)
            return true;

        std::string var_name = vd->getNameAsString();
        clang::SourceLocation vd_loc = vd->getLocation();

        if (kind == BorrowKind::Owner) {
            llvm::outs() << std::format(
                "{} [AST Scope {}] Registered Owner: {}\n", formatLoc(vd_loc),
                scope_depth, var_name);
            return true;
        }

        const clang::Expr *init = vd->getInit();
        const clang::VarDecl *origin_vd = extractOriginVar(init);

        if (!origin_vd) {
            llvm::errs() << std::format("{} [AST ERROR] Borrow '{}' target "
                                        "origin could not be resolved\n",
                                        formatLoc(vd_loc), var_name);
            return true;
        }

        std::string origin_name = origin_vd->getNameAsString();

        for (const auto &b : active_borrows) {
            if (b.origin_var != origin_name)
                continue;

            if (kind == BorrowKind::MutBorrow ||
                b.kind == BorrowKind::MutBorrow) {
                llvm::errs() << std::format(
                    "{} [BORROW VIOLATION] Cannot acquire {} '{}' on origin "
                    "'{}': conflict with active {} '{}' (declared at {})\n",
                    formatLoc(vd_loc),
                    (kind == BorrowKind::MutBorrow ? "MUT_BORROW" : "BORROW"),
                    var_name, origin_name,
                    (b.kind == BorrowKind::MutBorrow ? "MUT_BORROW" : "BORROW"),
                    b.borrow_var, formatLoc(b.decl_loc));
            }
        }

        active_borrows.push_back({.borrow_var = var_name,
                                  .origin_var = origin_name,
                                  .kind = kind,
                                  .scope_depth = scope_depth,
                                  .decl_loc = vd_loc});

        llvm::outs() << std::format(
            "{} [AST Scope {}] Registered Borrow: {} -> {} ({})\n",
            formatLoc(vd_loc), scope_depth, var_name, origin_name,
            kind == BorrowKind::MutBorrow ? "MUT" : "SHARED");
        return true;
    }

    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
        const clang::CXXMethodDecl *method = call->getMethodDecl();
        if (!method)
            return true;

        bool is_invalidating = hasAnnotation(method, "bc_invalidates");
        bool is_destroying = hasAnnotation(method, "bc_destroy");

        if (!is_invalidating && !is_destroying)
            return true;

        const clang::Expr *obj_expr = call->getImplicitObjectArgument();
        const clang::VarDecl *origin_vd = extractOriginVar(obj_expr);
        if (!origin_vd)
            return true;

        std::string origin_name = origin_vd->getNameAsString();
        clang::SourceLocation call_loc = call->getExprLoc();

        for (const auto &b : active_borrows) {
            if (b.origin_var == origin_name) {
                llvm::errs() << std::format(
                    "{} [BORROW VIOLATION] Method '{}' invalidates '{}' while "
                    "active borrow '{}' (declared at {}) exists in scope\n",
                    formatLoc(call_loc), method->getNameAsString(), origin_name,
                    b.borrow_var, formatLoc(b.decl_loc));
            }
        }
        return true;
    }
};

bool CFGVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *rd) {
    if (!rd->isCompleteDefinition())
        return true;

    for (const auto *field : rd->fields()) {
        BorrowKind kind = getBorrowKind(field);
        if (kind != BorrowKind::None) {
            llvm::outs() << std::format("Record '{}' Field '{}' annotated\n",
                                        rd->getNameAsString(),
                                        field->getNameAsString());
        }
    }
    rd->print(llvm::outs(), this->ctx.getLangOpts());
    llvm::outs() << "\n";
    return true;
}

bool CFGVisitor::VisitFunctionDecl(clang::FunctionDecl *fd) {
    if (!fd || !fd->hasBody())
        return true;

    llvm::outs() << std::format("Analyzing function {}\n",
                                fd->getQualifiedNameAsString());

    LexicalASTVisitor ast_visitor(this->ctx);
    ast_visitor.TraverseStmt(fd->getBody());

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

    // todo: output facts instead of lexical lifetimes
    for (const clang::CFGBlock *block : *cfg) {
        const auto block_id = block->getBlockID();

        for (size_t idx = 0; idx < block->size(); idx++) {
            Point p{block_id, idx};
            const clang::CFGElement &elem = (*block)[idx];

            if (auto cfg_stmt = elem.getAs<clang::CFGStmt>()) {
                if (const auto *ds =
                        llvm::dyn_cast<clang::DeclStmt>(cfg_stmt->getStmt())) {
                    for (const auto *decl : ds->decls()) {
                        if (const auto *vd =
                                llvm::dyn_cast<clang::VarDecl>(decl)) {
                            BorrowKind kind = getBorrowKind(vd);
                            if (kind != BorrowKind::None) {
                                llvm::outs() << std::format(
                                    "CFG Point ({}, {}): Decl {}\n", p.block,
                                    p.statement, vd->getNameAsString());
                            }
                        }
                    }
                }
            }
        }

        for (const clang::CFGBlock::AdjacentBlock &succ : block->succs()) {
            if (succ.isReachable() && succ.getReachableBlock()) {
                llvm::outs()
                    << std::format("CFG Edge {} -> {}\n", block_id,
                                   succ.getReachableBlock()->getBlockID());
            }
        }
    }

    llvm::outs() << "\n";
    return true;
}

ExtractedFactSet
run_extract(const clang::tooling::CompilationDatabase &compilations) {
    ExtractedFactSet facts = {};
    // clang::tooling::ClangTool Tool(Compilations, SourcePaths);

    // Tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
    //     {"-fsyntax-only", "-DBCHK_ANNOTATIONS=1"},
    //     clang::tooling::ArgumentInsertPosition::BEGIN));
    // return Tool.run(
    //     clang::tooling::newFrontendActionFactory<CFGAction>().get());

    return std::move(facts);
}

} // namespace bchk