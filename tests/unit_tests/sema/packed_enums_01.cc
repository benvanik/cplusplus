// RUN: %cxx -verify -fsyntax-only %s

// expected-no-diagnostics

namespace packed_enums {

enum __attribute__((packed)) Empty {};
enum [[gnu::packed]] Byte { byte = 255 };
enum SignedByte { low = -128, high = 127 } __attribute__((packed));
enum [[using gnu: __packed__]] Short { short_value = 65535 };
enum __attribute__((packed)) Word { word_value = 65536 };
enum __attribute__((packed)) Wide { wide_value = 1ull << 40 };
enum __attribute__((packed)) Full { full_value = 0xffffffffffffffffull };
enum __attribute__((packed)) Mixed {
  mixed_low = -1,
  mixed_high = 0xffffffffu
};

static_assert(sizeof(Empty) == 1);
static_assert(__is_same(__underlying_type(Empty), unsigned char));
static_assert(sizeof(Byte) == 1 && alignof(Byte) == 1);
static_assert(__is_same(__underlying_type(Byte), unsigned char));
static_assert(sizeof(SignedByte) == 1 && alignof(SignedByte) == 1);
static_assert(__is_same(__underlying_type(SignedByte), signed char));
static_assert(sizeof(Short) == 2 && alignof(Short) == 2);
static_assert(__is_same(__underlying_type(Short), unsigned short));
static_assert(sizeof(Word) == 4 && alignof(Word) == 4);
static_assert(__is_same(__underlying_type(Word), unsigned int));
static_assert(sizeof(Wide) == 8 && alignof(Wide) == 8);
static_assert(sizeof(Full) == 8 && alignof(Full) == 8);
static_assert(sizeof(Mixed) == 8 && alignof(Mixed) == 8);
#if __SIZEOF_LONG__ == 8
static_assert(__is_same(__underlying_type(Wide), unsigned long));
static_assert(__is_same(__underlying_type(Full), unsigned long));
static_assert(__is_same(__underlying_type(Mixed), long));
#else
static_assert(__is_same(__underlying_type(Wide), unsigned long long));
static_assert(__is_same(__underlying_type(Full), unsigned long long));
static_assert(__is_same(__underlying_type(Mixed), long long));
#endif

static_assert(__is_same(decltype(+byte), int));
static_assert(__is_same(decltype(+low), int));
static_assert(__is_same(decltype(+short_value), int));
static_assert(__is_same(decltype(+word_value), int));
#if __SIZEOF_LONG__ == 8
static_assert(__is_same(decltype(+wide_value), long));
static_assert(__is_same(decltype(+full_value), unsigned long));
static_assert(__is_same(decltype(+mixed_high), long));
#else
static_assert(__is_same(decltype(+wide_value), long long));
static_assert(__is_same(decltype(+full_value), unsigned long long));
static_assert(__is_same(decltype(+mixed_high), long long));
#endif

constexpr int choose(int) { return 1; }
constexpr int choose(signed char) { return 2; }
constexpr int choose(unsigned char) { return 3; }
constexpr int choose(unsigned short) { return 4; }
constexpr int choose(unsigned int) { return 5; }

static_assert(choose(byte) == 1);
static_assert(choose(low) == 1);
static_assert(choose(short_value) == 1);
static_assert(choose(word_value) == 1);

enum __attribute__((packed)) Fixed : unsigned char { fixed_byte = 255 };
static_assert(sizeof(Fixed) == 1);
static_assert(__is_same(__underlying_type(Fixed), unsigned char));
static_assert(choose(fixed_byte) == 3);

enum class __attribute__((packed)) Scoped { value = 255 };
static_assert(sizeof(Scoped) == 4);
static_assert(__is_same(__underlying_type(Scoped), int));

enum class __attribute__((packed)) ScopedWide : unsigned long long {
  value = 255
};
static_assert(sizeof(ScopedWide) == 8);
static_assert(__is_same(__underlying_type(ScopedWide), unsigned long long));

template <unsigned Value>
struct Container {
  enum Kind { value = Value } __attribute__((packed));
};

static_assert(sizeof(Container<255>::Kind) == 1);
static_assert(sizeof(Container<256>::Kind) == 2);
static_assert(sizeof(Container<65536>::Kind) == 4);
static_assert(choose(Container<255>::value) == 1);
static_assert(choose(Container<65536>::value) == 1);

}  // namespace packed_enums
