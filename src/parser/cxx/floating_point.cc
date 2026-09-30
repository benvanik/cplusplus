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

#include <algorithm>
#include <cmath>
#include <limits>

namespace cxx {

namespace {

auto roundFloatingPoint(long double value, int precision,
                        int minimumNormalExponent, int overflowExponent)
    -> float {
  if (!std::isfinite(value) || value == 0) return static_cast<float>(value);

  const auto magnitude = std::abs(value);
  int exponent = 0;
  std::frexp(magnitude, &exponent);
  if (exponent > overflowExponent)
    return std::copysign(std::numeric_limits<float>::infinity(), value);

  // Subnormals share the minimum normal spacing. Scaling by this exact power
  // of two preserves the rounding decision without an intermediate float
  // conversion, which could double-round a wider source.
  const auto quantum = std::max(exponent, minimumNormalExponent) - precision;
  const auto scaled = std::ldexp(magnitude, -quantum);
  auto integral = std::floor(scaled);
  const auto remainder = scaled - integral;
  if (remainder > 0.5L || (remainder == 0.5L && std::fmod(integral, 2.0L) != 0))
    integral = std::ceil(scaled);

  auto rounded = std::ldexp(integral, quantum);
  if (rounded >= std::ldexp(1.0L, overflowExponent))
    rounded = std::numeric_limits<float>::infinity();
  return static_cast<float>(std::copysign(rounded, value));
}

}  // namespace

auto roundFloat16(long double value) -> float {
  return roundFloatingPoint(value, 11, -13, 16);
}

auto roundBFloat16(long double value) -> float {
  return roundFloatingPoint(value, 8, -125, 128);
}

}  // namespace cxx
