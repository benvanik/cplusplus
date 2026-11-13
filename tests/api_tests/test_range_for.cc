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
#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "test_utils.h"

using namespace cxx;

namespace {

class RangeCollector final : public ASTVisitor {
 public:
  void visit(ForRangeStatementAST* ast) override { ranges.push_back(ast); }

  std::vector<ForRangeStatementAST*> ranges;
};

[[nodiscard]] auto declarationVariable(StatementAST* statement)
    -> VariableSymbol* {
  auto declaration = ast_cast<DeclarationStatementAST>(statement);
  auto simple = declaration
                    ? ast_cast<SimpleDeclarationAST>(declaration->declaration)
                    : nullptr;
  auto declarator = simple && simple->initDeclaratorList
                        ? simple->initDeclaratorList->value
                        : nullptr;
  return declarator ? symbol_cast<VariableSymbol>(declarator->symbol) : nullptr;
}

struct LoweredRange {
  VariableSymbol* range = nullptr;
  VariableSymbol* begin = nullptr;
  VariableSymbol* end = nullptr;
  ForStatementAST* loop = nullptr;
};

[[nodiscard]] auto loweredRange(ForRangeStatementAST* range) -> LoweredRange {
  std::vector<StatementAST*> statements;
  if (range->range.statement) {
    for (auto statement : ListView{range->range.statement->statementList})
      statements.push_back(statement);
  }
  if (statements.size() < 4) return {};

  const auto base = statements.size() - 4;
  return {
      .range = declarationVariable(statements[base]),
      .begin = declarationVariable(statements[base + 1]),
      .end = declarationVariable(statements[base + 2]),
      .loop = ast_cast<ForStatementAST>(statements[base + 3]),
  };
}

[[nodiscard]] auto elementVariable(ForStatementAST* loop) -> VariableSymbol* {
  auto body = loop ? ast_cast<CompoundStatementAST>(loop->statement) : nullptr;
  auto first =
      body && body->statementList ? body->statementList->value : nullptr;
  return declarationVariable(first);
}

}  // namespace

TEST(RangeFor, LowersArraysThroughCheckedDeclarations) {
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
    auto lowered = loweredRange(range);
    ASSERT_NE(lowered.range, nullptr);
    ASSERT_NE(lowered.begin, nullptr);
    ASSERT_NE(lowered.end, nullptr);
    ASSERT_NE(lowered.loop, nullptr);

    auto array = type_cast<BoundedArrayType>(range->rangeInitializer->type);
    ASSERT_NE(array, nullptr);
    auto pointer = source.control()->getPointerType(array->elementType());
    EXPECT_EQ(lowered.begin->type(), pointer);
    EXPECT_EQ(lowered.end->type(), pointer);
    EXPECT_EQ(lowered.range->type(),
              source.control()->getLvalueReferenceType(array));
    for (auto variable :
         std::array{lowered.range, lowered.begin, lowered.end}) {
      ASSERT_NE(variable->initializer(), nullptr);
      EXPECT_EQ(Initializer{variable->initializer()}.form(),
                InitializerForm::kEqual);
    }

    auto element = elementVariable(lowered.loop);
    ASSERT_NE(element, nullptr);
    EXPECT_NE(element->initializer(), nullptr);
  }
}

TEST(RangeFor, RetainsCheckedIteratorAndSentinelInitialization) {
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

  auto lowered = loweredRange(collector.ranges.front());
  ASSERT_NE(lowered.range, nullptr);
  ASSERT_NE(lowered.begin, nullptr);
  ASSERT_NE(lowered.end, nullptr);
  ASSERT_NE(lowered.loop, nullptr);
  EXPECT_NE(lowered.begin->type(), lowered.end->type());
  EXPECT_NE(lowered.begin->initializer(), nullptr);
  EXPECT_NE(lowered.begin->constructor(), nullptr);
  EXPECT_NE(lowered.range->initializer(), nullptr);
  EXPECT_NE(lowered.end->initializer(), nullptr);
  ASSERT_NE(lowered.loop->condition, nullptr);
  EXPECT_EQ(lowered.loop->condition->type, source.control()->getBoolType());
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

  auto lowered = loweredRange(collector.ranges.front());
  ASSERT_NE(lowered.range, nullptr);
  ASSERT_NE(lowered.begin, nullptr);
  ASSERT_NE(lowered.end, nullptr);
  ASSERT_NE(lowered.loop, nullptr);
  EXPECT_NE(type_cast<PointerType>(lowered.begin->type()), nullptr);
  EXPECT_NE(type_cast<PointerType>(lowered.end->type()), nullptr);
  auto element = elementVariable(lowered.loop);
  ASSERT_NE(element, nullptr);
  EXPECT_NE(type_cast<LvalueReferenceType>(element->type()), nullptr);
}
