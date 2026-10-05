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

#include <cxx/ast.h>
#include <cxx/control.h>
#include <cxx/diagnostics_client.h>
#include <cxx/memory_layout.h>
#include <cxx/preprocessor.h>
#include <cxx/symbols.h>
#include <cxx/translation_unit.h>
#include <cxx/types.h>
#include <cxx/views/symbol_chain.h>
#include <cxx/views/symbols.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace cxx {
namespace {

auto findFunctionDefinition(TranslationUnit& unit) -> FunctionDefinitionAST* {
  auto root = ast_cast<TranslationUnitAST>(unit.ast());
  if (!root) return nullptr;
  for (auto declaration : ListView{root->declarationList}) {
    if (auto function = ast_cast<FunctionDefinitionAST>(declaration)) {
      return function;
    }
  }
  return nullptr;
}

auto functionStatement(TranslationUnit& unit) -> CompoundStatementAST* {
  auto definition = findFunctionDefinition(unit);
  if (!definition) return nullptr;
  auto body =
      ast_cast<CompoundStatementFunctionBodyAST>(definition->functionBody);
  return body ? body->statement : nullptr;
}

auto findTypeAlias(TranslationUnit& unit, std::string_view name)
    -> TypeAliasSymbol* {
  for (auto candidate : unit.globalScope()->find(name)) {
    if (auto alias = symbol_cast<TypeAliasSymbol>(candidate)) return alias;
  }
  return nullptr;
}

auto findMemberFunctionDefinition(TranslationUnit& unit,
                                  std::string_view className,
                                  std::string_view functionName)
    -> FunctionDefinitionAST* {
  ClassSymbol* classSymbol = nullptr;
  for (auto candidate : unit.globalScope()->find(className)) {
    if ((classSymbol = symbol_cast<ClassSymbol>(candidate))) break;
  }
  if (!classSymbol) return nullptr;
  for (auto candidate : classSymbol->find(functionName)) {
    for (auto function : views::each_function(candidate)) {
      return function->declaration();
    }
  }
  return nullptr;
}

void setSourceWithSystemHeader(TranslationUnit& unit, std::string source,
                               std::string header) {
  unit.beginPreprocessing(std::move(source), "main.cc");
  while (true) {
    auto state = unit.continuePreprocessing();
    if (std::holds_alternative<ProcessingComplete>(state)) break;
    if (auto include = std::get_if<PendingInclude>(&state)) {
      include->resolveWith("system.h", /*isSystemHeader=*/true);
    } else if (auto content = std::get_if<PendingFileContent>(&state)) {
      content->setContent(header);
    }
  }
  unit.endPreprocessing();
}

TEST(DeclarationAnalysis, SkipsFunctionBodiesWithoutFullTypeChecking) {
  constexpr std::string_view source = R"(
    struct Widget {
      int method() { return missing_member; }
    };
    int answer() {
      return missing_name;
    }
  )";

  CapturingDiagnosticsClient declarationDiagnostics;
  TranslationUnit declarationUnit{&declarationDiagnostics};
  MemoryLayout declarationLayout{32};
  declarationUnit.control()->setMemoryLayout(&declarationLayout);
  declarationUnit.setSource(std::string(source), "declarations.cc");
  declarationUnit.parse({.analysisMode = ParserAnalysisMode::kDeclarations});

  ASSERT_TRUE(declarationDiagnostics.diagnostics.empty());
  auto declarationStatement = functionStatement(declarationUnit);
  ASSERT_NE(declarationStatement, nullptr);
  EXPECT_EQ(declarationStatement->statementList, nullptr);
  EXPECT_TRUE(declarationUnit.isFunctionBodyUnparsed(
      findFunctionDefinition(declarationUnit)));
  auto declarationMember =
      findMemberFunctionDefinition(declarationUnit, "Widget", "method");
  ASSERT_NE(declarationMember, nullptr);
  EXPECT_TRUE(declarationUnit.isFunctionBodyUnparsed(declarationMember));
  auto declarationMemberBody = ast_cast<CompoundStatementFunctionBodyAST>(
      declarationMember->functionBody);
  ASSERT_NE(declarationMemberBody, nullptr);
  EXPECT_EQ(declarationMemberBody->statement->statementList, nullptr);

  CapturingDiagnosticsClient syntaxDiagnostics;
  TranslationUnit syntaxUnit{&syntaxDiagnostics};
  MemoryLayout syntaxLayout{32};
  syntaxUnit.control()->setMemoryLayout(&syntaxLayout);
  syntaxUnit.setSource(std::string(source), "syntax.cc");
  syntaxUnit.parse({.analysisMode = ParserAnalysisMode::kNoTypeChecking});

  ASSERT_TRUE(syntaxDiagnostics.diagnostics.empty());
  auto syntaxStatement = functionStatement(syntaxUnit);
  ASSERT_NE(syntaxStatement, nullptr);
  EXPECT_NE(syntaxStatement->statementList, nullptr);
  EXPECT_FALSE(
      syntaxUnit.isFunctionBodyUnparsed(findFunctionDefinition(syntaxUnit)));
  auto syntaxMember =
      findMemberFunctionDefinition(syntaxUnit, "Widget", "method");
  ASSERT_NE(syntaxMember, nullptr);
  EXPECT_FALSE(syntaxUnit.isFunctionBodyUnparsed(syntaxMember));
  auto syntaxMemberBody =
      ast_cast<CompoundStatementFunctionBodyAST>(syntaxMember->functionBody);
  ASSERT_NE(syntaxMemberBody, nullptr);
  EXPECT_NE(syntaxMemberBody->statement->statementList, nullptr);

  CapturingDiagnosticsClient fullDiagnostics;
  TranslationUnit fullUnit{&fullDiagnostics};
  MemoryLayout fullLayout{32};
  fullUnit.control()->setMemoryLayout(&fullLayout);
  fullUnit.setSource(std::string(source), "full.cc");
  fullUnit.parse({.analysisMode = ParserAnalysisMode::kFull});
  EXPECT_FALSE(fullDiagnostics.diagnostics.empty());
}

TEST(DeclarationAnalysis, ResolvesOnlyUserTemplateIds) {
  constexpr std::string_view source = R"(
    #include <system.h>
    template <typename T>
    struct UserBox {};
    using UserIntBox = UserBox<int>;
  )";
  constexpr std::string_view systemHeader = R"(
    template <typename T>
    struct SystemBox {};
    using SystemIntBox = SystemBox<int>;
  )";

  CapturingDiagnosticsClient diagnostics;
  TranslationUnit unit{&diagnostics};
  setSourceWithSystemHeader(unit, std::string(source),
                            std::string(systemHeader));
  unit.parse({.analysisMode = ParserAnalysisMode::kDeclarations});

  ASSERT_TRUE(diagnostics.diagnostics.empty());
  auto userAlias = findTypeAlias(unit, "UserIntBox");
  auto systemAlias = findTypeAlias(unit, "SystemIntBox");
  ASSERT_NE(userAlias, nullptr);
  ASSERT_NE(systemAlias, nullptr);
  EXPECT_EQ(to_string(userAlias->type()), "::UserBox<int>");
  EXPECT_EQ(to_string(systemAlias->type()), "::SystemBox");
}

}  // namespace
}  // namespace cxx
