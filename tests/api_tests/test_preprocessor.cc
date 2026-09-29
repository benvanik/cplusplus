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

#include <variant>
#include <vector>

using namespace cxx;

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
