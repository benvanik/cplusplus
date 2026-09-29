// RUN: %cxx -verify -fsyntax-only -fvalidate-ast %s
// RUN: %cxx -toolchain macos -verify -fsyntax-only -fvalidate-ast %s

enum Empty {};
enum Small { zero, one };

enum UnsignedStep {
  unsigned_last = 0xffffffffu,
  unsigned_next,
  unsigned_step_is_wide = sizeof(decltype(unsigned_next)) == 8,
};

enum SignedStep {
  signed_last = 0x7fffffffffffffffLL,
  signed_next,
  signed_step_is_unsigned = __is_unsigned(decltype(signed_next)),
};

enum NegativeStep {
  negative_first = -2,
  negative_second,
  negative_third,
};

enum TemporaryTypes {
  initial = 0u,
  initial_is_unsigned = __is_same(decltype(initial), unsigned),
  following = initial - 1u,
};

enum WideTemporaryTypes {
  wide_initial = 1ULL << 40,
  wide_following = wide_initial + 1,
  wide_initial_is_unsigned_long_long =
      __is_same(decltype(wide_initial), unsigned long long),
};

enum Source : unsigned long { source = 1 };
enum FromUnscopedEnum {
  copied = source,
  copied_is_unsigned_long = __is_same(decltype(copied), unsigned long),
};

enum FixedTemporaryTypes : unsigned char {
  fixed_initial = 1,
  fixed_initial_is_unsigned_char =
      __is_same(decltype(fixed_initial), unsigned char),
};

enum WideRange { wide = 1ULL << 40, wide_after };
enum MixedRange { mixed_low = -1, mixed_high = 0xffffffffu };
enum FullRange { full = 0xffffffffffffffffULL };
enum NegativeWideRange { negative_wide = -(1LL << 40) };

enum class Byte : unsigned char { first = 254, final };

static_assert(__is_same(__underlying_type(Empty), int));
static_assert(__is_same(__underlying_type(Small), unsigned int));

#if __SIZEOF_LONG__ == 8
static_assert(__is_same(__underlying_type(WideRange), unsigned long));
static_assert(__is_same(__underlying_type(MixedRange), long));
static_assert(__is_same(__underlying_type(FullRange), unsigned long));
static_assert(__is_same(__underlying_type(NegativeWideRange), long));
#else
static_assert(__is_same(__underlying_type(WideRange), unsigned long long));
static_assert(__is_same(__underlying_type(MixedRange), long long));
static_assert(__is_same(__underlying_type(FullRange), unsigned long long));
static_assert(__is_same(__underlying_type(NegativeWideRange), long long));
#endif

static_assert(unsigned_next == 0x100000000ULL);
static_assert(unsigned_step_is_wide);
static_assert(signed_next == 0x8000000000000000ULL);
static_assert(signed_step_is_unsigned);
static_assert(negative_second == -1 && negative_third == 0);
static_assert(initial_is_unsigned && following == 0xffffffffu);
static_assert(wide_initial_is_unsigned_long_long);
static_assert(wide_following == (1ULL << 40) + 1);
static_assert(copied_is_unsigned_long);
static_assert(fixed_initial_is_unsigned_char);
static_assert(__is_same(decltype(initial), TemporaryTypes));
static_assert(__is_same(decltype(wide_initial), WideTemporaryTypes));
static_assert(__is_same(decltype(fixed_initial), FixedTemporaryTypes));
static_assert(unsigned(Byte::final) == 255);

template <unsigned long long Value>
constexpr auto inferred_value() {
  enum Kind { first = Value, next, last = next + 1 };
  return last;
}

template <class T, T Value>
constexpr T fixed_value() {
  enum class Kind : T { first = Value, next };
  return T(Kind::next);
}

static_assert(inferred_value<1>() == 3);
static_assert(inferred_value<(1ULL << 40)>() == (1ULL << 40) + 2);
static_assert(fixed_value<unsigned char, 254>() == 255);
static_assert(fixed_value<long long, -2>() == -1);
