// RUN: %cxx --target=x86_64-unknown-linux-gnu -verify -fsyntax-only %s
// RUN: %cxx --target=wasm32-unknown-wasi -verify -fsyntax-only %s
// RUN: %cxx --target=x86_64-pc-windows-msvc -verify -fsyntax-only %s

static_assert(__has_unique_object_representations(bool));
static_assert(__has_unique_object_representations(const int));
static_assert(__has_unique_object_representations(unsigned));
static_assert(!__has_unique_object_representations(float));
static_assert(!__has_unique_object_representations(void));
static_assert(__has_unique_object_representations(int*));
static_assert(!__has_unique_object_representations(int&));
static_assert(!__has_unique_object_representations(void()));
static_assert(__has_unique_object_representations(int[3]));
static_assert(__has_unique_object_representations(int[]));
static_assert(!__has_unique_object_representations(float[3]));
static_assert(__has_unique_object_representations(_BitInt(8)));
static_assert(!__has_unique_object_representations(_BitInt(7)));

enum PlainEnum { kPlain };
enum class ScopedEnum : unsigned char { kScoped };

static_assert(__has_unique_object_representations(PlainEnum));
static_assert(__has_unique_object_representations(ScopedEnum));

struct NoPadding {
  int value;
};

struct InternalPadding {
  char first;
  int second;
};

struct TrailingPadding {
  int first;
  char second;
};

struct Empty {};
struct OtherEmpty {};

struct EmptyBase : Empty {
  int value;
};

struct TwoEmptyBases : Empty, OtherEmpty {
  int value;
};

struct EmptyMember {
  [[no_unique_address]] Empty empty;
  int value;
};

struct NestedPadding {
  TrailingPadding value;
  char tail;
};

struct ReusableTailPadding {
  ReusableTailPadding();
  int first;
  char second;
};

struct ReusedTailPadding {
  [[no_unique_address]] ReusableTailPadding value;
  char tail0;
  char tail1;
  char tail2;
};

struct ReferenceMember {
  int& value;
};

struct Dynamic {
  virtual void method();
};

struct NonTrivial {
  NonTrivial(const NonTrivial&) {}
  int value;
};

static_assert(__has_unique_object_representations(NoPadding));
static_assert(!__has_unique_object_representations(InternalPadding));
static_assert(!__has_unique_object_representations(TrailingPadding));
static_assert(!__has_unique_object_representations(Empty));
static_assert(__has_unique_object_representations(EmptyBase));

#if !defined(_WIN64)
static_assert(__has_unique_object_representations(TwoEmptyBases));
static_assert(__has_unique_object_representations(EmptyMember));
#endif

static_assert(!__has_unique_object_representations(NestedPadding));

#if !defined(_WIN64)
static_assert(__has_unique_object_representations(ReusedTailPadding));
#endif

static_assert(__has_unique_object_representations(ReferenceMember));
static_assert(!__has_unique_object_representations(Dynamic));
static_assert(!__has_unique_object_representations(NonTrivial));
static_assert(__has_unique_object_representations(NoPadding[2]));
static_assert(!__has_unique_object_representations(TrailingPadding[2]));

union SameSizeUnion {
  int signedValue;
  unsigned unsignedValue;
};

union DifferentSizeUnion {
  int word;
  char byte;
};

union NonUniqueUnion {
  int word;
  float floating;
};

union EmptyUnion {};

static_assert(__has_unique_object_representations(SameSizeUnion));
static_assert(!__has_unique_object_representations(DifferentSizeUnion));
static_assert(!__has_unique_object_representations(NonUniqueUnion));
static_assert(!__has_unique_object_representations(EmptyUnion));

#if !defined(_WIN32)
struct FullBitField {
  unsigned value : 32;
};

struct SplitBitFields {
  unsigned low : 1;
  unsigned high : 31;
};

struct PartialBitField {
  unsigned value : 31;
};

struct BitFieldGap {
  unsigned low : 1;
  unsigned : 1;
  unsigned high : 30;
};

static_assert(__has_unique_object_representations(FullBitField));
static_assert(__has_unique_object_representations(SplitBitFields));
static_assert(!__has_unique_object_representations(PartialBitField));
static_assert(!__has_unique_object_representations(BitFieldGap));
#endif

struct MemberBase {
  int field;
  void method();
};

struct SecondBase {
  int other;
};

struct MultipleBases : MemberBase, SecondBase {};
struct VirtualBase : virtual MemberBase {};
struct PolymorphicDerived : MemberBase {
  virtual void other();
};
struct Incomplete;

static_assert(__has_unique_object_representations(int MemberBase::*));
static_assert(
    __has_unique_object_representations(void (MemberBase::*)()));
static_assert(__has_unique_object_representations(int MultipleBases::*));

#if defined(_WIN64)
static_assert(
    !__has_unique_object_representations(void (MultipleBases::*)()));
#else
static_assert(
    __has_unique_object_representations(void (MultipleBases::*)()));
#endif

static_assert(__has_unique_object_representations(int VirtualBase::*));
static_assert(
    __has_unique_object_representations(void (VirtualBase::*)()));
static_assert(__has_unique_object_representations(int PolymorphicDerived::*));

#if defined(_WIN64)
static_assert(
    !__has_unique_object_representations(void (PolymorphicDerived::*)()));
#else
static_assert(
    __has_unique_object_representations(void (PolymorphicDerived::*)()));
#endif

static_assert(__has_unique_object_representations(int Incomplete::*));

#if defined(_WIN64)
static_assert(
    !__has_unique_object_representations(void (Incomplete::*)()));
#else
static_assert(
    __has_unique_object_representations(void (Incomplete::*)()));
#endif

template <typename T>
constexpr bool has_unique_representations =
    __has_unique_object_representations(T);

static_assert(has_unique_representations<NoPadding>);
static_assert(!has_unique_representations<TrailingPadding>);
