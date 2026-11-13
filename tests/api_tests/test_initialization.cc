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
#include <cxx/initialization.h>
#include <cxx/symbols.h>
#include <cxx/views/symbols.h>
#include <gtest/gtest.h>

#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>

#include "test_utils.h"

using namespace cxx;

namespace {

class InitializerCollector final : public ASTVisitor {
 public:
  void visit(InitDeclaratorAST* ast) override {
    if (auto variable = symbol_cast<VariableSymbol>(ast->symbol)) {
      initializers.emplace(to_string(variable->name()), ast->initializer);
    }
    ASTVisitor::visit(ast);
  }

  // Normalized local initializers indexed by their authored names.
  std::unordered_map<std::string, ExpressionAST*> initializers;
};

}  // namespace

TEST(Initialization, PreservesAggregateFormAfterNormalizationAndCloning) {
  auto source = R"(
struct Pair { int first; int second = 5; };
Pair braced{3};
Pair parenthesized(3);
int braced_array[2]{7};
int parenthesized_array[2](7);
)"_cxx;

  for (auto [name, expected] :
       {std::pair{"braced", InitializerForm::kList},
        std::pair{"parenthesized", InitializerForm::kParen},
        std::pair{"braced_array", InitializerForm::kList},
        std::pair{"parenthesized_array", InitializerForm::kParen}}) {
    SCOPED_TRACE(name);
    auto variable = source.getAs<VariableSymbol>(name);
    ASSERT_NE(variable, nullptr);
    auto initializer = variable->initializer();
    ASSERT_NE(initializer, nullptr);
    EXPECT_EQ(Initializer{initializer}.form(), expected);
    auto cloned = initializer->clone(source.unit.arena());
    EXPECT_EQ(Initializer{cloned}.form(), expected);
  }
}

TEST(Initialization, PreservesAggregateFormInInstantiatedBodies) {
  auto source = R"(
struct Pair { int first; int second = 5; };
template <typename T>
constexpr int evaluate(T value) {
  Pair braced{value};
  Pair parenthesized(value);
  return braced.first + braced.second + parenthesized.first + parenthesized.second;
}
static_assert(evaluate(3) == 16);
)"_cxx;

  auto functions = views::each_function(source.get("evaluate"));
  ASSERT_EQ(std::ranges::distance(functions), 1);
  auto pattern = *functions.begin();
  ASSERT_EQ(pattern->specializations().size(), 1u);
  auto function =
      symbol_cast<FunctionSymbol>(pattern->specializations().front().symbol);
  ASSERT_NE(function, nullptr);
  InitializerCollector collector;
  collector.accept(function->declaration());
  EXPECT_EQ(Initializer{collector.initializers.at("braced")}.form(),
            InitializerForm::kList);
  EXPECT_EQ(Initializer{collector.initializers.at("parenthesized")}.form(),
            InitializerForm::kParen);
}
