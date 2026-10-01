// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

using UShort2 = unsigned short __attribute__((ext_vector_type(2)));

constexpr auto halves = __builtin_bit_cast(UShort2, 0x12345678u);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
static_assert(halves[0] == 0x5678u);
static_assert(halves[1] == 0x1234u);
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
static_assert(halves[0] == 0x1234u);
static_assert(halves[1] == 0x5678u);
#else
#error unsupported byte order
#endif
static_assert(__builtin_bit_cast(unsigned, halves) == 0x12345678u);

constexpr auto negativeZero = __builtin_bit_cast(unsigned, -0.0f);
static_assert(negativeZero == 0x80000000u);

constexpr auto signalingNaN = __builtin_bit_cast(float, 0x7f812345u);
static_assert(__builtin_bit_cast(unsigned, signalingNaN) == 0x7f812345u);
static_assert(__builtin_bit_cast(unsigned, +signalingNaN) == 0x7f812345u);
static_assert(__builtin_bit_cast(unsigned, -signalingNaN) == 0xff812345u);

constexpr auto halfPayload =
    __builtin_bit_cast(_Float16, static_cast<unsigned short>(0x7c01));
static_assert(__builtin_bit_cast(unsigned short, halfPayload) == 0x7c01);

constexpr auto bfloatPayload =
    __builtin_bit_cast(__bf16, static_cast<unsigned short>(0x7f81));
static_assert(__builtin_bit_cast(unsigned short, bfloatPayload) == 0x7f81);

constexpr auto e4m3fnPayload =
    __builtin_bit_cast(__float8_e4m3fn, static_cast<unsigned char>(0xff));
static_assert(__builtin_bit_cast(unsigned char, e4m3fnPayload) == 0xff);

constexpr auto e5m2Payload =
    __builtin_bit_cast(__float8_e5m2, static_cast<unsigned char>(0x7d));
static_assert(__builtin_bit_cast(unsigned char, e5m2Payload) == 0x7d);

constexpr auto doublePayload =
    __builtin_bit_cast(double, 0x7ff0000012345678ULL);
static_assert(__builtin_bit_cast(unsigned long long, doublePayload) ==
              0x7ff0000012345678ULL);

#if defined(__SIZEOF_INT128__)
using ULong2 = unsigned long long __attribute__((ext_vector_type(2)));

constexpr unsigned __int128 wide =
    (static_cast<unsigned __int128>(0x0123456789abcdefULL) << 64) |
    0xfedcba9876543210ULL;
constexpr auto words = __builtin_bit_cast(ULong2, wide);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
static_assert(words[0] == 0xfedcba9876543210ULL);
static_assert(words[1] == 0x0123456789abcdefULL);
#else
static_assert(words[0] == 0x0123456789abcdefULL);
static_assert(words[1] == 0xfedcba9876543210ULL);
#endif
static_assert(__builtin_bit_cast(unsigned __int128, words) == wide);
#endif
