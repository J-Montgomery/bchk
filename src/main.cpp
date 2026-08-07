#include <clang/Tooling/CommonOptionsParser.h>

#include <filesystem>
#include <format>

class BorrowckOptionsParser : public clang::tooling::CommonOptionsParser {
public:
  BorrowckOptionsParser(int &argc, const char **argv,
                        llvm::cl::OptionCategory &Category)
      : clang::tooling::CommonOptionsParser(argc, argv, Category) {}
};

int main(int argc, const char **argv) {
  llvm::cl::OptionCategory BorrowckCategory("borrowck options");
  BorrowckOptionsParser OptionsParser(argc, argv, BorrowckCategory);

  return 0;
}
