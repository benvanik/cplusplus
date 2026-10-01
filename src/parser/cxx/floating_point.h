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

#pragma once

#include <cxx/types_fwd.h>

#include <cstdint>
#include <optional>

namespace cxx {

// A typed floating constant retaining its target object representation.
// Numeric queries expose its arithmetic value while identity, copying, sign
// changes, archives, and code generation use the stored representation bits.
class ConstFloat {
 public:
  enum class Format : std::uint8_t {
    kFloat16,
    kBFloat16,
    kFloat8E4M3FN,
    kFloat8E5M2,
    kFloat,
    kDouble,
  };

  ConstFloat() = default;

  [[nodiscard]] static auto formatFor(TypeKind kind) -> std::optional<Format>;

  [[nodiscard]] static auto fromValue(Format format, long double value)
      -> ConstFloat;

  // Constructs a constant when bits contains exactly the selected format's
  // representation, with no set bits above that representation.
  [[nodiscard]] static auto fromBits(Format format, std::uint64_t bits)
      -> std::optional<ConstFloat>;

  [[nodiscard]] auto format() const -> Format { return format_; }

  [[nodiscard]] auto bits() const -> std::uint64_t { return bits_; }

  [[nodiscard]] auto bitWidth() const -> unsigned;

  [[nodiscard]] auto isNaN() const -> bool;

  [[nodiscard]] auto toDouble() const -> double;

  [[nodiscard]] auto toLongDouble() const -> long double;

  [[nodiscard]] auto negated() const -> ConstFloat;

  auto operator==(const ConstFloat&) const -> bool = default;

 private:
  ConstFloat(Format format, std::uint64_t bits)
      : bits_(bits), format_(format) {}

  // Complete target representation, including signed zero and NaN payloads.
  std::uint64_t bits_ = 0;

  // Floating format used to interpret bits_.
  Format format_ = Format::kFloat;
};

// Rounds to IEEE binary16 using round-to-nearest, ties-to-even. Every binary16
// value is represented exactly in the returned float; host half support is not
// required.
[[nodiscard]] auto roundFloat16(long double value) -> float;

// Rounds to IEEE bfloat16 using round-to-nearest, ties-to-even. Every
// bfloat16 value is represented exactly in the returned float.
[[nodiscard]] auto roundBFloat16(long double value) -> float;

// Rounds to E4M3FN using round-to-nearest, ties-to-even, with signed finite
// saturation. NaNs, gradual underflow, and signed zero are preserved.
[[nodiscard]] auto roundFloat8E4M3FN(long double value) -> float;

// Rounds to IEEE E5M2 using round-to-nearest, ties-to-even. Infinity, NaN,
// gradual underflow, and signed zero are preserved.
[[nodiscard]] auto roundFloat8E5M2(long double value) -> float;

}  // namespace cxx
