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
#include <cxx/diagnostics_client.h>
#include <cxx/initialization.h>
#include <cxx/symbols.h>
#include <gtest/gtest.h>

#include <vector>

#include "test_utils.h"

using namespace cxx;

namespace {
class RangeCollector final : public ASTVisitor {
 public:
  void visit(ForRangeStatementAST* ast) override { ranges.push_back(ast); }

  // Source loops in declaration order; all nodes belong to the source arena.
  std::vector<ForRangeStatementAST*> ranges;
};
}  // namespace

TEST(RangeFor, RetainsCheckedArrayIteration) {
  auto source = R"(
void iterate(const int (&values)[3], long (&rows)[2][4]) {
  for (auto value : values) {}
  for (auto& row : rows) {}
}
)"_cxx;
  ASSERT_EQ(source.diagnosticsClient.errorCount(), 0);
  RangeCollector collector;
  collector.accept(source.ast());
  ASSERT_EQ(collector.ranges.size(), 2);
  for (auto range : collector.ranges) {
    ASSERT_NE(range->rangeVariable, nullptr);
    ASSERT_NE(range->beginVariable, nullptr);
    ASSERT_NE(range->endVariable, nullptr);
    auto array = type_cast<BoundedArrayType>(range->rangeInitializer->type);
    ASSERT_NE(array, nullptr);
    auto pointer = source.control()->getPointerType(array->elementType());
    EXPECT_EQ(range->beginVariable->type(), pointer);
    EXPECT_EQ(range->endVariable->type(), pointer);
    EXPECT_EQ(range->rangeVariable->type(),
              source.control()->getLvalueReferenceType(array));
    for (auto variable :
         {range->rangeVariable, range->beginVariable, range->endVariable}) {
      ASSERT_NE(variable->initializer(), nullptr);
      EXPECT_EQ(Initializer{variable->initializer()}.form(),
                InitializerForm::kEqual);
    }
    ASSERT_NE(range->condition, nullptr);
    EXPECT_EQ(range->condition->type, source.control()->getBoolType());
    ASSERT_NE(range->increment, nullptr);
    EXPECT_EQ(range->increment->type, pointer);
    ASSERT_NE(range->element, nullptr);
    auto declaration = ast_cast<SimpleDeclarationAST>(range->rangeDeclaration);
    ASSERT_NE(declaration, nullptr);
    auto declarator = declaration->initDeclaratorList->value;
    auto element = symbol_cast<VariableSymbol>(declarator->symbol);
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->initializer(), range->element);
    // The source syntax remains a range declaration, without an authored '='.
    EXPECT_EQ(declarator->initializer, nullptr);
  }
}

TEST(RangeFor, ChecksIteratorCopyInitializationAndDistinctSentinels) {
  auto source = R"(
struct Sentinel { int limit; };
struct Iterator {
  int position;
  Iterator(const Iterator&);
  int operator*() const;
  void operator++();
  bool operator!=(Sentinel) const;
};
struct Range {
  Iterator& begin();
  Sentinel end();
};
void iterate(Range& range) {
  for (int value : range) {}
}
)"_cxx;
  ASSERT_EQ(source.diagnosticsClient.errorCount(), 0);
  RangeCollector collector;
  collector.accept(source.ast());
  ASSERT_EQ(collector.ranges.size(), 1);
  auto range = collector.ranges.front();
  ASSERT_NE(range->beginVariable, nullptr);
  ASSERT_NE(range->endVariable, nullptr);
  EXPECT_NE(range->beginVariable->type(), range->endVariable->type());
  EXPECT_NE(range->beginVariable->initializer(), nullptr);
  EXPECT_NE(range->beginVariable->constructor(), nullptr);
  EXPECT_NE(range->rangeVariable->initializer(), nullptr);
  EXPECT_NE(range->endVariable->initializer(), nullptr);
  ASSERT_NE(range->condition, nullptr);
  EXPECT_EQ(range->condition->type, source.control()->getBoolType());
}

TEST(RangeFor, RetainsArgumentDependentPointerIteration) {
  auto source = R"(
namespace custom {
struct Range { int* first; int* last; };
int* begin(Range&);
int* end(Range&);
}
void iterate(custom::Range& range) {
  for (int& value : range) ++value;
}
)"_cxx;
  ASSERT_EQ(source.diagnosticsClient.errorCount(), 0);
  RangeCollector collector;
  collector.accept(source.ast());
  ASSERT_EQ(collector.ranges.size(), 1);
  auto range = collector.ranges.front();
  EXPECT_FALSE(range->usesMemberBeginEnd);
  EXPECT_NE(range->rangeVariable->initializer(), nullptr);
  EXPECT_NE(range->beginVariable->initializer(), nullptr);
  EXPECT_NE(range->endVariable->initializer(), nullptr);
  ASSERT_NE(range->element, nullptr);
  EXPECT_EQ(range->element->valueCategory, ValueCategory::kLValue);
}
