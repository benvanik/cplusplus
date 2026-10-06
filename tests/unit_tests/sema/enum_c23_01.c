// RUN: %cxx -std=c23 -verify -fsyntax-only -xc %s
// RUN: %cxx -std=c23 -toolchain macos -verify -fsyntax-only -xc %s
// RUN: %cxx -std=c23 -toolchain windows -verify -fsyntax-only -xc %s

// expected-no-diagnostics

enum Basic {
  first,
  unsigned_zero = 0U,
  unsigned_zero_is_int = _Generic(unsigned_zero, int: 1, default: 0),
  following,
  following_is_int = _Generic(following, int: 1, default: 0),
};

static_assert(first == 0 && following == 2);
static_assert(unsigned_zero_is_int && following_is_int);
static_assert(_Generic(unsigned_zero, int: 1, default: 0));

enum SignedStep {
  signed_last = __INT_MAX__,
  signed_next,
  signed_next_is_wider_signed =
      _Generic(signed_next, long: 1, long long: 1, default: 0),
};

enum UnsignedStep {
  unsigned_last = __UINT32_MAX__,
  unsigned_next,
  unsigned_next_is_wider_unsigned =
      _Generic(unsigned_next, unsigned long: 1, unsigned long long: 1,
               default: 0),
};

static_assert(signed_next == 0x80000000ULL);
static_assert(signed_next_is_wider_signed);
static_assert(unsigned_next == 0x100000000ULL);
static_assert(unsigned_next_is_wider_unsigned);
static_assert(_Generic(+signed_next, unsigned int: 1, default: 0));
static_assert(_Generic(+unsigned_next, unsigned long: 1,
                       unsigned long long: 1, default: 0));

enum Wide {
  wide = 1ULL << 40,
  wide_initializer_is_ull =
      _Generic(wide, unsigned long long: 1, default: 0),
  wide_next = wide + 1,
};

enum Mixed { mixed_low = -1, mixed_high = __UINT32_MAX__ };

static_assert(wide_initializer_is_ull);
static_assert(wide_next == (1ULL << 40) + 1);
static_assert(sizeof(enum Wide) == 8);
static_assert(_Generic(+wide, unsigned long: 1, unsigned long long: 1,
                       default: 0));
static_assert(_Generic(+mixed_low, long: 1, long long: 1, default: 0));

enum Byte : unsigned char {
  byte = 255,
  byte_is_enum = _Generic(byte, enum Byte: 1, default: 0),
};

static_assert(byte_is_enum);
static_assert(sizeof(enum Byte) == 1);
static_assert(_Generic(+byte, int: 1, default: 0));

enum FromFixed {
  fixed_copy = byte,
  fixed_copy_is_int = _Generic(fixed_copy, int: 1, default: 0),
};

static_assert(fixed_copy_is_int);

enum __attribute__((packed)) PackedByte { packed_byte = 255 };
enum PackedSigned { packed_low = -128, packed_high = 127 }
    __attribute__((packed));

static_assert(sizeof(enum PackedByte) == 1);
static_assert(sizeof(enum PackedSigned) == 1);
static_assert(_Generic(packed_byte, int: 1, default: 0));
static_assert(_Generic(packed_low, int: 1, default: 0));
static_assert(_Generic(+packed_byte, int: 1, default: 0));
static_assert(_Generic(+packed_low, int: 1, default: 0));
