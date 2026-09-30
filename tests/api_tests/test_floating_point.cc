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
