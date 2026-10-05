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
#include <cxx/types.h>

#include <bit>
#include <cmath>
#include <limits>
#include <utility>

namespace cxx {
namespace {

[[nodiscard]] auto floatingPointFormat(ConstFloat::Format format)
    -> FloatingPointFormat {
  switch (format) {
    case ConstFloat::Format::kFloat16:
      return {.exponentBits = 5, .significandDigits = 11};
    case ConstFloat::Format::kBFloat16:
      return {.exponentBits = 8, .significandDigits = 8};
    case ConstFloat::Format::kFloat8E4M3FN:
      return {
          .exponentBits = 4,
          .significandDigits = 4,
          .specialValueEncoding =
              FloatingPointFormat::SpecialValueEncoding::kFiniteOnly,
      };
    case ConstFloat::Format::kFloat8E5M2:
      return {.exponentBits = 5, .significandDigits = 3};
    case ConstFloat::Format::kFloat:
      return {.exponentBits = 8, .significandDigits = 24};
    case ConstFloat::Format::kDouble:
      return {.exponentBits = 11, .significandDigits = 53};
  }
  std::unreachable();
}

[[nodiscard]] auto isNaNRepresentation(ConstFloat::Format format,
                                       std::uint64_t bits) -> bool {
  const auto description = floatingPointFormat(format);
  const auto fractionBits = description.fractionBits();
  const auto fractionMask = (std::uint64_t{1} << fractionBits) - 1;
  const auto exponentMask = (std::uint64_t{1} << description.exponentBits) - 1;
  const auto exponent = (bits >> fractionBits) & exponentMask;
  const auto fraction = bits & fractionMask;
  if (description.specialValueEncoding ==
      FloatingPointFormat::SpecialValueEncoding::kFiniteOnly)
    return exponent == exponentMask && fraction == fractionMask;
  return exponent == exponentMask && fraction != 0;
}

[[nodiscard]] auto widenNaNToFloat(std::uint64_t bits,
                                   ConstFloat::Format sourceFormat)
    -> std::uint32_t {
  const auto source = floatingPointFormat(sourceFormat);
  const auto target = floatingPointFormat(ConstFloat::Format::kFloat);
  const auto sourceFractionBits = source.fractionBits();
  const auto targetFractionBits = target.fractionBits();
  const auto sourceFractionMask = (std::uint64_t{1} << sourceFractionBits) - 1;
  const auto targetFractionMask = (std::uint64_t{1} << targetFractionBits) - 1;
  const auto negative =
      (bits >> (source.exponentBits + sourceFractionBits)) & 1;
  const auto sign = negative << (target.exponentBits + targetFractionBits);
  const auto exponentMask = (std::uint64_t{1} << target.exponentBits) - 1;

  auto payload = bits & sourceFractionMask;
  if (targetFractionBits < sourceFractionBits)
    payload >>= sourceFractionBits - targetFractionBits;
  else
    payload <<= targetFractionBits - sourceFractionBits;
  if (targetFractionBits != sourceFractionBits || !payload)
    payload |= std::uint64_t{1} << (targetFractionBits - 1);
  return static_cast<std::uint32_t>(sign |
                                    (exponentMask << targetFractionBits) |
                                    (payload & targetFractionMask));
}

}  // namespace

auto ConstFloat::formatFor(TypeKind kind) -> std::optional<Format> {
  switch (kind) {
    case TypeKind::kHalf:
    case TypeKind::kFloat16:
      return Format::kFloat16;
    case TypeKind::kBFloat16:
      return Format::kBFloat16;
    case TypeKind::kFloat8E4M3FN:
      return Format::kFloat8E4M3FN;
    case TypeKind::kFloat8E5M2:
      return Format::kFloat8E5M2;
    case TypeKind::kFloat:
      return Format::kFloat;
    case TypeKind::kDouble:
      return Format::kDouble;
    default:
      return std::nullopt;
  }
}

auto ConstFloat::fromValue(Format format, long double value) -> ConstFloat {
  const auto description = floatingPointFormat(format);
  return ConstFloat(
      format, static_cast<std::uint64_t>(description.representation(value)));
}

auto ConstFloat::fromBits(Format format, std::uint64_t bits)
    -> std::optional<ConstFloat> {
  const auto description = floatingPointFormat(format);
  const auto width = 1 + description.exponentBits + description.fractionBits();
  if (width < 64 && bits >> width) return std::nullopt;
  return ConstFloat(format, bits);
}

auto ConstFloat::bitWidth() const -> unsigned {
  const auto description = floatingPointFormat(format_);
  return 1 + description.exponentBits + description.fractionBits();
}

auto ConstFloat::isNaN() const -> bool {
  return isNaNRepresentation(format_, bits_);
}

auto ConstFloat::toDouble() const -> double {
  if constexpr (sizeof(float) == sizeof(std::uint32_t) &&
                std::numeric_limits<float>::is_iec559) {
    if (format_ == Format::kFloat)
      return std::bit_cast<float>(static_cast<std::uint32_t>(bits_));
    if (format_ != Format::kDouble && isNaN()) {
      return static_cast<double>(
          std::bit_cast<float>(widenNaNToFloat(bits_, format_)));
    }
  }
  if constexpr (sizeof(double) == sizeof(std::uint64_t) &&
                std::numeric_limits<double>::is_iec559)
    if (format_ == Format::kDouble) return std::bit_cast<double>(bits_);
  return floatingPointFormat(format_).value(bits_);
}

auto ConstFloat::toLongDouble() const -> long double {
  return static_cast<long double>(toDouble());
}

auto ConstFloat::negated() const -> ConstFloat {
  return ConstFloat(format_, bits_ ^ (std::uint64_t{1} << (bitWidth() - 1)));
}

}  // namespace cxx
