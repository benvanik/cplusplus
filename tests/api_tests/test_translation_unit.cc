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
#include <cxx/translation_unit.h>
#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <string_view>

using namespace cxx;

TEST(SourceLocations, ValidatesSegmentBounds) {
  DiagnosticsClient diagnostics;
  TranslationUnit unit{&diagnostics};
  unit.setSource("int value;", "segment.cc");

  const auto first = unit.locationOfIndex(1);
  const auto last = unit.locationOfIndex(unit.tokenCount() - 1);
  const auto onePast = unit.locationOfIndex(unit.tokenCount());

  ASSERT_TRUE(unit.ownsLocation(first));
  ASSERT_TRUE(unit.ownsLocation(last));
  ASSERT_FALSE(unit.ownsLocation(SourceLocation{}));
  ASSERT_FALSE(unit.ownsLocation(onePast));
  (void)unit.tokenAt(SourceLocation{});

  unit.setTokenSegmentBase(64);

  ASSERT_EQ(unit.locationOfIndex(1).index(), 65);
}

TEST(SourceLocations, RejectsRangeOverflow) {
  DiagnosticsClient diagnostics;
  TranslationUnit unit{&diagnostics};
  unit.setSource("int value;", "overflow.cc");

  const auto limit = std::numeric_limits<unsigned>::max();
  const auto largestBase = limit - unit.tokenCount();

  unit.setTokenSegmentBase(largestBase);
  ASSERT_EQ(unit.locationOfIndex(unit.tokenCount()).index(), limit);
}

TEST(MemberInstantiations, PreservesOrderAndDeduplicatesEachBatch) {
  DiagnosticsClient diagnostics;
  TranslationUnit unit{&diagnostics};
  auto first = unit.control()->newClassSymbol(nullptr, {});
  auto second = unit.control()->newClassSymbol(nullptr, {});

  unit.addPendingMemberInstantiation(first);
  unit.addPendingMemberInstantiation(second);
  unit.addPendingMemberInstantiation(first);
  EXPECT_EQ(unit.takePendingMemberInstantiations(),
            (std::vector<ClassSymbol*>{first, second}));
  EXPECT_TRUE(unit.takePendingMemberInstantiations().empty());

  unit.addPendingMemberInstantiation(second);
  EXPECT_EQ(unit.takePendingMemberInstantiations(),
            (std::vector<ClassSymbol*>{second}));
}

TEST(MemberInstantiations, ProcessedClassesRequireReopening) {
  DiagnosticsClient diagnostics;
  TranslationUnit unit{&diagnostics};
  auto instance = unit.control()->newClassSymbol(nullptr, {});

  EXPECT_TRUE(unit.beginMemberInstantiation(instance));
  unit.addPendingMemberInstantiation(instance);
  EXPECT_TRUE(unit.takePendingMemberInstantiations().empty());
  EXPECT_FALSE(unit.beginMemberInstantiation(instance));

  unit.reopenMemberInstantiation(instance);
  unit.reopenMemberInstantiation(instance);
  EXPECT_EQ(unit.takePendingMemberInstantiations(),
            (std::vector<ClassSymbol*>{instance}));
  EXPECT_TRUE(unit.beginMemberInstantiation(instance));
  EXPECT_FALSE(unit.beginMemberInstantiation(instance));
}

TEST(SourceLocations, BuiltinMacroExpansionsRetainPhysicalSourceRanges) {
  struct BuiltinCase {
    std::string_view expression;
    std::string_view macroName;
    TokenKind replacementKind;
    std::string_view replacementSpelling;
  };

  constexpr BuiltinCase cases[] = {
      {"__FILE__", "__FILE__", TokenKind::T_STRING_LITERAL, "\"presumed.cc\""},
      {"__LINE__", "__LINE__", TokenKind::T_INTEGER_LITERAL, "700"},
      {"__COUNTER__", "__COUNTER__", TokenKind::T_INTEGER_LITERAL, "0"},
      {"__DATE__", "__DATE__", TokenKind::T_STRING_LITERAL, {}},
      {"__TIME__", "__TIME__", TokenKind::T_STRING_LITERAL, {}},
      {"__has_feature(cxx_exceptions)",
       "__has_feature",
       TokenKind::T_INTEGER_LITERAL,
       {}},
      {"__has_builtin(__builtin_strlen)",
       "__has_builtin",
       TokenKind::T_INTEGER_LITERAL,
       {}},
      {"__has_extension(cxx_exceptions)",
       "__has_extension",
       TokenKind::T_INTEGER_LITERAL,
       {}},
      {"__has_attribute(unused)", "__has_attribute",
       TokenKind::T_INTEGER_LITERAL, "1"},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.expression);

    std::string source = "#line 700 \"presumed.cc\"\nint value = ";
    source += test.expression;
    source += ";\n";

    DiagnosticsClient diagnostics;
    TranslationUnit unit{&diagnostics};
    unit.setSource(source, "physical.cc");

    const auto expectedOffset = source.find(test.macroName);
    ASSERT_NE(expectedOffset, std::string::npos);

    const Token* replacement = nullptr;
    SourceLocation replacementLocation;
    for (unsigned index = 1; index < unit.tokenCount(); ++index) {
      const auto& token = unit.tokens()[index];
      if (token.fileId() != unit.preprocessor()->mainSourceFileId()) continue;
      if (token.kind() != test.replacementKind) continue;
      replacement = &token;
      replacementLocation = unit.locationOfIndex(index);
      break;
    }

    ASSERT_NE(replacement, nullptr);
    EXPECT_EQ(replacement->offset(), expectedOffset);
    EXPECT_EQ(replacement->length(), test.macroName.size());
    if (!test.replacementSpelling.empty()) {
      EXPECT_EQ(replacement->spell(), test.replacementSpelling);
    }

    const auto start = unit.tokenStartPosition(replacementLocation);
    EXPECT_EQ(start.fileName, "physical.cc");
    EXPECT_EQ(start.line, 2);
    EXPECT_EQ(start.column, 13);

    const auto end = unit.tokenEndPosition(replacementLocation);
    EXPECT_EQ(end.fileName, start.fileName);
    EXPECT_EQ(end.line, start.line);
    EXPECT_EQ(end.column, start.column + test.macroName.size());

    const auto presumed = unit.presumedTokenStartPosition(replacementLocation);
    EXPECT_EQ(presumed.fileName, "presumed.cc");
    EXPECT_EQ(presumed.line, 700);
    EXPECT_EQ(presumed.column, start.column);
  }
}

TEST(SourceLocations, NestedBuiltinMacroExpansionRetainsInvocationRange) {
  std::string source =
      "#define CURRENT_LINE __LINE__\n"
      "#line 700 \"presumed.cc\"\n"
      "int value = CURRENT_LINE;\n";

  DiagnosticsClient diagnostics;
  TranslationUnit unit{&diagnostics};
  unit.setSource(source, "physical.cc");

  const auto expectedOffset =
      source.find("CURRENT_LINE", source.find("\n") + 1);
  ASSERT_NE(expectedOffset, std::string::npos);

  const Token* replacement = nullptr;
  SourceLocation replacementLocation;
  for (unsigned index = 1; index < unit.tokenCount(); ++index) {
    const auto& token = unit.tokens()[index];
    if (token.fileId() != unit.preprocessor()->mainSourceFileId()) continue;
    if (token.kind() != TokenKind::T_INTEGER_LITERAL) continue;
    replacement = &token;
    replacementLocation = unit.locationOfIndex(index);
    break;
  }

  ASSERT_NE(replacement, nullptr);
  EXPECT_EQ(replacement->spell(), "700");
  EXPECT_EQ(replacement->offset(), expectedOffset);
  EXPECT_EQ(replacement->length(), std::string_view("CURRENT_LINE").size());

  const auto start = unit.tokenStartPosition(replacementLocation);
  EXPECT_EQ(start.fileName, "physical.cc");
  EXPECT_EQ(start.line, 3);
  EXPECT_EQ(start.column, 13);

  const auto end = unit.tokenEndPosition(replacementLocation);
  EXPECT_EQ(end.fileName, start.fileName);
  EXPECT_EQ(end.line, start.line);
  EXPECT_EQ(end.column, start.column + std::string_view("CURRENT_LINE").size());

  const auto presumed = unit.presumedTokenStartPosition(replacementLocation);
  EXPECT_EQ(presumed.fileName, "presumed.cc");
  EXPECT_EQ(presumed.line, 700);
  EXPECT_EQ(presumed.column, start.column);
}
