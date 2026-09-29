// RUN: %cxx -verify -fsyntax-only %s

constexpr int int_min = -2147483647 - 1;
constexpr long long long_long_min = -9223372036854775807LL - 1;
constexpr _BitInt(33) bitint_one = 1;
constexpr _BitInt(33) bitint_max = 4294967295LL;
constexpr _BitInt(33) bitint_min = -4294967295LL - 1;
constexpr unsigned _BitInt(33) unsigned_bitint_zero = 0;
constexpr unsigned _BitInt(33) unsigned_bitint_one = 1;
constexpr unsigned _BitInt(33) unsigned_bitint_max = 8589934591LL;

static_assert(2147483646 + 1 == 2147483647);
static_assert(int_min + 1 == -2147483647);
static_assert(int_min - -1 == -2147483647);
static_assert(1073741823 * 2 == 2147483646);
static_assert(-1073741824 * 2 == int_min);
static_assert(int_min * 1 == int_min);
static_assert(-(-2147483647) == 2147483647);

static_assert(9223372036854775806LL + 1LL == 9223372036854775807LL);
static_assert(long_long_min + 1LL == -9223372036854775807LL);
static_assert(long_long_min - -1LL == -9223372036854775807LL);
static_assert(4611686018427387903LL * 2LL == 9223372036854775806LL);
static_assert(-4611686018427387904LL * 2LL == long_long_min);
static_assert(long_long_min * 1LL == long_long_min);
static_assert(-(-9223372036854775807LL) == 9223372036854775807LL);

static_assert(bitint_max - bitint_one < bitint_max);
static_assert(bitint_min + bitint_one > bitint_min);
static_assert(bitint_max * bitint_one == bitint_max);

static_assert(0xffffffffU + 1U == 0U);
static_assert(0U - 1U == 0xffffffffU);
static_assert(0x80000000U * 2U == 0U);
static_assert(-0x80000000U == 0x80000000U);
static_assert(0xffffffffffffffffULL + 1ULL == 0ULL);
static_assert(unsigned_bitint_max + unsigned_bitint_one ==
              unsigned_bitint_zero);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(2147483647 + 1);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(int_min + -1);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(int_min - 1);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(2147483647 - -1);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(1073741824 * 2);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(-1073741824 * 3);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(int_min * -1);

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(-int_min);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(9223372036854775807LL + 1LL);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(long_long_min - 1LL);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(4611686018427387904LL * 2LL);

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(-long_long_min);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(bitint_max + bitint_one);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(bitint_min - bitint_one);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(bitint_max * (_BitInt(33))2);

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(-bitint_min);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(int_min / -1);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(int_min % -1);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(1 / 0);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(1 << 32);

// expected-warning@+2 {{invalid binary expression}}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(1 << -1);
