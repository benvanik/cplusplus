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
#include <cxx/dependent_types.h>
#include <cxx/symbols.h>
#include <gtest/gtest.h>

#include "test_utils.h"

using namespace cxx;

namespace {

class InitializerVisitor final : public ASTVisitor {
 public:
  void visit(InitDeclaratorAST* ast) override {
    variable = symbol_cast<VariableSymbol>(ast->symbol);
  }

  VariableSymbol* variable = nullptr;
};

}  // namespace

TEST(DependentTypes, SelfReferenceDoesNotHideOtherDependentOperands) {
  struct Case {
    // Source containing one automatic variable initializer.
    const char* source;
    // Expected dependency classification for the initializer.
    bool dependent;
  };

  const Case cases[] = {
      {"int entry() { const int value = ((void)&value, 7); return value; }",
       false},
      {"template<int Count> int entry() { "
       "const int value = ((void)&value, Count); return value; }",
       true},
      {"template<int Count> int entry() { "
       "const int value = Count + ((void)&value, 7); return value; }",
       true},
      {"template<int Count> int entry() { "
       "const int value = ((void)&value, 7); return value; }",
       false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.source);
    auto parsed = Source{test.source};
    InitializerVisitor visitor;
    visitor.accept(parsed.ast());
    ASSERT_NE(visitor.variable, nullptr);

    auto* initializer = visitor.variable->initializer();
    ASSERT_NE(initializer, nullptr);
    EXPECT_EQ(isDependent(&parsed.unit, initializer), test.dependent);
    EXPECT_EQ(isDependentTemplateArgument(
                  &parsed.unit,
                  TemplateArgument{static_cast<Symbol*>(visitor.variable)}),
              test.dependent);
    EXPECT_EQ(isDependent(&parsed.unit, initializer), test.dependent);
  }
}
