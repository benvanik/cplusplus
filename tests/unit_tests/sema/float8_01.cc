// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

static_assert(__is_floating_point(__float8_e4m3fn));
static_assert(__is_floating_point(__float8_e5m2));
static_assert(__is_arithmetic(__float8_e4m3fn));
static_assert(__is_arithmetic(__float8_e5m2));
static_assert(__is_signed(__float8_e4m3fn));
static_assert(__is_signed(__float8_e5m2));
static_assert(!__is_same(__float8_e4m3fn, __float8_e5m2));

static_assert(sizeof(__float8_e4m3fn) == 1);
static_assert(alignof(__float8_e4m3fn) == 1);
static_assert(sizeof(__float8_e5m2) == 1);
static_assert(alignof(__float8_e5m2) == 1);

using E4M3FN4 = __float8_e4m3fn __attribute__((ext_vector_type(4)));
using E5M2_4 = __float8_e5m2 __attribute__((ext_vector_type(4)));
static_assert(sizeof(E4M3FN4) == 4);
static_assert(alignof(E4M3FN4) == 4);
static_assert(sizeof(E5M2_4) == 4);
static_assert(alignof(E5M2_4) == 4);

static_assert(__is_same(decltype(__float8_e4m3fn{} + 1),
                        __float8_e4m3fn));
static_assert(__is_same(decltype(1 + __float8_e5m2{}), __float8_e5m2));
static_assert(__is_same(decltype(__float8_e4m3fn{} + __bf16{}), __bf16));
static_assert(__is_same(decltype(_Float16{} + __float8_e5m2{}), _Float16));
static_assert(__is_same(decltype(__float8_e4m3fn{} + 1.0f), float));

constexpr __float8_e4m3fn e4_even = 1.0625;
constexpr __float8_e4m3fn e4_odd = 1.1875;
static_assert(e4_even == 1.0);
static_assert(e4_odd == 1.25);
static_assert((__float8_e4m3fn)0x1.1000000000001p0 == 1.125);
static_assert((__float8_e4m3fn)0x1.2ffffffffffffp0 == 1.125);
static_assert((__float8_e4m3fn)0x1p-10 == 0.0);
static_assert((__float8_e4m3fn)0x1.8p-9 == 0x1p-8);
static_assert((__float8_e4m3fn)1000.0 == 448.0);
static_assert((__float8_e4m3fn)__builtin_huge_val() == 448.0);
static_assert(__float8_e4m3fn(1000) == 448.0);
static_assert((__float8_e4m3fn)448.0 + (__float8_e4m3fn)448.0 ==
              448.0);
static_assert(-(__float8_e4m3fn)1.25 == -1.25);
static_assert((__float8_e4m3fn)1.25 > (__float8_e4m3fn)1.0);

constexpr __float8_e5m2 e5_even = 1.125;
constexpr __float8_e5m2 e5_odd = 1.375;
static_assert(e5_even == 1.0);
static_assert(e5_odd == 1.5);
static_assert((__float8_e5m2)0x1.2000000000001p0 == 1.25);
static_assert((__float8_e5m2)0x1p-17 == 0.0);
static_assert((__float8_e5m2)61439.0 == 57344.0);
static_assert((__float8_e5m2)61440.0 == __builtin_huge_valf());

static_assert((__float8_e5m2)(__float8_e4m3fn)1.125 == 1.0);
static_assert((__float8_e4m3fn)(__float8_e5m2)512.0 == 448.0);
