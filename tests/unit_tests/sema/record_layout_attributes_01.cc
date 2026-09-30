// RUN: %cxx -verify -fsyntax-only -fvalidate-ast %s

// expected-no-diagnostics

struct __attribute__((packed)) ForwardPacked;

struct ForwardPacked {
  unsigned char tag;
  unsigned value;
};

static_assert(sizeof(ForwardPacked) == 5);
static_assert(alignof(ForwardPacked) == 1);
static_assert(__builtin_offsetof(ForwardPacked, value) == 1);

struct __attribute__((aligned(16))) ForwardAligned;

struct ForwardAligned {
  char value;
};

static_assert(sizeof(ForwardAligned) == 16);
static_assert(alignof(ForwardAligned) == 16);

struct __attribute__((aligned(8))) RedeclaredAligned;
struct __attribute__((aligned(32))) RedeclaredAligned;

struct RedeclaredAligned {
  char value;
};

static_assert(sizeof(RedeclaredAligned) == 32);
static_assert(alignof(RedeclaredAligned) == 32);

struct __attribute__((aligned(8), aligned(32))) RepeatedAligned {
  char value;
};

static_assert(sizeof(RepeatedAligned) == 32);
static_assert(alignof(RepeatedAligned) == 32);

struct [[gnu::packed, gnu::aligned(8)]] CxxForwardAttributes;

struct CxxForwardAttributes {
  unsigned char tag;
  unsigned value;
};

static_assert(sizeof(CxxForwardAttributes) == 8);
static_assert(alignof(CxxForwardAttributes) == 8);
static_assert(__builtin_offsetof(CxxForwardAttributes, value) == 1);

struct TrailingAttributes {
  unsigned char tag;
  unsigned value;
} __attribute__((packed, aligned(16)));

static_assert(sizeof(TrailingAttributes) == 16);
static_assert(alignof(TrailingAttributes) == 16);
static_assert(__builtin_offsetof(TrailingAttributes, value) == 1);

template <typename T>
struct __attribute__((packed)) ForwardPackedTemplate;

template <typename T>
struct ForwardPackedTemplate {
  unsigned char tag;
  T value;
};

static_assert(sizeof(ForwardPackedTemplate<unsigned>) == 5);
static_assert(alignof(ForwardPackedTemplate<unsigned>) == 1);
static_assert(
    __builtin_offsetof(ForwardPackedTemplate<unsigned>, value) == 1);

template <typename T>
struct __attribute__((aligned(32))) ForwardAlignedTemplate;

template <typename T>
struct ForwardAlignedTemplate {
  T value;
};

static_assert(sizeof(ForwardAlignedTemplate<char>) == 32);
static_assert(alignof(ForwardAlignedTemplate<char>) == 32);

template <int Alignment>
struct [[gnu::packed, gnu::aligned(Alignment)]] DependentAttributes {
  unsigned char tag;
  unsigned value;
};

static_assert(sizeof(DependentAttributes<16>) == 16);
static_assert(alignof(DependentAttributes<16>) == 16);
static_assert(__builtin_offsetof(DependentAttributes<16>, value) == 1);
