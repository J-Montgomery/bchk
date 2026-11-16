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
#include <clang/Lex/Lexer.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cfg_abstractions.hpp"
#include "extractor.hpp"
#include "rules.hpp"

namespace bchk {

using clang::FunctionDecl;
using clang::RecordDecl;
using clang::Stmt;
using clang::TypedefDecl;
using clang::VarDecl;

class FactExtractorVisitor
    : public clang::RecursiveASTVisitor<FactExtractorVisitor> {
  public:
    FactExtractorVisitor(clang::ASTContext &context, Interner &interner,
                         Facts &facts, PointSourceMap &point_loc)
        : context(context), interner(interner), facts(facts),
          point_loc(point_loc), control(interner, facts) {}

    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl *tu);

  private:
    void handleRecord(const RecordDecl *rd);
    void handleTypedef(const TypedefDecl *td);
    void handleGlobal(const VarDecl *global);
    void handleFunction(const FunctionDecl *fd);

    void extractFunction(const FunctionDecl *fd, Sym fn);
    void emitCfgEdges(const FunctionDecl *fd, Sym entry, Sym exit);

    void decodeStmt(const Stmt *stmt);
    void decodeIf(const clang::IfStmt *stmt, Sym point);
    void decodeWhile(const clang::WhileStmt *stmt, Sym point);
    void decodeDo(const clang::DoStmt *stmt, Sym point);
    void decodeFor(const clang::ForStmt *stmt, Sym point);
    void decodeSwitch(const clang::SwitchStmt *stmt, Sym point);
    void decodeReturn(const clang::ReturnStmt *stmt, Sym point);
    void decodeDecl(const clang::DeclStmt *stmt, Sym point);
    Sym decodeExpr(const clang::Expr *expr, Sym point);
    Sym decodeBinary(const clang::BinaryOperator *expr, Sym point);
    Sym decodeAssign(const clang::BinaryOperator *expr, Sym point);
    Sym decodeCompoundAssign(const clang::BinaryOperator *expr, Sym point);
    void decodeLValue(const clang::Expr *expr, Sym point);
    Sym decodeCall(const clang::CallExpr *expr, Sym point);

    Sym mintPoint(const Stmt *stmt);
    Sym placeForVar(const VarDecl *var_decl);
    Sym placeOf(const clang::Expr *expr);
    Sym fieldPlace(Sym base, const clang::FieldDecl *field);
    Sym elemPlace(Sym base, Sym index, bool constant);
    void emitValueType(Sym value, clang::QualType type);
    void emitValueLiteral(Sym value, const clang::Expr *expr);
    std::string calleeName(const clang::CallExpr *expr) const;
    void unsupportedAt(Sym point, std::string_view reason);
    void bindAggregateInit(Sym place, clang::QualType type,
                           const clang::InitListExpr *init, Sym point);
    Sym derefPlace(Sym place);
    bool tryArgPlace(const clang::Expr *expr, Sym &place);

    static std::string_view roleForAnnotation(llvm::StringRef annotation);

    void recordSourceRangeLoc(Sym point, const clang::SourceRange range);
    void recordPointLoc(Sym point, const clang::Decl *decl);
    void declKind(Sym place, std::string_view kind, std::string_view name);
    Sym typeSym(clang::QualType qt);
    Sym argIndex(size_t index);
    Sym stmtPoint(const Stmt *stmt) const;

    bool isPointerType(clang::QualType qt) const;
    bool typeContainsPointer(clang::QualType qt) const;
    bool arrayElementContainsPointer(clang::QualType qt) const;
    bool hasLifetimeBound(const clang::Decl *decl) const;
    bool hasRole(const clang::Decl *decl) const;
    bool hasRole(clang::QualType qt) const;

    void emitDeclFacts(const clang::Decl *decl, Sym place, clang::QualType qt);
    void emitAnnotations(const clang::Decl *decl, Sym target);
    void emitParameterAnnotations(const FunctionDecl *fd, unsigned index,
                                  Sym place);

    bool isAddressOf(const clang::Expr *expr) const;
    bool isStoreTarget(const clang::Expr *expr) const;
    const char *tag(const Stmt *stmt);

    Sym readValue(Sym place, Sym point);
    Sym valueCall(const std::string &callee, Sym point);
    Sym valueAddr(Sym target, Sym point);
    Sym valueArith(Sym base, Sym point);
    Sym valueCast(Sym source, std::string_view kind, Sym point);
    Sym valueConst(std::string_view kind);
    Sym valueUnknown();
    Sym freshValue(std::string_view prefix);
    void eventBind(Sym place, Sym value, Sym point);
    void eventStore(Sym place, Sym value, Sym point);
    void writePlace(Sym place, Sym value, Sym point, bool store);
    clang::ASTContext &context;
    Interner &interner;
    Facts &facts;
    PointSourceMap &point_loc;

    std::string current_fn;
    Sym current_fn_sym = -1;
    Sym fn_exit = -1;
    Sym current_point = -1;

    ControlBuilder control;

    std::unordered_map<const Stmt *, Sym> stmt_pt;
    std::unordered_map<const clang::ValueDecl *, Sym> var_places;
    std::unordered_map<const clang::Expr *, Sym> expr_values;
    std::unordered_map<std::string, Sym> place_cache;
    std::unordered_map<std::string, Sym> type_cache;

    // These are used to de-duplicate how many objs we track
    // so we don't waste time processing unnecessary facts
    // on the datalog side
    std::unordered_set<Sym> declared_fn;
    std::unordered_set<const clang::TagDecl *> handled_records;
    std::unordered_set<Sym> checked_return_role;
    std::unordered_set<Sym> variadic_fn;
    std::unordered_set<Sym> emitted_decl_facts;
    unsigned value_seq = 0;
};

/****************** Extractor public visitors ******************/

bool FactExtractorVisitor::VisitTranslationUnitDecl(
    clang::TranslationUnitDecl *tu) {
    for (const clang::Decl *decl : tu->decls()) {
        if (const auto *fd = dyn_cast<FunctionDecl>(decl)) {
            handleFunction(fd);
        } else if (const auto *global = dyn_cast<VarDecl>(decl)) {
            handleGlobal(global);
        } else if (const auto *record = dyn_cast<RecordDecl>(decl)) {
            handleRecord(record);
        } else if (const auto *typedef_decl = dyn_cast<TypedefDecl>(decl)) {
            handleTypedef(typedef_decl);
        }
    }
    return false;
}

void FactExtractorVisitor::handleRecord(const RecordDecl *rd) {
    if (!handled_records.insert(rd->getCanonicalDecl()).second) {
        return;
    }
    const std::string name = rd->getNameAsString();
    const Sym at = control.point((rd->isUnion() ? "union:" : "struct:") + name);
    recordPointLoc(at, rd);
    if (rd->isUnion()) {
        facts.union_decl.push_back({at, interner.intern(name)});
    } else {
        facts.struct_decl.push_back({at, interner.intern(name)});
    }
    typeSym(context.getRecordType(rd));
}

void FactExtractorVisitor::handleTypedef(const TypedefDecl *td) {
    const Sym alias = interner.intern("type-alias:" + td->getNameAsString());
    if (const auto *record =
            td->getUnderlyingType()->getAs<clang::RecordType>()) {
        handleRecord(record->getDecl());
    }
    facts.typedef_type.push_back({alias, typeSym(td->getUnderlyingType())});
    emitAnnotations(td, alias);
    for (const clang::Attr *attr : td->attrs()) {
        if (const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr)) {
            facts.type_annotation.push_back(
                {alias, interner.intern(annotated->getAnnotation().str())});
        }
    }
}

void FactExtractorVisitor::handleGlobal(const VarDecl *global) {
    const Sym place = placeForVar(global);
    facts.global_decl.push_back({place});
    facts.global_var.push_back({place});

    const std::string name = global->getNameAsString();
    const Sym entry = control.point("global-entry:" + name);
    recordPointLoc(entry, global);
    facts.global_entry.push_back({place, entry});

    if (typeContainsPointer(global->getType()) &&
        !isPointerType(global->getType())) {
        declKind(place, "global pointer type", name);
    }

    if (global->hasInit()) {
        facts.var_init.push_back({place});
        current_fn.clear();
        current_fn_sym = -1;
        fn_exit = -1;
        current_point = entry;
        const clang::Expr *init = global->getInit();
        if (const auto *list = dyn_cast<clang::InitListExpr>(
                init->IgnoreParenImpCasts())) {
            bindAggregateInit(place, global->getType(), list, entry);
        } else {
            eventBind(place, decodeExpr(init, entry), entry);
        }
        if (isa<clang::CallExpr>(init->IgnoreParenImpCasts())) {
            facts.call_result_place.push_back({entry, place});
        }
    }
}

void FactExtractorVisitor::handleFunction(const FunctionDecl *fd) {
    const std::string name = fd->getNameAsString();
    const Sym fn = interner.intern(name);
    const bool first_declaration = declared_fn.insert(fn).second;
    const FunctionDecl *signature = fd->getDefinition();
    if (signature == nullptr) {
        signature = fd->getCanonicalDecl();
    }
    if (first_declaration) {
        const Sym decl_pt = control.point("fn-decl:" + name);
        recordPointLoc(decl_pt, fd);
        facts.fn_decl.push_back({fn, decl_pt});
    }
    if (fd->isVariadic() && variadic_fn.insert(fn).second) {
        facts.fn_variadic.push_back({fn});
    }
    if (fd->isNoReturn()) {
        facts.fn_noreturn.push_back({fn});
    }

    facts.fn_return_type.push_back({fn, typeSym(fd->getReturnType())});
    bool return_role = false;
    bool return_init = false;
    bool lifetime_bound = false;
    bool pointer_return = false;
    for (const FunctionDecl *redecl : fd->redecls()) {
        emitAnnotations(redecl, fn);
        lifetime_bound |= hasLifetimeBound(redecl);
        return_role |= hasRole(redecl->getReturnType());
        pointer_return |= isPointerType(redecl->getReturnType());
        for (const clang::Attr *attr : redecl->attrs()) {
            const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
            if (annotated == nullptr) {
                continue;
            }
            const llvm::StringRef tag = annotated->getAnnotation();
            const std::string_view role = roleForAnnotation(tag);
            if (!role.empty()) {
                facts.fn_return_role.push_back({fn, interner.intern(role)});
                return_role = true;
            }
            return_init |= tag == "bc_init";
        }
        return_role |= hasRole(redecl);
        for (clang::QualType current = redecl->getReturnType();
             !current.isNull();) {
            const auto *alias = current->getAs<clang::TypedefType>();
            if (alias == nullptr) {
                break;
            }
            const auto *typedef_decl = alias->getDecl();
            const Sym alias_sym = interner.intern(
                "type-alias:" + typedef_decl->getNameAsString());
            for (const clang::Attr *attr : typedef_decl->attrs()) {
                const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
                if (annotated == nullptr) {
                    continue;
                }
                const llvm::StringRef tag = annotated->getAnnotation();
                facts.type_annotation.push_back(
                    {alias_sym, interner.intern(tag.str())});
                const std::string_view role = roleForAnnotation(tag);
                if (!role.empty()) {
                    facts.fn_return_role.push_back(
                        {fn, interner.intern(role)});
                    return_role = true;
                }
                return_init |= tag == "bc_init";
                lifetime_bound |= tag == "bc_lifetime_bound" ||
                                  tag == "bchk_lifetime_bound";
            }
            current = typedef_decl->getUnderlyingType();
        }
    }
    if (return_init) {
        facts.fn_return_init.push_back({fn});
    }
    if (pointer_return && !return_role && checked_return_role.insert(fn).second) {
        const Sym at = control.point("ptr-missing-role:" + name + " return");
        recordPointLoc(at, fd);
        facts.ptr_missing_role.push_back(
            {at, interner.intern(name + " return")});
    }

    if (first_declaration) {
        for (unsigned i = 0; i < signature->getNumParams(); ++i) {
            const clang::ParmVarDecl *param = signature->getParamDecl(i);
            const Sym place = placeForVar(param);
            facts.param_idx.push_back({place, fn, argIndex(i)});
            emitParameterAnnotations(fd, i, place);
            bool param_lifetime_bound = lifetime_bound ||
                                        hasLifetimeBound(param);
            for (const FunctionDecl *redecl : fd->redecls()) {
                if (i < redecl->getNumParams()) {
                    param_lifetime_bound |=
                        hasLifetimeBound(redecl->getParamDecl(i));
                }
            }
            if (isPointerType(param->getType()) && param_lifetime_bound &&
                isPointerType(signature->getReturnType())) {
                facts.fn_lifetime_bound.push_back({fn, place});
                facts.fn_return_lifetime_bound.push_back({fn, place});
            }
        }
    }
    if (fd->doesThisDeclarationHaveABody()) {
        extractFunction(fd, fn);
    }
}

void FactExtractorVisitor::extractFunction(const FunctionDecl *fd, Sym fn) {
    current_fn = fd->getNameAsString();
    current_fn_sym = fn;

    const Sym entry = control.point("entry:" + current_fn);
    const Sym exit = control.point("exit:" + current_fn);
    fn_exit = exit;
    recordPointLoc(entry, fd);
    recordPointLoc(exit, fd);

    facts.fn_defined.push_back({fn});
    facts.fn_entry.push_back({fn, entry});
    facts.fn_exit.push_back({fn, exit});

    for (unsigned i = 0; i < fd->getNumParams(); ++i) {
        const clang::ParmVarDecl *param = fd->getParamDecl(i);
        const Sym place = placeForVar(param);
        facts.param_entry.push_back({place, fn, entry});
        if (param->isKNRPromoted()) {
            facts.knr_promoted.push_back({place, fn});
        }

        const auto type = param->getType();
        if (arrayElementContainsPointer(type)) {
            declKind(place, "array containing pointers",
                     param->getNameAsString());
        }
        if (type.getCanonicalType()->isRecordType() && typeContainsPointer(type)) {
            declKind(place, "struct containing pointer",
                     param->getNameAsString());
        }
    }

    current_point = entry;
    emitCfgEdges(fd, entry, exit);
    decodeStmt(fd->getBody());
}

void FactExtractorVisitor::emitCfgEdges(const FunctionDecl *fd, Sym entry,
                                        Sym exit) {
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
            const clang::CFGElement element = *it;
            if (const std::optional<clang::CFGStmt> stmt =
                    element.getAs<clang::CFGStmt>()) {
                const Sym pt = mintPoint(stmt->getStmt());

                if (first < 0)
                    first = pt;
                if (last >= 0)
                    facts.cfg_edge.push_back({last, pt});

                last = pt;
            } else if (const std::optional<clang::CFGLifetimeEnds> lifetime =
                           element.getAs<clang::CFGLifetimeEnds>()) {
                const VarDecl *var_decl = lifetime->getVarDecl();
                if (var_decl == nullptr) {
                    continue;
                }
                const Sym pt = control.point("lifetime-end:" +
                                             var_decl->getNameAsString());
                if (first < 0) {
                    first = pt;
                }
                if (last >= 0) {
                    facts.cfg_edge.push_back({last, pt});
                }

                last = pt;
                facts.lifetime_end.push_back({placeForVar(var_decl), pt});
            }
        }

        if (const Stmt *terminator = block->getTerminatorStmt()) {
            const Sym pt = mintPoint(terminator);
            if (first < 0)
                first = pt;
            if (last >= 0)
                facts.cfg_edge.push_back({last, pt});

            last = pt;
        }

        if (first < 0) {
            first = control.point("block");
            last = first;
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

    std::unordered_map<const Stmt *, const clang::CFGBlock *> hdr_block;
    for (const clang::CFGBlock *block : *cfg) {
        if (const Stmt *terminator = block->getTerminatorStmt()) {
            // hasta la vista, baby
            hdr_block[terminator] = block;
        }
    }

    for (const clang::CFGBlock *block : *cfg) {
        const Stmt *loop = block->getLoopTarget();
        if (loop == nullptr) {
            continue;
        }

        const auto hdr = hdr_block.find(loop);
        if (hdr == hdr_block.end()) {
            continue;
        }

        facts.loop_back_edge.push_back({head[hdr->second], tail[block]});
    }
}

void FactExtractorVisitor::decodeStmt(const Stmt *stmt) {
    if (stmt == nullptr) {
        return;
    }
    const Sym point = stmtPoint(stmt);
    current_point = point;
    if (const auto *compound = dyn_cast<clang::CompoundStmt>(stmt)) {
        for (const Stmt *child : compound->body()) {
            decodeStmt(child);
        }
    } else if (const auto *s = dyn_cast<clang::IfStmt>(stmt)) {
        decodeIf(s, point);
    } else if (const auto *s = dyn_cast<clang::WhileStmt>(stmt)) {
        decodeWhile(s, point);
    } else if (const auto *s = dyn_cast<clang::DoStmt>(stmt)) {
        decodeDo(s, point);
    } else if (const auto *s = dyn_cast<clang::ForStmt>(stmt)) {
        decodeFor(s, point);
    } else if (const auto *s = dyn_cast<clang::SwitchStmt>(stmt)) {
        decodeSwitch(s, point);
    } else if (const auto *s = dyn_cast<clang::ReturnStmt>(stmt)) {
        decodeReturn(s, point);
    } else if (const auto *s = dyn_cast<clang::DeclStmt>(stmt)) {
        decodeDecl(s, point);
    } else if (const auto *s = dyn_cast<clang::CaseStmt>(stmt)) {
        decodeExpr(s->getLHS(), point);
        if (s->getRHS() != nullptr) {
            decodeExpr(s->getRHS(), point);
        }
        decodeStmt(s->getSubStmt());
    } else if (const auto *s = dyn_cast<clang::DefaultStmt>(stmt)) {
        decodeStmt(s->getSubStmt());
    } else if (const auto *s = dyn_cast<clang::LabelStmt>(stmt)) {
        decodeStmt(s->getSubStmt());
    } else if (const auto *s = dyn_cast<clang::AttributedStmt>(stmt)) {
        decodeStmt(s->getSubStmt());
    } else if (const auto *s = dyn_cast<clang::IndirectGotoStmt>(stmt)) {
        decodeExpr(s->getTarget(), point);
    } else if (const auto *expr = dyn_cast<clang::Expr>(stmt)) {
        decodeExpr(expr, point);
    } else if (!isa<clang::NullStmt>(stmt) && !isa<clang::BreakStmt>(stmt) &&
               !isa<clang::ContinueStmt>(stmt) && !isa<clang::GotoStmt>(stmt)) {
        unsupportedAt(point, std::string("unsupported statement: ") +
                                stmt->getStmtClassName());
    }
}

void FactExtractorVisitor::decodeLValue(const clang::Expr *expr, Sym point) {
    const clang::Expr *e = expr->IgnoreParenImpCasts();
    if (const auto *member = dyn_cast<clang::MemberExpr>(e)) {
        if (member->isArrow()) {
            decodeExpr(member->getBase(), point);
        }
    } else if (const auto *subscript = dyn_cast<clang::ArraySubscriptExpr>(e)) {
        decodeExpr(subscript->getBase(), point);
        decodeExpr(subscript->getIdx(), point);
    } else if (const auto *unary = dyn_cast<clang::UnaryOperator>(e);
               unary != nullptr && unary->getOpcode() == clang::UO_Deref) {
        decodeExpr(unary->getSubExpr(), point);
    }
}

void FactExtractorVisitor::decodeIf(const clang::IfStmt *stmt, Sym point) {
    if (stmt->getInit() != nullptr) {
        decodeStmt(stmt->getInit());
    }
    if (stmt->getCond() != nullptr) {
        decodeExpr(stmt->getCond(), point);
    }
    decodeStmt(stmt->getThen());
    decodeStmt(stmt->getElse());
}

void FactExtractorVisitor::decodeWhile(const clang::WhileStmt *stmt,
                                       Sym point) {
    decodeExpr(stmt->getCond(), point);
    decodeStmt(stmt->getBody());
}

void FactExtractorVisitor::decodeDo(const clang::DoStmt *stmt, Sym point) {
    decodeStmt(stmt->getBody());
    decodeExpr(stmt->getCond(), point);
    current_point = point;
}

void FactExtractorVisitor::decodeFor(const clang::ForStmt *stmt, Sym point) {
    if (const Stmt *init = stmt->getInit()) {
        if (isa<clang::DeclStmt>(init)) {
            decodeStmt(init);
        } else if (const auto *expr = dyn_cast<clang::Expr>(init)) {
            decodeExpr(expr, point);
        }
    }
    if (stmt->getCond() != nullptr) {
        decodeExpr(stmt->getCond(), point);
    }
    decodeStmt(stmt->getBody());
    if (stmt->getInc() != nullptr) {
        decodeExpr(stmt->getInc(), point);
    }
}

void FactExtractorVisitor::decodeSwitch(const clang::SwitchStmt *stmt,
                                        Sym point) {
    if (stmt->getInit() != nullptr) {
        decodeStmt(stmt->getInit());
    }
    decodeExpr(stmt->getCond(), point);
    decodeStmt(stmt->getBody());
}

void FactExtractorVisitor::decodeReturn(const clang::ReturnStmt *stmt,
                                        Sym point) {
    const clang::Expr *expr = stmt->getRetValue();
    if (expr == nullptr) {
        return;
    }
    const clang::Expr *value_expr = expr->IgnoreParenImpCasts();
    const Sym value = decodeExpr(expr, point);
    facts.return_value.push_back({current_fn_sym, value, point});
    Sym place = -1;
    if (tryArgPlace(value_expr, place)) {
        facts.return_place.push_back({current_fn_sym, place, point});
    }
    if (const auto *call = dyn_cast<clang::CallExpr>(value_expr)) {
        if (const FunctionDecl *direct = call->getDirectCallee()) {
            facts.return_call.push_back(
                {current_fn_sym, interner.intern(direct->getNameAsString()),
                 point});
        }
    }
}

void FactExtractorVisitor::decodeDecl(const clang::DeclStmt *stmt,
                                      Sym point) {
    for (const clang::Decl *decl : stmt->decls()) {
        if (const auto *record = dyn_cast<RecordDecl>(decl)) {
            handleRecord(record);
            continue;
        }
        if (const auto *typedef_decl = dyn_cast<TypedefDecl>(decl)) {
            handleTypedef(typedef_decl);
            continue;
        }
        const auto *var = dyn_cast<VarDecl>(decl);
        if (var == nullptr) {
            continue;
        }
        const Sym place = placeForVar(var);
        facts.local_var.push_back({current_fn_sym, place});
        const std::string name = var->getNameAsString();
        if (arrayElementContainsPointer(var->getType())) {
            declKind(place, "array containing pointers", name);
        }
        if (var->getType().getCanonicalType()->isRecordType() &&
            typeContainsPointer(var->getType())) {
            declKind(place, "struct containing pointer", name);
        }
        if (!var->hasInit()) {
            continue;
        }
        facts.var_init.push_back({place});
        const clang::Expr *initializer = var->getInit();
        const clang::Expr *init = initializer->IgnoreParenImpCasts();
        if (const auto *list = dyn_cast<clang::InitListExpr>(init)) {
            bindAggregateInit(place, var->getType(), list, point);
        } else {
            const Sym value = decodeExpr(initializer, point);
            eventBind(place, value, point);
            Sym source = -1;
            if (tryArgPlace(initializer, source)) {
                facts.event_copy.push_back({place, source, point});
            }
            if (isa<clang::CallExpr>(init)) {
                facts.call_result_place.push_back({point, place});
            }
        }
    }
}

void FactExtractorVisitor::bindAggregateInit(
    Sym place, clang::QualType type, const clang::InitListExpr *init,
    Sym point) {
    const clang::QualType canonical = type.getCanonicalType();
    if (const auto *record = canonical->getAs<clang::RecordType>()) {
        const RecordDecl *definition = record->getDecl()->getDefinition();
        if (definition != nullptr) {
            auto field = definition->field_begin();
            for (unsigned i = 0; i < init->getNumInits() &&
                                 field != definition->field_end();
                 ++i, ++field) {
                const clang::Expr *value = init->getInit(i);
                if (value == nullptr) {
                    continue;
                }
                const Sym child = fieldPlace(place, *field);
                if (const auto *nested = dyn_cast<clang::InitListExpr>(
                        value->IgnoreParenImpCasts())) {
                    bindAggregateInit(child, (*field)->getType(), nested,
                                      point);
                } else {
                    const Sym source = decodeExpr(value, point);
                    eventBind(child, source, point);
                    Sym from_place = -1;
                    if (tryArgPlace(value, from_place)) {
                        facts.event_copy.push_back({child, from_place, point});
                    }
                }
            }
        }
    } else if (const auto *array =
                   dyn_cast<clang::ArrayType>(canonical.getTypePtr())) {
        for (unsigned i = 0; i < init->getNumInits(); ++i) {
            const clang::Expr *value = init->getInit(i);
            if (value == nullptr) {
                continue;
            }
            const Sym index = interner.intern("index:" + std::to_string(i));
            const Sym child = elemPlace(place, index, true);
            if (const auto *nested = dyn_cast<clang::InitListExpr>(
                    value->IgnoreParenImpCasts())) {
                bindAggregateInit(child, array->getElementType(), nested,
                                  point);
            } else {
                eventBind(child, decodeExpr(value, point), point);
            }
        }
    }
    eventBind(place, valueUnknown(), point);
}

Sym FactExtractorVisitor::placeForVar(const VarDecl *var_decl) {
    if (const auto found = var_places.find(var_decl); found != var_places.end()) {
        return found->second;
    }
    Sym place;
    if (const auto *param = dyn_cast<clang::ParmVarDecl>(var_decl)) {
        const auto *owner = dyn_cast<FunctionDecl>(param->getDeclContext());
        const std::string fn =
            owner != nullptr ? owner->getNameAsString() : current_fn;
        place = interner.intern("param:" + fn + "#" +
                                std::to_string(param->getFunctionScopeIndex()));
        interner.set_display(place, var_decl->getNameAsString());
    } else if (var_decl->isFileVarDecl() || var_decl->hasExternalStorage()) {
        place = interner.intern(var_decl->getNameAsString());
    } else {
        place = interner.freshen(var_decl->getNameAsString());
    }
    var_places[var_decl] = place;
    if (emitted_decl_facts.insert(place).second) {
        emitDeclFacts(var_decl, place, var_decl->getType());
    }
    return place;
}

Sym FactExtractorVisitor::placeOf(const clang::Expr *expr) {
    const clang::Expr *e = expr->IgnoreParenImpCasts();
    if (!e->isLValue() && isPointerType(e->getType()) &&
        !isa<clang::BinaryOperator>(e)) {
        if (const auto *cast = dyn_cast<clang::CastExpr>(e);
            cast == nullptr || cast->getCastKind() != clang::CK_ArrayToPointerDecay) {
            if (const auto found = expr_values.find(e);
                found != expr_values.end()) {
                return found->second;
            }
        }
    }
    if (const auto *ref = dyn_cast<clang::DeclRefExpr>(e)) {
        if (const auto *var = dyn_cast<VarDecl>(ref->getDecl())) {
            return placeForVar(var);
        }
        if (const auto *fn = dyn_cast<FunctionDecl>(ref->getDecl())) {
            return interner.intern("function:" + fn->getNameAsString());
        }
    }
    if (const auto *literal = dyn_cast<clang::StringLiteral>(e)) {
        const std::string key = "string:" +
                                std::to_string(literal->getBeginLoc().getRawEncoding());
        if (const auto found = place_cache.find(key); found != place_cache.end()) {
            return found->second;
        }
        const Sym place = interner.freshen("string literal");
        place_cache.emplace(key, place);
        facts.global_decl.push_back({place});
        facts.global_var.push_back({place});
        facts.place_type.push_back({place, typeSym(e->getType())});
        return place;
    }
    if (const auto *compound = dyn_cast<clang::CompoundLiteralExpr>(e)) {
        const std::string key = "compound:" +
                                std::to_string(compound->getBeginLoc().getRawEncoding());
        if (const auto found = place_cache.find(key); found != place_cache.end()) {
            return found->second;
        }
        const Sym place = interner.freshen("compound");
        place_cache.emplace(key, place);
        facts.place_type.push_back({place, typeSym(e->getType())});
        facts.local_var.push_back({current_fn_sym, place});
        facts.var_init.push_back({place});
        const clang::Expr *init = compound->getInitializer();
        if (const auto *list = dyn_cast<clang::InitListExpr>(
                init->IgnoreParenImpCasts())) {
            bindAggregateInit(place, compound->getType(), list, current_point);
        } else {
            eventBind(place, decodeExpr(init, current_point), current_point);
        }
        return place;
    }
    if (const auto *member = dyn_cast<clang::MemberExpr>(e)) {
        Sym base = placeOf(member->getBase());
        if (base < 0) {
            return base;
        }
        if (member->isArrow() &&
            !isa<clang::BinaryOperator>(
                member->getBase()->IgnoreParenImpCasts())) {
            base = derefPlace(base);
        }
        return fieldPlace(base, dyn_cast<clang::FieldDecl>(
                                    member->getMemberDecl()));
    }
    if (const auto *subscript = dyn_cast<clang::ArraySubscriptExpr>(e)) {
        const Sym base = placeOf(subscript->getBase());
        if (base < 0) {
            return base;
        }
        const clang::Expr *index = subscript->getIdx()->IgnoreParenImpCasts();
        const bool constant = isa<clang::IntegerLiteral>(index);
        const llvm::StringRef text = clang::Lexer::getSourceText(
            clang::CharSourceRange::getTokenRange(index->getSourceRange()),
            context.getSourceManager(), context.getLangOpts());
        const Sym child = elemPlace(base, interner.intern(text.str()), constant);
        facts.place_type.push_back({child, typeSym(e->getType())});
        if (isPointerType(e->getType())) {
            facts.pointer_place.push_back({child});
        }
        return child;
    }
    if (const auto *unary = dyn_cast<clang::UnaryOperator>(e);
        unary != nullptr && unary->getOpcode() == clang::UO_Deref) {
        const clang::Expr *operand = unary->getSubExpr()->IgnoreParenImpCasts();
        Sym child = -1;
        if (const auto *binary = dyn_cast<clang::BinaryOperator>(operand);
            binary != nullptr &&
            (binary->getOpcode() == clang::BO_Add ||
             binary->getOpcode() == clang::BO_Sub)) {
            child = placeOf(binary);
        } else {
            child = derefPlace(placeOf(operand));
        }
        if (child >= 0) {
            facts.place_type.push_back({child, typeSym(e->getType())});
            if (isPointerType(e->getType())) {
                facts.pointer_place.push_back({child});
            }
        }
        return child;
    }
    if (const auto *cast = dyn_cast<clang::CastExpr>(e)) {
        return placeOf(cast->getSubExpr());
    }
    if (const auto *binary = dyn_cast<clang::BinaryOperator>(e);
        binary != nullptr &&
        (binary->getOpcode() == clang::BO_Add ||
         binary->getOpcode() == clang::BO_Sub)) {
        const clang::Expr *base_expr = isPointerType(binary->getLHS()->getType())
                                           ? binary->getLHS()
                                           : binary->getRHS();
        const clang::Expr *index_expr = base_expr == binary->getLHS()
                                            ? binary->getRHS()
                                            : binary->getLHS();
        const Sym base = placeOf(base_expr);
        if (base < 0) {
            return base;
        }
        const clang::Expr *index = index_expr->IgnoreParenImpCasts();
        const bool constant = isa<clang::IntegerLiteral>(index);
        const llvm::StringRef text = clang::Lexer::getSourceText(
            clang::CharSourceRange::getTokenRange(index->getSourceRange()),
            context.getSourceManager(), context.getLangOpts());
        return elemPlace(base, interner.intern(text.str()), constant);
    }
    unsupportedAt(stmtPoint(e), std::string("unsupported place: ") +
                                   e->getStmtClassName());
    return -1;
}

Sym FactExtractorVisitor::fieldPlace(Sym base,
                                     const clang::FieldDecl *field) {
    if (field == nullptr) {
        return -1;
    }
    const unsigned index = field->getFieldIndex();
    const std::string key = "field:" + std::to_string(base) + ":" +
                            std::to_string(index);
    if (const auto found = place_cache.find(key); found != place_cache.end()) {
        return found->second;
    }
    const std::string name = field->getNameAsString();
    const Sym label = interner.intern(
        name.empty() ? "field:" + std::to_string(index) : name);
    const Sym child = interner.freshen(
        std::string(interner.lookup(base)) + "." +
        std::string(interner.lookup(label)));
    facts.place_field.push_back({base, label, child});
    emitDeclFacts(field, child, field->getType());
    place_cache.emplace(key, child);
    return child;
}

Sym FactExtractorVisitor::elemPlace(Sym base, Sym index, bool constant) {
    const std::string key = "element:" + std::to_string(base) + ":" +
                            (constant ? std::to_string(index) : "*");
    if (const auto found = place_cache.find(key); found != place_cache.end()) {
        return found->second;
    }
    const Sym child = interner.freshen(
        std::string(interner.lookup(base)) +
        (constant ? "[" + std::string(interner.lookup(index)) + "]" : "[*]"));
    if (constant) {
        facts.place_elem.push_back({base, index, child});
    } else {
        facts.place_bucket.push_back(
            {base, interner.intern("bucket:" +
                                   std::string(interner.lookup(base))),
             child});
    }
    place_cache.emplace(key, child);
    return child;
}

Sym FactExtractorVisitor::derefPlace(Sym place) {
    if (place < 0) {
        return place;
    }
    const std::string key = "deref:" + std::to_string(place);
    if (const auto found = place_cache.find(key); found != place_cache.end()) {
        return found->second;
    }
    const Sym child = interner.freshen("*" +
                                       std::string(interner.lookup(place)));
    facts.place_deref.push_back({place, child});
    place_cache.emplace(key, child);
    return child;
}

bool FactExtractorVisitor::tryArgPlace(const clang::Expr *expr, Sym &place) {
    const clang::Expr *e = expr->IgnoreParenImpCasts();
    if (isa<clang::DeclRefExpr>(e) || isa<clang::MemberExpr>(e) ||
        isa<clang::ArraySubscriptExpr>(e)) {
        place = placeOf(e);
        return place >= 0;
    }
    if (const auto *unary = dyn_cast<clang::UnaryOperator>(e)) {
        if (unary->getOpcode() == clang::UO_Deref) {
            place = placeOf(e);
            return place >= 0;
        }
        if (unary->getOpcode() == clang::UO_AddrOf) {
            place = placeOf(unary->getSubExpr());
            return place >= 0;
        }
    }
    return false;
}
Sym FactExtractorVisitor::decodeExpr(const clang::Expr *expr, Sym point) {
    if (expr == nullptr) {
        return valueUnknown();
    }
    if (const auto found = stmt_pt.find(expr); found != stmt_pt.end()) {
        point = found->second;
    }
    current_point = point;
    const clang::Expr *e = expr->IgnoreParens();
    if (const auto found = expr_values.find(e); found != expr_values.end()) {
        return found->second;
    }
    Sym value = -1;
    if (const auto *ref = dyn_cast<clang::DeclRefExpr>(e)) {
        if (const auto *var = dyn_cast<VarDecl>(ref->getDecl())) {
            value = readValue(placeForVar(var), point);
        } else if (isa<clang::EnumConstantDecl>(ref->getDecl())) {
            value = valueConst("enum");
        } else if (isa<FunctionDecl>(ref->getDecl())) {
            value = valueAddr(placeOf(e), point);
        }
    } else if (const auto *member = dyn_cast<clang::MemberExpr>(e)) {
        if (member->isArrow()) {
            decodeExpr(member->getBase(), point);
        }
        value = readValue(placeOf(e), point);
    } else if (const auto *subscript = dyn_cast<clang::ArraySubscriptExpr>(e)) {
        decodeExpr(subscript->getBase(), point);
        decodeExpr(subscript->getIdx(), point);
        value = readValue(placeOf(e), point);
    } else if (const auto *unary = dyn_cast<clang::UnaryOperator>(e)) {
        if (unary->getOpcode() == clang::UO_AddrOf) {
            const clang::Expr *operand = unary->getSubExpr()->IgnoreParenImpCasts();
            decodeLValue(operand, point);
            const Sym target = placeOf(operand);
            value = target >= 0 ? valueAddr(target, point) : valueUnknown();
        } else if (unary->getOpcode() == clang::UO_Deref) {
            decodeExpr(unary->getSubExpr(), point);
            value = readValue(placeOf(e), point);
        } else if (unary->isIncrementDecrementOp()) {
            const clang::Expr *operand = unary->getSubExpr()->IgnoreParenImpCasts();
            decodeLValue(operand, point);
            const Sym place = placeOf(operand);
            const Sym old = readValue(place, point);
            const Sym updated = isPointerType(operand->getType())
                                    ? valueArith(old, point)
                                    : valueUnknown();
            writePlace(place, updated, point, isStoreTarget(operand));
            value = unary->isPostfix() ? old : updated;
        } else {
            decodeExpr(unary->getSubExpr(), point);
        }
    } else if (const auto *binary = dyn_cast<clang::BinaryOperator>(e)) {
        value = decodeBinary(binary, point);
    } else if (const auto *call = dyn_cast<clang::CallExpr>(e)) {
        value = decodeCall(call, point);
    } else if (const auto *cast = dyn_cast<clang::CastExpr>(e)) {
        const clang::Expr *sub = cast->getSubExpr();
        if (cast->getCastKind() == clang::CK_ArrayToPointerDecay ||
            cast->getCastKind() == clang::CK_FunctionToPointerDecay) {
            const Sym target = placeOf(sub);
            value = target >= 0 ? valueAddr(target, point) : valueUnknown();
        } else if (cast->getCastKind() == clang::CK_NullToPointer) {
            value = valueConst("null");
            emitValueLiteral(value, sub);
        } else if (cast->getCastKind() == clang::CK_LValueToRValue ||
                   cast->getCastKind() == clang::CK_NoOp) {
            value = decodeExpr(sub, point);
        } else {
            const Sym source = decodeExpr(sub, point);
            const bool dst_ptr = isPointerType(cast->getType());
            const bool src_ptr = isPointerType(sub->getType());
            if (dst_ptr && src_ptr) {
                value = valueCast(source, "ptr_ptr", point);
            } else if (dst_ptr) {
                value = valueCast(source, "int_ptr", point);
            } else if (src_ptr) {
                value = valueCast(source, "ptr_int", point);
            } else {
                value = source;
            }
        }
    } else if (const auto *constant = dyn_cast<clang::ConstantExpr>(e)) {
        value = decodeExpr(constant->getSubExpr(), point);
    } else if (const auto *conditional =
                   dyn_cast<clang::AbstractConditionalOperator>(e)) {
        decodeExpr(conditional->getCond(), point);
        const Sym yes = decodeExpr(conditional->getTrueExpr(), point);
        const Sym no = decodeExpr(conditional->getFalseExpr(), point);
        value = freshValue("choice");
        facts.value_choice.push_back({value, yes, point});
        facts.value_choice.push_back({value, no, point});
    } else if (const auto *selection =
                   dyn_cast<clang::GenericSelectionExpr>(e)) {
        value = decodeExpr(selection->getResultExpr(), point);
    } else if (const auto *choose = dyn_cast<clang::ChooseExpr>(e)) {
        value = decodeExpr(choose->getChosenSubExpr(), point);
    } else if (const auto *opaque = dyn_cast<clang::OpaqueValueExpr>(e)) {
        if (opaque->getSourceExpr() != nullptr) {
            value = decodeExpr(opaque->getSourceExpr(), point);
        }
    } else if (const auto *designated =
                   dyn_cast<clang::DesignatedInitExpr>(e)) {
        value = decodeExpr(designated->getInit(), point);
    } else if (const auto *list = dyn_cast<clang::InitListExpr>(e)) {
        for (const clang::Expr *init : list->inits()) {
            if (init != nullptr) {
                decodeExpr(init, point);
            }
        }
    } else if (const auto *literal = dyn_cast<clang::IntegerLiteral>(e)) {
        value = valueConst(literal->getValue().isZero() ? "int_zero" : "int");
        emitValueLiteral(value, e);
    } else if (isa<clang::CharacterLiteral>(e)) {
        value = valueConst("char");
        emitValueLiteral(value, e);
    } else if (isa<clang::FloatingLiteral>(e)) {
        value = valueConst("float");
        emitValueLiteral(value, e);
    } else if (isa<clang::StringLiteral>(e)) {
        value = valueAddr(placeOf(e), point);
        emitValueLiteral(value, e);
    } else if (isa<clang::GNUNullExpr>(e)) {
        value = valueConst("null");
        emitValueLiteral(value, e);
    } else if (isa<clang::ImplicitValueInitExpr>(e)) {
        value = valueConst("zero");
        emitValueLiteral(value, e);
    } else if (isa<clang::UnaryExprOrTypeTraitExpr>(e) ||
               isa<clang::OffsetOfExpr>(e) || isa<clang::PredefinedExpr>(e)) {
        value = valueConst("constant");
        emitValueLiteral(value, e);
    } else if (isa<clang::CompoundLiteralExpr>(e)) {
        value = valueAddr(placeOf(e), point);
    } else if (const auto *va_arg = dyn_cast<clang::VAArgExpr>(e)) {
        decodeExpr(va_arg->getSubExpr(), point);
    } else {
        for (const Stmt *child : e->children()) {
            if (const auto *child_expr = dyn_cast_or_null<clang::Expr>(child)) {
                decodeExpr(child_expr, point);
            } else if (child != nullptr) {
                decodeStmt(child);
            }
        }
        unsupportedAt(point, std::string("unsupported expression: ") +
                                e->getStmtClassName());
    }
    if (value < 0) {
        value = valueUnknown();
    }
    emitValueType(value, e->getType());
    expr_values.emplace(e, value);
    return value;
}

Sym FactExtractorVisitor::decodeBinary(const clang::BinaryOperator *expr,
                                       Sym point) {
    if (expr->isAssignmentOp()) {
        return expr->getOpcode() == clang::BO_Assign
                   ? decodeAssign(expr, point)
                   : decodeCompoundAssign(expr, point);
    }
    const Sym lhs = decodeExpr(expr->getLHS(), point);
    const Sym rhs = decodeExpr(expr->getRHS(), point);
    if (expr->getOpcode() == clang::BO_Comma) {
        return rhs;
    }
    if ((expr->getOpcode() == clang::BO_Add ||
         expr->getOpcode() == clang::BO_Sub) &&
        isPointerType(expr->getType())) {
        const clang::Expr *base = isPointerType(expr->getLHS()->getType())
                                      ? expr->getLHS()
                                      : expr->getRHS();
        const Sym base_value = base == expr->getLHS() ? lhs : rhs;
        return valueArith(base_value, point);
    }
    return valueUnknown();
}

Sym FactExtractorVisitor::decodeAssign(const clang::BinaryOperator *expr,
                                       Sym point) {
    const clang::Expr *lhs = expr->getLHS()->IgnoreParenImpCasts();
    decodeLValue(lhs, point);
    const Sym dst = placeOf(lhs);
    const Sym value = decodeExpr(expr->getRHS(), point);
    if (dst < 0) {
        return value;
    }
    writePlace(dst, value, point, isStoreTarget(lhs));
    Sym source = -1;
    if (tryArgPlace(expr->getRHS(), source)) {
        facts.event_copy.push_back({dst, source, point});
    }
    if (isa<clang::CallExpr>(expr->getRHS()->IgnoreParenImpCasts())) {
        facts.call_result_place.push_back({point, dst});
    }
    return value;
}

Sym FactExtractorVisitor::decodeCompoundAssign(
    const clang::BinaryOperator *expr, Sym point) {
    const clang::Expr *lhs = expr->getLHS()->IgnoreParenImpCasts();
    decodeLValue(lhs, point);
    const Sym dst = placeOf(lhs);
    if (dst < 0) {
        decodeExpr(expr->getRHS(), point);
        return valueUnknown();
    }
    const Sym base = readValue(dst, point);
    decodeExpr(expr->getRHS(), point);
    Sym value = valueUnknown();
    if (isPointerType(lhs->getType()) &&
        (expr->getOpcode() == clang::BO_AddAssign ||
         expr->getOpcode() == clang::BO_SubAssign)) {
        value = valueArith(base, point);
    }
    writePlace(dst, value, point, isStoreTarget(lhs));
    return value;
}

Sym FactExtractorVisitor::decodeCall(const clang::CallExpr *expr, Sym point) {
    const FunctionDecl *direct = expr->getDirectCallee();
    const std::string name = calleeName(expr);
    if (direct != nullptr) {
        const Sym callee = interner.intern(name);
        facts.call_site.push_back({point, callee});
        if (current_fn_sym >= 0) {
            facts.fn_call.push_back({current_fn_sym, point, callee});
        }
    } else {
        facts.call_indirect.push_back({point});
        decodeExpr(expr->getCallee(), point);
    }
    for (unsigned i = 0; i < expr->getNumArgs(); ++i) {
        const clang::Expr *arg = expr->getArg(i);
        const Sym value = decodeExpr(arg, point);
        const Sym index = argIndex(i);
        Sym actual = -1;
        if (tryArgPlace(arg, actual)) {
            facts.call_arg.push_back({point, index, actual});
        }
        facts.call_arg_value.push_back({point, index, value});
        if (direct == nullptr || i >= direct->getNumParams() ||
            !isPointerType(direct->getParamDecl(i)->getType())) {
            continue;
        }
        Sym store_place = -1;
        if (isAddressOf(arg)) {
            store_place = placeOf(
                cast<clang::UnaryOperator>(arg->IgnoreParenImpCasts())
                    ->getSubExpr());
        } else if (actual >= 0) {
            store_place = derefPlace(actual);
        }
        if (store_place >= 0) {
            facts.call_store_place.push_back({point, index, store_place});
        }
    }
    return valueCall(name, point);
}

Sym FactExtractorVisitor::readValue(Sym place, Sym point) {
    if (place < 0) {
        return valueUnknown();
    }
    facts.event_load.push_back({place, point});
    return place;
}

Sym FactExtractorVisitor::valueCall(const std::string &callee, Sym point) {
    const Sym value = freshValue("call:" + callee);
    facts.value_call.push_back({value, interner.intern(callee), point});
    return value;
}

Sym FactExtractorVisitor::valueAddr(Sym target, Sym point) {
    const Sym value = freshValue("addr");
    facts.value_addr.push_back({value, target, point});
    return value;
}

Sym FactExtractorVisitor::valueArith(Sym base, Sym point) {
    const Sym value = freshValue("arith");
    facts.value_arith.push_back({value, base, point});
    return value;
}

Sym FactExtractorVisitor::valueCast(Sym source, std::string_view kind,
                                    Sym point) {
    const Sym value = freshValue("cast:" + std::string(kind));
    facts.value_cast.push_back(
        {value, source, interner.intern(kind), point});
    return value;
}

Sym FactExtractorVisitor::valueConst(std::string_view kind) {
    const Sym value = freshValue("const:" + std::string(kind));
    facts.value_const.push_back({value, interner.intern(kind)});
    return value;
}

Sym FactExtractorVisitor::valueUnknown() {
    const Sym value = interner.intern("value:unknown");
    if (facts.value_unknown.empty()) {
        facts.value_unknown.push_back({value});
    }
    return value;
}

Sym FactExtractorVisitor::freshValue(std::string_view prefix) {
    return interner.freshen("value:" + std::string(prefix) + ":" +
                            std::to_string(++value_seq));
}

void FactExtractorVisitor::emitValueType(Sym value, clang::QualType type) {
    facts.value_type.push_back({value, typeSym(type)});
}
void FactExtractorVisitor::emitValueLiteral(Sym value,
                                             const clang::Expr *expr) {
    std::string spelling = clang::Lexer::getSourceText(
                               clang::CharSourceRange::getTokenRange(
                                   expr->getSourceRange()),
                               context.getSourceManager(), context.getLangOpts())
                               .str();
    if (spelling.empty()) {
        spelling = expr->getStmtClassName();
    }
    facts.value_literal.push_back({value, interner.intern(spelling)});
}

void FactExtractorVisitor::eventBind(Sym place, Sym value, Sym point) {
    facts.event_bind.push_back({place, value, point});
}

void FactExtractorVisitor::eventStore(Sym place, Sym value, Sym point) {
    facts.event_store.push_back({place, value, point});
}

void FactExtractorVisitor::writePlace(Sym place, Sym value, Sym point,
                                      bool store) {
    if (place < 0) {
        return;
    }
    eventBind(place, value, point);
    if (store) {
        eventStore(place, value, point);
    }
}

std::string FactExtractorVisitor::calleeName(
    const clang::CallExpr *expr) const {
    if (const FunctionDecl *direct = expr->getDirectCallee()) {
        return direct->getNameAsString();
    }
    const clang::SourceRange range = expr->getCallee()->getSourceRange();
    return clang::Lexer::getSourceText(
               clang::CharSourceRange::getTokenRange(range),
               context.getSourceManager(), context.getLangOpts())
        .str();
}

void FactExtractorVisitor::unsupportedAt(Sym point,
                                         std::string_view reason) {
    if (point < 0) {
        point = fn_exit >= 0 ? fn_exit : control.point("unsupported");
    }
    facts.err_internal.push_back({point, interner.intern(reason)});
}

bool FactExtractorVisitor::isStoreTarget(const clang::Expr *expr) const {
    const clang::Expr *e = expr->IgnoreParenImpCasts();
    if (isa<clang::MemberExpr>(e) || isa<clang::ArraySubscriptExpr>(e)) {
        return true;
    }
    const auto *unary = dyn_cast<clang::UnaryOperator>(e);
    return unary != nullptr && unary->getOpcode() == clang::UO_Deref;
}

Sym FactExtractorVisitor::mintPoint(const Stmt *stmt) {
    const Sym pt = control.point(tag(stmt));
    stmt_pt[stmt] = pt;
    recordSourceRangeLoc(pt, stmt->getSourceRange());
    return pt;
}

void FactExtractorVisitor::recordSourceRangeLoc(
    Sym point, const clang::SourceRange range) {
    if (range.isInvalid())
        return;
    const clang::PresumedLoc begin =
        context.getSourceManager().getPresumedLoc(range.getBegin());
    const clang::PresumedLoc end =
        context.getSourceManager().getPresumedLoc(range.getEnd());
    if (begin.isValid() && end.isValid()) {
        point_loc[point] = {begin.getFilename(), begin.getLine(),
                            begin.getColumn(), end.getLine(), end.getColumn()};
    }
}

void FactExtractorVisitor::recordPointLoc(Sym point, const clang::Decl *decl) {
    const auto range = decl->getSourceRange();
    recordSourceRangeLoc(point, range);
}

void FactExtractorVisitor::declKind(Sym place, std::string_view kind,
                                    std::string_view name) {
    facts.decl_kind.push_back(
        {place, interner.intern(kind), interner.intern(name)});
}


std::string_view FactExtractorVisitor::roleForAnnotation(
    llvm::StringRef annotation) {
    if (annotation == "bc_owner") {
        return "owner";
    }
    if (annotation == "bc_borrow") {
        return "borrow";
    }
    if (annotation == "bc_mut_borrow") {
        return "mut_borrow";
    }
    return {};
}

bool FactExtractorVisitor::isPointerType(clang::QualType qt) const {
    return !qt.isNull() && qt.getCanonicalType()->isPointerType();
}

bool FactExtractorVisitor::typeContainsPointer(clang::QualType qt) const {
    const clang::Type *type = qt.getCanonicalType().getTypePtrOrNull();
    if (type == nullptr) {
        return false;
    }
    if (type->isPointerType()) {
        return true;
    }
    if (const auto *array = dyn_cast<clang::ArrayType>(type)) {
        return typeContainsPointer(array->getElementType());
    }
    if (const auto *record = type->getAs<clang::RecordType>()) {
        const RecordDecl *definition = record->getDecl()->getDefinition();
        if (definition == nullptr) {
            return false;
        }
        for (const clang::FieldDecl *field : definition->fields()) {
            if (typeContainsPointer(field->getType())) {
                return true;
            }
        }
    }
    return false;
}

bool FactExtractorVisitor::arrayElementContainsPointer(
    clang::QualType qt) const {
    const auto *array = dyn_cast<clang::ArrayType>(
        qt.getCanonicalType().getTypePtrOrNull());
    return array != nullptr && typeContainsPointer(array->getElementType());
}

void FactExtractorVisitor::emitDeclFacts(const clang::Decl *decl, Sym place,
                                         clang::QualType qt) {
    const Sym type = typeSym(qt);
    facts.place_type.push_back({place, type});
    emitAnnotations(decl, place);
    auto emit_tag = [&](llvm::StringRef tag) {
        const std::string_view role = roleForAnnotation(tag);
        if (!role.empty()) {
            facts.role.push_back({place, interner.intern(role)});
        }
        if (tag == "bc_init") {
            facts.assume_init.push_back({place});
        }
    };
    for (const clang::Attr *attr : decl->attrs()) {
        if (const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr)) {
            emit_tag(annotated->getAnnotation());
        }
    }
    for (clang::QualType current = qt; !current.isNull();) {
        const auto *alias = current->getAs<clang::TypedefType>();
        if (alias == nullptr) {
            break;
        }
        const auto *typedef_decl = alias->getDecl();
        const Sym alias_sym = interner.intern(
            "type-alias:" + typedef_decl->getNameAsString());
        for (const clang::Attr *attr : typedef_decl->attrs()) {
            const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
            if (annotated == nullptr) {
                continue;
            }
            const llvm::StringRef tag = annotated->getAnnotation();
            facts.annotation.push_back(
                {place, interner.intern(tag.str())});
            facts.type_annotation.push_back(
                {alias_sym, interner.intern(tag.str())});
            emit_tag(tag);
        }
        current = typedef_decl->getUnderlyingType();
    }
    if (isPointerType(qt)) {
        facts.pointer_decl.push_back({place});
        facts.pointer_place.push_back({place});
        if (!hasRole(decl) && !hasRole(qt)) {
            const auto *named = dyn_cast<clang::NamedDecl>(decl);
            const std::string name = named == nullptr
                                         ? std::string{}
                                         : named->getNameAsString();
            const Sym at = control.point("ptr-missing-role:" + name);
            recordPointLoc(at, decl);
            facts.ptr_missing_role.push_back({at, interner.intern(name)});
        }
    }
    if (const auto *var = dyn_cast<VarDecl>(decl)) {
        const bool tls = var->getTLSKind() != VarDecl::TLS_None;
        const clang::QualType canonical = qt.getCanonicalType();
        const bool atomic = canonical->isAtomicType() ||
                            (canonical->isPointerType() &&
                             canonical->getPointeeType()->isAtomicType());
        if (tls || atomic) {
            const std::string name = var->getNameAsString();
            const Sym at = control.point("decl:" + name);
            recordPointLoc(at, decl);
            if (tls) {
                declKind(at, "tls", name);
            }
            if (atomic) {
                declKind(at, "atomic", name);
            }
        }
    }
}

void FactExtractorVisitor::emitAnnotations(const clang::Decl *decl,
                                           Sym target) {
    for (const clang::Attr *attr : decl->attrs()) {
        if (const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr)) {
            facts.annotation.push_back(
                {target, interner.intern(annotated->getAnnotation().str())});
        }
    }
}

void FactExtractorVisitor::emitParameterAnnotations(const FunctionDecl *fd,
                                                    unsigned index,
                                                    Sym place) {
    for (const FunctionDecl *redecl : fd->redecls()) {
        if (index >= redecl->getNumParams()) {
            continue;
        }
        const clang::ParmVarDecl *param = redecl->getParamDecl(index);
        emitAnnotations(param, place);
        for (const clang::Attr *attr : param->attrs()) {
            const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
            if (annotated == nullptr) {
                continue;
            }
            const llvm::StringRef tag = annotated->getAnnotation();
            const std::string_view role = roleForAnnotation(tag);
            if (!role.empty()) {
                facts.role.push_back({place, interner.intern(role)});
            }
            if (tag == "bc_init") {
                facts.assume_init.push_back({place});
            }
        }
        for (clang::QualType current = param->getType();
             !current.isNull();) {
            const auto *alias = current->getAs<clang::TypedefType>();
            if (alias == nullptr) {
                break;
            }
            const auto *typedef_decl = alias->getDecl();
            const Sym alias_sym = interner.intern(
                "type-alias:" + typedef_decl->getNameAsString());
            for (const clang::Attr *attr : typedef_decl->attrs()) {
                const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
                if (annotated == nullptr) {
                    continue;
                }
                const llvm::StringRef tag = annotated->getAnnotation();
                facts.annotation.push_back(
                    {place, interner.intern(tag.str())});
                facts.type_annotation.push_back(
                    {alias_sym, interner.intern(tag.str())});
                const std::string_view role = roleForAnnotation(tag);
                if (!role.empty()) {
                    facts.role.push_back({place, interner.intern(role)});
                }
                if (tag == "bc_init") {
                    facts.assume_init.push_back({place});
                }
            }
            current = typedef_decl->getUnderlyingType();
        }
    }
}

bool FactExtractorVisitor::hasRole(const clang::Decl *decl) const {
    auto annotated_role = [](const clang::Decl *candidate) {
        for (const clang::Attr *attr : candidate->attrs()) {
            if (const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
                annotated != nullptr &&
                !roleForAnnotation(annotated->getAnnotation()).empty()) {
                return true;
            }
        }
        return false;
    };
    if (annotated_role(decl)) {
        return true;
    }
    const auto *param = dyn_cast<clang::ParmVarDecl>(decl);
    if (param == nullptr) {
        return false;
    }
    if (hasRole(param->getType())) {
        return true;
    }
    const auto *owner = dyn_cast<FunctionDecl>(param->getDeclContext());
    if (owner == nullptr) {
        return false;
    }
    const unsigned index = param->getFunctionScopeIndex();
    for (const FunctionDecl *redecl : owner->redecls()) {
        if (index >= redecl->getNumParams()) {
            continue;
        }
        const clang::ParmVarDecl *candidate = redecl->getParamDecl(index);
        if (annotated_role(candidate) || hasRole(candidate->getType())) {
            return true;
        }
    }
    return false;
}

bool FactExtractorVisitor::hasRole(clang::QualType qt) const {
    while (!qt.isNull()) {
        const auto *alias = qt->getAs<clang::TypedefType>();
        if (alias == nullptr) {
            break;
        }
        if (hasRole(alias->getDecl())) {
            return true;
        }
        qt = alias->getDecl()->getUnderlyingType();
    }
    return false;
}

bool FactExtractorVisitor::hasLifetimeBound(const clang::Decl *decl) const {
    if (decl->hasAttr<clang::LifetimeBoundAttr>()) {
        return true;
    }
    for (const clang::Attr *attr : decl->attrs()) {
        if (const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
            annotated != nullptr &&
            (annotated->getAnnotation() == "bc_lifetime_bound" ||
             annotated->getAnnotation() == "bchk_lifetime_bound")) {
            return true;
        }
    }
    return false;
}

Sym FactExtractorVisitor::typeSym(clang::QualType qt) {
    const clang::QualType canonical = qt.getCanonicalType();
    const std::string name = canonical.getAsString();
    if (const auto found = type_cache.find(name); found != type_cache.end()) {
        return found->second;
    }
    const Sym type = interner.intern("type:" + name);
    type_cache.emplace(name, type);
    if (canonical.isConstQualified()) {
        facts.type_const.push_back({type});
    }
    if (canonical.isVolatileQualified()) {
        facts.type_volatile.push_back({type});
    }
    if (canonical.isRestrictQualified()) {
        facts.type_restrict.push_back({type});
    }
    if (canonical->isPointerType()) {
        facts.type_pointer.push_back({type});
        facts.type_pointee.push_back(
            {type, typeSym(canonical->getPointeeType())});
    }
    if (canonical->isAtomicType()) {
        facts.type_atomic.push_back({type});
    }
    if (canonical->isIntegerType()) {
        facts.type_integer.push_back({type});
    }
    if (canonical->isFloatingType()) {
        facts.type_floating.push_back({type});
    }
    if (canonical->isVoidType()) {
        facts.type_void.push_back({type});
    }
    if (canonical->isEnumeralType()) {
        facts.type_enum.push_back({type});
    }
    if (const auto *array = dyn_cast<clang::ArrayType>(canonical.getTypePtr())) {
        facts.type_element.push_back(
            {type, typeSym(array->getElementType())});
        if (typeContainsPointer(array->getElementType())) {
            facts.type_array_of_pointer.push_back({type});
        }
    }
    if (const auto *record = canonical->getAs<clang::RecordType>()) {
        facts.type_record.push_back({type});
        if (record->getDecl()->isUnion()) {
            facts.type_union.push_back({type});
        }
        if (const RecordDecl *definition = record->getDecl()->getDefinition()) {
            for (const clang::FieldDecl *field : definition->fields()) {
                const Sym index = interner.intern(
                    "field-index:" + std::to_string(field->getFieldIndex()));
                const std::string field_name = field->getNameAsString();
                const Sym name_sym = interner.intern(
                    field_name.empty()
                        ? "field:" + std::to_string(field->getFieldIndex())
                        : field_name);
                facts.type_field.push_back(
                    {type, index, name_sym, typeSym(field->getType())});
            }
        }
    }
    if (const auto *function = canonical->getAs<clang::FunctionType>()) {
        facts.type_fn_result.push_back(
            {type, typeSym(function->getReturnType())});
        if (const auto *prototype =
                dyn_cast<clang::FunctionProtoType>(function)) {
            if (prototype->isVariadic()) {
                facts.type_fn_variadic.push_back({type});
            }
            for (unsigned i = 0; i < prototype->getNumParams(); ++i) {
                facts.type_fn_param.push_back(
                    {type, argIndex(i), typeSym(prototype->getParamType(i))});
            }
        }
    }
    if (typeContainsPointer(canonical)) {
        facts.type_contains_pointer.push_back({type});
    }
    return type;
}

Sym FactExtractorVisitor::argIndex(size_t index) {
    return interner.intern("arg:" + std::to_string(index));
}

bool FactExtractorVisitor::isAddressOf(const clang::Expr *expr) const {
    const clang::Expr *e = expr->IgnoreParenImpCasts();
    if (const clang::UnaryOperator *uo =
            clang::dyn_cast<clang::UnaryOperator>(e)) {
        return uo->getOpcode() == clang::UO_AddrOf;
    }

    return false;
}

Sym FactExtractorVisitor::stmtPoint(const Stmt *stmt) const {
    if (const auto found = stmt_pt.find(stmt); found != stmt_pt.end()) {
        return found->second;
    }
    return current_point >= 0 ? current_point : fn_exit;
}

const char *FactExtractorVisitor::tag(const Stmt *stmt) {
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

/********************** LLVM setup code **********************/

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
