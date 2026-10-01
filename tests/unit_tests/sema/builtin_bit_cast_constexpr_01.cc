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

constexpr auto float8Payload =
    __builtin_bit_cast(__float8_e5m2, static_cast<unsigned char>(0x7d));
static_assert(__builtin_bit_cast(unsigned char, float8Payload) == 0x7d);
