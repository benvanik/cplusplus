// RUN: %cxx -verify -fsyntax-only %s

// expected-no-diagnostics

union NaturalBitfieldUnion {
  unsigned bits : 3;
};

static_assert(sizeof(NaturalBitfieldUnion) == 4);
static_assert(alignof(NaturalBitfieldUnion) == 4);

union __attribute__((packed)) PackedBitfieldUnion {
  unsigned bits : 3;
};

static_assert(sizeof(PackedBitfieldUnion) == 1);
static_assert(alignof(PackedBitfieldUnion) == 1);

union ExplicitlyAlignedBitfieldUnion {
  unsigned bits : 3 __attribute__((packed, aligned(2)));
};

static_assert(sizeof(ExplicitlyAlignedBitfieldUnion) == 2);
static_assert(alignof(ExplicitlyAlignedBitfieldUnion) == 2);

#pragma pack(push, 1)
union PragmaPackedBitfieldUnion {
  unsigned bits : 3;
};
#pragma pack(pop)

static_assert(sizeof(PragmaPackedBitfieldUnion) == 1);
static_assert(alignof(PragmaPackedBitfieldUnion) == 1);

struct PackedAlignedBitfield {
  char tag;
  unsigned bits : 3 __attribute__((packed, aligned(2)));
  char tail;
};

static_assert(sizeof(PackedAlignedBitfield) == 4);
static_assert(alignof(PackedAlignedBitfield) == 2);
static_assert(__builtin_offsetof(PackedAlignedBitfield, tail) == 3);

struct AlignedBitfield {
  char tag;
  unsigned bits : 3 __attribute__((aligned(16)));
  char tail;
};

static_assert(sizeof(AlignedBitfield) == 32);
static_assert(alignof(AlignedBitfield) == 16);
static_assert(__builtin_offsetof(AlignedBitfield, tail) == 17);

#pragma pack(push, 1)
struct PragmaCappedAlignedBitfield {
  char tag;
  unsigned bits : 3 __attribute__((aligned(16)));
  char tail;
};
#pragma pack(pop)

static_assert(sizeof(PragmaCappedAlignedBitfield) == 3);
static_assert(alignof(PragmaCappedAlignedBitfield) == 1);
static_assert(__builtin_offsetof(PragmaCappedAlignedBitfield, tail) == 2);

struct AlignedBitfieldRun {
  unsigned first : 3;
  unsigned second : 3 __attribute__((aligned(2)));
  char tail;
};

static_assert(sizeof(AlignedBitfieldRun) == 4);
static_assert(alignof(AlignedBitfieldRun) == 4);
static_assert(__builtin_offsetof(AlignedBitfieldRun, tail) == 3);

template <unsigned Alignment>
struct DependentAlignedBitfield {
  char tag;
  unsigned bits : 3 __attribute__((packed, aligned(Alignment)));
  char tail;
};

static_assert(sizeof(DependentAlignedBitfield<8>) == 16);
static_assert(alignof(DependentAlignedBitfield<8>) == 8);
static_assert(__builtin_offsetof(DependentAlignedBitfield<8>, tail) == 9);
