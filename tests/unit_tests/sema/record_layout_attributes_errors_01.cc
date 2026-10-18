// RUN: %cxx -verify -fsyntax-only %s

struct LatePacked {
  char tag;
  int value;
};

// expected-warning@+1 {{'packed' attribute declaration must precede definition}}
struct __attribute__((packed)) LatePacked;

static_assert(sizeof(LatePacked) == 8);
static_assert(alignof(LatePacked) == 4);

struct LateAligned {
  char value;
};

// expected-warning@+1 {{'aligned' attribute declaration must precede definition}}
struct [[gnu::aligned(16)]] LateAligned;

static_assert(sizeof(LateAligned) == 1);
static_assert(alignof(LateAligned) == 1);

struct LateRepeated {
  int value;
};

// expected-warning@+2 {{'packed' attribute declaration must precede definition}}
// expected-warning@+1 {{'aligned' attribute declaration must precede definition}}
struct __attribute__((packed, aligned(16))) LateRepeated;

static_assert(sizeof(LateRepeated) == 4);
static_assert(alignof(LateRepeated) == 4);

// expected-error@+1 {{requested alignment exceeds the supported layout range}}
struct __attribute__((aligned(2147483648LL))) ExcessiveAlignment {
  char value;
};

// expected-error@+1 {{'packed' attribute takes no arguments}}
struct __attribute__((packed(1))) PackedArgument { int value; };

// expected-error@+1 {{'packed' attribute requires a record, enumeration, or non-static data member}}
typedef unsigned PackedAlias __attribute__((packed));

// expected-error@+1 {{'packed' attribute requires a record, enumeration, or non-static data member}}
using PackedUsing = unsigned __attribute__((packed));

// expected-error@+1 {{'aligned' attribute requires an integer constant}}
using FloatingAlignedUsing = unsigned __attribute__((aligned(8.0)));

// expected-error@+1 {{'packed' attribute requires a record, enumeration, or non-static data member}}
unsigned packedVariable __attribute__((packed));

// expected-error@+1 {{'packed' attribute requires a record, enumeration, or non-static data member}}
void packedParameter(unsigned value __attribute__((packed)));

void packedCatch() {
  try {
  }
  // expected-error@+1 {{'packed' attribute requires a record, enumeration, or non-static data member}}
  catch (__attribute__((packed)) unsigned value) {
  }
}

struct PackedMemberPlacement {
  // expected-error@+1 {{'packed' attribute requires a record, enumeration, or non-static data member}}
  [[gnu::packed]] static unsigned value;
};

typedef unsigned AlignedAlias __attribute__((aligned(16)));

// expected-error@+1 {{array element size 4 is not a multiple of its alignment 16}}
AlignedAlias invalidArray[2];

// expected-error@+1 {{'aligned' attribute takes at most one argument}}
struct [[gnu::aligned(8, 16)]] MultipleAlignments { char value; };

// expected-error@+1 {{'aligned' attribute requires an integer constant}}
struct [[gnu::aligned(8.0)]] FloatingAlignment { char value; };

// expected-error@+1 {{requested alignment is not a power of 2}}
struct [[gnu::aligned(0)]] ZeroAlignment { char value; };

// expected-error@+1 {{requested alignment is not a power of 2}}
struct [[gnu::aligned(-8)]] NegativeAlignment { char value; };

// expected-error@+1 {{requested alignment is not a power of 2}}
struct [[gnu::aligned(3)]] NonPowerAlignment { char value; };

// expected-error@+1 {{record layout exceeds the supported size range}}
struct OversizedMember {
  unsigned char values[1ull << 31];
};

// expected-error@+1 {{record layout exceeds the supported size range}}
struct [[gnu::aligned(1 << 30)]] OversizedTailPadding {
  unsigned char values[(1 << 30) + 1];
};

// expected-error@+1 {{record layout exceeds the supported size range}}
struct OversizedAlignedMember {
  unsigned char tag;
  unsigned value [[gnu::aligned(1 << 30)]];
};

// expected-error@+1 {{bit-field width exceeds the supported layout range}}
struct OversizedBitFieldWidth {
  unsigned value : 1ull << 32;
};

// expected-error@+1 {{record layout exceeds the supported size range}}
struct OversizedBitFieldSequence {
  unsigned first : 0xffffffffull;
  unsigned second : 0xffffffffull;
  unsigned third : 0xffffffffull;
  unsigned fourth : 0xffffffffull;
};
