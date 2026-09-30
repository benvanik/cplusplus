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
