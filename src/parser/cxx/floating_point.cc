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
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>

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

auto roundFloat8E4M3FN(long double value) -> float {
  if (std::isnan(value)) return static_cast<float>(value);
  if (std::abs(value) > 448.0L) return std::copysign(448.0f, value);
  return roundFloatingPoint(value, 4, -5, 9);
}

auto roundFloat8E5M2(long double value) -> float {
  return roundFloatingPoint(value, 3, -13, 16);
}

namespace {

struct FloatEncoding {
  // Total object representation width.
  unsigned width;

  // Number of explicit fraction bits.
  unsigned fraction;

  // Bias of the stored exponent.
  int bias;
};

auto encoding(ConstFloat::Format format) -> FloatEncoding {
  switch (format) {
    case ConstFloat::Format::kFloat16:
      return {16, 10, 15};
    case ConstFloat::Format::kBFloat16:
      return {16, 7, 127};
    case ConstFloat::Format::kFloat8E4M3FN:
      return {8, 3, 7};
    case ConstFloat::Format::kFloat8E5M2:
      return {8, 2, 15};
    case ConstFloat::Format::kFloat:
      return {32, 23, 127};
    case ConstFloat::Format::kDouble:
      return {64, 52, 1023};
  }
  std::unreachable();
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
  static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
  static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);

  switch (format) {
    case Format::kFloat:
      return ConstFloat(
          format, std::bit_cast<std::uint32_t>(static_cast<float>(value)));
    case Format::kDouble:
      return ConstFloat(
          format, std::bit_cast<std::uint64_t>(static_cast<double>(value)));
    case Format::kFloat16:
      value = roundFloat16(value);
      break;
    case Format::kBFloat16:
      value = roundBFloat16(value);
      break;
    case Format::kFloat8E4M3FN:
      value = roundFloat8E4M3FN(value);
      break;
    case Format::kFloat8E5M2:
      value = roundFloat8E5M2(value);
      break;
  }

  const auto shape = encoding(format);
  const auto sign = std::uint64_t{std::signbit(value)} << (shape.width - 1);

  if (std::isnan(value)) {
    const auto payload =
        format == Format::kFloat8E4M3FN
            ? 0x7fu
            : (1u << (shape.width - 1)) - (1u << (shape.fraction - 1));
    return ConstFloat(format, sign | payload);
  }

  if (std::isinf(value)) {
    const auto infinity = (1u << (shape.width - 1)) - (1u << shape.fraction);
    return ConstFloat(format, sign | infinity);
  }

  if (value == 0) return ConstFloat(format, sign);

  value = std::abs(value);
  int power = 0;
  std::frexp(value, &power);
  const int exponent = power + shape.bias - 1;

  if (exponent <= 0) {
    const auto fraction = static_cast<std::uint64_t>(
        std::ldexp(value, shape.bias - 1 + shape.fraction));
    return ConstFloat(format, sign | fraction);
  }

  const auto significand = static_cast<std::uint64_t>(
      std::ldexp(value, static_cast<int>(shape.fraction) - power + 1));
  return ConstFloat(
      format, sign | (static_cast<std::uint64_t>(exponent) << shape.fraction) |
                  (significand - (std::uint64_t{1} << shape.fraction)));
}

auto ConstFloat::fromNanPayload(Format format, std::string_view tag)
    -> std::optional<ConstFloat> {
  const auto shape = encoding(format);
  auto payload = parseNanPayload(tag, shape.fraction);
  if (!payload) return std::nullopt;

  // E4M3FN has one positive NaN representation and no infinity. Its fraction
  // does not carry a distinct quiet payload.
  if (format == Format::kFloat8E4M3FN) return ConstFloat(format, 0x7f);

  const auto exponentBits = shape.width - shape.fraction - 1;
  const auto exponent = ((std::uint64_t{1} << exponentBits) - 1)
                        << shape.fraction;
  const auto quiet = std::uint64_t{1} << (shape.fraction - 1);
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
  const auto width = encoding(format).width;
  if (width < 64 && bits >> width) return std::nullopt;
  return ConstFloat(format, bits);
}

auto ConstFloat::bitWidth() const -> unsigned {
  return encoding(format_).width;
}

auto ConstFloat::isNaN() const -> bool {
  const auto shape = encoding(format_);
  const auto magnitude = bits_ & ((std::uint64_t{1} << (shape.width - 1)) - 1);

  if (format_ == Format::kFloat8E4M3FN) return magnitude == 0x7f;

  const auto infinity = (std::uint64_t{1} << (shape.width - 1)) -
                        (std::uint64_t{1} << shape.fraction);
  return magnitude > infinity;
}

auto ConstFloat::toDouble() const -> double {
  if (format_ == Format::kFloat)
    return std::bit_cast<float>(static_cast<std::uint32_t>(bits_));
  if (format_ == Format::kDouble) return std::bit_cast<double>(bits_);
  return static_cast<double>(toLongDouble());
}

auto ConstFloat::toLongDouble() const -> long double {
  if (format_ == Format::kFloat)
    return std::bit_cast<float>(static_cast<std::uint32_t>(bits_));
  if (format_ == Format::kDouble) return std::bit_cast<double>(bits_);

  const auto shape = encoding(format_);
  const bool negative = bits_ & (std::uint64_t{1} << (shape.width - 1));
  const auto magnitude = bits_ & ((std::uint64_t{1} << (shape.width - 1)) - 1);
  const auto fraction = magnitude & ((std::uint64_t{1} << shape.fraction) - 1);
  const auto exponent = magnitude >> shape.fraction;

  long double value = 0;
  if (isNaN()) {
    value = std::numeric_limits<long double>::quiet_NaN();
  } else if (format_ != Format::kFloat8E4M3FN &&
             exponent == (1u << (shape.width - shape.fraction - 1)) - 1) {
    value = std::numeric_limits<long double>::infinity();
  } else if (exponent == 0) {
    value = std::ldexp(static_cast<long double>(fraction),
                       1 - shape.bias - static_cast<int>(shape.fraction));
  } else {
    const auto significand = (std::uint64_t{1} << shape.fraction) | fraction;
    value = std::ldexp(static_cast<long double>(significand),
                       static_cast<int>(exponent) - shape.bias -
                           static_cast<int>(shape.fraction));
  }

  return std::copysign(value, negative ? -1.0L : 1.0L);
}

auto ConstFloat::negated() const -> ConstFloat {
  return ConstFloat(format_, bits_ ^ (std::uint64_t{1} << (bitWidth() - 1)));
}

}  // namespace cxx
