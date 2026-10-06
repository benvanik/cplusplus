// RUN: %cxx -std=c23 -verify -fsyntax-only -xc %s

// expected-no-diagnostics

enum Kind { first, second, copy = second, next };
enum Wide { large = 1ULL << 40, following };
enum Byte : unsigned char { byte = 255 };

static_assert(first == 0 && second == 1 && next == 2);
static_assert(sizeof(enum Wide) == 8);
static_assert(following == (1ULL << 40) + 1);
static_assert(sizeof(enum Byte) == 1 && byte == 255);

enum __attribute__((packed)) PackedByte { packed_byte = 255 };
enum PackedSigned { packed_low = -128, packed_high = 127 }
    __attribute__((packed));

static_assert(sizeof(enum PackedByte) == 1);
static_assert(sizeof(enum PackedSigned) == 1);
static_assert(sizeof(+packed_byte) == sizeof(int));
static_assert(sizeof(+packed_low) == sizeof(int));
