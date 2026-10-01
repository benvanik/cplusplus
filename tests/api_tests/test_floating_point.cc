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
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

using namespace cxx;

namespace {

auto positiveFloat16Value(std::uint16_t bits) -> float {
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

TEST(FloatingPoint, RoundFloat16SpecialValues) {
  EXPECT_EQ(0.0f, roundFloat16(0.0L));
  EXPECT_FALSE(std::signbit(roundFloat16(0.0L)));
  EXPECT_EQ(0.0f, roundFloat16(-0.0L));
  EXPECT_TRUE(std::signbit(roundFloat16(-0.0L)));

  EXPECT_EQ(std::numeric_limits<float>::infinity(),
            roundFloat16(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            roundFloat16(-std::numeric_limits<long double>::infinity()));
  EXPECT_TRUE(
      std::isnan(roundFloat16(std::numeric_limits<long double>::quiet_NaN())));
}

TEST(FloatingPoint, RoundFloat16NormalValues) {
  EXPECT_EQ(1.0f, roundFloat16(1.00048828125L));
  EXPECT_EQ(1.001953125f, roundFloat16(1.00146484375L));
  EXPECT_EQ(1.0009765625f, roundFloat16(0x1.0020000000001p0L));
  EXPECT_EQ(-1.001953125f, roundFloat16(-1.00146484375L));
}

TEST(FloatingPoint, RoundFloat16SubnormalValues) {
  EXPECT_EQ(0.0f, roundFloat16(0x1p-25L));
  EXPECT_EQ(0x1p-24f, roundFloat16(0x1.0000000000001p-25L));
  EXPECT_EQ(0x1p-23f, roundFloat16(0x1.8p-24L));
  EXPECT_EQ(0x1p-14f, roundFloat16(0x1.ffcp-15L));
}

TEST(FloatingPoint, RoundFloat16Overflow) {
  EXPECT_EQ(65504.0f, roundFloat16(65519.0L));
  EXPECT_EQ(std::numeric_limits<float>::infinity(), roundFloat16(65520.0L));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(), roundFloat16(-65520.0L));
}

TEST(FloatingPoint, RoundFloat16FiniteBoundaries) {
  for (std::uint16_t bits = 0; bits < 0x7bff; ++bits) {
    SCOPED_TRACE(bits);
    const auto lower = positiveFloat16Value(bits);
    const auto upper = positiveFloat16Value(bits + 1);
    const auto midpoint =
        (static_cast<long double>(lower) + static_cast<long double>(upper)) / 2;
    const auto expectedMidpoint = (bits & 1) == 0 ? lower : upper;

    EXPECT_EQ(lower, roundFloat16(std::nextafter(
                         midpoint, static_cast<long double>(lower))));
    EXPECT_EQ(expectedMidpoint, roundFloat16(midpoint));
    EXPECT_EQ(upper, roundFloat16(std::nextafter(
                         midpoint, static_cast<long double>(upper))));
    EXPECT_EQ(-expectedMidpoint, roundFloat16(-midpoint));
  }
}

TEST(FloatingPoint, RoundBFloat16SpecialValues) {
  EXPECT_EQ(0.0f, roundBFloat16(0.0L));
  EXPECT_FALSE(std::signbit(roundBFloat16(0.0L)));
  EXPECT_EQ(0.0f, roundBFloat16(-0.0L));
  EXPECT_TRUE(std::signbit(roundBFloat16(-0.0L)));

  EXPECT_EQ(std::numeric_limits<float>::infinity(),
            roundBFloat16(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            roundBFloat16(-std::numeric_limits<long double>::infinity()));
  EXPECT_TRUE(
      std::isnan(roundBFloat16(std::numeric_limits<long double>::quiet_NaN())));
}

TEST(FloatingPoint, RoundBFloat16NormalValues) {
  EXPECT_EQ(1.0f, roundBFloat16(1.00390625L));
  EXPECT_EQ(1.015625f, roundBFloat16(1.01171875L));
  EXPECT_EQ(1.0078125f, roundBFloat16(0x1.0100000000001p0L));
  EXPECT_EQ(-1.015625f, roundBFloat16(-1.01171875L));
}

TEST(FloatingPoint, RoundBFloat16SubnormalValues) {
  EXPECT_EQ(0.0f, roundBFloat16(0x1p-134L));
  EXPECT_EQ(0x1p-133f, roundBFloat16(0x1.0000000000001p-134L));
  EXPECT_EQ(0x1p-132f, roundBFloat16(0x1.8p-133L));
  EXPECT_EQ(0x1p-126f, roundBFloat16(0x1.fep-127L));
}

TEST(FloatingPoint, RoundBFloat16Overflow) {
  constexpr auto kMaximum = 0x1.fep127f;
  constexpr auto kThreshold = 0x1.ffp127L;
  EXPECT_EQ(kMaximum, roundBFloat16(std::nextafter(
                          kThreshold, static_cast<long double>(kMaximum))));
  EXPECT_EQ(std::numeric_limits<float>::infinity(), roundBFloat16(kThreshold));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            roundBFloat16(-kThreshold));
}

TEST(FloatingPoint, RoundBFloat16FiniteBoundaries) {
  for (std::uint16_t bits = 0; bits < 0x7f7f; ++bits) {
    SCOPED_TRACE(bits);
    const auto lower = positiveBFloat16Value(bits);
    const auto upper = positiveBFloat16Value(bits + 1);
    const auto midpoint =
        (static_cast<long double>(lower) + static_cast<long double>(upper)) / 2;
    const auto expectedMidpoint = (bits & 1) == 0 ? lower : upper;

    EXPECT_EQ(lower, roundBFloat16(std::nextafter(
                         midpoint, static_cast<long double>(lower))));
    EXPECT_EQ(expectedMidpoint, roundBFloat16(midpoint));
    EXPECT_EQ(upper, roundBFloat16(std::nextafter(
                         midpoint, static_cast<long double>(upper))));
    EXPECT_EQ(-expectedMidpoint, roundBFloat16(-midpoint));
  }
}

TEST(FloatingPoint, RoundFloat8E4M3FNSpecialValues) {
  EXPECT_EQ(0.0f, roundFloat8E4M3FN(0.0L));
  EXPECT_FALSE(std::signbit(roundFloat8E4M3FN(0.0L)));
  EXPECT_EQ(0.0f, roundFloat8E4M3FN(-0.0L));
  EXPECT_TRUE(std::signbit(roundFloat8E4M3FN(-0.0L)));

  EXPECT_EQ(448.0f,
            roundFloat8E4M3FN(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(-448.0f,
            roundFloat8E4M3FN(-std::numeric_limits<long double>::infinity()));
  EXPECT_TRUE(std::isnan(
      roundFloat8E4M3FN(std::numeric_limits<long double>::quiet_NaN())));
}

TEST(FloatingPoint, RoundFloat8E4M3FNValues) {
  EXPECT_EQ(1.0f, roundFloat8E4M3FN(1.0625L));
  EXPECT_EQ(1.25f, roundFloat8E4M3FN(1.1875L));
  EXPECT_EQ(1.125f, roundFloat8E4M3FN(0x1.1000000000001p0L));
  EXPECT_EQ(-1.25f, roundFloat8E4M3FN(-1.1875L));

  EXPECT_EQ(0.0f, roundFloat8E4M3FN(0x1p-10L));
  EXPECT_EQ(0x1p-9f, roundFloat8E4M3FN(0x1.0000000000001p-10L));
  EXPECT_EQ(0x1p-8f, roundFloat8E4M3FN(0x1.8p-9L));
  EXPECT_EQ(0x1p-6f, roundFloat8E4M3FN(0x1.fp-7L));

  EXPECT_EQ(448.0f, roundFloat8E4M3FN(449.0L));
  EXPECT_EQ(-448.0f, roundFloat8E4M3FN(-449.0L));
}

TEST(FloatingPoint, RoundFloat8E4M3FNFiniteBoundaries) {
  for (std::uint8_t bits = 0; bits < 0x7e; ++bits) {
    SCOPED_TRACE(static_cast<unsigned>(bits));
    const auto lower = positiveFloat8E4M3FNValue(bits);
    const auto upper = positiveFloat8E4M3FNValue(bits + 1);
    const auto midpoint =
        (static_cast<long double>(lower) + static_cast<long double>(upper)) / 2;
    const auto expectedMidpoint = (bits & 1) == 0 ? lower : upper;

    EXPECT_EQ(lower, roundFloat8E4M3FN(std::nextafter(
                         midpoint, static_cast<long double>(lower))));
    EXPECT_EQ(expectedMidpoint, roundFloat8E4M3FN(midpoint));
    EXPECT_EQ(upper, roundFloat8E4M3FN(std::nextafter(
                         midpoint, static_cast<long double>(upper))));
    EXPECT_EQ(-expectedMidpoint, roundFloat8E4M3FN(-midpoint));
  }
}

TEST(FloatingPoint, RoundFloat8E5M2SpecialValues) {
  EXPECT_EQ(0.0f, roundFloat8E5M2(0.0L));
  EXPECT_FALSE(std::signbit(roundFloat8E5M2(0.0L)));
  EXPECT_EQ(0.0f, roundFloat8E5M2(-0.0L));
  EXPECT_TRUE(std::signbit(roundFloat8E5M2(-0.0L)));

  EXPECT_EQ(std::numeric_limits<float>::infinity(),
            roundFloat8E5M2(std::numeric_limits<long double>::infinity()));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            roundFloat8E5M2(-std::numeric_limits<long double>::infinity()));
  EXPECT_TRUE(std::isnan(
      roundFloat8E5M2(std::numeric_limits<long double>::quiet_NaN())));
}

TEST(FloatingPoint, RoundFloat8E5M2Values) {
  EXPECT_EQ(1.0f, roundFloat8E5M2(1.125L));
  EXPECT_EQ(1.5f, roundFloat8E5M2(1.375L));
  EXPECT_EQ(1.25f, roundFloat8E5M2(0x1.2000000000001p0L));
  EXPECT_EQ(-1.5f, roundFloat8E5M2(-1.375L));

  EXPECT_EQ(0.0f, roundFloat8E5M2(0x1p-17L));
  EXPECT_EQ(0x1p-16f, roundFloat8E5M2(0x1.0000000000001p-17L));
  EXPECT_EQ(0x1p-15f, roundFloat8E5M2(0x1.8p-16L));
  EXPECT_EQ(0x1p-14f, roundFloat8E5M2(0x1.cp-15L));

  EXPECT_EQ(57344.0f, roundFloat8E5M2(61439.0L));
  EXPECT_EQ(std::numeric_limits<float>::infinity(), roundFloat8E5M2(61440.0L));
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            roundFloat8E5M2(-61440.0L));
}

TEST(FloatingPoint, RoundFloat8E5M2FiniteBoundaries) {
  for (std::uint8_t bits = 0; bits < 0x7b; ++bits) {
    SCOPED_TRACE(static_cast<unsigned>(bits));
    const auto lower = positiveFloat8E5M2Value(bits);
    const auto upper = positiveFloat8E5M2Value(bits + 1);
    const auto midpoint =
        (static_cast<long double>(lower) + static_cast<long double>(upper)) / 2;
    const auto expectedMidpoint = (bits & 1) == 0 ? lower : upper;

    EXPECT_EQ(lower, roundFloat8E5M2(std::nextafter(
                         midpoint, static_cast<long double>(lower))));
    EXPECT_EQ(expectedMidpoint, roundFloat8E5M2(midpoint));
    EXPECT_EQ(upper, roundFloat8E5M2(std::nextafter(
                         midpoint, static_cast<long double>(upper))));
    EXPECT_EQ(-expectedMidpoint, roundFloat8E5M2(-midpoint));
  }
}

TEST(FloatingPoint, ConstFloatRejectsExcessRepresentationBits) {
  using Format = ConstFloat::Format;

  EXPECT_FALSE(ConstFloat::fromBits(Format::kFloat8E4M3FN, 0x100));
  EXPECT_FALSE(ConstFloat::fromBits(Format::kFloat8E5M2, 0x100));
  EXPECT_FALSE(ConstFloat::fromBits(Format::kFloat16, 0x10000));
  EXPECT_FALSE(ConstFloat::fromBits(Format::kBFloat16, 0x10000));
  EXPECT_FALSE(ConstFloat::fromBits(Format::kFloat, std::uint64_t{1} << 32));
  EXPECT_TRUE(ConstFloat::fromBits(Format::kDouble, ~std::uint64_t{0}));
}

TEST(FloatingPoint, ConstFloatPreservesWideRepresentations) {
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

TEST(FloatingPoint, EveryFiniteNarrowRepresentationRoundTrips) {
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
