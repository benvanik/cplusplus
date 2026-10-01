// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

using Int4 = int __attribute__((ext_vector_type(4)));
using UInt4 = unsigned __attribute__((ext_vector_type(4)));
using SByte4 = signed char __attribute__((ext_vector_type(4)));
using UByte4 = unsigned char __attribute__((ext_vector_type(4)));
using Float4 = float __attribute__((ext_vector_type(4)));
using Bool4 = bool __attribute__((ext_vector_type(4)));

static_assert(__has_builtin(__builtin_convertvector));

template <typename To, typename From>
constexpr To convert(From value) {
  return __builtin_convertvector(value, To);
}

constexpr Float4 floating{1.75f, -2.75f, 257.5f, 0.0f};
constexpr auto integers = convert<Int4>(floating);
static_assert(integers[0] == 1 && integers[1] == -2);
static_assert(integers[2] == 257 && integers[3] == 0);

constexpr auto bytes = convert<UByte4>(integers);
static_assert(bytes[0] == 1 && bytes[1] == 254);
static_assert(bytes[2] == 1 && bytes[3] == 0);

constexpr SByte4 signedBytes{-1, -2, 126, 127};
constexpr auto signedIntegers = convert<Int4>(signedBytes);
static_assert(signedIntegers[0] == -1 && signedIntegers[1] == -2);
static_assert(signedIntegers[2] == 126 && signedIntegers[3] == 127);

constexpr auto booleans = convert<Bool4>(floating);
static_assert(booleans[0] && booleans[1] && booleans[2] && !booleans[3]);
constexpr auto booleanIntegers = convert<UInt4>(booleans);
static_assert(booleanIntegers[0] == 1 && booleanIntegers[3] == 0);

using UInt64x2 = unsigned long long __attribute__((ext_vector_type(2)));
using Double2 = double __attribute__((ext_vector_type(2)));
constexpr UInt64x2 large{0xffffffffffffffffULL, 0x8000000000000000ULL};
constexpr auto doubles = convert<Double2>(large);
static_assert(doubles[0] == 0x1p64 && doubles[1] == 0x1p63);

using UInt128x2 = unsigned __int128 __attribute__((ext_vector_type(2)));
constexpr UInt128x2 veryLarge{
    (static_cast<unsigned __int128>(1) << 100) + 1,
    static_cast<unsigned __int128>(1) << 127};
constexpr auto veryLargeDoubles = convert<Double2>(veryLarge);
static_assert(veryLargeDoubles[0] == 0x1p100);
static_assert(veryLargeDoubles[1] == 0x1p127);
