#include "clang/Tooling/CompilationDatabase.h"
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
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

using clang::FunctionDecl;
using clang::Stmt;

class FactExtractorVisitor
    : public clang::RecursiveASTVisitor<FactExtractorVisitor> {
    clang::ASTContext &context;
    Interner &interner;
    Facts &facts;
    PointSourceMap &point_loc;

    std::string current_fn;
    Sym fn_exit = -1;
    Sym current_point = -1;
    ControlBuilder control;
    PlaceBuilder places;

    std::unordered_map<const Stmt *, Sym> stmt_pt;
    std::unordered_map<const clang::ValueDecl *, Sym> var_places;
    std::unordered_map<Sym, Sym> current_alloc;
    std::unordered_map<Sym, std::string> place_role;

  public:
    FactExtractorVisitor(clang::ASTContext &context, Interner &interner,
                         Facts &facts, PointSourceMap &point_loc)
        : context(context), interner(interner), facts(facts),
          point_loc(point_loc), control(interner, facts),
          places(interner, facts) {}

    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl *tu) {
        for (const clang::Decl *decl : tu->decls()) {
            if (const FunctionDecl *fd = clang::dyn_cast<FunctionDecl>(decl)) {
                if (fd->doesThisDeclarationHaveABody()) {
                    extractFunction(fd);
                } else {
                    // TODO: We don't need to error if we can see a function
                    // summary

                    const auto fname = fd->getNameAsString();
                    Sym pt = control.point(std::format("fn {}", fname));
                    recordPointLoc(pt, fd);
                    facts.unbodied_fn.push_back({pt, interner.intern(fname)});
                    continue;
                }
            }

            if (const clang::RecordDecl *r =
                    clang::dyn_cast<clang::RecordDecl>(decl)) {
                if (r->isUnion()) {
                    const auto uname = r->getNameAsString();
                    Sym pt = control.point(std::format("union {}", uname));
                    recordPointLoc(pt, r);
                    facts.banned_decl.push_back({pt, interner.intern(uname)});

                    continue;
                }
            }

            if (const clang::TypedefDecl *td =
                    clang::dyn_cast<clang::TypedefDecl>(decl)) {
                if (isUnionType(td->getUnderlyingType())) {
                    const auto uname = td->getNameAsString();
                    Sym pt = control.point(std::format("union {}", uname));
                    recordPointLoc(pt, td);
                    facts.banned_decl.push_back({pt, interner.intern(uname)});
                    continue;
                }
            }

            if (const clang::VarDecl *global_var =
                    clang::dyn_cast<clang::VarDecl>(decl)) {
                const clang::QualType type = global_var->getType();
                if (global_var->getTLSKind() !=
                        clang::VarDecl::TLSKind::TLS_None ||
                    type->isAtomicType()) {
                    const auto varname = global_var->getNameAsString();
                    Sym pt =
                        control.point(std::format("tls/atomic {}", varname));
                    recordPointLoc(pt, global_var);
                    facts.banned_decl.push_back({pt, interner.intern(varname)});
                }
                continue;
            }
        }

        return true;
    }

    bool VisitCallExpr(clang::CallExpr *ce) {
        if (current_point >= 0) {
            std::string name = calleeName(ce);
            control.call_site(current_point, name);
        }

        return true;
    }

  private:
    void extractFunction(const FunctionDecl *fd) {
        current_fn = fd->getNameAsString();
        const Sym entry = control.point("entry:" + current_fn);
        const Sym exit = control.point("exit:" + current_fn);
        fn_exit = exit;

        // TODO: This doesn't work after the interface change for recordPointLoc
        recordPointLoc(entry, fd);
        recordPointLoc(exit, fd);
        control.begin_fn(current_fn, entry, exit);

        // Gather the param annotations
        for (const clang::ParmVarDecl *param : fd->parameters()) {
            const Sym param_place = placeForVar(param);
            const std::string role = roleOf(param);
            if (!role.empty()) {
                places.role(param_place, role);
                place_role[param_place] = role;
            } else {
                clang::QualType type = param->getType();
                // TODO: isPointerType doesn't handle member pointers if this
                // ever supports C++
                if (type->isPointerType()) {
                    const auto pname = param->getNameAsString();
                    Sym pt = control.point(std::format("ptr {}", pname));
                    recordPointLoc(pt, param);
                    facts.ptr_missing_role.push_back(
                        {pt, interner.intern(pname)});
                }
            }
        }

        emitCfgEdges(fd, entry, exit);
        if (fd->getBody()) {
            TraverseStmt(fd->getBody());
        } else {
            const auto fname = fd->getNameAsString();
            Sym pt = control.point(std::format("fn {}", fname));
            recordPointLoc(pt, fd);
            facts.unbodied_fn.push_back({pt, interner.intern(fname)});
        }
    }

    void emitCfgEdges(const FunctionDecl *fd, Sym entry, Sym exit) {
        clang::CFG::BuildOptions opts;
        opts.AddLifetime = true;
        opts.AddScopes = true;

        std::unique_ptr<clang::CFG> cfg =
            clang::CFG::buildCFG(fd, fd->getBody(), &context, opts);
        if (!cfg) {
            const auto cfgname = fd->getNameAsString();
            Sym pt = control.point(
                std::format("couldn't build function cfg {}", cfgname));
            recordPointLoc(pt, fd);
            facts.err_internal.push_back({pt, interner.intern(cfgname)});
            return;
        }

        std::unordered_map<const clang::CFGBlock *, Sym> head;
        std::unordered_map<const clang::CFGBlock *, Sym> tail;

        head[&cfg->getEntry()] = entry;
        tail[&cfg->getEntry()] = entry;
        head[&cfg->getExit()] = exit;
        tail[&cfg->getExit()] = exit;

        for (const clang::CFGBlock *block : *cfg) {
            if (block == &cfg->getEntry() || block == &cfg->getExit()) {
                continue;
            }

            Sym first = -1;
            Sym last = -1;

            if (block->getLabel() != nullptr) {
                first = mintPoint(block->getLabel());
                last = first;
            }

            for (clang::CFGBlock::const_iterator it = block->begin();
                 it != block->end(); it++) {
                if (const std::optional<clang::CFGStmt> stmt =
                        it->getAs<clang::CFGStmt>()) {
                    const Sym p = mintPoint(stmt->getStmt());
                    if (first < 0)
                        first = p;
                    if (last >= 0)
                        facts.cfg_edge.push_back({last, p});

                    last = p;
                    current_point = p;
                }
                // TODO: handle failure to get CFG stmt?
            }

            if (const Stmt *term = block->getTerminatorStmt()) {
                const Sym p = mintPoint(term);
                if (first < 0)
                    first = p;
                if (last >= 0)
                    facts.cfg_edge.push_back({last, p});

                last = p;
            }

            if (first < 0) {
                first = last = control.point("block");
            }
            head[block] = first;
            tail[block] = last;
        }

        for (const clang::CFGBlock *block : *cfg) {
            for (const auto *succ_it = block->succ_begin();
                 succ_it != block->succ_end(); succ_it++) {
                if (*succ_it != nullptr) {
                    facts.cfg_edge.push_back({tail[block], head[*succ_it]});
                }
            }
        }
    }

    std::string roleOf(const clang::ValueDecl *decl) const {
        std::string role;
        for (const clang::Attr *attr : decl->attrs()) {
            if (const auto *annotated_attr =
                    clang::dyn_cast<clang::AnnotateAttr>(attr)) {
                std::string tag = annotated_attr->getAnnotation().str();
                if (tag != "bc_owner" && tag != "bc_borrow" &&
                    tag != "bc_mut_borrow")
                    continue;

                role = tag;
            }
        }
        return role;
    }

    Sym placeForVar(const clang::VarDecl *var_decl) {
        auto found = var_places.find(var_decl);
        if (found != var_places.end()) {
            return found->second;
        }

        Sym place;
        if (const clang::ParmVarDecl *parm =
                clang::dyn_cast<clang::ParmVarDecl>(var_decl)) {
            place =
                interner.intern("param:" + current_fn + "#" +
                                std::to_string(parm->getFunctionScopeIndex()));
            interner.set_display(place, var_decl->getNameAsString());
        } else { // non-param var
            const clang::VarDecl *var =
                clang::dyn_cast<clang::VarDecl>(var_decl);
            place =
                (var != nullptr && var->isFileVarDecl())
                    ? interner.intern(
                          var_decl->getNameAsString()) // deduplicate globals
                                                       // across all TUs
                    : places.base(
                          var_decl
                              ->getNameAsString()); // give locals a unique id
        }

        var_places[var_decl] = place;
        if (var_decl->hasInit() || (getAnnotation(var_decl) == "bc_init")) {
            facts.var_init.push_back({place});
        }

        if (var_decl->getType()->isPointerType()) {
            facts.pointer_decl.push_back({place});
            if (roleOf(var_decl) != "") {
                places.role(place, roleOf(var_decl));
            }
        }

        if (var_decl->hasGlobalStorage()) {
            // TODO: This also tracks static locals,
            // so maybe we can lift them to globals?
            facts.global_decl.push_back({place});
        }
        return place;
    }

    std::string calleeName(const clang::CallExpr *ce) const {
        if (const FunctionDecl *fd = ce->getDirectCallee()) {
            return fd->getNameAsString();
        }
        const auto *callee = ce->getCallee();
        return callee ? src(callee) : "?";
    }

    std::string src(const Stmt *stmt) const {
        const clang::SourceRange range = stmt->getSourceRange();
        if (!range.isValid()) {
            return "?";
        }
        return clang::Lexer::getSourceText(
                   clang::CharSourceRange::getTokenRange(range),
                   context.getSourceManager(), clang::LangOptions())
            .str();
    }

    Sym mintPoint(const Stmt *stmt) {
        const Sym p = control.point(tag(stmt));
        stmt_pt[stmt] = p;
        recordSourceRangeLoc(p, stmt->getSourceRange());
        return p;
    }

    void recordSourceRangeLoc(Sym point, const clang::SourceRange range) {
        if (range.isInvalid())
            return;
        const clang::PresumedLoc begin =
            context.getSourceManager().getPresumedLoc(range.getBegin());
        const clang::PresumedLoc end =
            context.getSourceManager().getPresumedLoc(range.getEnd());
        if (begin.isValid() && end.isValid()) {
            point_loc[point] = {begin.getFilename(), begin.getLine(),
                                begin.getColumn(), end.getLine(),
                                end.getColumn()};
        }
    }

    void recordPointLoc(Sym point, const clang::Decl *decl) {
        const auto range = decl->getSourceRange();
        recordSourceRangeLoc(point, range);
    }

    void logConsoleError(std::string error_str, clang::SourceLocation loc) {
        if (loc.isInvalid()) {
            llvm::errs() << "ERROR: " << error_str << " (unknown location)\n";
            return;
        }

        const clang::PresumedLoc pres =
            context.getSourceManager().getPresumedLoc(loc);
        if (!pres.isValid()) {
            llvm::errs() << "ERROR: " << error_str << " (invalid location)\n";
            return;
        }

        std::string location_str = std::format(
            "{}:{}:{}", pres.getFilename(), pres.getLine(), pres.getColumn());
        llvm::errs() << "ERROR: " << error_str << " at " << location_str
                     << "\n";
    }

    static bool isUnionType(clang::QualType qt) {
        const clang::QualType t = qt.getCanonicalType();
        if (const clang::RecordType *rt = t->getAs<clang::RecordType>()) {
            return rt->getDecl()->isUnion();
        }
        return false;
    }

    static std::string getAnnotation(const clang::Decl *decl) {
        for (const clang::Attr *attr : decl->attrs()) {
            if (const auto *annotated_attr =
                    clang::dyn_cast<clang::AnnotateAttr>(attr)) {
                return annotated_attr->getAnnotation().str();
            }
        }
        return "";
    }
    bool isAddressOf(const clang::Expr *expr) const {
        const clang::Expr *e = expr->IgnoreParenImpCasts();
        const clang::UnaryOperator *uo =
            clang::dyn_cast<clang::UnaryOperator>(e);
        return uo != nullptr && uo->getOpcode() == clang::UO_AddrOf;
    }

    const char *tag(const Stmt *stmt) {
        switch (stmt->getStmtClass()) {
        case Stmt::IfStmtClass:
            return "if";
        case Stmt::WhileStmtClass:
            return "while";
        case Stmt::ForStmtClass:
            return "for";
        case Stmt::SwitchStmtClass:
            return "switch";
        case Stmt::ReturnStmtClass:
            return "return";
        case Stmt::DeclStmtClass:
            return "decl";
        case Stmt::CallExprClass:
            return "call";
        case Stmt::BinaryOperatorClass:
            return "assign";
        case Stmt::CaseStmtClass:
            return "case";
        case Stmt::DefaultStmtClass:
            return "default";
        case Stmt::NullStmtClass:
            return "null";
        default:
            return "stmt";
        }
    }
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

std::unique_ptr<clang::FrontendAction> FactExtractorFactory::create() {
    return std::make_unique<FactExtractorAction>(interner, facts, point_loc);
}

} // namespace bchk