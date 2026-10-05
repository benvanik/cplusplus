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
#include <cxx/preprocessor.h>
#include <cxx/symbols.h>
#include <cxx/toolchain.h>
#include <cxx/translation_unit.h>
#include <cxx/type_traits.h>
#include <cxx/types.h>
#include <cxx/views/symbol_chain.h>
#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace cxx;

namespace {

class BitCastContext {
 public:
  explicit BitCastContext(MemoryLayout::ByteOrder byteOrder,
                          std::string_view source = {})
      : memoryLayout_(64, byteOrder), unit_(&diagnostics_) {
    unit_.control()->setMemoryLayout(&memoryLayout_);
    if (!source.empty()) {
      unit_.setSource(std::string(source), "<test>");
      unit_.parse({.analysisMode = ParserAnalysisMode::kFull});
    }
  }

  [[nodiscard]] auto control() -> Control* { return unit_.control(); }

  [[nodiscard]] auto unit() -> TranslationUnit* { return &unit_; }

  [[nodiscard]] auto get(ScopeSymbol* scope, std::string_view name) -> Symbol* {
    Symbol* result = nullptr;
    for (auto candidate : scope->find(name)) {
      if (result) return nullptr;
      result = candidate;
    }
    return result;
  }

  [[nodiscard]] auto integer(const Type* type, ConstInt::UWide bits)
      -> std::optional<ConstValue> {
    auto representation = unit_.typeTraits().integral_representation(type);
    if (!representation) return std::nullopt;
    auto value = ConstInt::make(std::bit_cast<ConstInt::Wide>(bits),
                                representation->bits, representation->isSigned);
    if (!value) return std::nullopt;
    return ConstValue{*value};
  }

  [[nodiscard]] auto vector(const Type* elementType,
                            const std::vector<ConstInt::UWide>& elements)
      -> std::optional<ConstValue> {
    auto list = std::make_shared<InitializerList>();
    list->elements.reserve(elements.size());
    for (auto bits : elements) {
      auto value = integer(elementType, bits);
      if (!value) return std::nullopt;
      list->elements.emplace_back(std::move(*value), elementType);
    }
    return ConstValue{std::move(list)};
  }

 private:
  MemoryLayout memoryLayout_;
  DiagnosticsClient diagnostics_;
  TranslationUnit unit_;
};

[[nodiscard]] auto integerBits(const ConstValue& value) -> ConstInt::UWide {
  return std::get<ConstInt>(value).toUWide();
}

class MacroToolchain final : public Toolchain {
 public:
  MacroToolchain(Preprocessor* preprocessor, MemoryLayout::ByteOrder byteOrder)
      : Toolchain(preprocessor, Triple{"x86_64-unknown-linux-gnu"}) {
    setMemoryLayout(std::make_unique<MemoryLayout>(64, byteOrder));
  }

  void addSystemIncludePaths() override {}
  void addSystemCppIncludePaths() override {}
  void addPredefinedMacros() override {}
};

}  // namespace

TEST(BitCast, RegroupsScalarsAndVectorsInTargetByteOrder) {
  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    BitCastContext context{byteOrder};
    ASTInterpreter interpreter{context.unit()};
    auto control = context.control();
    auto unsignedType = control->getUnsignedIntType();
    auto shortType = control->getUnsignedShortIntType();
    auto shortVector = control->getVectorType(shortType, 2, VectorKind::kExt);
    auto scalar = context.integer(unsignedType, 0x12345678);
    ASSERT_TRUE(scalar);

    auto halves = interpreter.bitCast(*scalar, unsignedType, shortVector);
    ASSERT_TRUE(halves);
    auto list = std::get<std::shared_ptr<InitializerList>>(*halves);
    ASSERT_TRUE(list);
    ASSERT_EQ(list->elements.size(), 2u);
    if (byteOrder == MemoryLayout::ByteOrder::kLittleEndian) {
      EXPECT_EQ(integerBits(std::get<0>(list->elements[0])), 0x5678);
      EXPECT_EQ(integerBits(std::get<0>(list->elements[1])), 0x1234);
    } else {
      EXPECT_EQ(integerBits(std::get<0>(list->elements[0])), 0x1234);
      EXPECT_EQ(integerBits(std::get<0>(list->elements[1])), 0x5678);
    }

    auto roundTrip = interpreter.bitCast(*halves, shortVector, unsignedType);
    ASSERT_TRUE(roundTrip);
    EXPECT_EQ(integerBits(*roundTrip), 0x12345678);
  }
}

#if defined(__SIZEOF_INT128__)
TEST(BitCast, PreservesWideIntegerRepresentations) {
  const auto low = ConstInt::UWide{0xfedcba9876543210ULL};
  const auto high = ConstInt::UWide{0x0123456789abcdefULL};
  const auto bits = (high << 64) | low;

  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    BitCastContext context{byteOrder};
    ASTInterpreter interpreter{context.unit()};
    auto control = context.control();
    auto wideType = control->getUnsignedInt128Type();
    auto wordType = control->getUnsignedLongLongIntType();
    auto wordVector = control->getVectorType(wordType, 2, VectorKind::kExt);
    auto scalar = context.integer(wideType, bits);
    ASSERT_TRUE(scalar);

    auto words = interpreter.bitCast(*scalar, wideType, wordVector);
    ASSERT_TRUE(words);
    auto list = std::get<std::shared_ptr<InitializerList>>(*words);
    ASSERT_TRUE(list);
    ASSERT_EQ(list->elements.size(), 2u);
    const auto first = integerBits(std::get<0>(list->elements[0]));
    const auto second = integerBits(std::get<0>(list->elements[1]));
    if (byteOrder == MemoryLayout::ByteOrder::kLittleEndian) {
      EXPECT_EQ(first, low);
      EXPECT_EQ(second, high);
    } else {
      EXPECT_EQ(first, high);
      EXPECT_EQ(second, low);
    }

    auto roundTrip = interpreter.bitCast(*words, wordVector, wideType);
    ASSERT_TRUE(roundTrip);
    EXPECT_EQ(integerBits(*roundTrip), bits);
  }
}
#endif

TEST(BitCast, PreservesFloatingPayloads) {
  BitCastContext context{MemoryLayout::ByteOrder::kLittleEndian};
  ASTInterpreter interpreter{context.unit()};
  auto control = context.control();
  auto unsignedType = control->getUnsignedIntType();
  auto floatType = control->getFloatType();
  auto payload = context.integer(unsignedType, 0x7f812345);
  ASSERT_TRUE(payload);

  auto floating = interpreter.bitCast(*payload, unsignedType, floatType);
  ASSERT_TRUE(floating);
  auto exact = std::get_if<ConstFloat>(&*floating);
  ASSERT_TRUE(exact);
  EXPECT_EQ(exact->format(), ConstFloat::Format::kFloat);
  EXPECT_EQ(exact->bits(), 0x7f812345);

  auto roundTrip = interpreter.bitCast(*floating, floatType, unsignedType);
  ASSERT_TRUE(roundTrip);
  EXPECT_EQ(integerBits(*roundTrip), 0x7f812345);
}

TEST(BitCast, HandlesInvalidPaddedAndIndeterminateRepresentations) {
  BitCastContext context{MemoryLayout::ByteOrder::kLittleEndian};
  ASTInterpreter interpreter{context.unit()};
  auto control = context.control();
  auto byteType = control->getUnsignedCharType();
  auto boolType = control->getBoolType();
  auto unsignedType = control->getUnsignedIntType();
  auto wordType = control->getUnsignedLongLongIntType();

  auto invalidBool = context.integer(byteType, 2);
  ASSERT_TRUE(invalidBool);
  EXPECT_FALSE(interpreter.bitCast(*invalidBool, byteType, boolType));

  auto boolVector = control->getVectorType(boolType, 8, VectorKind::kExt);
  auto bools = context.vector(boolType, {0, 1, 0, 1, 0, 1, 0, 1});
  ASSERT_TRUE(bools);
  EXPECT_FALSE(interpreter.bitCast(*bools, boolVector, byteType));

  auto paddedVector = control->getVectorType(byteType, 3, VectorKind::kExt);
  auto bytes = context.vector(byteType, {1, 2, 3});
  ASSERT_TRUE(bytes);
  EXPECT_FALSE(interpreter.bitCast(*bytes, paddedVector, unsignedType));

  auto pointerType = control->getPointerType(control->getIntType());
  auto word = context.integer(wordType, 0);
  ASSERT_TRUE(word);
  EXPECT_FALSE(interpreter.bitCast(*word, wordType, pointerType));

  auto indeterminate =
      interpreter.bitCast(ConstValue{IndeterminateValue{}}, byteType, byteType);
  ASSERT_TRUE(indeterminate);
  EXPECT_TRUE(std::holds_alternative<IndeterminateValue>(*indeterminate));
}

TEST(BitCast, PlacesBitFieldsInTargetByteOrder) {
  constexpr std::string_view source = R"(
    struct Bits {
      unsigned char first : 3;
      unsigned char second : 5;
    };
  )";

  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    BitCastContext context{byteOrder, source};
    ASTInterpreter interpreter{context.unit()};
    auto bitsClass = symbol_cast<ClassSymbol>(
        context.get(context.unit()->globalScope(), "Bits"));
    ASSERT_TRUE(bitsClass);
    auto first = symbol_cast<FieldSymbol>(context.get(bitsClass, "first"));
    auto second = symbol_cast<FieldSymbol>(context.get(bitsClass, "second"));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);

    auto firstValue = context.integer(first->type(), 5);
    auto secondValue = context.integer(second->type(), 17);
    ASSERT_TRUE(firstValue);
    ASSERT_TRUE(secondValue);
    auto object = std::make_shared<ConstObject>(bitsClass->type());
    object->addMember(first, std::move(*firstValue));
    object->addMember(second, std::move(*secondValue));

    auto byte =
        interpreter.bitCast(ConstValue{std::move(object)}, bitsClass->type(),
                            context.control()->getUnsignedCharType());
    ASSERT_TRUE(byte);
    EXPECT_EQ(
        integerBits(*byte),
        byteOrder == MemoryLayout::ByteOrder::kLittleEndian ? 0x8d : 0xb1);
  }
}

TEST(BitCast, ByteOrderControlsPredefinedMacros) {
  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    Control control;
    DiagnosticsClient diagnostics;
    Preprocessor preprocessor{&control, &diagnostics};
    MacroToolchain toolchain{&preprocessor, byteOrder};
    toolchain.addCommonMacros();

    std::ostringstream output;
    preprocessor.printMacros(output);
    const auto macros = std::move(output).str();
    if (byteOrder == MemoryLayout::ByteOrder::kLittleEndian) {
      EXPECT_NE(macros.find("#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__\n"),
                std::string::npos);
      EXPECT_NE(macros.find("#define __LITTLE_ENDIAN__ 1\n"),
                std::string::npos);
      EXPECT_EQ(macros.find("#define __BIG_ENDIAN__ 1\n"), std::string::npos);
    } else {
      EXPECT_NE(macros.find("#define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__\n"),
                std::string::npos);
      EXPECT_NE(macros.find("#define __BIG_ENDIAN__ 1\n"), std::string::npos);
      EXPECT_EQ(macros.find("#define __LITTLE_ENDIAN__ 1\n"),
                std::string::npos);
    }
  }
}
