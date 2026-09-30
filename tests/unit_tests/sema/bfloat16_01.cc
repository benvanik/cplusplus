// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

static_assert(__is_floating_point(__bf16));
static_assert(__is_arithmetic(__bf16));
static_assert(__is_same(decltype(1.0bf16), __bf16));
static_assert(__is_same(decltype(1.0BF16 + 1), __bf16));
static_assert(__is_same(decltype(1.0bf16 + 1.0f), float));
static_assert(__is_same(decltype(__bf16{} + _Float16{}), _Float16));
static_assert(__is_same(decltype(_Float16{} + __bf16{}), _Float16));
static_assert(sizeof(__bf16) == 2);
static_assert(alignof(__bf16) == 2);
using BFloat4 = __bf16 __attribute__((ext_vector_type(4)));
static_assert(sizeof(BFloat4) == 8);

constexpr __bf16 even = 1.00390625;
constexpr __bf16 odd = 1.01171875;
static_assert(even == 1.0);
static_assert(odd == 1.015625);
static_assert((__bf16)0x1.0100000000001p0 == 1.0078125);
static_assert((__bf16)0x1p-134 == 0.0);
static_assert((__bf16)0x1.8p-133 == 0x1p-132);
static_assert((__bf16)0x1.fep-127 == 0x1p-126);
static_assert((__bf16)0x1.ffp127 == __builtin_huge_valf());

static_assert((__bf16)1.0 / (__bf16)3.0 == 0.333984375);
static_assert(1.01171875BF16 == 1.015625);
static_assert(0x1p-134BF16 == 0.0);
static_assert(0x1.ffp127BF16 == __builtin_huge_valf());
static_assert(+((__bf16)1.0 + (__bf16)0x1p-8) == 1.0);

static_assert((__bf16)511u == 512.0);
