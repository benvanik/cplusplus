// Copyright (c) 2026 Roberto Raggi <roberto.raggi@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cxx/control.h>
#include <cxx/diagnostics_client.h>
#include <cxx/preprocessor.h>
#include <cxx/token.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <future>
#include <latch>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace cxx;

TEST(Preprocessor, ConcurrentConstructionHasIndependentCompilationTimes) {
  constexpr unsigned kPreprocessorCount = 8;
  std::latch start(1);
  std::vector<std::future<std::vector<std::string>>> tasks;
  tasks.reserve(kPreprocessorCount);
  for (unsigned i = 0; i < kPreprocessorCount; ++i) {
    tasks.push_back(std::async(std::launch::async, [&start] {
      start.wait();
      Control control;
      DiagnosticsClient diagnostics;
      Preprocessor preprocessor{&control, &diagnostics};
      std::vector<Token> tokens;
      preprocessor.beginPreprocessing("__DATE__, __TIME__", "main.cc", tokens);
      while (true) {
        auto state = preprocessor.continuePreprocessing(tokens);
        if (std::holds_alternative<ProcessingComplete>(state)) break;
      }
      preprocessor.endPreprocessing(tokens);
      std::vector<std::string> spellings;
      for (const auto& token : tokens) {
        if (token.kind() == TokenKind::T_STRING_LITERAL) {
          spellings.push_back(token.spell());
        }
      }
      return spellings;
    }));
  }

  start.count_down();
  for (auto& task : tasks) {
    const auto spellings = task.get();
    ASSERT_EQ(spellings.size(), 2u);
    EXPECT_EQ(spellings[0].size(), 13u);
    EXPECT_EQ(spellings[1].size(), 10u);
    EXPECT_EQ(spellings[0].front(), '"');
    EXPECT_EQ(spellings[0].back(), '"');
    EXPECT_EQ(spellings[1].front(), '"');
    EXPECT_EQ(spellings[1].back(), '"');
  }
}

TEST(Preprocessor, IncludeContinuationRetainsRequestState) {
  Control control;
  DiagnosticsClient diagnostics;
  Preprocessor preprocessor{&control, &diagnostics};
  std::vector<Token> tokens;
  preprocessor.beginPreprocessing("#include <header.h>\n", "main.cc", tokens);

  bool resolvedInclude = false;
  while (!resolvedInclude) {
    auto includeState = preprocessor.continuePreprocessing(tokens);
    ASSERT_FALSE(std::holds_alternative<ProcessingComplete>(includeState));

    auto* include = std::get_if<PendingInclude>(&includeState);
    if (!include) continue;

    ASSERT_EQ(&include->preprocessor, &preprocessor);
    const auto expectedLocation = include->location.raw();
    ASSERT_NE(expectedLocation, 0);

    // Resolving transfers the request state to the deferred continuation.
    // Subsequent caller changes must not alter the pending file request.
    include->resolveWith("header.h");
    include->location = {};

    auto contentState = preprocessor.continuePreprocessing(tokens);
    auto* content = std::get_if<PendingFileContent>(&contentState);
    ASSERT_NE(content, nullptr);
    EXPECT_EQ(&content->preprocessor, &preprocessor);
    EXPECT_EQ(content->fileName, "header.h");
    EXPECT_EQ(content->location.raw(), expectedLocation);
    content->setContent("int from_header;\n");
    resolvedInclude = true;
  }

  while (true) {
    auto state = preprocessor.continuePreprocessing(tokens);
    if (std::holds_alternative<ProcessingComplete>(state)) break;
    ASSERT_FALSE(std::holds_alternative<PendingInclude>(state));
    ASSERT_FALSE(std::holds_alternative<PendingFileContent>(state));
  }

  preprocessor.endPreprocessing(tokens);
}

TEST(Preprocessor, IncludeDirectoriesRetainFilesystemRoots) {
  Control control;
  DiagnosticsClient diagnostics;
  Preprocessor preprocessor{&control, &diagnostics};

  auto root = std::filesystem::current_path().root_path();
  ASSERT_FALSE(root.empty());
  root.make_preferred();

  preprocessor.addSystemIncludePath(root.string());

  ASSERT_EQ(preprocessor.systemIncludePaths().size(), 1u);
  EXPECT_EQ(preprocessor.systemIncludePaths().front(), root.generic_string());
}

TEST(Preprocessor, IncludeCandidatesUseGenericPaths) {
  namespace fs = std::filesystem;

  Control control;
  DiagnosticsClient diagnostics;
  Preprocessor preprocessor{&control, &diagnostics};

  const auto includeDirectory = fs::path{"project"} / "includes";
  auto configuredIncludeDirectory = includeDirectory / "";
  configuredIncludeDirectory.make_preferred();
  preprocessor.addUserIncludePath(configuredIncludeDirectory.string());

  ASSERT_EQ(preprocessor.userIncludePaths().size(), 1u);
  EXPECT_EQ(preprocessor.userIncludePaths().front(),
            includeDirectory.generic_string());

  auto mainFile = fs::path{"project"} / "sources" / "main.cc";
  mainFile.make_preferred();
  std::vector<Token> tokens;
  preprocessor.beginPreprocessing("#include \"header.h\"\n", mainFile.string(),
                                  tokens);

  const auto currentDirectory = fs::path{"project"} / "sources";
  const auto currentCandidate = currentDirectory / "header.h";
  const auto userCandidate = includeDirectory / "header.h";
  bool resolvedInclude = false;
  while (!resolvedInclude) {
    auto state = preprocessor.continuePreprocessing(tokens);
    ASSERT_FALSE(std::holds_alternative<ProcessingComplete>(state));

    auto* include = std::get_if<PendingInclude>(&state);
    if (!include) continue;

    EXPECT_EQ(preprocessor.currentPath(), currentDirectory.generic_string());
    const auto candidates = include->candidates();
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0].fileName, currentCandidate.generic_string());
    EXPECT_FALSE(candidates[0].isSystemHeader);
    EXPECT_EQ(candidates[1].fileName, userCandidate.generic_string());
    EXPECT_FALSE(candidates[1].isSystemHeader);
    include->resolveWith(candidates[1].fileName, candidates[1].isSystemHeader);
    resolvedInclude = true;
  }

  auto contentState = preprocessor.continuePreprocessing(tokens);
  auto* content = std::get_if<PendingFileContent>(&contentState);
  ASSERT_NE(content, nullptr);
  EXPECT_EQ(content->fileName, userCandidate.generic_string());
  content->setContent("int from_header;\n");

  while (true) {
    auto state = preprocessor.continuePreprocessing(tokens);
    if (std::holds_alternative<ProcessingComplete>(state)) break;
    ASSERT_FALSE(std::holds_alternative<PendingInclude>(state));
    ASSERT_FALSE(std::holds_alternative<PendingFileContent>(state));
  }

  preprocessor.endPreprocessing(tokens);

  ASSERT_EQ(preprocessor.includedFiles().size(), 1u);
  EXPECT_EQ(preprocessor.includedFiles().front().first,
            userCandidate.generic_string());
}

TEST(Preprocessor, UnmatchedConditionalDirectivesPreserveRootState) {
  constexpr std::string_view directives[] = {
      "#elif 1", "#elifdef NAME", "#elifndef NAME", "#else", "#endif",
  };

  for (auto directive : directives) {
    Control control;
    CapturingDiagnosticsClient diagnostics;
    Preprocessor preprocessor{&control, &diagnostics};
    std::vector<Token> tokens;
    preprocessor.preprocess(std::format("{}\nint after;\n", directive),
                            "main.cc", tokens);

    ASSERT_EQ(diagnostics.diagnostics.size(), 1u) << directive;
    EXPECT_EQ(diagnostics.diagnostics.front().message(),
              std::format("unexpected '{}'",
                          directive.substr(0, directive.find(' '))))
        << directive;
    EXPECT_TRUE(std::ranges::any_of(tokens, [](const Token& token) {
      return token.kind() == TokenKind::T_IDENTIFIER &&
             token.spell() == "after";
    })) << directive;
  }
}

namespace {

auto preprocessSpellings(Preprocessor& preprocessor, std::string source)
    -> std::vector<std::string> {
  std::vector<Token> tokens;
  preprocessor.preprocess(std::move(source), "macros.cc", tokens);
  std::vector<std::string> spellings;
  EXPECT_EQ(tokens.front().kind(), TokenKind::T_ERROR);
  EXPECT_EQ(tokens.back().kind(), TokenKind::T_EOF_SYMBOL);
  for (const auto& token : std::span(tokens).subspan(1, tokens.size() - 2)) {
    spellings.push_back(token.spell());
  }
  return spellings;
}

class MacroComments final : public CommentHandler {
 public:
  void handleComment(Preprocessor*, const Token&) override { ++count; }

  // Number of replacement-list comments actually tokenized.
  int count = 0;
};

class MacroDiagnostics final : public DiagnosticsClient {
 public:
  void report(const Diagnostic& diagnostic) override {
    messages.push_back(diagnostic.message());
  }

  // Messages emitted by definition changes and conditional directives.
  std::vector<std::string> messages;
};

}  // namespace

TEST(Preprocessor, PredefinedMacrosTokenizeOnExpansionOncePerInstance) {
  static constexpr PredefinedMacro definitions[] = {
      {"VALUE", "/* replacement list */ 3"},
  };
  Control control;
  DiagnosticsClient diagnostics;
  Preprocessor preprocessor{&control, &diagnostics};
  MacroComments comments;
  preprocessor.setCommentHandler(&comments);
  static constexpr PredefinedMacros predefined{definitions};
  preprocessor.setPredefinedMacros(predefined);
  EXPECT_EQ(comments.count, 0);
  EXPECT_EQ(preprocessSpellings(preprocessor,
                                "#if defined(VALUE)\npresent\n#endif\n"),
            (std::vector<std::string>{"present"}));
  EXPECT_EQ(comments.count, 0);
  EXPECT_EQ(preprocessSpellings(preprocessor, "VALUE VALUE"),
            (std::vector<std::string>{"3", "3"}));
  EXPECT_EQ(comments.count, 1);

  Preprocessor independent{&control, &diagnostics};
  independent.setCommentHandler(&comments);
  independent.setPredefinedMacros(predefined);
  EXPECT_EQ(preprocessSpellings(independent, "VALUE"),
            (std::vector<std::string>{"3"}));
  EXPECT_EQ(comments.count, 2);
}

TEST(Preprocessor, PredefinedMacrosPreserveExpansionAndDefinitionChanges) {
  static constexpr PredefinedMacro definitions[] = {
      {"ALIAS", "VALUE"}, {"EMPTY", ""},  {"INCREMENT(x)", "((x) + 1)"},
      {"REMOVED", "0"},   {"VALUE", "3"},
  };
  Control control;
  MacroDiagnostics diagnostics;
  Preprocessor preprocessor{&control, &diagnostics};
  static constexpr PredefinedMacros predefined{definitions};
  preprocessor.setPredefinedMacros(predefined);
  preprocessor.undefMacro("REMOVED");
  auto spellings = preprocessSpellings(preprocessor, R"(
#if defined(REMOVED) || !defined(EMPTY) || !defined(INCREMENT)
#error incorrect initial environment
#endif
#define VALUE 3
ALIAS EMPTY INCREMENT(4)
#define VALUE 5
ALIAS
#undef VALUE
#ifdef VALUE
#error undefinition did not mask the predefined value
#endif
VALUE
#define VALUE 7
ALIAS
#undef ALIAS
ALIAS
)");
  EXPECT_EQ(spellings,
            (std::vector<std::string>{"3", "(", "(", "4", ")", "+", "1", ")",
                                      "5", "VALUE", "7", "ALIAS"}));
  EXPECT_EQ(diagnostics.messages,
            (std::vector<std::string>{"'VALUE' macro redefined"}));
}

TEST(Preprocessor, PredefinedMacrosPreservePrintingAndSnapshotEnvironment) {
  static constexpr PredefinedMacro definitions[] = {
      {"ALIAS", "VALUE"},
      {"FUNCTION(x)", "x ## U"},
      {"REMOVED", "5"},
      {"VALUE", "3"},
  };
  Control control;
  DiagnosticsClient diagnostics;
  Preprocessor preprocessor{&control, &diagnostics};
  static constexpr PredefinedMacros predefined{definitions};
  preprocessor.setPredefinedMacros(predefined);
  preprocessor.undefMacro("REMOVED");
  const auto snapshot = preprocessor.snapshot();
  std::ostringstream printed;
  preprocessor.printMacros(printed);
  EXPECT_NE(printed.str().find("#define ALIAS VALUE"), std::string::npos);
  EXPECT_NE(printed.str().find("#define FUNCTION(x)"), std::string::npos);
  EXPECT_EQ(printed.str().find("REMOVED"), std::string::npos);

  Preprocessor restored{&control, &diagnostics};
  restored.setPredefinedMacros(predefined);
  restored.restore(snapshot);
  EXPECT_EQ(preprocessSpellings(restored, R"(
#ifdef REMOVED
#error snapshot resurrected an undefined macro
#endif
ALIAS FUNCTION(4)
)"),
            (std::vector<std::string>{"3", "4U"}));
  EXPECT_EQ(diagnostics.errorCount(), 0);
}

TEST(Preprocessor, PredefinedIndexPreservesPrefixesAndLongIdentifiers) {
  static constexpr PredefinedMacro definitions[] = {
      {"A", "1"},
      {"AB", "2"},
      {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_long",
       "3"},
      {"AC(x)", "x"},
      {"Z", "4"},
  };
  static constexpr PredefinedMacros predefined{definitions};
  for (const auto& definition : definitions) {
    EXPECT_EQ(predefined.find(definition.identifier()), &definition);
  }
  EXPECT_EQ(predefined.find(""), nullptr);
  EXPECT_EQ(predefined.find("ABC"), nullptr);
  EXPECT_EQ(predefined.find("AC(x)"), nullptr);
  EXPECT_EQ(predefined.find("ZA"), nullptr);

  static constexpr PredefinedMacros empty{std::span<const PredefinedMacro>{}};
  EXPECT_EQ(empty.find("A"), nullptr);
}
