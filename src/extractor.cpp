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
          point_loc(point_loc), control(interner, facts),
          places(interner, facts) {}

    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl *tu);

  private:
    void handleUnion(const RecordDecl *rd);
    void handleUnion(const TypedefDecl *td);
    void handleGlobal(const VarDecl *global);
    void handleFunction(const FunctionDecl *fd);

    void extractFunction(const FunctionDecl *fd, Sym fn);
    void emitCfgEdges(const FunctionDecl *fd, Sym entry, Sym exit);

    Sym mintPoint(const Stmt *stmt);
    Sym placeForVar(const VarDecl *var_decl);

    static std::string getAnnotation(const clang::Decl *decl);
    static bool isUnionType(clang::QualType qt);

    void recordSourceRangeLoc(Sym point, const clang::SourceRange range);
    void recordPointLoc(Sym point, const clang::Decl *decl);
    void declKind(Sym place, std::string_view kind, std::string_view name);
    Sym typeSym(clang::QualType qt);
    Sym argIndex(size_t index);

    bool isPointerType(clang::QualType qt) const;
    bool typeContainsPointer(clang::QualType qt) const;
    bool arrayElementContainsPointer(clang::QualType qt) const;
    bool hasLifetimeBound(const FunctionDecl *fd) const;
    bool hasRole(const clang::Decl *decl) const;

    void emitDeclFacts(const clang::Decl *decl, Sym place, clang::QualType qt);

    bool isAddressOf(const clang::Expr *expr) const;
    const char *tag(const Stmt *stmt);

    clang::ASTContext &context;
    Interner &interner;
    Facts &facts;
    PointSourceMap &point_loc;

    std::string current_fn;
    Sym current_fn_sym = -1;
    Sym fn_exit = -1;
    Sym current_point = -1;

    ControlBuilder control;
    PlaceBuilder places;

    std::unordered_map<const Stmt *, Sym> stmt_pt;
    std::unordered_map<const clang::ValueDecl *, Sym> var_places;
    std::unordered_map<std::string, Sym> place_cache;
    std::unordered_map<std::string, Sym> type_cache;

    // These are used to de-duplicate how many objs we track
    // so we don't waste time processing unnecessary facts
    // on the datalog side
    std::unordered_set<Sym> declared_fn;
    std::unordered_set<Sym> variadic_fn;
    std::unordered_set<Sym> emitted_decl_facts;
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
            if (record->isUnion()) {
                handleUnion(record);
            }
            // TODO: handle struct defs?
        } else if (const auto *typedef_decl = dyn_cast<TypedefDecl>(decl)) {
            if (isUnionType(typedef_decl->getUnderlyingType())) {
                handleUnion(typedef_decl);
            }
        }
    }

    return false;
}

/**************** Extractor private functions ****************/

void FactExtractorVisitor::handleUnion(const RecordDecl *rd) {
    const auto name = rd->getNameAsString();
    const Sym at = control.point("union:" + name);
    recordPointLoc(at, rd);
    facts.union_decl.push_back({at, interner.intern(name)});
}

void FactExtractorVisitor::handleUnion(const TypedefDecl *td) {
    const auto name = td->getNameAsString();
    const Sym at = control.point("union:" + name);
    recordPointLoc(at, td);
    facts.union_decl.push_back({at, interner.intern(name)});
}

void FactExtractorVisitor::handleGlobal(const VarDecl *global) {
    const Sym place = placeForVar(global);
    facts.global_var.push_back({place});

    const auto varname = global->getNameAsString();
    const Sym entry = control.point("global-entry:" + varname);
    recordPointLoc(entry, global);
    facts.global_entry.push_back({place, entry});

    if (typeContainsPointer(global->getType()) &&
        !isPointerType(global->getType())) {
        declKind(place, "global pointer type", varname);
    }

    if (global->hasInit()) {
        facts.var_init.push_back({place});
        current_fn.clear();
        current_fn_sym = -1;
        fn_exit = -1;
        current_point = entry;

        // TODO: exprs
        // const Sym value = decodeExpr(global->getInit(), entry);
        // eventBind(place, value, entry);
        if (clang::isa<clang::CallExpr>(
                global->getInit()->IgnoreParenImpCasts())) {
            facts.call_result_place.push_back({entry, place});
        }
    }
}

void FactExtractorVisitor::handleFunction(const FunctionDecl *fd) {
    const Sym fn = interner.intern(fd->getNameAsString());

    if (declared_fn.insert(fn).second) {
        const Sym decl_pt = control.point("fn-decl:" + fd->getNameAsString());
        recordPointLoc(decl_pt, fd);
        facts.fn_decl.push_back({fn, decl_pt});
    }

    if (fd->isVariadic() && variadic_fn.insert(fn).second) {
        facts.fn_variadic.push_back({fn});
    }

    // TODO: get the param idx to handle lifetime bounds properly
    if (hasLifetimeBound(fd)) {
        for (const clang::ParmVarDecl *param : fd->parameters()) {
            if (isPointerType(param->getType())) {
                facts.fn_lifetime_bound.push_back({fn, placeForVar(param)});
                if (!hasRole(param)) {
                    const Sym pt = control.point("ptr-missing-role:" +
                                                 param->getNameAsString());
                    recordPointLoc(pt, param);
                    facts.ptr_missing_role.push_back(
                        {pt, interner.intern(param->getNameAsString())});
                }
            }

            // TODO: Handle structs passed by copy containing pointers?
            // Probably better to do that at the call site...
        }
    }

    // TODO: test unbodied fn errors across TUs
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
    // control.begin_fn(current_fn, entry, exit);

    facts.fn_defined.push_back({fn});
    facts.fn_entry.push_back({fn, entry});
    facts.fn_exit.push_back({fn, exit});

    if (fd->isNoReturn()) {
        facts.fn_noreturn.push_back({fn});
    }

    // Gather the param annotations
    for (unsigned i = 0; i < fd->getNumParams(); ++i) {
        const clang::ParmVarDecl *param = fd->getParamDecl(i);
        const Sym place = placeForVar(param);
        facts.param_entry.push_back({place, fn, entry});
        facts.param_idx.push_back({place, argIndex(i)});

        if (param->isKNRPromoted()) {
            facts.knr_promoted.push_back({place, fn});
        }

        if (arrayElementContainsPointer(param->getType())) {
            declKind(place, "array containing pointers",
                     param->getNameAsString());
        }
        if (param->getType()->isRecordType() &&
            typeContainsPointer(param->getType())) {
            declKind(place, "struct containing pointer",
                     param->getNameAsString());
        }
    }

    current_point = entry;
    emitCfgEdges(fd, entry, exit);

    // TODO: stmts
    // decodeStmt(fd->getBody());
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

Sym FactExtractorVisitor::placeForVar(const VarDecl *var_decl) {
    auto found = var_places.find(var_decl);
    if (found != var_places.end()) {
        return found->second;
    }

    Sym place;
    if (const clang::ParmVarDecl *param =
            clang::dyn_cast<clang::ParmVarDecl>(var_decl)) {
        const auto *owner = dyn_cast<FunctionDecl>(param->getDeclContext());
        const std::string fn =
            owner != nullptr ? owner->getNameAsString() : current_fn;
        place = interner.intern("param:" + fn + "#" +
                                std::to_string(param->getFunctionScopeIndex()));
        interner.set_display(place, var_decl->getNameAsString());
    } else if (const auto *var = dyn_cast<VarDecl>(var_decl);
               var != nullptr && var->isFileVarDecl()) {
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

bool FactExtractorVisitor::isUnionType(clang::QualType qt) {
    const clang::QualType t = qt.getCanonicalType();
    if (const clang::RecordType *rt = t->getAs<clang::RecordType>()) {
        return rt->getDecl()->isUnion();
    }
    return false;
}

std::string FactExtractorVisitor::getAnnotation(const clang::Decl *decl) {
    for (const clang::Attr *attr : decl->attrs()) {
        if (const auto *annotated_attr =
                clang::dyn_cast<clang::AnnotateAttr>(attr)) {
            return annotated_attr->getAnnotation().str();
        }
    }
    return "";
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
        const auto *def = record->getDecl()->getDefinition();
        if (!def) {
            return false;
        }

        for (const clang::FieldDecl *field : record->getDecl()->fields()) {
            if (typeContainsPointer(field->getType())) {
                return true;
            }
        }
    }
    return false;
}

bool FactExtractorVisitor::arrayElementContainsPointer(
    clang::QualType qt) const {
    const clang::Type *type = qt.getCanonicalType().getTypePtrOrNull();
    if (type == nullptr) {
        return false;
    }
    if (const auto *array = dyn_cast<clang::ArrayType>(type)) {
        return typeContainsPointer(array->getElementType());
    }
    return false;
}

void FactExtractorVisitor::emitDeclFacts(const clang::Decl *decl, Sym place,
                                         clang::QualType qt) {
    const Sym type = typeSym(qt);

    facts.place_type.push_back({place, type});

    if (const auto *var = dyn_cast<VarDecl>(decl)) {
        const bool tls = var->getTLSKind() != VarDecl::TLS_None;
        const bool atomic =
            qt->isAtomicType() || qt->getPointeeType()->isAtomicType();
        if (tls || atomic) {
            const std::string name = var->getNameAsString();
            const Sym decl_point = control.point("decl:" + name);
            recordPointLoc(decl_point, decl);
            if (tls) {
                declKind(decl_point, "tls", name);
            }
            if (atomic) {
                declKind(decl_point, "atomic", name);
            }
        }
    }

    if (isPointerType(qt)) {
        facts.pointer_decl.push_back({place});
    }

    for (const clang::Attr *attr : decl->attrs()) {
        const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
        if (annotated == nullptr)
            continue;

        const std::string tag = annotated->getAnnotation().str();
        if (tag == "bc_owner") {
            facts.role.push_back({place, interner.intern("owner")});
        } else if (tag == "bc_borrow") {
            facts.role.push_back({place, interner.intern("borrow")});
        } else if (tag == "bc_mut_borrow") {
            facts.role.push_back({place, interner.intern("mut_borrow")});
        } else if (tag == "bc_init") {
            facts.assume_init.push_back({place});
        }
    }
}

bool FactExtractorVisitor::hasRole(const clang::Decl *decl) const {
    for (const clang::Attr *attr : decl->attrs()) {
        const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
        if (annotated == nullptr) {
            continue;
        }
        const clang::StringRef tag = annotated->getAnnotation();
        if (tag == "bc_owner" || tag == "bc_borrow" || tag == "bc_mut_borrow") {
            return true;
        }
    }
    return false;
}

bool FactExtractorVisitor::hasLifetimeBound(const FunctionDecl *fd) const {
    if (fd->hasAttr<clang::LifetimeBoundAttr>()) {
        return true;
    }
    for (const clang::Attr *attr : fd->attrs()) {
        const auto *annotated = dyn_cast<clang::AnnotateAttr>(attr);
        if (annotated != nullptr &&
            (annotated->getAnnotation() == "bc_lifetime_bound")) {
            return true;
        }
    }
    return false;
}

Sym FactExtractorVisitor::typeSym(clang::QualType qt) {
    const clang::QualType canonical = qt.getCanonicalType();
    const std::string canonical_name = canonical.getAsString();
    if (const auto found = type_cache.find(canonical_name);
        found != type_cache.end()) {
        return found->second;
    }

    const Sym type = interner.intern("type:" + canonical_name);
    type_cache[canonical_name] = type;

    if (canonical->isPointerType()) {
        declKind(type, "pointer", canonical_name);
    }
    if (const auto *record = canonical->getAs<clang::RecordType>()) {
        if (record->getDecl()->isUnion()) {
            facts.union_decl.push_back({type, interner.intern(canonical_name)});
        } else {
            facts.struct_decl.push_back(
                {type, interner.intern(canonical_name)});
        }
    }
    if (const auto *array =
            dyn_cast<clang::ArrayType>(canonical.getTypePtr())) {
        if (typeContainsPointer(array->getElementType())) {
            declKind(type, "array containing pointers", canonical_name);
        }
    }
    if (typeContainsPointer(canonical)) {
        declKind(type, "struct containing pointer", canonical_name);
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
