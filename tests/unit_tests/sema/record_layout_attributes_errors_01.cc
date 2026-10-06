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
unsigned packedVariable __attribute__((packed));

// expected-error@+1 {{'packed' attribute requires a record, enumeration, or non-static data member}}
void packedParameter(unsigned value __attribute__((packed)));

struct PackedMemberPlacement {
  // expected-error@+1 {{'packed' attribute requires a record, enumeration, or non-static data member}}
  [[gnu::packed]] static unsigned value;
};

// expected-error@+1 {{'aligned' attribute requires a record, non-static data member, or variable}}
typedef unsigned AlignedAlias __attribute__((aligned(16)));

// GNU aligned without parentheses requests the target's preferred alignment.
struct [[gnu::aligned]] DefaultAligned { char value; };

// expected-error@+1 {{'aligned' attribute requires one argument}}
struct [[gnu::aligned()]] MissingAlignment { char value; };

// expected-error@+1 {{'aligned' attribute requires one argument}}
struct [[gnu::aligned(8, 16)]] MultipleAlignments { char value; };

// expected-error@+1 {{'aligned' attribute requires an integer constant}}
struct [[gnu::aligned(8.0)]] FloatingAlignment { char value; };

// expected-error@+1 {{requested alignment is not a positive power of 2}}
struct [[gnu::aligned(0)]] ZeroAlignment { char value; };

// expected-error@+1 {{requested alignment is not a positive power of 2}}
struct [[gnu::aligned(-8)]] NegativeAlignment { char value; };

// expected-error@+1 {{requested alignment is not a positive power of 2}}
struct [[gnu::aligned(3)]] NonPowerAlignment { char value; };
