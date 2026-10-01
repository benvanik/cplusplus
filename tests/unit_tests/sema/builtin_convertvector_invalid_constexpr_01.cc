// RUN: %cxx -verify -fsyntax-only %s

using Float4 = float __attribute__((ext_vector_type(4)));
using Int4 = int __attribute__((ext_vector_type(4)));

constexpr Float4 nonFinite{__builtin_inf(), 0.0f, 1.0f, 2.0f};
// expected-error@1 {{constexpr variable must be initialized}}
constexpr Int4 convertedNonFinite =
    __builtin_convertvector(nonFinite, Int4);

constexpr Float4 outOfRange{0x1p32f, 0.0f, 1.0f, 2.0f};
// expected-error@1 {{constexpr variable must be initialized}}
constexpr Int4 convertedOutOfRange =
    __builtin_convertvector(outOfRange, Int4);
