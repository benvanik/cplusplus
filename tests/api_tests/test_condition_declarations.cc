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
#include <cxx/ast_visitor.h>
#include <cxx/symbols.h>
#include <cxx/views/symbols.h>
#include <gtest/gtest.h>

#include <ranges>
#include <vector>

#include "test_utils.h"

using namespace cxx;

namespace {

class ConditionReferenceVisitor final : public ASTVisitor {
 public:
  void visit(ConditionExpressionAST* ast) override {
    conditions.push_back(ast);
    ASTVisitor::visit(ast);
  }

  void visit(IdExpressionAST* ast) override {
    if (symbol_cast<VariableSymbol>(ast->symbol)) references.push_back(ast);
    ASTVisitor::visit(ast);
  }

  std::vector<ConditionExpressionAST*> conditions;
  std::vector<IdExpressionAST*> references;
};

}  // namespace

TEST(ConditionDeclarations, InstantiationRemapsTheDecisionVariable) {
  auto source = R"(
template <int Value>
constexpr int select() {
  if (int decision = ((void)&decision, Value)) return decision;
  return 0;
}

static_assert(select<5>() == 5);
)"_cxx;

  auto functions = views::each_function(source.get("select"));
  ASSERT_EQ(std::ranges::distance(functions), 1);

  auto* pattern = *functions.begin();
  ASSERT_TRUE(pattern->isTemplatePattern());
  ASSERT_EQ(pattern->specializations().size(), 1u);

  auto* specialization =
      symbol_cast<FunctionSymbol>(pattern->specializations().front().symbol);
  ASSERT_NE(specialization, nullptr);

  ConditionReferenceVisitor visitor;
  visitor.accept(specialization->declaration());
  ASSERT_EQ(visitor.conditions.size(), 1u);
  ASSERT_EQ(visitor.references.size(), 2u);

  auto* decisionVariable = visitor.conditions.front()->symbol;
  ASSERT_NE(decisionVariable, nullptr);
  for (auto* reference : visitor.references) {
    EXPECT_EQ(reference->symbol, decisionVariable);
  }
}
