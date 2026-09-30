// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

static_assert(__is_floating_point(_Float16));
static_assert(__is_arithmetic(_Float16));
static_assert(__is_same(decltype(1.0f16), _Float16));
static_assert(__is_same(decltype(1.0F16 + 1), _Float16));
static_assert(__is_same(decltype(1.0f16 + 1.0f), float));
using Half4 = _Float16 __attribute__((ext_vector_type(4)));
static_assert(sizeof(Half4) == 8);

constexpr _Float16 even = 1.00048828125;
constexpr _Float16 odd = 1.00146484375;
static_assert(even == 1.0);
static_assert(odd == 1.001953125);
static_assert((_Float16)0x1.0020000000001p0 == 1.0009765625);
static_assert((_Float16)0x1p-25 == 0.0);
static_assert((_Float16)0x1.8p-24 == 0x1p-23);
static_assert((_Float16)0x1.ffcp-15 == 0x1p-14);
static_assert((_Float16)65519.0 == 65504.0);
static_assert((_Float16)65520.0 == __builtin_huge_valf());

static_assert((_Float16)1.0 / (_Float16)3.0 == 0.333251953125);
static_assert(1.00146484375F16 == 1.001953125);
static_assert(0x1p-25F16 == 0.0);
static_assert(65520.0F16 == __builtin_huge_valf());
static_assert(+((_Float16)1.0 + (_Float16)0x1p-11) == 1.0);

static_assert((_Float16)4095u == 4096.0);
