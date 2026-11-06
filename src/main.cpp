#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/TextDiagnostic.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

// #include <filesystem>
// #include <format>
#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>

#include "dump.hpp"
#include "interface.hpp"
#include "rules.hpp"

using namespace bchk;

const char *bchk_version = "0.0.1";

static llvm::cl::OptionCategory bchk_category("bchk options");
llvm::cl::list<std::string>
    summary_specs("bchk-summary", llvm::cl::cat(bchk_category),
                  llvm::cl::desc("declare a function effect"));
llvm::cl::list<std::string>
    summary_files("bchk-summaries", llvm::cl::cat(bchk_category),
                  llvm::cl::desc("path to a summary file"));
llvm::cl::list<std::string> disable_classes(
    "bchk-disable", llvm::cl::cat(bchk_category),
    llvm::cl::desc("suppress comma-separated errors and warnings"),
    llvm::cl::CommaSeparated);
llvm::cl::opt<std::string>
    dump_dir("bchk-dump", llvm::cl::cat(bchk_category),
             llvm::cl::desc("dump generated facts to a TSV file under DIR"));
llvm::cl::opt<bool> quiet("bchk-quiet", llvm::cl::cat(bchk_category),
                          llvm::cl::desc("disable summary"));

class DiagnosticMsg {
  public:
    DiagnosticMsg()
        : os(llvm::errs()), diag_ids(new clang::DiagnosticIDs()),
          diag_opts(new clang::DiagnosticOptions()),
          diags(diag_ids, diag_opts, new clang::IgnoringDiagConsumer()),
          file_manager(fs_opts), src_manager(diags, file_manager) {
        diag_opts->ShowColors = true;
        diag_opts->ShowLocation = true;
    }
    void emit(const SrcLoc &loc, clang::DiagnosticsEngine::Level level,
              llvm::StringRef text) {
        if (!loc.valid()) {
            llvm::errs() << "ERROR: " << text << "\n";
            return;
        }

        llvm::Expected<clang::FileEntryRef> file_entry =
            file_manager.getFileRef(loc.file, true);
        if (!file_entry) {
            llvm::errs() << "ERROR: cannot open " << loc.file
                         << " to show source context\n";
            return;
        }
        clang::FileID fid = src_manager.translateFile(*file_entry);
        if (fid.isInvalid()) {
            fid = src_manager.createFileID(*file_entry, clang::SourceLocation(),
                                           clang::SrcMgr::C_User);
        }

        clang::SourceLocation begin =
            src_manager.translateLineCol(fid, loc.begin_line, loc.begin_col);
        clang::SourceLocation end =
            src_manager.translateLineCol(fid, loc.end_line, loc.end_col);
        if (begin.isInvalid() || end.isInvalid()) {
            const std::string err_msg =
                std::format("ERROR: in {} from {}:{} to {}:{}, {}\n", loc.file,
                            loc.begin_line, loc.begin_col, loc.end_line,
                            loc.end_col, text.str());
            llvm::errs() << err_msg;
            return;
        }
        clang::CharSourceRange range =
            clang::CharSourceRange::getTokenRange(begin, end);
        clang::TextDiagnostic text_diags(os, lang_opts, diag_opts.get());
        text_diags.emitDiagnostic(clang::FullSourceLoc(begin, src_manager),
                                  level, text, {range}, {});
        os.flush();
    }

  private:
    llvm::raw_ostream &os;
    clang::LangOptions lang_opts;
    llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diag_ids;
    llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> diag_opts;
    clang::DiagnosticsEngine diags;
    clang::FileSystemOptions fs_opts;
    clang::FileManager file_manager;
    clang::SourceManager src_manager;
};

int main(int argc, const char **argv) {
    llvm::cl::HideUnrelatedOptions(bchk_category);
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream &os) {
        os << "bchk v" << bchk_version << " (LLVM v" << LLVM_VERSION << ")\n";
    });

    const char *overview = "C borrowchecker\n";
    auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc, argv, bchk_category, llvm::cl::OneOrMore, overview);
    if (!expected_parser) {
        llvm::errs() << expected_parser.takeError();
        return 1;
    }
    clang::tooling::CommonOptionsParser &options_parser = expected_parser.get();

    clang::tooling::ClangTool tool(options_parser.getCompilations(),
                                   options_parser.getSourcePathList());

    // TODO: Create library model
    // TODO: Load builtin decls

    bchk::ExtractedFactSet facts = bchk::generateFacts(tool);
    bchk::Database db;
    std::unordered_set<std::string> disabled(disable_classes.begin(),
                                             disable_classes.end());
    const bchk::Report report = bchk::analyzeFacts(db, facts, disabled);

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

    DiagnosticMsg diag;
    for (const bchk::Error &error : errors) {
        diag.emit(error.loc,
                  error.warning ? clang::DiagnosticsEngine::Warning
                                : clang::DiagnosticsEngine::Error,
                  error.text);
    }

    if (!quiet) {
        if (report.verified) {
            llvm::outs() << "verified\n";
        } else {
            llvm::errs() << "verification failed\n";
        }
    }

    if (!dump_dir.empty()) {
        bchk_cli::dumpRelations(db, facts.interner, dump_dir.getValue());
    }

    return report.verified ? 0 : 1;
}
