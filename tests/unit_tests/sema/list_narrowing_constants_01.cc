// RUN: %cxx -verify -fsyntax-only %s

namespace integer_constants {

_Float16 half_exact{4096};
// expected-error@1 {{narrowing conversion from 'int' to '_Float16' in braced-init-list: integer constant 4095 is not exactly representable by '_Float16'}}
_Float16 half_inexact{4095};
// expected-error@1 {{narrowing conversion from 'int' to '_Float16' in braced-init-list: integer constant -4095 is not exactly representable by '_Float16'}}
_Float16 half_negative_inexact{-4095};

__bf16 bfloat_exact{512};
// expected-error@1 {{narrowing conversion from 'int' to '__bf16' in braced-init-list: integer constant 511 is not exactly representable by '__bf16'}}
__bf16 bfloat_inexact{511};

__float8_e4m3fn e4m3fn_exact{448};
// expected-error@1 {{narrowing conversion from 'int' to '__float8_e4m3fn' in braced-init-list: integer constant 300 is not exactly representable by '__float8_e4m3fn'}}
__float8_e4m3fn e4m3fn_inexact{300};

__float8_e5m2 e5m2_exact{57344};
// expected-error@1 {{narrowing conversion from 'int' to '__float8_e5m2' in braced-init-list: integer constant 57345 is not exactly representable by '__float8_e5m2'}}
__float8_e5m2 e5m2_inexact{57345};

float float_u64_exact{0x8000000000000000ULL};
// expected-error@1 {{narrowing conversion from 'unsigned long long' to 'float' in braced-init-list: integer constant 18446744073709551615 is not exactly representable by 'float'}}
float float_u64_inexact{0xffffffffffffffffULL};
// expected-error@1 {{narrowing conversion from 'unsigned long long' to 'double' in braced-init-list: integer constant 18446744073709551615 is not exactly representable by 'double'}}
double double_u64_inexact{0xffffffffffffffffULL};
// expected-error@1 {{narrowing conversion from 'unsigned long long' to 'long long' in braced-init-list: integer constant 18446744073709551615 is outside the range of 'long long'}}
long long signed_u64_inexact{0xffffffffffffffffULL};

constexpr unsigned long long large_u64 = 0xffffffffffffffffULL;
// expected-error@1 {{narrowing conversion from 'const unsigned long long' to 'float' in braced-init-list: integer constant 18446744073709551615 is not exactly representable by 'float'}}
float float_constexpr_u64_inexact{large_u64};

constexpr unsigned __int128 exact_u128 =
    static_cast<unsigned __int128>(1) << 100;
constexpr unsigned __int128 inexact_u128 = exact_u128 + 1;
float float_u128_exact{exact_u128};
double double_u128_exact{exact_u128};
// expected-error@1 {{narrowing conversion from 'const __uint128_t' to 'float' in braced-init-list: integer constant 1267650600228229401496703205377 is not exactly representable by 'float'}}
float float_u128_inexact{inexact_u128};
// expected-error@1 {{narrowing conversion from 'const __uint128_t' to 'double' in braced-init-list: integer constant 1267650600228229401496703205377 is not exactly representable by 'double'}}
double double_u128_inexact{inexact_u128};

}  // namespace integer_constants

namespace floating_constants {

_Float16 half_max{65504.0f};
_Float16 half_rounded_max{65519.0f};
// expected-error@1 {{narrowing conversion from 'float' to '_Float16' in braced-init-list: constant value 65520 rounds to infinity in '_Float16'; maximum finite value is 65504}}
_Float16 half_overflow{65520.0f};

__bf16 bfloat_max{0x1.fep127};
__bf16 bfloat_rounded_max{0x1.fefffffffffffp127};
// expected-error@1 {{narrowing conversion from 'double' to '__bf16' in braced-init-list: constant value 3.3961775292304600553e+38 rounds to infinity in '__bf16'; maximum finite value is 3.3895313892515354759e+38}}
__bf16 bfloat_overflow{0x1.ffp127};

__float8_e4m3fn e4m3fn_rounded{300.0f};
__float8_e4m3fn e4m3fn_max{448.0f};
// expected-error@1 {{narrowing conversion from 'float' to '__float8_e4m3fn' in braced-init-list: constant value 449 is outside the finite range of '__float8_e4m3fn'; maximum magnitude is 448}}
__float8_e4m3fn e4m3fn_overflow{449.0f};
// expected-error@1 {{narrowing conversion from 'float' to '__float8_e4m3fn' in braced-init-list: constant value -449 is outside the finite range of '__float8_e4m3fn'; maximum magnitude is 448}}
__float8_e4m3fn e4m3fn_negative_overflow{-449.0f};
// expected-error@1 {{narrowing conversion from 'float' to '__float8_e4m3fn' in braced-init-list: infinity is not representable by '__float8_e4m3fn'}}
__float8_e4m3fn e4m3fn_infinity{__builtin_inff()};
// expected-error@1 {{narrowing conversion from 'float' to '__float8_e4m3fn' in braced-init-list: negative infinity is not representable by '__float8_e4m3fn'}}
__float8_e4m3fn e4m3fn_negative_infinity{-__builtin_inff()};
__float8_e4m3fn e4m3fn_nan{__builtin_nanf("")};

__float8_e5m2 e5m2_max{57344.0f};
__float8_e5m2 e5m2_rounded_max{61439.0f};
// expected-error@1 {{narrowing conversion from 'float' to '__float8_e5m2' in braced-init-list: constant value 61440 rounds to infinity in '__float8_e5m2'; maximum finite value is 57344}}
__float8_e5m2 e5m2_overflow{61440.0f};
__float8_e5m2 e5m2_infinity{__builtin_inff()};

// The source remains finite in the target long-double model even though
// converting it to a host double produces infinity.
// expected-error@1 {{narrowing conversion from 'long double' to 'float' in braced-init-list: constant value 1.14813069527425452423e+602 rounds to infinity in 'float'; maximum finite value is 3.4028234663852885981e+38}}
float float_long_double_overflow{0x1p+2000L};
// expected-error@1 {{narrowing conversion from 'long double' to 'double' in braced-init-list: constant value 1.14813069527425452423e+602 rounds to infinity in 'double'; maximum finite value is 1.7976931348623157081e+308}}
double double_long_double_overflow{0x1p+2000L};

}  // namespace floating_constants
