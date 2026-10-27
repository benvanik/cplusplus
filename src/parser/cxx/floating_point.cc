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

#include <array>
#include <bit>
#include <limits>
#include <string_view>
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

[[nodiscard]] auto nanPayloadDigit(char ch) -> int {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

[[nodiscard]] auto isAsciiSpace(char ch) -> bool {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' ||
         ch == '\v';
}

struct NanPayload {
  std::uint64_t low = 0;
  std::uint64_t high = 0;
};

void truncateNanPayload(NanPayload& payload, unsigned width) {
  if (width <= 64) {
    payload.high = 0;
    if (width < 64) payload.low &= (std::uint64_t{1} << width) - 1;
  } else if (width < 128) {
    payload.high &= (std::uint64_t{1} << (width - 64)) - 1;
  }
}

// Multiplies the unsigned 128-bit payload by a base no larger than 16, adds a
// digit, and discards overflow above the selected significand width.
void appendNanPayloadDigit(NanPayload& payload, unsigned base, unsigned digit,
                           unsigned width) {
  const auto lowHalf = static_cast<std::uint32_t>(payload.low);
  const auto highHalf = static_cast<std::uint32_t>(payload.low >> 32);
  const auto lowProduct = static_cast<std::uint64_t>(lowHalf) * base + digit;
  const auto highProduct =
      static_cast<std::uint64_t>(highHalf) * base + (lowProduct >> 32);

  payload.low = (highProduct << 32) | static_cast<std::uint32_t>(lowProduct);
  payload.high = payload.high * base + (highProduct >> 32);
  truncateNanPayload(payload, width);
}

// Parses the integer-tag grammar used by __builtin_nan. Accumulating modulo
// the requested width implements the specified truncation without making the
// result depend on the host integer width or strtol overflow behavior.
[[nodiscard]] auto parseNanPayload(std::string_view tag, unsigned width)
    -> std::optional<NanPayload> {
  if (width == 0 || width > 128) return std::nullopt;
  if (tag.empty()) return NanPayload{};

  std::size_t position = 0;
  while (position < tag.size() && isAsciiSpace(tag[position])) ++position;

  bool negative = false;
  if (position < tag.size() && (tag[position] == '+' || tag[position] == '-')) {
    negative = tag[position] == '-';
    ++position;
  }
  if (position == tag.size()) return std::nullopt;

  unsigned base = 10;
  if (position + 2 <= tag.size() && tag[position] == '0' &&
      (tag[position + 1] == 'x' || tag[position + 1] == 'X')) {
    base = 16;
    position += 2;
  } else if (tag[position] == '0') {
    base = 8;
  }

  NanPayload payload;
  bool hasDigit = false;
  for (; position < tag.size(); ++position) {
    const int digit = nanPayloadDigit(tag[position]);
    if (digit < 0 || static_cast<unsigned>(digit) >= base) return std::nullopt;
    hasDigit = true;
    appendNanPayloadDigit(payload, base, static_cast<unsigned>(digit), width);
  }
  if (!hasDigit) return std::nullopt;
  if (negative) {
    payload.low = ~payload.low + 1;
    payload.high = ~payload.high + (payload.low == 0);
    truncateNanPayload(payload, width);
  }
  return payload;
}

void storeNativeWord(std::array<unsigned char, sizeof(long double)>& bytes,
                     std::size_t offset, std::uint64_t word) {
  if constexpr (std::endian::native == std::endian::little) {
    for (std::size_t i = 0; i < sizeof(word); ++i)
      bytes[offset + i] = static_cast<unsigned char>(word >> (i * 8));
  } else {
    for (std::size_t i = 0; i < sizeof(word); ++i)
      bytes[offset + i] =
          static_cast<unsigned char>(word >> ((sizeof(word) - i - 1) * 8));
  }
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

auto ConstFloat::fromNanPayload(Format format, std::string_view tag)
    -> std::optional<ConstFloat> {
  const auto description = floatingPointFormat(format);
  const auto fractionBits = description.fractionBits();
  auto payload = parseNanPayload(tag, fractionBits);
  if (!payload) return std::nullopt;

  // E4M3FN has one positive NaN representation and no infinity. Its fraction
  // does not carry a distinct quiet payload.
  if (description.specialValueEncoding ==
      FloatingPointFormat::SpecialValueEncoding::kFiniteOnly) {
    const auto exponent =
        ((std::uint64_t{1} << description.exponentBits) - 1) << fractionBits;
    const auto fraction = (std::uint64_t{1} << fractionBits) - 1;
    return ConstFloat(format, exponent | fraction);
  }

  const auto exponent =
      ((std::uint64_t{1} << description.exponentBits) - 1) << fractionBits;
  const auto quiet = std::uint64_t{1} << (fractionBits - 1);
  return ConstFloat(format, exponent | quiet | payload->low);
}

auto longDoubleNanFromPayload(std::string_view tag)
    -> std::optional<long double> {
  std::array<unsigned char, sizeof(long double)> bytes{};

  if constexpr (std::numeric_limits<long double>::digits == 53 &&
                std::numeric_limits<long double>::max_exponent == 1024 &&
                sizeof(long double) == 8) {
    auto payload = parseNanPayload(tag, 52);
    if (!payload) return std::nullopt;
    const auto bits = std::uint64_t{0x7ff8000000000000ULL} | payload->low;
    storeNativeWord(bytes, 0, bits);
  } else if constexpr (std::numeric_limits<long double>::digits == 64 &&
                       std::numeric_limits<long double>::max_exponent ==
                           16384 &&
                       sizeof(long double) >= 10 &&
                       std::endian::native == std::endian::little) {
    auto payload = parseNanPayload(tag, 63);
    if (!payload) return std::nullopt;
    const auto significand =
        std::uint64_t{0xc000000000000000ULL} | payload->low;
    storeNativeWord(bytes, 0, significand);
    bytes[8] = 0xff;
    bytes[9] = 0x7f;
  } else if constexpr (std::numeric_limits<long double>::digits == 113 &&
                       std::numeric_limits<long double>::max_exponent ==
                           16384 &&
                       sizeof(long double) == 16) {
    auto payload = parseNanPayload(tag, 112);
    if (!payload) return std::nullopt;
    const auto high = std::uint64_t{0x7fff800000000000ULL} | payload->high;
    if constexpr (std::endian::native == std::endian::little) {
      storeNativeWord(bytes, 0, payload->low);
      storeNativeWord(bytes, 8, high);
    } else {
      storeNativeWord(bytes, 0, high);
      storeNativeWord(bytes, 8, payload->low);
    }
  } else {
    return std::nullopt;
  }

  return std::bit_cast<long double>(bytes);
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
