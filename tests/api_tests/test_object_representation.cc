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
#include <cxx/symbols.h>
#include <cxx/translation_unit.h>
#include <cxx/type_traits.h>
#include <cxx/types.h>
#include <cxx/views/symbol_chain.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace cxx;

namespace {

class ObjectRepresentationContext {
 public:
  ObjectRepresentationContext(MemoryLayout::ByteOrder byteOrder,
                              std::string_view source)
      : memoryLayout_(64, byteOrder), unit_(&diagnostics_) {
    unit_.control()->setMemoryLayout(&memoryLayout_);
    unit_.setSource(std::string(source), "<test>");
    unit_.parse({.analysisMode = ParserAnalysisMode::kFull});
  }

  [[nodiscard]] auto control() -> Control* { return unit_.control(); }

  [[nodiscard]] auto unit() -> TranslationUnit* { return &unit_; }

  [[nodiscard]] auto symbol(std::string_view name) -> Symbol* {
    Symbol* result = nullptr;
    for (auto candidate : unit_.globalScope()->find(name)) {
      if (result) return nullptr;
      result = candidate;
    }
    return result;
  }

  [[nodiscard]] auto variable(std::string_view name) -> VariableSymbol* {
    return symbol_cast<VariableSymbol>(symbol(name));
  }

  [[nodiscard]] auto representation(std::string_view name)
      -> std::optional<std::vector<std::uint8_t>> {
    auto source = variable(name);
    if (!source || !source->constValue()) return std::nullopt;
    ASTInterpreter interpreter{&unit_};
    return interpreter.objectRepresentation(*source->constValue(),
                                            source->type());
  }

 private:
  MemoryLayout memoryLayout_;
  DiagnosticsClient diagnostics_;
  TranslationUnit unit_;
};

constexpr std::string_view kSource = R"(
  struct Base { unsigned short base; };
  struct Entry : Base { unsigned char tag; unsigned value; };
  struct Bits { unsigned char first : 3; unsigned char second : 5; };
  struct Pair { unsigned first; unsigned second; };
  struct Owned {
    constexpr Owned(unsigned value) : value(value) {}
    Owned(const Owned&) {}
    unsigned value;
  };

  using Bytes = unsigned char __attribute__((vector_size(4)));

  static constexpr unsigned word = 0x12345678u;
  static constexpr unsigned short matrix[2][2] = {
      {0x1234u, 0x5678u}, {0x9abcu, 0xdef0u}};
  static constexpr Entry entry{{0x1234u}, 0x56u, 0x789abcdeu};
  static constexpr Bits bits{5u, 17u};
  static constexpr Bytes bytes{1u, 2u, 3u, 4u};
  static constexpr const unsigned* pointer = nullptr;
  static constexpr Owned owned{7u};
)";

}  // namespace

TEST(ObjectRepresentation, PreservesTargetObjectLayout) {
  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    ObjectRepresentationContext context{byteOrder, kSource};

    auto word = context.representation("word");
    ASSERT_TRUE(word);
    EXPECT_EQ(*word, byteOrder == MemoryLayout::ByteOrder::kLittleEndian
                         ? (std::vector<std::uint8_t>{0x78, 0x56, 0x34, 0x12})
                         : (std::vector<std::uint8_t>{0x12, 0x34, 0x56, 0x78}));

    auto matrix = context.representation("matrix");
    ASSERT_TRUE(matrix);
    EXPECT_EQ(*matrix,
              byteOrder == MemoryLayout::ByteOrder::kLittleEndian
                  ? (std::vector<std::uint8_t>{0x34, 0x12, 0x78, 0x56, 0xbc,
                                               0x9a, 0xf0, 0xde})
                  : (std::vector<std::uint8_t>{0x12, 0x34, 0x56, 0x78, 0x9a,
                                               0xbc, 0xde, 0xf0}));

    auto entry = context.representation("entry");
    ASSERT_TRUE(entry);
    EXPECT_EQ(*entry,
              byteOrder == MemoryLayout::ByteOrder::kLittleEndian
                  ? (std::vector<std::uint8_t>{0x34, 0x12, 0x56, 0x00, 0xde,
                                               0xbc, 0x9a, 0x78})
                  : (std::vector<std::uint8_t>{0x12, 0x34, 0x56, 0x00, 0x78,
                                               0x9a, 0xbc, 0xde}));

    auto bits = context.representation("bits");
    ASSERT_TRUE(bits);
    EXPECT_EQ(*bits, byteOrder == MemoryLayout::ByteOrder::kLittleEndian
                         ? (std::vector<std::uint8_t>{0x8d})
                         : (std::vector<std::uint8_t>{0xb1}));

    auto bytes = context.representation("bytes");
    ASSERT_TRUE(bytes);
    EXPECT_EQ(*bytes, (std::vector<std::uint8_t>{1, 2, 3, 4}));
  }
}

TEST(ObjectRepresentation, RejectsUnsupportedRepresentations) {
  ObjectRepresentationContext context{MemoryLayout::ByteOrder::kLittleEndian,
                                      kSource};
  EXPECT_FALSE(context.representation("pointer"));
  EXPECT_FALSE(context.representation("owned"));

  ASTInterpreter interpreter{context.unit()};
  auto unsignedType = context.control()->getUnsignedIntType();
  auto arrayType = context.control()->getBoundedArrayType(unsignedType, 2);
  auto incompleteArray = std::make_shared<InitializerList>();
  auto word = context.variable("word");
  ASSERT_TRUE(word);
  ASSERT_TRUE(word->constValue());
  incompleteArray->elements.emplace_back(*word->constValue(), unsignedType);
  EXPECT_FALSE(interpreter.objectRepresentation(
      ConstValue{std::move(incompleteArray)}, arrayType));

  auto pairClass = symbol_cast<ClassSymbol>(context.symbol("Pair"));
  ASSERT_TRUE(pairClass);
  auto fields = context.unit()->typeTraits().aggregate_elements(pairClass);
  ASSERT_EQ(fields.size(), 2u);
  auto incompleteObject = std::make_shared<ConstObject>(pairClass->type());
  incompleteObject->addMember(fields.front(), *word->constValue());
  EXPECT_FALSE(interpreter.objectRepresentation(
      ConstValue{std::move(incompleteObject)}, pairClass->type()));

  EXPECT_FALSE(interpreter.objectRepresentation(
      ConstValue{IndeterminateValue{}}, unsignedType));
}
