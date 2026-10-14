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

#include <array>
#include <filesystem>
#include <future>
#include <latch>
#include <ranges>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace cxx;

TEST(Preprocessor, ConcurrentConstructionProducesValidCompilationTimes) {
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
    const auto& date = spellings[0];
    ASSERT_EQ(date.size(), 13u);
    constexpr std::array<std::string_view, 12> monthNames{
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    EXPECT_TRUE(
        std::ranges::contains(monthNames, std::string_view{date}.substr(1, 3)));
    EXPECT_EQ(date[0], '"');
    EXPECT_EQ(date[4], ' ');
    EXPECT_EQ(date[7], ' ');
    EXPECT_EQ(date[12], '"');
    const auto isDigit = [](char value) {
      return value >= '0' && value <= '9';
    };
    EXPECT_TRUE(date[5] == ' ' || isDigit(date[5]));
    for (auto index : {6u, 8u, 9u, 10u, 11u}) EXPECT_TRUE(isDigit(date[index]));

    const auto& time = spellings[1];
    ASSERT_EQ(time.size(), 10u);
    EXPECT_EQ(time[0], '"');
    EXPECT_EQ(time[3], ':');
    EXPECT_EQ(time[6], ':');
    EXPECT_EQ(time[9], '"');
    for (auto index : {1u, 2u, 4u, 5u, 7u, 8u})
      EXPECT_TRUE(isDigit(time[index]));
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
  preprocessor.addSystemFrameworkPath(root.string());
  preprocessor.addQuoteIncludePath(root.string());
  preprocessor.addUserIncludePath(root.string());

  ASSERT_EQ(preprocessor.systemIncludePaths().size(), 1u);
  EXPECT_EQ(preprocessor.systemIncludePaths().front(), root.generic_string());
  ASSERT_EQ(preprocessor.systemFrameworkPaths().size(), 1u);
  EXPECT_EQ(preprocessor.systemFrameworkPaths().front(),
            root.generic_string());
  ASSERT_EQ(preprocessor.quoteIncludePaths().size(), 1u);
  EXPECT_EQ(preprocessor.quoteIncludePaths().front(), root.generic_string());
  ASSERT_EQ(preprocessor.userIncludePaths().size(), 1u);
  EXPECT_EQ(preprocessor.userIncludePaths().front(), root.generic_string());
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

TEST(Preprocessor, FrameworkIncludeCandidatesUseGenericPaths) {
  namespace fs = std::filesystem;

  Control control;
  DiagnosticsClient diagnostics;
  Preprocessor preprocessor{&control, &diagnostics};

  const auto frameworkDirectory = fs::path{"SDK"} / "Frameworks";
  auto configuredFrameworkDirectory = frameworkDirectory / "";
  configuredFrameworkDirectory.make_preferred();
  preprocessor.addSystemFrameworkPath(configuredFrameworkDirectory.string());

  auto mainFile = fs::path{"project"} / "sources" / "main.cc";
  mainFile.make_preferred();
  std::vector<Token> tokens;
  preprocessor.beginPreprocessing("#include <Kit/Header.h>\n",
                                  mainFile.string(), tokens);

  const auto framework = frameworkDirectory / "Kit.framework";
  const auto publicHeader = framework / "Headers" / "Header.h";
  const auto privateHeader = framework / "PrivateHeaders" / "Header.h";
  bool resolvedInclude = false;
  while (!resolvedInclude) {
    auto state = preprocessor.continuePreprocessing(tokens);
    ASSERT_FALSE(std::holds_alternative<ProcessingComplete>(state));

    auto* include = std::get_if<PendingInclude>(&state);
    if (!include) continue;

    const auto candidates = include->candidates();
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0].fileName, publicHeader.generic_string());
    EXPECT_TRUE(candidates[0].isSystemHeader);
    EXPECT_EQ(candidates[1].fileName, privateHeader.generic_string());
    EXPECT_TRUE(candidates[1].isSystemHeader);
    include->resolveWith(candidates[0].fileName, candidates[0].isSystemHeader);
    resolvedInclude = true;
  }

  auto contentState = preprocessor.continuePreprocessing(tokens);
  auto* content = std::get_if<PendingFileContent>(&contentState);
  ASSERT_NE(content, nullptr);
  EXPECT_EQ(content->fileName, publicHeader.generic_string());
  content->setContent("int from_framework;\n");

  while (true) {
    auto state = preprocessor.continuePreprocessing(tokens);
    if (std::holds_alternative<ProcessingComplete>(state)) break;
    ASSERT_FALSE(std::holds_alternative<PendingInclude>(state));
    ASSERT_FALSE(std::holds_alternative<PendingFileContent>(state));
  }

  preprocessor.endPreprocessing(tokens);
}
