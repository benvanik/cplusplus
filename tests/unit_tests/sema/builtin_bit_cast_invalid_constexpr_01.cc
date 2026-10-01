// RUN: %cxx -verify -fsyntax-only %s

// expected-error@+1 {{constexpr variable must be initialized}}
constexpr auto invalidBool =
    __builtin_bit_cast(bool, static_cast<unsigned char>(2));

using Byte3 = unsigned char __attribute__((ext_vector_type(3)));
// expected-error@+1 {{constexpr variable must be initialized}}
constexpr auto paddedVector = __builtin_bit_cast(unsigned, Byte3{1, 2, 3});

using Bool8 = bool __attribute__((ext_vector_type(8)));
// expected-error@+1 {{constexpr variable must be initialized}}
constexpr auto packedBoolVector =
    __builtin_bit_cast(unsigned char, Bool8{false});

// expected-error@+1 {{constexpr variable must be initialized}}
constexpr auto pointer = __builtin_bit_cast(int*, 0u);
