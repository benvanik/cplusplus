// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

using Int4 = int __attribute__((ext_vector_type(4)));
using GnuInt4 = int __attribute__((vector_size(16)));

constexpr Int4 zero{};
constexpr Int4 first{7};
constexpr Int4 partial{7, 11};
constexpr Int4 full{1, 2, 3, 4};
constexpr Int4 splat = 9;
constexpr GnuInt4 gnu{5, 6, 7, 8};
constexpr Int4 converted = gnu;

static_assert(zero[3] == 0);
static_assert(first[0] == 7 && first[3] == 0);
static_assert(partial[1] == 11 && partial[2] == 0);
static_assert(full[0] == 1 && full[3] == 4);
static_assert(splat[0] == 9 && splat[3] == 9);
static_assert(converted[0] == 5 && converted[3] == 8);

using Half4 = _Float16 __attribute__((ext_vector_type(4)));
constexpr Half4 halves{(_Float16)1.00048828125, (_Float16)-0.0};
static_assert(halves[0] == 1.0f && halves[3] == 0.0f);

using Float8x4 = __float8_e4m3fn __attribute__((ext_vector_type(4)));
constexpr Float8x4 float8s{__float8_e4m3fn(0x1.1000000000001p0),
                           __float8_e4m3fn(-0.0)};
static_assert(float8s[0] == 1.125 && float8s[3] == 0.0);

constexpr int mutateParameterCopy(Int4 value) {
  value[0] = 8;
  return value[0];
}

constexpr int mutateLocalCopy() {
  auto copy = full;
  copy[0] = 9;
  return copy[0];
}

static_assert(mutateParameterCopy(full) == 8);
static_assert(full[0] == 1);
static_assert(mutateLocalCopy() == 9);
static_assert(full[0] == 1);
