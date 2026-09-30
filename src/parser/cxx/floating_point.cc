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

auto roundFloat16(long double value) -> float {
  constexpr int kPrecision = 11;
  constexpr int kMinimumQuantumExponent = -24;
  constexpr int kOverflowExponent = 16;

  if (!std::isfinite(value) || value == 0) return static_cast<float>(value);

  const auto magnitude = std::abs(value);
  int exponent = 0;
  std::frexp(magnitude, &exponent);
  if (exponent > kOverflowExponent)
    return std::copysign(std::numeric_limits<float>::infinity(), value);

  // Normal values have eleven significant bits. Subnormals share a fixed
  // quantum of 2^-24, including rounding across zero and the normal boundary.
  const auto quantum = std::max(exponent - kPrecision, kMinimumQuantumExponent);
  const auto scaled = std::ldexp(magnitude, -quantum);
  auto integral = std::floor(scaled);
  const auto remainder = scaled - integral;
  if (remainder > 0.5L || (remainder == 0.5L && std::fmod(integral, 2.0L) != 0))
    integral = std::ceil(scaled);

  auto rounded = std::ldexp(integral, quantum);
  if (rounded >= std::ldexp(1.0L, kOverflowExponent))
    rounded = std::numeric_limits<float>::infinity();
  return static_cast<float>(std::copysign(rounded, value));
}

}  // namespace cxx
