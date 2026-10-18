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

typedef unsigned Aligned16 __attribute__((aligned(16)));
typedef Aligned16 InheritedAlignment;
typedef Aligned16 ReducedAlignment __attribute__((aligned(2)));
typedef const Aligned16 ConstAligned16;
using UsingAlignment [[gnu::aligned(32)]] = unsigned;
using SuffixAlignment = unsigned __attribute__((aligned(32)));
typedef unsigned* AlignedPointer __attribute__((aligned(16)));
typedef unsigned& IgnoredReferenceAlignment __attribute__((aligned(16)));

static_assert(sizeof(Aligned16) == sizeof(unsigned));
static_assert(alignof(Aligned16) == 16);
static_assert(alignof(InheritedAlignment) == 16);
static_assert(alignof(ReducedAlignment) == 2);
static_assert(alignof(ConstAligned16) == 16);
static_assert(alignof(UsingAlignment) == 32);
static_assert(alignof(SuffixAlignment) == alignof(unsigned));
static_assert(alignof(AlignedPointer) == 16);
static_assert(alignof(IgnoredReferenceAlignment) == alignof(unsigned));
static_assert(__is_same(Aligned16, unsigned));
static_assert(__is_same(ReducedAlignment, unsigned));

struct TypedefAlignedMember {
  unsigned char tag;
  Aligned16 value;
  unsigned char tail;
};

static_assert(__builtin_offsetof(TypedefAlignedMember, value) == 16);
static_assert(__builtin_offsetof(TypedefAlignedMember, tail) == 20);
static_assert(sizeof(TypedefAlignedMember) == 32);
static_assert(alignof(TypedefAlignedMember) == 16);

struct [[gnu::packed]] PackedTypedefAlignedMember {
  unsigned char tag;
  Aligned16 value;
};

static_assert(__builtin_offsetof(PackedTypedefAlignedMember, value) == 1);
static_assert(sizeof(PackedTypedefAlignedMember) == 5);
static_assert(alignof(PackedTypedefAlignedMember) == 1);

typedef unsigned AlignedArray[2] __attribute__((aligned(8)));
static_assert(sizeof(AlignedArray) == 8);
static_assert(alignof(AlignedArray) == 8);

Aligned16 alignedSource = 0;
auto alignedAuto = alignedSource;
Aligned16* alignedAddress = &alignedSource;
static_assert(alignof(decltype(alignedAuto)) == 16);
static_assert(alignof(decltype(*alignedAddress)) == 16);

using RemovedConstAlignment = __remove_cv(ConstAligned16);
using DecayedAlignment = __decay(Aligned16);
static_assert(alignof(RemovedConstAlignment) == 16);
static_assert(alignof(DecayedAlignment) == 16);

template <typename T>
struct TemplateAlignment {
  static constexpr unsigned value = alignof(T);
};

static_assert(TemplateAlignment<Aligned16>::value == alignof(unsigned));
static_assert(__is_same(TemplateAlignment<Aligned16>,
                        TemplateAlignment<unsigned>));
static_assert(__is_same(TemplateAlignment<Aligned16*>,
                        TemplateAlignment<unsigned*>));

template <typename T>
constexpr unsigned deducedAlignment(T) {
  return alignof(T);
}

static_assert(deducedAlignment(Aligned16{}) == alignof(unsigned));

template <Aligned16 Value>
struct ConstantAlignment {
  static constexpr unsigned value = alignof(decltype(Value));
};

static_assert(ConstantAlignment<0>::value == alignof(unsigned));
