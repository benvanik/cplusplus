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
#include <cxx/ast_interpreter.h>
#include <cxx/ast_visitor.h>
#include <cxx/attributes.h>
#include <cxx/control.h>
#include <cxx/diagnostics_client.h>
#include <cxx/initialization.h>
#include <cxx/literals.h>
#include <cxx/memory_layout.h>
#include <cxx/names.h>
#include <cxx/pch.h>
#include <cxx/preprocessor.h>
#include <cxx/symbols.h>
#include <cxx/translation_unit.h>
#include <cxx/types.h>
#include <cxx/views/symbol_chain.h>
#include <cxx/views/symbols.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace cxx;

namespace {

auto keys() -> PrecompiledHeaderKeys {
  return {precompiledHeaderSerializationAbi(), "wasm32", "c++/c++23", ""};
}

class Prefix {
 public:
  explicit Prefix(std::string source) {
    unit_ = std::make_unique<TranslationUnit>(&diagnostics_);
    unit_->control()->setMemoryLayout(&memoryLayout_);
    // The builtin declarations are written in terms of the target macros a
    // toolchain would supply; these tests run without one.
    for (const auto& [name, body] : {
             std::pair{"__SIZE_TYPE__", "unsigned long"},
             std::pair{"__PTRDIFF_TYPE__", "long"},
             std::pair{"__INT8_TYPE__", "signed char"},
             std::pair{"__INT16_TYPE__", "short"},
             std::pair{"__INT32_TYPE__", "int"},
             std::pair{"__INT64_TYPE__", "long long"},
             std::pair{"__UINT8_TYPE__", "unsigned char"},
             std::pair{"__UINT16_TYPE__", "unsigned short"},
             std::pair{"__UINT32_TYPE__", "unsigned int"},
             std::pair{"__UINT64_TYPE__", "unsigned long long"},
             std::pair{"__INTPTR_TYPE__", "long"},
             std::pair{"__UINTPTR_TYPE__", "unsigned long"},
             std::pair{"__WCHAR_TYPE__", "int"},
             std::pair{"__CHAR16_TYPE__", "unsigned short"},
             std::pair{"__CHAR32_TYPE__", "unsigned int"},
         }) {
      unit_->preprocessor()->defineMacro(name, body);
    }
    unit_->setSource(std::move(source), "prefix.h");
    unit_->parse({.analysisMode = ParserAnalysisMode::kFull});
  }

  [[nodiscard]] auto unit() -> TranslationUnit* { return unit_.get(); }

  [[nodiscard]] auto emit() -> std::vector<std::uint8_t> {
    PrecompiledHeaderWriter writer{unit_.get(), keys()};
    writer.setPreprocessorState(unit_->preprocessor()->snapshot());
    auto data = writer();
    errors_ = writer.errors();
    return data;
  }

  [[nodiscard]] auto errors() const -> const std::vector<std::string>& {
    return errors_;
  }

 private:
  DiagnosticsClient diagnostics_;
  MemoryLayout memoryLayout_{32};
  std::unique_ptr<TranslationUnit> unit_;
  std::vector<std::string> errors_;
};

[[nodiscard]] auto findMember(ScopeSymbol* scope, std::string_view name)
    -> Symbol* {
  for (auto member : scope->members()) {
    auto id = name_cast<Identifier>(member->name());
    if (id && id->name() == name) return member;
  }
  return nullptr;
}

[[nodiscard]] auto constantAddress(ScopeSymbol* scope, std::string_view name)
    -> std::shared_ptr<ConstAddress> {
  auto variable = symbol_cast<VariableSymbol>(findMember(scope, name));
  if (!variable || !variable->constValue()) return {};
  auto address =
      std::get_if<std::shared_ptr<ConstAddress>>(&*variable->constValue());
  return address ? *address : nullptr;
}

template <typename T>
void expectSubobject(const ConstAddress& address, std::size_t index,
                     T expected) {
  ASSERT_LT(index, address.subobjectPath().size());
  auto actual = std::get_if<T>(&address.subobjectPath()[index]);
  ASSERT_TRUE(actual);
  EXPECT_EQ(*actual, expected);
}

[[nodiscard]] auto findBuiltinOffsetof(ExpressionAST* expression)
    -> BuiltinOffsetofExpressionAST* {
  struct Finder final : ASTVisitor {
    BuiltinOffsetofExpressionAST* result = nullptr;

    void visit(BuiltinOffsetofExpressionAST* ast) override { result = ast; }
  } finder;

  finder.accept(expression);
  return finder.result;
}

class BranchSelectionVisitor final : public ASTVisitor {
 public:
  void visit(IfStatementAST* ast) override {
    selections.push_back(ast->constexprValue);
    ASTVisitor::visit(ast);
  }

  std::vector<std::optional<bool>> selections;
};

class SubobjectPathVisitor final : public ASTVisitor {
 public:
  void visit(IdExpressionAST* ast) override {
    if (ast->subobjectPath) {
      implicitPaths.emplace_back(ListView(ast->subobjectPath).begin(),
                                 ListView(ast->subobjectPath).end());
    }
    ASTVisitor::visit(ast);
  }

  void visit(ImplicitCastExpressionAST* ast) override {
    if (ast->subobjectPath) {
      auto& paths = ast->castKind == ImplicitCastKind::kBaseToDerivedConversion
                        ? inversePaths
                        : castPaths;
      paths.emplace_back(ListView(ast->subobjectPath).begin(),
                         ListView(ast->subobjectPath).end());
    }
    ASTVisitor::visit(ast);
  }

  std::vector<std::vector<Symbol*>> implicitPaths;
  std::vector<std::vector<Symbol*>> castPaths;
  std::vector<std::vector<Symbol*>> inversePaths;
};

[[nodiscard]] auto containsBasePath(
    const std::vector<std::vector<Symbol*>>& paths,
    std::initializer_list<ClassSymbol*> expected) -> bool {
  for (const auto& path : paths) {
    if (path.size() != expected.size()) continue;
    auto expectedSymbol = expected.begin();
    const bool matches = std::ranges::all_of(path, [&](Symbol* step) {
      auto base = symbol_cast<BaseClassSymbol>(step);
      return base && base->symbol() == *expectedSymbol++;
    });
    if (matches) return true;
  }
  return false;
}

[[nodiscard]] auto containsAnonymousFieldPath(
    const std::vector<std::vector<Symbol*>>& paths) -> bool {
  return std::ranges::any_of(paths, [](const auto& path) {
    return path.size() == 2 &&
           std::ranges::all_of(path, [](Symbol* step) {
             auto field = symbol_cast<FieldSymbol>(step);
             return field && !field->name();
           });
  });
}

auto branchSelections(AST* ast) -> std::vector<std::optional<bool>> {
  BranchSelectionVisitor visitor;
  visitor.accept(ast);
  return visitor.selections;
}

class ConvertVectorVisitor final : public ASTVisitor {
 public:
  void visit(BuiltinConvertVectorExpressionAST* ast) override {
    expressions.push_back(ast);
    ASTVisitor::visit(ast);
  }

  std::vector<BuiltinConvertVectorExpressionAST*> expressions;
};

class VectorComponentVisitor final : public ASTVisitor {
 public:
  void visit(MemberExpressionAST* ast) override {
    if (ast->vectorComponentKind != VectorComponentKind::kNone)
      kinds.push_back(ast->vectorComponentKind);
    ASTVisitor::visit(ast);
  }

  std::vector<VectorComponentKind> kinds;
};

auto vectorComponentKinds(AST* ast) -> std::vector<VectorComponentKind> {
  VectorComponentVisitor visitor;
  visitor.accept(ast);
  return visitor.kinds;
}

auto convertVectorExpressions(AST* ast)
    -> std::vector<BuiltinConvertVectorExpressionAST*> {
  ConvertVectorVisitor visitor;
  visitor.accept(ast);
  return visitor.expressions;
}

void expectConvertVectorExpression(AST* ast) {
  const auto expressions = convertVectorExpressions(ast);
  ASSERT_EQ(expressions.size(), 1u);

  auto expression = expressions.front();
  ASSERT_TRUE(expression->expression);
  ASSERT_TRUE(expression->typeId);
  EXPECT_EQ(expression->valueCategory, ValueCategory::kPrValue);

  auto sourceType = unqualified_cast<VectorType>(expression->expression->type);
  ASSERT_TRUE(sourceType);
  EXPECT_EQ(sourceType->elementCount(), 4u);
  EXPECT_TRUE(type_cast<IntType>(sourceType->elementType()));

  auto typeIdType = unqualified_cast<VectorType>(expression->typeId->type);
  ASSERT_TRUE(typeIdType);
  EXPECT_EQ(typeIdType->elementCount(), 4u);
  EXPECT_TRUE(type_cast<FloatType>(typeIdType->elementType()));

  auto resultType = unqualified_cast<VectorType>(expression->type);
  ASSERT_TRUE(resultType);
  EXPECT_TRUE(expression->typeId->type == expression->type);
}

}  // namespace

TEST(PrecompiledHeader, RestoresTheGlobalScope) {
  Prefix prefix{R"(
struct Point {
  int x;
  int y;
};

int origin;
)"};

  const auto data = prefix.emit();

  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};

  ASSERT_TRUE(reader(data)) << reader.error();
  ASSERT_TRUE(consumer.hasAdoptedPrefix());

  auto globalScope = consumer.globalScope();
  ASSERT_TRUE(globalScope);

  auto point = symbol_cast<ClassSymbol>(findMember(globalScope, "Point"));
  ASSERT_TRUE(point);
  ASSERT_TRUE(point->isComplete());

  auto x = symbol_cast<FieldSymbol>(findMember(point, "x"));
  ASSERT_TRUE(x);
  ASSERT_TRUE(type_cast<IntType>(x->type()));

  ASSERT_TRUE(symbol_cast<VariableSymbol>(findMember(globalScope, "origin")));
}

TEST(PrecompiledHeader, RestoresAggregateInitializationForms) {
  Prefix prefix{R"(
struct Pair { int first; int second = 5; };
Pair braced{3};
Pair parenthesized(3);
int parenthesized_array[2](7);
)"};

  for (auto [name, expected] :
       {std::pair{"braced", InitializerForm::kList},
        std::pair{"parenthesized", InitializerForm::kParen},
        std::pair{"parenthesized_array", InitializerForm::kParen}}) {
    SCOPED_TRACE(name);
    auto variable = symbol_cast<VariableSymbol>(
        findMember(prefix.unit()->globalScope(), name));
    ASSERT_NE(variable, nullptr);
    EXPECT_EQ(Initializer{variable->initializer()}.form(), expected);
  }

  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");
  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  for (auto [name, expected] :
       {std::pair{"braced", InitializerForm::kList},
        std::pair{"parenthesized", InitializerForm::kParen},
        std::pair{"parenthesized_array", InitializerForm::kParen}}) {
    SCOPED_TRACE(name);
    auto variable =
        symbol_cast<VariableSymbol>(findMember(consumer.globalScope(), name));
    ASSERT_NE(variable, nullptr);
    EXPECT_EQ(Initializer{variable->initializer()}.form(), expected);
  }
}

TEST(PrecompiledHeader, RestoresBFloat16Type) {
  Prefix prefix{"__bf16 value;"};

  const auto data = prefix.emit();

  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto value =
      symbol_cast<VariableSymbol>(findMember(consumer.globalScope(), "value"));
  ASSERT_TRUE(value);
  ASSERT_TRUE(type_cast<BFloat16Type>(value->type()));
}

TEST(PrecompiledHeader, RestoresFloat8Types) {
  Prefix prefix{"__float8_e4m3fn e4m3fn; __float8_e5m2 e5m2;"};

  const auto data = prefix.emit();

  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto e4m3fn =
      symbol_cast<VariableSymbol>(findMember(consumer.globalScope(), "e4m3fn"));
  ASSERT_TRUE(e4m3fn);
  ASSERT_TRUE(type_cast<Float8E4M3FNType>(e4m3fn->type()));

  auto e5m2 =
      symbol_cast<VariableSymbol>(findMember(consumer.globalScope(), "e5m2"));
  ASSERT_TRUE(e5m2);
  ASSERT_TRUE(type_cast<Float8E5M2Type>(e5m2->type()));
}

TEST(PrecompiledHeader, RestoresExactFloatingRepresentations) {
  using Format = ConstFloat::Format;
  Prefix prefix{R"(
_Float16 f16;
__bf16 bf16;
__float8_e4m3fn e4m3fn;
__float8_e5m2 e5m2;
float f32;
double f64;
)"};

  struct Expected {
    const char* name;
    Format format;
    std::uint64_t bits;
  };
  const Expected expected[] = {
      {"f16", Format::kFloat16, 0x7c01},
      {"bf16", Format::kBFloat16, 0x8000},
      {"e4m3fn", Format::kFloat8E4M3FN, 0xff},
      {"e5m2", Format::kFloat8E5M2, 0x7d},
      {"f32", Format::kFloat, 0x80000000},
      {"f64", Format::kDouble, 0x7ff0000012345678ULL},
  };
  for (const auto& [name, format, bits] : expected) {
    auto variable = symbol_cast<VariableSymbol>(
        findMember(prefix.unit()->globalScope(), name));
    ASSERT_TRUE(variable);
    auto value = ConstFloat::fromBits(format, bits);
    ASSERT_TRUE(value);
    variable->setConstValue(*value);
  }

  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  for (const auto& [name, format, bits] : expected) {
    auto variable =
        symbol_cast<VariableSymbol>(findMember(consumer.globalScope(), name));
    ASSERT_TRUE(variable);
    ASSERT_TRUE(variable->constValue());
    auto value = std::get_if<ConstFloat>(&*variable->constValue());
    ASSERT_TRUE(value);
    EXPECT_EQ(value->format(), format);
    EXPECT_EQ(value->bits(), bits);
  }
}

TEST(PrecompiledHeader, RestoresVectorConstant) {
  Prefix prefix{R"(
using Int4 = int __attribute__((ext_vector_type(4)));
constexpr Int4 lanes{1, 2};
)"};

  const auto data = prefix.emit();

  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto lanes =
      symbol_cast<VariableSymbol>(findMember(consumer.globalScope(), "lanes"));
  ASSERT_TRUE(lanes);

  auto vectorType = unqualified_cast<VectorType>(lanes->type());
  ASSERT_TRUE(vectorType);
  EXPECT_EQ(vectorType->elementCount(), 4u);
  ASSERT_TRUE(type_cast<IntType>(vectorType->elementType()));

  ASSERT_TRUE(lanes->constValue());
  auto list =
      std::get_if<std::shared_ptr<InitializerList>>(&*lanes->constValue());
  ASSERT_TRUE(list);
  ASSERT_TRUE(*list);
  ASSERT_EQ((*list)->size(), 4u);

  ASTInterpreter interpreter{&consumer, consumer.globalScope()};
  const std::intmax_t expected[] = {1, 2, 0, 0};
  for (std::size_t index = 0; index < (*list)->size(); ++index) {
    const auto& [value, type] = (*list)->element(index);
    EXPECT_TRUE(consumer.typeTraits().is_same(type, vectorType->elementType()));
    auto integer = interpreter.toInt(value);
    ASSERT_TRUE(integer);
    EXPECT_EQ(*integer, expected[index]);
  }
}

TEST(PrecompiledHeader, EvaluatesAggregateDefaultsAfterRestore) {
  std::vector<std::uint8_t> data;
  {
    Prefix prefix{R"(
struct Descriptor {
  unsigned elements;
  unsigned words = this->elements / 8;
};
struct Outer {
  unsigned elements;
  constexpr unsigned compute() const {
    Descriptor first{this->elements};
    Descriptor second{this->elements * 2};
    return first.words + second.words + this->elements;
  }
};
constexpr unsigned count(unsigned elements) {
  return Outer{elements}.compute();
}
)"};

    data = prefix.emit();
    ASSERT_TRUE(prefix.errors().empty());
    ASSERT_FALSE(data.empty());
  }

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  MemoryLayout memoryLayout{32};
  consumer.control()->setMemoryLayout(&memoryLayout);
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto functions =
      views::each_function(findMember(consumer.globalScope(), "count"));
  ASSERT_EQ(std::ranges::distance(functions), 1);
  auto function = *functions.begin();

  ASTInterpreter interpreter{&consumer, consumer.globalScope()};
  for (std::intmax_t elements : {16, 32, 64}) {
    SCOPED_TRACE(elements);
    auto value = interpreter.evaluateCall(function, {elements});
    ASSERT_TRUE(value);
    auto integer = interpreter.toInt(*value);
    ASSERT_TRUE(integer);
    EXPECT_EQ(*integer, elements + 3 * elements / 8);
  }
}

TEST(PrecompiledHeader, ExecutesNestedConstructorsAfterRestore) {
  std::vector<std::uint8_t> data;
  {
    Prefix prefix{R"(
struct Format {
  unsigned elements = 32;
  unsigned words = elements / 8;
};
struct Configuration : Format {
  Format groups[2];
};
constexpr unsigned read(const Format& format) {
  return format.words;
}
constexpr unsigned count(unsigned extra) {
  Configuration config = Configuration();
  return config.words + read(Format()) + config.groups[1].words + extra;
}
struct Matrix {
  Format groups[2][2];
};
constexpr Matrix matrix = Matrix();
)"};

    data = prefix.emit();
    ASSERT_TRUE(prefix.errors().empty());
    ASSERT_FALSE(data.empty());
  }

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  MemoryLayout memoryLayout{32};
  consumer.control()->setMemoryLayout(&memoryLayout);
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto functions =
      views::each_function(findMember(consumer.globalScope(), "count"));
  ASSERT_EQ(std::ranges::distance(functions), 1);

  ASTInterpreter interpreter{&consumer, consumer.globalScope()};
  for (std::intmax_t extra : {16, 32, 64}) {
    SCOPED_TRACE(extra);
    auto value = interpreter.evaluateCall(*functions.begin(), {extra});
    ASSERT_TRUE(value);
    auto integer = interpreter.toInt(*value);
    ASSERT_TRUE(integer);
    EXPECT_EQ(*integer, 12 + extra);
  }

  auto matrix =
      symbol_cast<VariableSymbol>(findMember(consumer.globalScope(), "matrix"));
  ASSERT_TRUE(matrix);
  ASSERT_TRUE(matrix->constValue());
  auto object =
      std::get_if<std::shared_ptr<ConstObject>>(&*matrix->constValue());
  ASSERT_TRUE(object);
  ASSERT_TRUE(*object);
  ASSERT_EQ((*object)->members().size(), 1u);

  auto rows = std::get_if<std::shared_ptr<InitializerList>>(
      &(*object)->members().front().value);
  ASSERT_TRUE(rows);
  ASSERT_TRUE(*rows);
  ASSERT_EQ((*rows)->size(), 2u);
  for (std::size_t rowIndex = 0; rowIndex < (*rows)->size(); ++rowIndex) {
    const auto& [row, rowType] = (*rows)->element(rowIndex);
    ASSERT_TRUE(rowType);
    auto columns = std::get_if<std::shared_ptr<InitializerList>>(&row);
    ASSERT_TRUE(columns);
    ASSERT_TRUE(*columns);
    ASSERT_EQ((*columns)->size(), 2u);
    for (std::size_t columnIndex = 0; columnIndex < (*columns)->size();
         ++columnIndex) {
      const auto& [column, columnType] = (*columns)->element(columnIndex);
      ASSERT_TRUE(columnType);
      auto format = std::get_if<std::shared_ptr<ConstObject>>(&column);
      ASSERT_TRUE(format);
      ASSERT_TRUE(*format);
      ASSERT_EQ((*format)->members().size(), 2u);
      auto elements = interpreter.toInt((*format)->members()[0].value);
      auto words = interpreter.toInt((*format)->members()[1].value);
      ASSERT_TRUE(elements);
      ASSERT_TRUE(words);
      EXPECT_EQ(*elements, 32);
      EXPECT_EQ(*words, 4);
    }
  }
}

TEST(PrecompiledHeader, OmitsUnnamedBitFieldSubobjectsAfterRestore) {
  std::vector<std::uint8_t> data;
  {
    Prefix prefix{R"(
struct Padded {
  unsigned : 3;
  unsigned value;
};
union Storage {
  Padded padded;
  unsigned other;
};
constexpr Storage activate() {
  Storage storage;
  storage.padded.value = 9;
  return storage;
}
constexpr Storage activated = activate();
static_assert(activated.padded.value == 9);
)"};

    data = prefix.emit();
    ASSERT_TRUE(prefix.errors().empty());
    ASSERT_FALSE(data.empty());
  }

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  MemoryLayout memoryLayout{32};
  consumer.control()->setMemoryLayout(&memoryLayout);
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto activated = symbol_cast<VariableSymbol>(
      findMember(consumer.globalScope(), "activated"));
  ASSERT_TRUE(activated);
  ASSERT_TRUE(activated->constValue());
  auto storage =
      std::get_if<std::shared_ptr<ConstObject>>(&*activated->constValue());
  ASSERT_TRUE(storage);
  ASSERT_TRUE(*storage);
  ASSERT_EQ((*storage)->members().size(), 1u);

  auto padded = std::get_if<std::shared_ptr<ConstObject>>(
      &(*storage)->members().front().value);
  ASSERT_TRUE(padded);
  ASSERT_TRUE(*padded);
  ASSERT_EQ((*padded)->members().size(), 1u);

  ASTInterpreter interpreter{&consumer, consumer.globalScope()};
  auto value = interpreter.toInt((*padded)->members().front().value);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 9);
}

TEST(PrecompiledHeader, RestoresBuiltinConvertVectorExpression) {
  Prefix prefix{R"(
using Int4 = int __attribute__((ext_vector_type(4)));
using Float4 = float __attribute__((ext_vector_type(4)));
constexpr Int4 source{1, 2, 3, 4};
constexpr Float4 converted = __builtin_convertvector(source, Float4);
)"};

  expectConvertVectorExpression(prefix.unit()->ast());
  expectConvertVectorExpression(
      prefix.unit()->ast()->clone(prefix.unit()->arena()));

  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();
  expectConvertVectorExpression(consumer.ast());
}

TEST(PrecompiledHeader, RestoresVectorComponentClassifications) {
  Prefix prefix{R"(
using Int4 = int __attribute__((ext_vector_type(4)));
void select(Int4& value) {
  (void)value.xy;
  (void)value.s31;
  (void)value.hi;
  (void)value.rgba;
}
)"};

  const std::vector expected{
      VectorComponentKind::kXyzw,
      VectorComponentKind::kHexadecimal,
      VectorComponentKind::kHigh,
      VectorComponentKind::kRgba,
  };
  EXPECT_EQ(vectorComponentKinds(prefix.unit()->ast()), expected);
  EXPECT_EQ(
      vectorComponentKinds(prefix.unit()->ast()->clone(prefix.unit()->arena())),
      expected);

  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();
  EXPECT_EQ(vectorComponentKinds(consumer.ast()), expected);
}

TEST(PrecompiledHeader, RestoresRecordLayoutAttributes) {
  Prefix prefix{R"(
struct __attribute__((packed, aligned(16))) Packet;
struct Packet {
  unsigned char tag;
  unsigned value;
};
typedef unsigned AlignedValue __attribute__((aligned(16)));
)"};

  const auto data = prefix.emit();

  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto packet =
      symbol_cast<ClassSymbol>(findMember(consumer.globalScope(), "Packet"));
  ASSERT_TRUE(packet);
  ASSERT_TRUE(findAttribute(packet->attributes(), AttributeKind::kPacked));
  ASSERT_EQ(packet->minimumAlignment(), 16);
  ASSERT_EQ(packet->alignment(), 16);
  ASSERT_EQ(packet->sizeInBytes(), 16);

  auto value = symbol_cast<FieldSymbol>(findMember(packet, "value"));
  ASSERT_TRUE(value);
  ASSERT_TRUE(packet->layout());
  auto info = packet->layout()->getFieldInfo(value);
  ASSERT_TRUE(info);
  ASSERT_EQ(info->offset, 1u);

  auto alignedValue = symbol_cast<TypeAliasSymbol>(
      findMember(consumer.globalScope(), "AlignedValue"));
  ASSERT_TRUE(alignedValue);
  auto alignedType = type_cast<AlignedType>(alignedValue->type());
  ASSERT_TRUE(alignedType);
  EXPECT_EQ(alignedType->alignment(), 16u);
  EXPECT_TRUE(type_cast<UnsignedIntType>(alignedType->elementType()));
}

TEST(PrecompiledHeader, RestoresPackedEnumSemantics) {
  Prefix prefix{R"(
enum Packed { value = 255 } __attribute__((packed));
)"};

  const auto data = prefix.emit();

  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto packed =
      symbol_cast<EnumSymbol>(findMember(consumer.globalScope(), "Packed"));
  ASSERT_TRUE(packed);
  ASSERT_TRUE(findAttribute(packed->attributes(), AttributeKind::kPacked));
  ASSERT_TRUE(type_cast<UnsignedCharType>(packed->underlyingType()));
  ASSERT_TRUE(type_cast<IntType>(packed->promotionType()));
}

TEST(PrecompiledHeader, RestoresResolvedCompoundOffsetof) {
  Prefix prefix{R"(
struct Item {
  char lead;
  int value;
};

struct Container {
  char prefix;
  Item items[2];
};

constexpr auto saved_offset =
    __builtin_offsetof(Container, items[1].value);
)"};

  const auto data = prefix.emit();

  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto savedOffset = symbol_cast<VariableSymbol>(
      findMember(consumer.globalScope(), "saved_offset"));
  ASSERT_TRUE(savedOffset);

  auto builtin = findBuiltinOffsetof(savedOffset->initializer());
  ASSERT_TRUE(builtin);
  ASSERT_TRUE(builtin->value);
  EXPECT_EQ(*builtin->value, 16u);

  ASTInterpreter interpreter{&consumer, consumer.globalScope()};
  auto value = interpreter.evaluate(savedOffset->initializer());
  ASSERT_TRUE(value);
  auto integer = std::get_if<ConstInt>(&*value);
  ASSERT_TRUE(integer);
  EXPECT_EQ(integer->toUIntMax(), 16u);
}

TEST(PrecompiledHeader, RestoresRootedSubobjectAddresses) {
  std::vector<std::uint8_t> data;
  {
    Prefix prefix{R"(
struct Parameters {
  unsigned scale;
  unsigned bias;
  constexpr const unsigned& get() const { return scale; }
};
struct Pair {
  Parameters left;
  Parameters right;
};
struct AddressBase {
  unsigned value;
  constexpr const unsigned& get() const { return value; }
};
struct AddressLeft : AddressBase {};
struct AddressRight : AddressBase {};
struct RepeatedAddress : AddressLeft, AddressRight {};
constexpr Parameters first{3, 5};
constexpr Parameters second{7, 11};
constexpr Pair pair{{3, 5}, {7, 11}};
constexpr Parameters table[2][2] = {{{3, 5}, {7, 11}},
                                     {{13, 17}, {19, 23}}};
extern Parameters external;
extern RepeatedAddress repeated_address;
template <const unsigned& value>
struct Ref {};
static_assert(__is_same(Ref<first.get()>, Ref<first.scale>));
static_assert(__is_same(Ref<external.get()>, Ref<external.scale>));
static_assert(__is_same(Ref<repeated_address.AddressRight::get()>,
                        Ref<repeated_address.AddressRight::value>));
constexpr const unsigned* first_get_address = &first.get();
constexpr const unsigned* first_scale_address = &first.scale;
constexpr const unsigned* second_scale_address = &second.scale;
constexpr const unsigned* pair_left_scale_address = &pair.left.scale;
constexpr const unsigned* table_one_scale_address = &table[1][0].scale;
constexpr const unsigned* external_scale_address = &external.scale;
constexpr const unsigned* repeated_left_get_address =
    &repeated_address.AddressLeft::get();
constexpr const unsigned* repeated_left_value_address =
    &repeated_address.AddressLeft::value;
constexpr const unsigned* repeated_right_get_address =
    &repeated_address.AddressRight::get();
constexpr const unsigned* repeated_right_value_address =
    &repeated_address.AddressRight::value;
)"};
    data = prefix.emit();
    ASSERT_TRUE(prefix.errors().empty());
    ASSERT_FALSE(data.empty());
  }

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto globalScope = consumer.globalScope();
  auto parameters =
      symbol_cast<ClassSymbol>(findMember(globalScope, "Parameters"));
  auto pairType = symbol_cast<ClassSymbol>(findMember(globalScope, "Pair"));
  auto addressBase =
      symbol_cast<ClassSymbol>(findMember(globalScope, "AddressBase"));
  auto addressLeft =
      symbol_cast<ClassSymbol>(findMember(globalScope, "AddressLeft"));
  auto addressRight =
      symbol_cast<ClassSymbol>(findMember(globalScope, "AddressRight"));
  auto repeatedAddressType =
      symbol_cast<ClassSymbol>(findMember(globalScope, "RepeatedAddress"));
  auto first = findMember(globalScope, "first");
  auto second = findMember(globalScope, "second");
  auto pair = findMember(globalScope, "pair");
  auto table = findMember(globalScope, "table");
  auto external = findMember(globalScope, "external");
  auto repeatedAddress = findMember(globalScope, "repeated_address");
  ASSERT_TRUE(parameters);
  ASSERT_TRUE(pairType);
  ASSERT_TRUE(addressBase);
  ASSERT_TRUE(addressLeft);
  ASSERT_TRUE(addressRight);
  ASSERT_TRUE(repeatedAddressType);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  ASSERT_TRUE(pair);
  ASSERT_TRUE(table);
  ASSERT_TRUE(external);
  ASSERT_TRUE(repeatedAddress);
  auto scale = findMember(parameters, "scale");
  auto left = findMember(pairType, "left");
  auto addressValue = findMember(addressBase, "value");
  ASSERT_TRUE(scale);
  ASSERT_TRUE(left);
  ASSERT_TRUE(addressValue);
  ASSERT_EQ(addressLeft->baseClasses().size(), 1u);
  ASSERT_EQ(addressRight->baseClasses().size(), 1u);
  ASSERT_EQ(repeatedAddressType->baseClasses().size(), 2u);

  auto firstGet = constantAddress(globalScope, "first_get_address");
  auto firstScale = constantAddress(globalScope, "first_scale_address");
  auto secondScale = constantAddress(globalScope, "second_scale_address");
  auto pairLeftScale = constantAddress(globalScope, "pair_left_scale_address");
  auto tableOneScale = constantAddress(globalScope, "table_one_scale_address");
  auto externalScale = constantAddress(globalScope, "external_scale_address");
  auto repeatedLeftGet =
      constantAddress(globalScope, "repeated_left_get_address");
  auto repeatedLeftValue =
      constantAddress(globalScope, "repeated_left_value_address");
  auto repeatedRightGet =
      constantAddress(globalScope, "repeated_right_get_address");
  auto repeatedRightValue =
      constantAddress(globalScope, "repeated_right_value_address");
  ASSERT_TRUE(firstGet);
  ASSERT_TRUE(firstScale);
  ASSERT_TRUE(secondScale);
  ASSERT_TRUE(pairLeftScale);
  ASSERT_TRUE(tableOneScale);
  ASSERT_TRUE(externalScale);
  ASSERT_TRUE(repeatedLeftGet);
  ASSERT_TRUE(repeatedLeftValue);
  ASSERT_TRUE(repeatedRightGet);
  ASSERT_TRUE(repeatedRightValue);

  EXPECT_TRUE(firstGet->sameObject(*firstScale));
  EXPECT_FALSE(firstScale->sameObject(*secondScale));
  EXPECT_EQ(firstGet->root(), first);
  EXPECT_EQ(firstScale->root(), first);
  EXPECT_EQ(secondScale->root(), second);
  EXPECT_EQ(pairLeftScale->root(), pair);
  EXPECT_EQ(tableOneScale->root(), table);
  EXPECT_EQ(externalScale->root(), external);
  EXPECT_EQ(repeatedLeftGet->root(), repeatedAddress);
  EXPECT_EQ(repeatedRightGet->root(), repeatedAddress);
  EXPECT_TRUE(repeatedLeftGet->sameObject(*repeatedLeftValue));
  EXPECT_TRUE(repeatedRightGet->sameObject(*repeatedRightValue));
  EXPECT_FALSE(repeatedLeftValue->sameObject(*repeatedRightValue));

  ASSERT_EQ(firstScale->subobjectPath().size(), 1u);
  expectSubobject(*firstScale, 0, scale);
  ASSERT_EQ(pairLeftScale->subobjectPath().size(), 2u);
  expectSubobject(*pairLeftScale, 0, left);
  expectSubobject(*pairLeftScale, 1, scale);
  ASSERT_EQ(tableOneScale->subobjectPath().size(), 3u);
  expectSubobject(*tableOneScale, 0, std::size_t{1});
  expectSubobject(*tableOneScale, 1, std::size_t{0});
  expectSubobject(*tableOneScale, 2, scale);
  ASSERT_EQ(repeatedLeftValue->subobjectPath().size(), 3u);
  expectSubobject(*repeatedLeftValue, 0,
                  static_cast<Symbol*>(repeatedAddressType->baseClasses()[0]));
  expectSubobject(*repeatedLeftValue, 1,
                  static_cast<Symbol*>(addressLeft->baseClasses()[0]));
  expectSubobject(*repeatedLeftValue, 2, addressValue);
  EXPECT_EQ(repeatedLeftGet->subobjectPath(),
            repeatedLeftValue->subobjectPath());
  ASSERT_EQ(repeatedRightValue->subobjectPath().size(), 3u);
  expectSubobject(*repeatedRightValue, 0,
                  static_cast<Symbol*>(repeatedAddressType->baseClasses()[1]));
  expectSubobject(*repeatedRightValue, 1,
                  static_cast<Symbol*>(addressRight->baseClasses()[0]));
  expectSubobject(*repeatedRightValue, 2, addressValue);
  EXPECT_EQ(repeatedRightGet->subobjectPath(),
            repeatedRightValue->subobjectPath());
}

TEST(PrecompiledHeader, RestoresLifetimeExtendedTemporaryIdentity) {
  std::vector<std::uint8_t> data;
  {
    Prefix prefix{R"(
struct Value {
  unsigned number = 9;
  const Value* self = this;
};
constexpr const Value& reference = Value{};
constexpr const Value* saved_self = reference.self;
constexpr const unsigned& saved_number = Value{}.number;
constexpr const unsigned* saved_number_address = &saved_number;
constexpr const Value (&values)[2] = {};
constexpr const Value* saved_second = values[1].self;
static_assert(reference.self == &reference);
static_assert(saved_self->number == 9);
static_assert(values[0].self == &values[0]);
static_assert(values[0].self + 1 == &values[1]);
static_assert(saved_second == &values[1]);
)"};
    data = prefix.emit();
    ASSERT_TRUE(prefix.errors().empty());
    ASSERT_FALSE(data.empty());
  }

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");
  {
    PrecompiledHeaderReader reader{&consumer, keys()};
    ASSERT_TRUE(reader(data)) << reader.error();
  }

  auto globalScope = consumer.globalScope();
  auto value = symbol_cast<ClassSymbol>(findMember(globalScope, "Value"));
  auto number =
      value ? symbol_cast<FieldSymbol>(findMember(value, "number")) : nullptr;
  auto self =
      value ? symbol_cast<FieldSymbol>(findMember(value, "self")) : nullptr;
  auto reference =
      symbol_cast<VariableSymbol>(findMember(globalScope, "reference"));
  auto savedNumber =
      symbol_cast<VariableSymbol>(findMember(globalScope, "saved_number"));
  auto values = symbol_cast<VariableSymbol>(findMember(globalScope, "values"));
  ASSERT_TRUE(number);
  ASSERT_TRUE(self);
  ASSERT_TRUE(reference);
  ASSERT_TRUE(savedNumber);
  ASSERT_TRUE(values);

  auto referenceAddress = constantAddress(globalScope, "reference");
  auto savedSelf = constantAddress(globalScope, "saved_self");
  auto savedNumberAddress =
      constantAddress(globalScope, "saved_number_address");
  auto valuesAddress = constantAddress(globalScope, "values");
  auto savedSecond = constantAddress(globalScope, "saved_second");
  ASSERT_TRUE(referenceAddress);
  ASSERT_TRUE(savedSelf);
  ASSERT_TRUE(savedNumberAddress);
  ASSERT_TRUE(valuesAddress);
  ASSERT_TRUE(savedSecond);

  auto referenceTemporary = lifetime_extended_temporary(reference);
  auto savedNumberTemporary = lifetime_extended_temporary(savedNumber);
  auto valuesTemporary = lifetime_extended_temporary(values);
  ASSERT_TRUE(referenceTemporary);
  ASSERT_TRUE(savedNumberTemporary);
  ASSERT_TRUE(valuesTemporary);
  EXPECT_EQ(referenceAddress->root(), referenceTemporary);
  EXPECT_TRUE(referenceTemporary->isConstexpr());
  ASSERT_TRUE(savedNumber->constValue());
  auto savedNumberBinding =
      std::get_if<std::shared_ptr<ConstAddress>>(&*savedNumber->constValue());
  ASSERT_TRUE(savedNumberBinding);
  ASSERT_TRUE(*savedNumberBinding);
  EXPECT_EQ((*savedNumberBinding)->root(), savedNumberTemporary);
  EXPECT_FALSE(savedNumberTemporary->isConstexpr());
  EXPECT_EQ(savedNumberTemporary->lifetimeExtendingDeclaration(), savedNumber);
  ASSERT_EQ((*savedNumberBinding)->subobjectPath().size(), 1u);
  expectSubobject(**savedNumberBinding, 0, static_cast<Symbol*>(number));
  EXPECT_TRUE((*savedNumberBinding)->sameObject(*savedNumberAddress));
  EXPECT_EQ(valuesAddress->root(), valuesTemporary);
  EXPECT_EQ(referenceTemporary->lifetimeExtendingDeclaration(), reference);
  EXPECT_EQ(valuesTemporary->lifetimeExtendingDeclaration(), values);

  ASSERT_TRUE(referenceTemporary->constValue());
  auto referenceObject = std::get_if<std::shared_ptr<ConstObject>>(
      &*referenceTemporary->constValue());
  ASSERT_TRUE(referenceObject);
  ASSERT_TRUE(*referenceObject);
  ASSERT_EQ((*referenceObject)->members().size(), 2u);
  auto numberValue = (*referenceObject)->subobject(number);
  auto selfValue = (*referenceObject)->subobject(self);
  ASSERT_TRUE(numberValue);
  ASSERT_TRUE(selfValue);
  auto integer = std::get_if<ConstInt>(numberValue);
  auto objectSelf = std::get_if<std::shared_ptr<ConstAddress>>(selfValue);
  ASSERT_TRUE(integer);
  ASSERT_TRUE(objectSelf);
  ASSERT_TRUE(*objectSelf);
  EXPECT_EQ(integer->toUIntMax(), 9u);
  EXPECT_TRUE((*objectSelf)->sameObject(*referenceAddress));
  EXPECT_TRUE(savedSelf->sameObject(*referenceAddress));

  ASSERT_TRUE(valuesTemporary->constValue());
  auto elements = std::get_if<std::shared_ptr<InitializerList>>(
      &*valuesTemporary->constValue());
  ASSERT_TRUE(elements);
  ASSERT_TRUE(*elements);
  ASSERT_EQ((*elements)->size(), 2u);
  const auto& [firstValue, firstType] = (*elements)->element(0);
  const auto& [secondValue, secondType] = (*elements)->element(1);
  ASSERT_TRUE(firstType);
  ASSERT_TRUE(secondType);
  auto firstObject = std::get_if<std::shared_ptr<ConstObject>>(&firstValue);
  auto secondObject = std::get_if<std::shared_ptr<ConstObject>>(&secondValue);
  ASSERT_TRUE(firstObject);
  ASSERT_TRUE(secondObject);
  ASSERT_TRUE(*firstObject);
  ASSERT_TRUE(*secondObject);
  ASSERT_EQ((*firstObject)->members().size(), 2u);
  ASSERT_EQ((*secondObject)->members().size(), 2u);
  auto firstSelfValue = (*firstObject)->subobject(self);
  auto secondSelfValue = (*secondObject)->subobject(self);
  ASSERT_TRUE(firstSelfValue);
  ASSERT_TRUE(secondSelfValue);
  auto firstSelf = std::get_if<std::shared_ptr<ConstAddress>>(firstSelfValue);
  auto secondSelf = std::get_if<std::shared_ptr<ConstAddress>>(secondSelfValue);
  ASSERT_TRUE(firstSelf);
  ASSERT_TRUE(secondSelf);
  ASSERT_TRUE(*firstSelf);
  ASSERT_TRUE(*secondSelf);
  EXPECT_TRUE((*firstSelf)->sameTarget(*valuesAddress));
  EXPECT_TRUE((*secondSelf)->sameTarget(*valuesAddress));
  EXPECT_EQ((*firstSelf)->offset(), 0);
  EXPECT_EQ((*secondSelf)->offset(), 1);
  EXPECT_FALSE((*firstSelf)->sameObject(**secondSelf));
  EXPECT_TRUE((*secondSelf)->sameObject(*savedSecond));
  EXPECT_TRUE(savedSecond->sameTarget(*valuesAddress));
  EXPECT_EQ(savedSecond->root(), valuesTemporary);
}

TEST(PrecompiledHeader, RestoresImplicitObjectSubobjectPaths) {
  Prefix prefix{R"(
struct Base { unsigned value; };
struct Left : Base {};
struct Right : Base {};
struct Both : Left, Right {
  constexpr unsigned sum() const { return Left::value + Right::value; }
};
const Left& selectLeft(const Both& object) {
  return static_cast<const Left&>(object);
}
struct Unique : Left {};
const Unique& recover(const Base& base) {
  return static_cast<const Unique&>(base);
}
struct Anonymous {
  struct {
    struct {
      unsigned value;
    };
  };
  constexpr unsigned get() const { return value; }
};
)"};

  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto base =
      symbol_cast<ClassSymbol>(findMember(consumer.globalScope(), "Base"));
  auto left =
      symbol_cast<ClassSymbol>(findMember(consumer.globalScope(), "Left"));
  auto right =
      symbol_cast<ClassSymbol>(findMember(consumer.globalScope(), "Right"));
  ASSERT_TRUE(base);
  ASSERT_TRUE(left);
  ASSERT_TRUE(right);

  SubobjectPathVisitor restored;
  restored.accept(consumer.ast());
  EXPECT_TRUE(containsBasePath(restored.implicitPaths, {left, base}));
  EXPECT_TRUE(containsBasePath(restored.implicitPaths, {right, base}));
  EXPECT_TRUE(containsAnonymousFieldPath(restored.implicitPaths));
  EXPECT_TRUE(containsBasePath(restored.castPaths, {left}));
  EXPECT_TRUE(containsBasePath(restored.inversePaths, {left, base}));

  auto cloned = consumer.ast()->clone(consumer.arena());
  SubobjectPathVisitor clonedPaths;
  clonedPaths.accept(cloned);
  EXPECT_TRUE(containsBasePath(clonedPaths.implicitPaths, {left, base}));
  EXPECT_TRUE(containsBasePath(clonedPaths.implicitPaths, {right, base}));
  EXPECT_TRUE(containsAnonymousFieldPath(clonedPaths.implicitPaths));
  EXPECT_TRUE(containsBasePath(clonedPaths.castPaths, {left}));
  EXPECT_TRUE(containsBasePath(clonedPaths.inversePaths, {left, base}));
}

TEST(PrecompiledHeader, RestoresConstexprBranchSelections) {
  Prefix prefix{R"(
void known() {
  if constexpr (true) {}
  if constexpr (false) {}
}

void runtime(bool value) {
  if (value) {}
}

template <bool Value>
void deferred() {
  if constexpr (Value) {}
}
)"};

  const std::vector<std::optional<bool>> expected = {true, false, std::nullopt,
                                                     std::nullopt};
  EXPECT_EQ(branchSelections(prefix.unit()->ast()), expected);
  EXPECT_EQ(
      branchSelections(prefix.unit()->ast()->clone(prefix.unit()->arena())),
      expected);

  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();
  EXPECT_EQ(branchSelections(consumer.ast()), expected);
}

TEST(PrecompiledHeader, RestoresScopeLookup) {
  Prefix prefix{"struct Point { int x; };\nint origin;\n"};

  const auto data = prefix.emit();

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto globalScope = consumer.globalScope();

  // Lookup, not just member order: `buckets_` and `Symbol::link_` are rebuilt
  // on load and a name resolves by interned-pointer identity.
  ASSERT_NE(globalScope->find("Point").begin(),
            globalScope->find("Point").end());
  ASSERT_NE(globalScope->find("origin").begin(),
            globalScope->find("origin").end());

  auto point = symbol_cast<ClassSymbol>(findMember(globalScope, "Point"));
  ASSERT_TRUE(point);
  ASSERT_NE(point->find("x").begin(), point->find("x").end());
}

TEST(PrecompiledHeader, RestoresTheMacroEnvironment) {
  Prefix prefix{"#define PREFIX_VALUE 42\nint anchor;\n"};

  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  std::vector<Token> tokens;
  consumer.preprocessor()->preprocess("PREFIX_VALUE", "macro.cc", tokens);

  bool expandedToFortyTwo = false;
  for (const auto& token : tokens) {
    if (token.kind() != TokenKind::T_INTEGER_LITERAL) continue;
    if (token.value().literalValue->value() == "42") expandedToFortyTwo = true;
  }

  ASSERT_TRUE(expandedToFortyTwo);
}

TEST(PrecompiledHeader, RebasesTheConsumerTokenSegment) {
  Prefix prefix{"int anchor;\n"};

  const auto prefixTokenCount = prefix.unit()->tokenCount();
  const auto data = prefix.emit();

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  ASSERT_EQ(consumer.tokenSegmentBase(), prefixTokenCount);
}

TEST(PrecompiledHeader, RendersAPrefixLocationWithoutPrefixTokens) {
  Prefix prefix{"int anchor;\n"};

  const auto data = prefix.emit();

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto anchor = findMember(consumer.globalScope(), "anchor");
  ASSERT_TRUE(anchor);

  const auto location = anchor->location();
  ASSERT_FALSE(consumer.ownsLocation(location));

  const auto position = consumer.tokenStartPosition(location);
  ASSERT_EQ(position.fileName, "prefix.h");
  ASSERT_EQ(position.line, 1);
}

TEST(PrecompiledHeader, RendersALocationDerivedFromAPrefixLocation) {
  Prefix prefix{"int anchor;\n"};

  const auto data = prefix.emit();

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto anchor = findMember(consumer.globalScope(), "anchor");
  ASSERT_TRUE(anchor);

  const auto pastTheEnd = anchor->location().next();
  ASSERT_FALSE(consumer.ownsLocation(pastTheEnd));

  const auto end = consumer.tokenEndPosition(anchor->location());
  const auto position = consumer.tokenStartPosition(pastTheEnd);
  ASSERT_EQ(position.fileName, "prefix.h");
  ASSERT_EQ(position.line, end.line);
  ASSERT_EQ(position.column, end.column);
}

TEST(PrecompiledHeader, IsDeterministic) {
  Prefix first{"struct S { int a; int b; };\ntemplate <typename T> T id(T);\n"};
  Prefix second{
      "struct S { int a; int b; };\ntemplate <typename T> T id(T);\n"};

  // __DATE__ and __TIME__ are compilation inputs, so compare equal inputs.
  second.unit()->preprocessor()->restore(
      first.unit()->preprocessor()->snapshot());
  ASSERT_EQ(first.emit(), second.emit());
}

TEST(PrecompiledHeader, RejectsAForeignTarget) {
  Prefix prefix{"int anchor;\n"};
  const auto data = prefix.emit();

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{
      &consumer,
      {precompiledHeaderSerializationAbi(), "x86_64", "c++/c++23", ""}};

  ASSERT_FALSE(reader(data));
  ASSERT_FALSE(consumer.hasAdoptedPrefix());
}

TEST(PrecompiledHeader, RejectsAForeignOptionDigest) {
  Prefix prefix{"int anchor;\n"};
  const auto data = prefix.emit();

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{
      &consumer,
      {precompiledHeaderSerializationAbi(), "wasm32", "c++/c++23", "check=0"}};

  ASSERT_FALSE(reader(data));
  ASSERT_FALSE(consumer.hasAdoptedPrefix());
}

TEST(PrecompiledHeader, RejectsForeignData) {
  std::vector<std::uint8_t> data{1, 2, 3, 4};

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};

  ASSERT_FALSE(reader(data));
  ASSERT_FALSE(reader.error().empty());
  ASSERT_FALSE(consumer.hasAdoptedPrefix());
}

TEST(PrecompiledHeader, RejectsATruncatedArchive) {
  Prefix prefix{"int anchor;\n"};
  auto data = prefix.emit();

  ASSERT_GT(data.size(), 32u);
  data.resize(data.size() / 2);

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");

  PrecompiledHeaderReader reader{&consumer, keys()};

  ASSERT_FALSE(reader(data));
  ASSERT_FALSE(consumer.hasAdoptedPrefix());
}

TEST(PrecompiledHeader, DiscoversIncludePreambleDuringPreprocessing) {
  for (const auto& source : {
           std::string("// heading\n # include /* header */ <header.h>\n\nint "
                       "value;\n"),
           std::string("#include \\\n<header.h>\nint value;\n"),
           std::string("#include <header.h>\nTOKEN value;\n"),
           std::string("#include <header.h>\n/* trailing */ int value;\n"),
           std::string("#include <header.h> // trailing\n\n\nint value;\n"),
           std::string(
               "#include <header.h>\n#include <header.h>\nint value;\n"),
       }) {
    Control control;
    DiagnosticsClient diagnostics;
    Preprocessor pp(&control, &diagnostics);
    pp.setPreambleOnly(true);
    std::vector<Token> tokens;
    pp.beginPreprocessing(source, "main.cc", tokens);
    while (true) {
      auto state = pp.continuePreprocessing(tokens);
      if (std::holds_alternative<ProcessingComplete>(state)) break;
      if (auto include = std::get_if<PendingInclude>(&state)) {
        include->resolveWith("header.h", false);
      } else if (auto content = std::get_if<PendingFileContent>(&state)) {
        content->setContent("#define TOKEN int\nstruct Header {};\n");
      }
    }
    pp.endPreprocessing(tokens);
    const std::string_view header = "<header.h>";
    auto expected = source.rfind(header) + header.size();
    ASSERT_EQ(pp.preambleSize(), expected);
    ASSERT_TRUE(pp.canSnapshot());
    for (const auto& token : tokens) {
      if (token.fileId() != pp.mainSourceFileId()) continue;
      EXPECT_EQ(token.kind(), TokenKind::T_EOF_SYMBOL);
      EXPECT_EQ(token.offset(), expected);
    }
  }
}

TEST(PrecompiledHeader, RejectsOtherDirectivesBeforeFirstMainToken) {
  for (const auto& source : {
           "#define X 1\n#include <header.h>\nint x;",
           "#include <header.h>\n#undef X\nint x;",
           "#ifndef GUARD\n#define GUARD\n#include <header.h>\n#endif\n",
           "#include <header.h>\n#pragma once\nint x;",
           "#include <header.h>\n#\nint x;",
           "int x;\n#include <header.h>\n",
       }) {
    Control control;
    DiagnosticsClient diagnostics;
    Preprocessor pp(&control, &diagnostics);
    pp.setPreambleOnly(true);
    std::vector<Token> tokens;
    pp.beginPreprocessing(source, "main.cc", tokens);
    while (true) {
      auto state = pp.continuePreprocessing(tokens);
      if (std::holds_alternative<ProcessingComplete>(state)) break;
      if (auto include = std::get_if<PendingInclude>(&state)) {
        include->resolveWith("header.h", false);
      } else if (auto content = std::get_if<PendingFileContent>(&state)) {
        content->setContent("struct Header {};\n");
      }
    }
    pp.endPreprocessing(tokens);
    EXPECT_FALSE(pp.preambleSize());
  }
}

TEST(PrecompiledHeader, RejectsPreamblesThatDoNotEndAtAnEmittedToken) {
  for (const auto& source : {
           "#include <header.h>\n",
           "#include <header.h>",
           "#include <header.h>\n/* unterminated\n",
           "#include <",
           "#include \\\n<header.h>",
       }) {
    Control control;
    DiagnosticsClient diagnostics;
    Preprocessor pp(&control, &diagnostics);
    pp.setPreambleOnly(true);
    std::vector<Token> tokens;
    pp.beginPreprocessing(source, "main.cc", tokens);
    while (true) {
      auto state = pp.continuePreprocessing(tokens);
      if (std::holds_alternative<ProcessingComplete>(state)) break;
      if (auto include = std::get_if<PendingInclude>(&state)) {
        include->resolveWith("header.h", false);
      } else if (auto content = std::get_if<PendingFileContent>(&state)) {
        content->setContent("struct Header {};\n");
      }
    }
    pp.endPreprocessing(tokens);
    EXPECT_FALSE(pp.preambleSize());
  }
}

TEST(PrecompiledHeader, ReportsIncludesThatCannotBeRead) {
  Control control;
  std::vector<std::string> messages;
  struct Collector final : DiagnosticsClient {
    std::vector<std::string>& messages;
    explicit Collector(std::vector<std::string>& messages)
        : messages(messages) {}
    void report(const Diagnostic& diag) override {
      messages.push_back(diag.message());
    }
  } collector{messages};

  Preprocessor pp(&control, &collector);
  pp.setPreambleOnly(true);
  std::vector<Token> tokens;
  std::string source = "#include <header.h>\nint x;\n";
  pp.beginPreprocessing(source, "main.cc", tokens);
  while (true) {
    auto state = pp.continuePreprocessing(tokens);
    if (std::holds_alternative<ProcessingComplete>(state)) break;
    if (auto include = std::get_if<PendingInclude>(&state)) {
      include->resolveWith("header.h", false);
    } else if (auto content = std::get_if<PendingFileContent>(&state)) {
      content->setContent(std::nullopt);
    }
  }
  pp.endPreprocessing(tokens);

  ASSERT_EQ(messages.size(), 1);
  EXPECT_EQ(messages[0], "cannot read file 'header.h'");
}

TEST(PrecompiledHeader, RestoresDependentExceptionSpecifications) {
  Prefix prefix{R"(
template<bool B> using Function = int() noexcept(B);
using Nothrow = Function<true>;
using Throwing = Function<false>;
)"};
  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");
  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  auto alias = symbol_cast<TypeAliasSymbol>(
      findMember(consumer.globalScope(), "Function"));
  ASSERT_TRUE(alias);
  auto function = type_cast<FunctionType>(alias->type());
  ASSERT_TRUE(function);
  auto expression = ast_cast<IdExpressionAST>(function->noexceptExpression());
  ASSERT_TRUE(expression);
  auto parameter = symbol_cast<NonTypeParameterSymbol>(expression->symbol);
  ASSERT_TRUE(parameter);
  EXPECT_EQ(parameter->parent(), alias->templateParameters());

  auto nothrow = findMember(consumer.globalScope(), "Nothrow");
  auto throwing = findMember(consumer.globalScope(), "Throwing");
  ASSERT_TRUE(nothrow);
  ASSERT_TRUE(throwing);
  auto nothrowType = type_cast<FunctionType>(nothrow->type());
  auto throwingType = type_cast<FunctionType>(throwing->type());
  ASSERT_TRUE(nothrowType);
  ASSERT_TRUE(throwingType);
  EXPECT_EQ(nothrowType->noexceptExpression(), nullptr);
  EXPECT_EQ(throwingType->noexceptExpression(), nullptr);
  EXPECT_TRUE(nothrowType->isNoexcept());
  EXPECT_FALSE(throwingType->isNoexcept());
}

TEST(PrecompiledHeader, RestoresMemberInstantiationQueueMembership) {
  Prefix prefix{"struct First {}; struct Second {};"};
  auto first = symbol_cast<ClassSymbol>(
      findMember(prefix.unit()->globalScope(), "First"));
  auto second = symbol_cast<ClassSymbol>(
      findMember(prefix.unit()->globalScope(), "Second"));
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  ASSERT_TRUE(prefix.unit()->beginMemberInstantiation(first));
  prefix.unit()->addPendingMemberInstantiation(second);

  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());
  ASSERT_FALSE(data.empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");
  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  first = symbol_cast<ClassSymbol>(findMember(consumer.globalScope(), "First"));
  second =
      symbol_cast<ClassSymbol>(findMember(consumer.globalScope(), "Second"));
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  consumer.addPendingMemberInstantiation(first);
  consumer.addPendingMemberInstantiation(second);
  EXPECT_EQ(consumer.takePendingMemberInstantiations(),
            (std::vector<ClassSymbol*>{second}));
  EXPECT_FALSE(consumer.beginMemberInstantiation(first));
  EXPECT_TRUE(consumer.beginMemberInstantiation(second));

  consumer.reopenMemberInstantiation(first);
  EXPECT_EQ(consumer.takePendingMemberInstantiations(),
            (std::vector<ClassSymbol*>{first}));
  EXPECT_TRUE(consumer.beginMemberInstantiation(first));
}

TEST(PrecompiledHeader, RestoresMacroAliasSpellings) {
  Prefix prefix{"#define TYPE __typeof__\n#define AND and\nint anchor;\n"};
  const auto data = prefix.emit();
  ASSERT_TRUE(prefix.errors().empty());

  DiagnosticsClient diagnostics;
  TranslationUnit consumer{&diagnostics};
  consumer.setSource("", "consumer.cc");
  PrecompiledHeaderReader reader{&consumer, keys()};
  ASSERT_TRUE(reader(data)) << reader.error();

  std::vector<Token> tokens;
  consumer.preprocessor()->preprocess("TYPE(anchor) AND true", "macro.cc",
                                      tokens);
  ASSERT_EQ(tokens.size(), 8u);
  EXPECT_EQ(tokens[1].kind(), TokenKind::T_TYPEOF);
  EXPECT_EQ(tokens[1].spell(), "__typeof__");
  EXPECT_GT(tokens[1].value().intValue, 0);
  EXPECT_EQ(tokens[5].kind(), TokenKind::T_AMP_AMP);
  EXPECT_EQ(tokens[5].spell(), "and");
  EXPECT_GT(tokens[5].value().intValue, 0);
}
