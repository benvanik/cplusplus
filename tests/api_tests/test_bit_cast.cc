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

#include <cxx/const_value.h>
#include <cxx/control.h>
#include <cxx/diagnostics_client.h>
#include <cxx/memory_layout.h>
#include <cxx/preprocessor.h>
#include <cxx/symbols.h>
#include <cxx/toolchain.h>
#include <cxx/translation_unit.h>
#include <cxx/views/symbol_chain.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <string_view>

using namespace cxx;

namespace {

class ParsedBitCasts {
 public:
  ParsedBitCasts(MemoryLayout::ByteOrder byteOrder, std::string source)
      : memoryLayout_(64, byteOrder), unit_(&diagnostics_) {
    unit_.control()->setMemoryLayout(&memoryLayout_);
    unit_.setSource(std::move(source), "<test>");
    unit_.parse({.checkTypes = true});
  }

  [[nodiscard]] auto errorCount() const -> int {
    return diagnostics_.errorCount();
  }

  [[nodiscard]] auto value(std::string_view name) const -> const ConstValue* {
    VariableSymbol* variable = nullptr;
    for (auto candidate : unit_.globalScope()->find(name)) {
      auto next = symbol_cast<VariableSymbol>(candidate);
      if (!next || variable) return nullptr;
      variable = next;
    }
    if (!variable || !variable->constValue()) return nullptr;
    return &*variable->constValue();
  }

 private:
  MemoryLayout memoryLayout_;
  DiagnosticsClient diagnostics_;
  TranslationUnit unit_;
};

[[nodiscard]] auto integerBits(const ConstValue* value) -> ConstInt::UWide {
  return std::get<ConstInt>(*value).toUWide();
}

[[nodiscard]] auto elements(const ConstValue* value) -> const InitializerList* {
  auto list = std::get_if<std::shared_ptr<InitializerList>>(value);
  return list && *list ? list->get() : nullptr;
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
  constexpr std::string_view source = R"(
    using UShort2 = unsigned short __attribute__((ext_vector_type(2)));
    constexpr auto halves = __builtin_bit_cast(UShort2, 0x12345678u);
    constexpr auto roundTrip = __builtin_bit_cast(unsigned, halves);
  )";

  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    ParsedBitCasts context{byteOrder, std::string(source)};
    ASSERT_EQ(context.errorCount(), 0);

    const auto* halves = elements(context.value("halves"));
    ASSERT_TRUE(halves);
    ASSERT_EQ(halves->size(), 2u);
    const auto first = integerBits(&std::get<0>(halves->element(0)));
    const auto second = integerBits(&std::get<0>(halves->element(1)));
    if (byteOrder == MemoryLayout::ByteOrder::kLittleEndian) {
      EXPECT_EQ(first, 0x5678u);
      EXPECT_EQ(second, 0x1234u);
    } else {
      EXPECT_EQ(first, 0x1234u);
      EXPECT_EQ(second, 0x5678u);
    }

    const auto* roundTrip = context.value("roundTrip");
    ASSERT_TRUE(roundTrip);
    EXPECT_EQ(integerBits(roundTrip), 0x12345678u);
  }
}

#if defined(__SIZEOF_INT128__)
TEST(BitCast, PreservesWideIntegerRepresentations) {
  constexpr std::string_view source = R"(
    using ULong2 = unsigned long long __attribute__((ext_vector_type(2)));
    constexpr unsigned __int128 wide =
        (static_cast<unsigned __int128>(0x0123456789abcdefULL) << 64) |
        0xfedcba9876543210ULL;
    constexpr auto words = __builtin_bit_cast(ULong2, wide);
    constexpr auto roundTrip =
        __builtin_bit_cast(unsigned __int128, words);
  )";
  const auto low = ConstInt::UWide{0xfedcba9876543210ULL};
  const auto high = ConstInt::UWide{0x0123456789abcdefULL};
  const auto wide = (high << 64) | low;

  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    ParsedBitCasts context{byteOrder, std::string(source)};
    ASSERT_EQ(context.errorCount(), 0);

    const auto* words = elements(context.value("words"));
    ASSERT_TRUE(words);
    ASSERT_EQ(words->size(), 2u);
    const auto first = integerBits(&std::get<0>(words->element(0)));
    const auto second = integerBits(&std::get<0>(words->element(1)));
    if (byteOrder == MemoryLayout::ByteOrder::kLittleEndian) {
      EXPECT_EQ(first, low);
      EXPECT_EQ(second, high);
    } else {
      EXPECT_EQ(first, high);
      EXPECT_EQ(second, low);
    }

    const auto* roundTrip = context.value("roundTrip");
    ASSERT_TRUE(roundTrip);
    EXPECT_EQ(integerBits(roundTrip), wide);
  }
}
#endif

TEST(BitCast, PreservesExactFloatingPayloads) {
  ParsedBitCasts context{MemoryLayout::ByteOrder::kLittleEndian, R"(
    constexpr auto payload = __builtin_bit_cast(float, 0x7f812345u);
    constexpr auto roundTrip = __builtin_bit_cast(unsigned, payload);
  )"};
  ASSERT_EQ(context.errorCount(), 0);

  const auto* payload = context.value("payload");
  ASSERT_TRUE(payload);
  const auto* floating = std::get_if<ConstFloat>(payload);
  ASSERT_TRUE(floating);
  EXPECT_EQ(floating->format(), ConstFloat::Format::kFloat);
  EXPECT_EQ(floating->bits(), 0x7f812345u);

  const auto* roundTrip = context.value("roundTrip");
  ASSERT_TRUE(roundTrip);
  EXPECT_EQ(integerBits(roundTrip), 0x7f812345u);
}

TEST(BitCast, PlacesBitFieldsInTargetByteOrder) {
  constexpr std::string_view source = R"(
    struct ByteBits {
      unsigned char first : 3;
      unsigned char second : 5;
    };
    constexpr auto byteBits =
        __builtin_bit_cast(unsigned char, ByteBits{5, 17});

    struct WordBits {
      unsigned first : 4;
      unsigned second : 12;
      unsigned third : 16;
    };
    constexpr auto wordBits =
        __builtin_bit_cast(unsigned, WordBits{0xa, 0xbcd, 0x1234});
  )";

  for (auto byteOrder : {MemoryLayout::ByteOrder::kLittleEndian,
                         MemoryLayout::ByteOrder::kBigEndian}) {
    ParsedBitCasts context{byteOrder, std::string(source)};
    ASSERT_EQ(context.errorCount(), 0);

    const auto* byteBits = context.value("byteBits");
    ASSERT_TRUE(byteBits);
    EXPECT_EQ(
        integerBits(byteBits),
        byteOrder == MemoryLayout::ByteOrder::kLittleEndian ? 0x8du : 0xb1u);

    const auto* wordBits = context.value("wordBits");
    ASSERT_TRUE(wordBits);
    EXPECT_EQ(integerBits(wordBits),
              byteOrder == MemoryLayout::ByteOrder::kLittleEndian
                  ? 0x1234bcdaULL
                  : 0xabcd1234ULL);
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

    const auto macros = std::format("{}", preprocessor.macros());
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
