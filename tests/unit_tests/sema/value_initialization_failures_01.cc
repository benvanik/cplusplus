// RUN: %cxx -std=c++23 -fsyntax-only -verify %s

namespace constant_default {
struct Descriptor {
  Descriptor() = default;
  unsigned value = 9;
};

static_assert(Descriptor().value == 9);
}  // namespace constant_default

namespace nonconstant_default {
unsigned runtimeValue();

struct Descriptor {
  Descriptor() = default;
  unsigned value = runtimeValue();
};

// expected-error@+1 {{constexpr variable must be initialized}}
constexpr Descriptor invalid = Descriptor();
}  // namespace nonconstant_default

namespace invalid_arithmetic_default {
struct Descriptor {
  Descriptor() = default;
  unsigned divisor;
  // expected-warning@+1 {{invalid binary expression}}
  unsigned quotient = 32 / divisor;
};

// expected-error@+1 {{constexpr variable must be initialized}}
constexpr Descriptor invalid = Descriptor();
}  // namespace invalid_arithmetic_default
