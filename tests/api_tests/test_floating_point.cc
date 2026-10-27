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

#include <cxx/floating_point.h>
#include <cxx/memory_layout.h>
#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>

using namespace cxx;

namespace {

const FloatingPointFormat kBinary16{.exponentBits = 5, .significandDigits = 11};
const FloatingPointFormat kBFloat16{.exponentBits = 8, .significandDigits = 8};
const FloatingPointFormat kFloat8E4M3FN{
    .exponentBits = 4,
    .significandDigits = 4,
    .specialValueEncoding =
        FloatingPointFormat::SpecialValueEncoding::kFiniteOnly};
const FloatingPointFormat kFloat8E5M2{.exponentBits = 5,
                                      .significandDigits = 3};
const FloatingPointFormat kBinary32{.exponentBits = 8, .significandDigits = 24};
const FloatingPointFormat kBinary64{.exponentBits = 11,
                                    .significandDigits = 53};

auto binary16Bits(long double value) -> std::uint16_t {
  return static_cast<std::uint16_t>(kBinary16.representation(value));
}

auto roundBinary16(long double value) -> double {
  return kBinary16.value(binary16Bits(value));
}

auto bfloat16Bits(long double value) -> std::uint16_t {
  return static_cast<std::uint16_t>(kBFloat16.representation(value));
}

auto roundToBFloat16(long double value) -> double {
  return kBFloat16.value(bfloat16Bits(value));
}

auto float8E4M3FNBits(long double value) -> std::uint8_t {
  return static_cast<std::uint8_t>(kFloat8E4M3FN.representation(value));
}

auto roundE4M3FN(long double value) -> double {
  return kFloat8E4M3FN.value(float8E4M3FNBits(value));
}

auto float8E5M2Bits(long double value) -> std::uint8_t {
  return static_cast<std::uint8_t>(kFloat8E5M2.representation(value));
}

auto roundE5M2(long double value) -> double {
  return kFloat8E5M2.value(float8E5M2Bits(value));
}

auto positiveBinary16Value(std::uint16_t bits) -> float {
  const auto exponent = (bits >> 10) & 0x1f;
  const auto fraction = bits & 0x3ff;
  if (exponent == 0) return std::ldexp(static_cast<float>(fraction), -24);
  return std::ldexp(static_cast<float>(0x400 | fraction), exponent - 25);
}

auto positiveBFloat16Value(std::uint16_t bits) -> float {
  const auto exponent = (bits >> 7) & 0xff;
  const auto fraction = bits & 0x7f;
  if (exponent == 0) return std::ldexp(static_cast<float>(fraction), -133);
  return std::ldexp(static_cast<float>(0x80 | fraction), exponent - 134);
}

auto positiveFloat8E4M3FNValue(std::uint8_t bits) -> float {
  const auto exponent = (bits >> 3) & 0xf;
  const auto fraction = bits & 0x7;
  if (exponent == 0) return std::ldexp(static_cast<float>(fraction), -9);
  return std::ldexp(static_cast<float>(0x8 | fraction), exponent - 10);
}

auto positiveFloat8E5M2Value(std::uint8_t bits) -> float {
  const auto exponent = (bits >> 2) & 0x1f;
  const auto fraction = bits & 0x3;
  if (exponent == 0) return std::ldexp(static_cast<float>(fraction), -16);
  return std::ldexp(static_cast<float>(0x4 | fraction), exponent - 17);
}

}  // namespace

TEST(FloatingPointFormat, Binary16SpecialValues) {
  EXPECT_EQ(0x0000, binary16Bits(0.0L));
  EXPECT_EQ(0x8000, binary16Bits(-0.0L));
  EXPECT_EQ(0x7c00, binary16Bits(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(0xfc00,
            binary16Bits(-std::numeric_limits<long double>::infinity()));

  EXPECT_EQ(std::numeric_limits<float>::infinity(),
            roundBinary16(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            roundBinary16(-std::numeric_limits<long double>::infinity()));
  EXPECT_TRUE(
      std::isnan(roundBinary16(std::numeric_limits<long double>::quiet_NaN())));
}

TEST(FloatingPointFormat, Binary16NormalValues) {
  EXPECT_EQ(1.0f, roundBinary16(1.00048828125L));
  EXPECT_EQ(1.001953125f, roundBinary16(1.00146484375L));
  EXPECT_EQ(1.0009765625f, roundBinary16(0x1.0020000000001p0L));
  EXPECT_EQ(-1.001953125f, roundBinary16(-1.00146484375L));
}

TEST(FloatingPointFormat, Binary16SubnormalValues) {
  EXPECT_EQ(0.0f, roundBinary16(0x1p-25L));
  EXPECT_EQ(0x1p-24f, roundBinary16(0x1.0000000000001p-25L));
  EXPECT_EQ(0x1p-23f, roundBinary16(0x1.8p-24L));
  EXPECT_EQ(0x1p-14f, roundBinary16(0x1.ffcp-15L));
}

TEST(FloatingPointFormat, Binary16Overflow) {
  EXPECT_EQ(65504.0L, kBinary16.maximumFiniteMagnitude());
  EXPECT_EQ(65504.0f, roundBinary16(65519.0L));
  EXPECT_EQ(std::numeric_limits<float>::infinity(), roundBinary16(65520.0L));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(), roundBinary16(-65520.0L));

  EXPECT_TRUE(kBinary16.conversionDoesNotOverflow(65519.0L));
  EXPECT_TRUE(kBinary16.conversionDoesNotOverflow(-65519.0L));
  EXPECT_FALSE(kBinary16.conversionDoesNotOverflow(65520.0L));
  EXPECT_TRUE(kBinary16.conversionDoesNotOverflow(
      std::numeric_limits<long double>::infinity()));
}

TEST(FloatingPointFormat, Binary16FiniteBoundaries) {
  for (std::uint16_t bits = 0; bits < 0x7bff; ++bits) {
    SCOPED_TRACE(bits);
    const auto lower = positiveBinary16Value(bits);
    const auto upper = positiveBinary16Value(bits + 1);
    const auto midpoint =
        (static_cast<long double>(lower) + static_cast<long double>(upper)) / 2;
    const auto expectedMidpoint = (bits & 1) == 0 ? lower : upper;

    EXPECT_EQ(lower, roundBinary16(std::nextafter(
                         midpoint, static_cast<long double>(lower))));
    EXPECT_EQ(expectedMidpoint, roundBinary16(midpoint));
    EXPECT_EQ(upper, roundBinary16(std::nextafter(
                         midpoint, static_cast<long double>(upper))));
    EXPECT_EQ(-expectedMidpoint, roundBinary16(-midpoint));
  }
}

TEST(FloatingPointFormat, BFloat16SpecialValues) {
  EXPECT_EQ(0x0000, bfloat16Bits(0.0L));
  EXPECT_EQ(0x8000, bfloat16Bits(-0.0L));
  EXPECT_EQ(0x7f80, bfloat16Bits(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(0xff80,
            bfloat16Bits(-std::numeric_limits<long double>::infinity()));

  EXPECT_EQ(std::numeric_limits<float>::infinity(),
            roundToBFloat16(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            roundToBFloat16(-std::numeric_limits<long double>::infinity()));
  EXPECT_TRUE(std::isnan(
      roundToBFloat16(std::numeric_limits<long double>::quiet_NaN())));
}

TEST(FloatingPointFormat, BFloat16NormalValues) {
  EXPECT_EQ(1.0f, roundToBFloat16(1.00390625L));
  EXPECT_EQ(1.015625f, roundToBFloat16(1.01171875L));
  EXPECT_EQ(1.0078125f, roundToBFloat16(0x1.0100000000001p0L));
  EXPECT_EQ(-1.015625f, roundToBFloat16(-1.01171875L));
}

TEST(FloatingPointFormat, BFloat16SubnormalValues) {
  EXPECT_EQ(0.0f, roundToBFloat16(0x1p-134L));
  EXPECT_EQ(0x1p-133f, roundToBFloat16(0x1.0000000000001p-134L));
  EXPECT_EQ(0x1p-132f, roundToBFloat16(0x1.8p-133L));
  EXPECT_EQ(0x1p-126f, roundToBFloat16(0x1.fep-127L));
}

TEST(FloatingPointFormat, BFloat16Overflow) {
  constexpr auto kMaximum = 0x1.fep127f;
  constexpr auto kThreshold = 0x1.ffp127L;
  EXPECT_EQ(0x1.fep127L, kBFloat16.maximumFiniteMagnitude());
  EXPECT_EQ(kMaximum, roundToBFloat16(std::nextafter(
                          kThreshold, static_cast<long double>(kMaximum))));
  EXPECT_EQ(std::numeric_limits<float>::infinity(),
            roundToBFloat16(kThreshold));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            roundToBFloat16(-kThreshold));

  EXPECT_TRUE(kBFloat16.conversionDoesNotOverflow(
      std::nextafter(kThreshold, static_cast<long double>(kMaximum))));
  EXPECT_FALSE(kBFloat16.conversionDoesNotOverflow(kThreshold));
}

TEST(FloatingPointFormat, BFloat16FiniteBoundaries) {
  for (std::uint16_t bits = 0; bits < 0x7f7f; ++bits) {
    SCOPED_TRACE(bits);
    const auto lower = positiveBFloat16Value(bits);
    const auto upper = positiveBFloat16Value(bits + 1);
    const auto midpoint =
        (static_cast<long double>(lower) + static_cast<long double>(upper)) / 2;
    const auto expectedMidpoint = (bits & 1) == 0 ? lower : upper;

    EXPECT_EQ(lower, roundToBFloat16(std::nextafter(
                         midpoint, static_cast<long double>(lower))));
    EXPECT_EQ(expectedMidpoint, roundToBFloat16(midpoint));
    EXPECT_EQ(upper, roundToBFloat16(std::nextafter(
                         midpoint, static_cast<long double>(upper))));
    EXPECT_EQ(-expectedMidpoint, roundToBFloat16(-midpoint));
  }
}

TEST(FloatingPointFormat, Float8E4M3FNSpecialValues) {
  EXPECT_EQ(448.0L, kFloat8E4M3FN.maximumFiniteMagnitude());
  EXPECT_EQ(0x00, float8E4M3FNBits(0.0L));
  EXPECT_EQ(0x80, float8E4M3FNBits(-0.0L));
  EXPECT_EQ(0x7e,
            float8E4M3FNBits(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(0xfe,
            float8E4M3FNBits(-std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(0x7f,
            float8E4M3FNBits(std::numeric_limits<long double>::quiet_NaN()));

  EXPECT_EQ(0.0f, roundE4M3FN(0.0L));
  EXPECT_FALSE(std::signbit(roundE4M3FN(0.0L)));
  EXPECT_EQ(0.0f, roundE4M3FN(-0.0L));
  EXPECT_TRUE(std::signbit(roundE4M3FN(-0.0L)));

  EXPECT_EQ(448.0f, roundE4M3FN(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(-448.0f,
            roundE4M3FN(-std::numeric_limits<long double>::infinity()));
  EXPECT_TRUE(
      std::isnan(roundE4M3FN(std::numeric_limits<long double>::quiet_NaN())));

  EXPECT_TRUE(kFloat8E4M3FN.conversionDoesNotOverflow(448.0L));
  EXPECT_FALSE(kFloat8E4M3FN.conversionDoesNotOverflow(449.0L));
  EXPECT_FALSE(kFloat8E4M3FN.conversionDoesNotOverflow(
      std::numeric_limits<long double>::infinity()));
  EXPECT_TRUE(kFloat8E4M3FN.conversionDoesNotOverflow(
      std::numeric_limits<long double>::quiet_NaN()));
  EXPECT_TRUE(kFloat8E4M3FN.representsInteger(448));
  EXPECT_TRUE(kFloat8E4M3FN.representsInteger(-448));
  EXPECT_FALSE(kFloat8E4M3FN.representsInteger(480));
  EXPECT_FALSE(kFloat8E4M3FN.representsInteger(-480));
}

TEST(FloatingPointFormat, Float8E4M3FNValues) {
  EXPECT_EQ(1.0f, roundE4M3FN(1.0625L));
  EXPECT_EQ(1.25f, roundE4M3FN(1.1875L));
  EXPECT_EQ(1.125f, roundE4M3FN(0x1.1000000000001p0L));
  EXPECT_EQ(-1.25f, roundE4M3FN(-1.1875L));

  EXPECT_EQ(0.0f, roundE4M3FN(0x1p-10L));
  EXPECT_EQ(0x1p-9f, roundE4M3FN(0x1.0000000000001p-10L));
  EXPECT_EQ(0x1p-8f, roundE4M3FN(0x1.8p-9L));
  EXPECT_EQ(0x1p-6f, roundE4M3FN(0x1.fp-7L));

  EXPECT_EQ(448.0f, roundE4M3FN(449.0L));
  EXPECT_EQ(-448.0f, roundE4M3FN(-449.0L));
}

TEST(FloatingPointFormat, Float8E4M3FNFiniteBoundaries) {
  for (std::uint8_t bits = 0; bits < 0x7e; ++bits) {
    SCOPED_TRACE(static_cast<unsigned>(bits));
    const auto lower = positiveFloat8E4M3FNValue(bits);
    const auto upper = positiveFloat8E4M3FNValue(bits + 1);
    const auto midpoint =
        (static_cast<long double>(lower) + static_cast<long double>(upper)) / 2;
    const auto expectedMidpoint = (bits & 1) == 0 ? lower : upper;

    EXPECT_EQ(lower, roundE4M3FN(std::nextafter(
                         midpoint, static_cast<long double>(lower))));
    EXPECT_EQ(expectedMidpoint, roundE4M3FN(midpoint));
    EXPECT_EQ(upper, roundE4M3FN(std::nextafter(
                         midpoint, static_cast<long double>(upper))));
    EXPECT_EQ(-expectedMidpoint, roundE4M3FN(-midpoint));
  }
}

TEST(FloatingPointFormat, Float8E5M2SpecialValues) {
  EXPECT_EQ(0x00, float8E5M2Bits(0.0L));
  EXPECT_EQ(0x80, float8E5M2Bits(-0.0L));
  EXPECT_EQ(0x7c, float8E5M2Bits(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(0xfc,
            float8E5M2Bits(-std::numeric_limits<long double>::infinity()));

  EXPECT_EQ(0.0f, roundE5M2(0.0L));
  EXPECT_FALSE(std::signbit(roundE5M2(0.0L)));
  EXPECT_EQ(0.0f, roundE5M2(-0.0L));
  EXPECT_TRUE(std::signbit(roundE5M2(-0.0L)));

  EXPECT_EQ(std::numeric_limits<float>::infinity(),
            roundE5M2(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            roundE5M2(-std::numeric_limits<long double>::infinity()));
  EXPECT_TRUE(
      std::isnan(roundE5M2(std::numeric_limits<long double>::quiet_NaN())));
}

TEST(FloatingPointFormat, Float8E5M2Values) {
  EXPECT_EQ(57344.0L, kFloat8E5M2.maximumFiniteMagnitude());
  EXPECT_EQ(1.0f, roundE5M2(1.125L));
  EXPECT_EQ(1.5f, roundE5M2(1.375L));
  EXPECT_EQ(1.25f, roundE5M2(0x1.2000000000001p0L));
  EXPECT_EQ(-1.5f, roundE5M2(-1.375L));

  EXPECT_EQ(0.0f, roundE5M2(0x1p-17L));
  EXPECT_EQ(0x1p-16f, roundE5M2(0x1.0000000000001p-17L));
  EXPECT_EQ(0x1p-15f, roundE5M2(0x1.8p-16L));
  EXPECT_EQ(0x1p-14f, roundE5M2(0x1.cp-15L));

  EXPECT_EQ(57344.0f, roundE5M2(61439.0L));
  EXPECT_EQ(std::numeric_limits<float>::infinity(), roundE5M2(61440.0L));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(), roundE5M2(-61440.0L));

  EXPECT_TRUE(kFloat8E5M2.conversionDoesNotOverflow(61439.0L));
  EXPECT_FALSE(kFloat8E5M2.conversionDoesNotOverflow(61440.0L));
  EXPECT_TRUE(kFloat8E5M2.conversionDoesNotOverflow(
      std::numeric_limits<long double>::infinity()));
}

TEST(FloatingPointFormat, WideIntegerRepresentability) {
#if defined(__SIZEOF_INT128__)
  const auto exact = ConstInt::make(
      static_cast<ConstInt::Wide>(ConstInt::UWide{1} << 100), 128, false);
  const auto inexact = ConstInt::make(
      static_cast<ConstInt::Wide>((ConstInt::UWide{1} << 100) + 1), 128, false);
  ASSERT_TRUE(exact);
  ASSERT_TRUE(inexact);
  EXPECT_TRUE(kBinary32.representsInteger(*exact));
  EXPECT_TRUE(kBinary64.representsInteger(*exact));
  EXPECT_FALSE(kBinary32.representsInteger(*inexact));
  EXPECT_FALSE(kBinary64.representsInteger(*inexact));
#endif
}

TEST(FloatingPointFormat, Float8E5M2FiniteBoundaries) {
  for (std::uint8_t bits = 0; bits < 0x7b; ++bits) {
    SCOPED_TRACE(static_cast<unsigned>(bits));
    const auto lower = positiveFloat8E5M2Value(bits);
    const auto upper = positiveFloat8E5M2Value(bits + 1);
    const auto midpoint =
        (static_cast<long double>(lower) + static_cast<long double>(upper)) / 2;
    const auto expectedMidpoint = (bits & 1) == 0 ? lower : upper;

    EXPECT_EQ(lower, roundE5M2(std::nextafter(
                         midpoint, static_cast<long double>(lower))));
    EXPECT_EQ(expectedMidpoint, roundE5M2(midpoint));
    EXPECT_EQ(upper, roundE5M2(std::nextafter(
                         midpoint, static_cast<long double>(upper))));
    EXPECT_EQ(-expectedMidpoint, roundE5M2(-midpoint));
  }
}

TEST(ConstFloat, UsesTheSharedFormatRepresentations) {
  const struct {
    TypeKind typeKind;
    ConstFloat::Format format;
    const FloatingPointFormat* description;
  } cases[] = {
      {TypeKind::kFloat16, ConstFloat::Format::kFloat16, &kBinary16},
      {TypeKind::kBFloat16, ConstFloat::Format::kBFloat16, &kBFloat16},
      {TypeKind::kFloat8E4M3FN, ConstFloat::Format::kFloat8E4M3FN,
       &kFloat8E4M3FN},
      {TypeKind::kFloat8E5M2, ConstFloat::Format::kFloat8E5M2, &kFloat8E5M2},
      {TypeKind::kFloat, ConstFloat::Format::kFloat, &kBinary32},
      {TypeKind::kDouble, ConstFloat::Format::kDouble, &kBinary64},
  };

  for (const auto& [typeKind, format, description] : cases) {
    EXPECT_EQ(ConstFloat::formatFor(typeKind), format);
    for (const auto value : {0.0L, -0.0L, 1.1875L, -13.5L}) {
      SCOPED_TRACE(static_cast<unsigned>(format));
      EXPECT_EQ(ConstFloat::fromValue(format, value).bits(),
                description->representation(value));
    }
  }

  EXPECT_EQ(ConstFloat::formatFor(TypeKind::kHalf),
            ConstFloat::Format::kFloat16);
  EXPECT_FALSE(ConstFloat::formatFor(TypeKind::kLongDouble));
  EXPECT_FALSE(ConstFloat::formatFor(TypeKind::kFloat128));
}

TEST(ConstFloat, RejectsExcessRepresentationBits) {
  using Format = ConstFloat::Format;

  EXPECT_FALSE(ConstFloat::fromBits(Format::kFloat8E4M3FN, 0x100));
  EXPECT_FALSE(ConstFloat::fromBits(Format::kFloat8E5M2, 0x100));
  EXPECT_FALSE(ConstFloat::fromBits(Format::kFloat16, 0x10000));
  EXPECT_FALSE(ConstFloat::fromBits(Format::kBFloat16, 0x10000));
  EXPECT_FALSE(ConstFloat::fromBits(Format::kFloat, std::uint64_t{1} << 32));
  EXPECT_TRUE(ConstFloat::fromBits(Format::kDouble, ~std::uint64_t{0}));
}

TEST(ConstFloat, ConstructsPortableNanPayloads) {
  using Format = ConstFloat::Format;

  const auto expectBits = [](Format format, std::string_view tag,
                             std::uint64_t expected) {
    auto value = ConstFloat::fromNanPayload(format, tag);
    ASSERT_TRUE(value);
    EXPECT_EQ(value->bits(), expected);
    EXPECT_TRUE(value->isNaN());
  };

  struct Expected {
    Format format;
    std::uint64_t bits;
  };
  const Expected expected[] = {
      {Format::kFloat16, 0x7e01},    {Format::kBFloat16, 0x7fc1},
      {Format::kFloat8E4M3FN, 0x7f}, {Format::kFloat8E5M2, 0x7f},
      {Format::kFloat, 0x7fc00001},  {Format::kDouble, 0x7ff8000000000001},
  };
  for (const auto& [format, bits] : expected) {
    expectBits(format, "1", bits);
  }

  expectBits(Format::kFloat, "077", 0x7fc0003f);
  expectBits(Format::kFloat, "0x800001", 0x7fc00001);
  expectBits(Format::kFloat, "-1", 0x7fffffff);
  expectBits(Format::kDouble, "  +0x123", 0x7ff8000000000123);
  expectBits(Format::kDouble, "", 0x7ff8000000000000);

  EXPECT_FALSE(ConstFloat::fromNanPayload(Format::kFloat, "08"));
  EXPECT_FALSE(ConstFloat::fromNanPayload(Format::kFloat, "0x"));
  EXPECT_FALSE(ConstFloat::fromNanPayload(Format::kFloat, "1tail"));
}

TEST(FloatingPoint, ConstructsSupportedHostLongDoubleNan) {
  const auto representation = [](long double value) {
    return std::bit_cast<std::array<unsigned char, sizeof(long double)>>(value);
  };

  const auto first = longDoubleNanFromPayload("0x123");
  ASSERT_TRUE(first);
  EXPECT_TRUE(std::isnan(*first));

  const auto second = longDoubleNanFromPayload("0x124");
  ASSERT_TRUE(second);
  EXPECT_TRUE(std::isnan(*second));
  EXPECT_NE(representation(*first), representation(*second));

  const auto empty = longDoubleNanFromPayload("");
  ASSERT_TRUE(empty);
  EXPECT_TRUE(std::isnan(*empty));

  EXPECT_FALSE(longDoubleNanFromPayload("not-an-integer"));
  EXPECT_FALSE(longDoubleNanFromPayload("08"));
  EXPECT_FALSE(longDoubleNanFromPayload("0x"));
}

TEST(ConstFloat, PreservesWideRepresentations) {
  using Format = ConstFloat::Format;

  const auto single = ConstFloat::fromBits(Format::kFloat, 0x7f812345);
  ASSERT_TRUE(single);
  EXPECT_EQ(single->format(), Format::kFloat);
  EXPECT_EQ(single->bitWidth(), 32u);
  EXPECT_TRUE(single->isNaN());
  EXPECT_TRUE(std::isnan(single->toLongDouble()));
  EXPECT_EQ(single->negated().bits(), 0xff812345);

  const auto wide =
      ConstFloat::fromBits(Format::kDouble, 0x7ff0000012345678ULL);
  ASSERT_TRUE(wide);
  EXPECT_EQ(wide->format(), Format::kDouble);
  EXPECT_EQ(wide->bitWidth(), 64u);
  EXPECT_TRUE(wide->isNaN());
  EXPECT_TRUE(std::isnan(wide->toLongDouble()));
  EXPECT_EQ(wide->negated().bits(), 0xfff0000012345678ULL);
}

TEST(ConstFloat, PreservesQuietNaNPayloadsThroughWidening) {
  using Format = ConstFloat::Format;
  struct Case {
    Format format;
    std::uint64_t firstBits;
    std::uint64_t secondBits;
  };
  constexpr Case cases[] = {
      {Format::kFloat8E4M3FN, 0x7f, 0x7f},
      {Format::kFloat8E5M2, 0x7e, 0x7f},
      {Format::kFloat16, 0x7e01, 0x7e02},
      {Format::kBFloat16, 0x7fc1, 0x7fc2},
      {Format::kFloat, 0x7fc00001, 0x7fc00002},
      {Format::kDouble, 0x7ff8000000000001ULL, 0x7ff8000000000002ULL},
  };

  for (const auto& [format, firstBits, secondBits] : cases) {
    SCOPED_TRACE(static_cast<unsigned>(format));
    const auto positiveFirst = ConstFloat::fromBits(format, firstBits);
    ASSERT_TRUE(positiveFirst);
    const auto sign = std::uint64_t{1} << (positiveFirst->bitWidth() - 1);
    for (auto signBits : {std::uint64_t{0}, sign}) {
      const auto signedFirstBits = firstBits | signBits;
      const auto signedSecondBits = secondBits | signBits;
      const auto first = ConstFloat::fromBits(format, signedFirstBits);
      const auto second = ConstFloat::fromBits(format, signedSecondBits);
      ASSERT_TRUE(first);
      ASSERT_TRUE(second);
      EXPECT_EQ(ConstFloat::fromValue(format, first->toLongDouble()).bits(),
                signedFirstBits);
      EXPECT_EQ(ConstFloat::fromValue(format, second->toLongDouble()).bits(),
                signedSecondBits);
    }
  }
}

TEST(ConstFloat, EveryFiniteNarrowRepresentationRoundTrips) {
  using Format = ConstFloat::Format;
  for (const auto format : {Format::kFloat16, Format::kBFloat16,
                            Format::kFloat8E4M3FN, Format::kFloat8E5M2}) {
    const auto zero = ConstFloat::fromBits(format, 0);
    ASSERT_TRUE(zero);
    const auto width = zero->bitWidth();
    for (std::uint32_t bits = 0; bits < (std::uint32_t{1} << width); ++bits) {
      SCOPED_TRACE(static_cast<unsigned>(format));
      SCOPED_TRACE(bits);
      const auto value = ConstFloat::fromBits(format, bits);
      ASSERT_TRUE(value);
      if (value->isNaN()) continue;
      EXPECT_EQ(ConstFloat::fromValue(format, value->toLongDouble()).bits(),
                bits);
    }
  }
}
