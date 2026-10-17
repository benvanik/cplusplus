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

#include <cxx/ast_interpreter.h>
#include <cxx/control.h>
#include <cxx/diagnostics_client.h>
#include <cxx/memory_layout.h>
#include <cxx/names.h>
#include <cxx/symbols.h>
#include <cxx/translation_unit.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <span>

using namespace cxx;

namespace {

class TestConstexprCallProvider final : public ConstexprCallProvider {
 public:
  auto evaluate(ASTInterpreter& interpreter, FunctionSymbol* function,
                std::span<const ConstValue> arguments)
      -> std::optional<ConstValue> override {
    ++callCount;
    auto name = name_cast<Identifier>(function->name());
    EXPECT_NE(name, nullptr);
    if (!name) return std::nullopt;
    EXPECT_EQ(name->name(), "provided");
    EXPECT_EQ(arguments.size(), 1u);
    if (arguments.size() != 1) return std::nullopt;
    EXPECT_EQ(interpreter.toInt(arguments.front()), 20);
    return ConstValue{ConstInt{std::intmax_t{42}}};
  }

  // Number of declaration-only calls evaluated by this provider.
  int callCount = 0;
};

}  // namespace

TEST(ConstexprCallProvider, EvaluatesDeclarationOnlyFunctions) {
  constexpr auto source = R"(
    constexpr int provided(int);
    constexpr int local(int value) { return value + 1; }
    constexpr int value = provided(20);
    static_assert(value == 42);
    static_assert(local(1) == 2);
  )";

  DiagnosticsClient diagnostics;
  MemoryLayout memoryLayout{64};
  TestConstexprCallProvider provider;
  TranslationUnit unit{&diagnostics};
  unit.control()->setMemoryLayout(&memoryLayout);
  unit.setConstexprCallProvider(&provider);
  unit.setSource(source, "provider.cc");
  unit.parse({.analysisMode = ParserAnalysisMode::kFull});

  EXPECT_EQ(diagnostics.errorCount(), 0);
  EXPECT_EQ(provider.callCount, 1);
}
