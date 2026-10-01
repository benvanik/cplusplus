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
#include <cxx/constant_bits.h>
#include <cxx/control.h>
#include <cxx/diagnostics_client.h>
#include <cxx/memory_layout.h>
#include <cxx/preprocessor.h>
#include <cxx/toolchain.h>
#include <cxx/translation_unit.h>
#include <cxx/type_traits.h>
#include <cxx/types.h>
#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace cxx;

namespace {

class ConstantContext {
 public:
  explicit ConstantContext(MemoryLayout::ByteOrder byteOrder)
      : memoryLayout_(64, byteOrder), unit_(&diagnostics_) {
    unit_.control()->setMemoryLayout(&memoryLayout_);
  }

  [[nodiscard]] auto control() -> Control* { return unit_.control(); }

  [[nodiscard]] auto unit() -> TranslationUnit* { return &unit_; }

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

TEST(ConstantBits, RegroupsScalarsAndVectorsInTargetByteOrder) {
  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    ConstantContext context{byteOrder};
    ASTInterpreter interpreter{context.unit()};
    auto control = context.control();
    auto unsignedType = control->getUnsignedIntType();
    auto shortType = control->getUnsignedShortIntType();
    auto shortVector = control->getVectorType(shortType, 2, VectorKind::kExt);
    auto scalar = context.integer(unsignedType, 0x12345678);
    ASSERT_TRUE(scalar);

    auto halves =
        bitCastConstant(interpreter, *scalar, unsignedType, shortVector);
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

    auto roundTrip =
        bitCastConstant(interpreter, *halves, shortVector, unsignedType);
    ASSERT_TRUE(roundTrip);
    EXPECT_EQ(integerBits(*roundTrip), 0x12345678);
  }
}

#if defined(__SIZEOF_INT128__)
TEST(ConstantBits, PreservesWideIntegerRepresentations) {
  const auto low = ConstInt::UWide{0xfedcba9876543210ULL};
  const auto high = ConstInt::UWide{0x0123456789abcdefULL};
  const auto bits = (high << 64) | low;

  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    ConstantContext context{byteOrder};
    ASTInterpreter interpreter{context.unit()};
    auto control = context.control();
    auto wideType = control->getUnsignedInt128Type();
    auto wordType = control->getUnsignedLongLongIntType();
    auto wordVector = control->getVectorType(wordType, 2, VectorKind::kExt);
    auto scalar = context.integer(wideType, bits);
    ASSERT_TRUE(scalar);

    auto words = bitCastConstant(interpreter, *scalar, wideType, wordVector);
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

    auto roundTrip = bitCastConstant(interpreter, *words, wordVector, wideType);
    ASSERT_TRUE(roundTrip);
    EXPECT_EQ(integerBits(*roundTrip), bits);
  }
}
#endif

TEST(ConstantBits, PreservesFloatingPayloads) {
  ConstantContext context{MemoryLayout::ByteOrder::kLittleEndian};
  ASTInterpreter interpreter{context.unit()};
  auto control = context.control();
  auto unsignedType = control->getUnsignedIntType();
  auto floatType = control->getFloatType();
  auto payload = context.integer(unsignedType, 0x7f812345);
  ASSERT_TRUE(payload);

  auto floating =
      bitCastConstant(interpreter, *payload, unsignedType, floatType);
  ASSERT_TRUE(floating);
  auto exact = std::get_if<ConstFloat>(&*floating);
  ASSERT_TRUE(exact);
  EXPECT_EQ(exact->format(), ConstFloat::Format::kFloat);
  EXPECT_EQ(exact->bits(), 0x7f812345);

  auto roundTrip =
      bitCastConstant(interpreter, *floating, floatType, unsignedType);
  ASSERT_TRUE(roundTrip);
  EXPECT_EQ(integerBits(*roundTrip), 0x7f812345);
}

TEST(ConstantBits, RejectsInvalidOrPaddedRepresentations) {
  ConstantContext context{MemoryLayout::ByteOrder::kLittleEndian};
  ASTInterpreter interpreter{context.unit()};
  auto control = context.control();
  auto byteType = control->getUnsignedCharType();
  auto boolType = control->getBoolType();
  auto unsignedType = control->getUnsignedIntType();
  auto wordType = control->getUnsignedLongLongIntType();

  auto invalidBool = context.integer(byteType, 2);
  ASSERT_TRUE(invalidBool);
  EXPECT_FALSE(bitCastConstant(interpreter, *invalidBool, byteType, boolType));

  auto boolVector = control->getVectorType(boolType, 8, VectorKind::kExt);
  auto bools = context.vector(boolType, {0, 1, 0, 1, 0, 1, 0, 1});
  ASSERT_TRUE(bools);
  EXPECT_FALSE(bitCastConstant(interpreter, *bools, boolVector, byteType));

  auto paddedVector = control->getVectorType(byteType, 3, VectorKind::kExt);
  auto bytes = context.vector(byteType, {1, 2, 3});
  ASSERT_TRUE(bytes);
  EXPECT_FALSE(
      bitCastConstant(interpreter, *bytes, paddedVector, unsignedType));

  auto pointerType = control->getPointerType(control->getIntType());
  auto word = context.integer(wordType, 0);
  ASSERT_TRUE(word);
  EXPECT_FALSE(bitCastConstant(interpreter, *word, wordType, pointerType));

  EXPECT_FALSE(bitCastConstant(interpreter, ConstValue{IndeterminateValue{}},
                               byteType, byteType));
}

TEST(ConstantBits, ByteOrderControlsPredefinedMacros) {
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
