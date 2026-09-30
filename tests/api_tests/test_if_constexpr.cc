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

#include <optional>
#include <ranges>
#include <vector>

#include "test_utils.h"

using namespace cxx;

namespace {

class BranchVisitor final : public ASTVisitor {
 public:
  void visit(IfStatementAST* ast) override { branches.push_back(ast); }

  std::vector<IfStatementAST*> branches;
};

}  // namespace

TEST(IfConstexpr, InstantiationRetainsSelectionAndOmitsDiscardedArm) {
  auto source = R"(
template <bool Value>
int select() {
  if constexpr (Value) return 11;
  else return 22;
}

int entry() { return select<true>() + select<false>(); }
)"_cxx;

  auto functions = views::each_function(source.get("select"));
  ASSERT_EQ(std::ranges::distance(functions), 1);

  auto* pattern = *functions.begin();
  ASSERT_TRUE(pattern->isTemplatePattern());
  ASSERT_EQ(pattern->specializations().size(), 2u);

  BranchVisitor primary;
  primary.accept(pattern->declaration());
  ASSERT_EQ(primary.branches.size(), 1u);
  EXPECT_EQ(primary.branches.front()->constexprValue, std::nullopt);

  std::vector<bool> selections;
  for (const auto& specialization : pattern->specializations()) {
    auto* function = symbol_cast<FunctionSymbol>(specialization.symbol);
    ASSERT_NE(function, nullptr);

    BranchVisitor visitor;
    visitor.accept(function->declaration());
    ASSERT_EQ(visitor.branches.size(), 1u);

    auto* branch = visitor.branches.front();
    ASSERT_TRUE(branch->constexprValue.has_value());
    selections.push_back(*branch->constexprValue);
    EXPECT_EQ(branch->statement != nullptr, *branch->constexprValue);
    EXPECT_EQ(branch->elseStatement != nullptr, !*branch->constexprValue);
  }

  EXPECT_EQ(selections, (std::vector<bool>{true, false}));
}
