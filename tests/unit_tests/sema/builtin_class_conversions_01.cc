// RUN: %cxx -verify -fsyntax-only %s

template <class T>
struct Scalar {
  T value;
  constexpr operator T() const { return value; }
};

constexpr Scalar<unsigned> width{5};
constexpr Scalar<unsigned> shift{3};
static_assert(width * 3u == 15u);
static_assert(3u * width == 15u);
static_assert(width * shift == 15u);
static_assert(width / 2u == 2u);
static_assert(20u / width == 4u);
static_assert(width % 3u == 2u);
static_assert(13u % width == 3u);
static_assert((width << 1u) == 10u);
static_assert((1u << shift) == 8u);
static_assert((width >> 1u) == 2u);
static_assert((64u >> shift) == 8u);
static_assert((width & 3u) == 1u);
static_assert((3u & width) == 1u);
static_assert((width ^ 3u) == 6u);
static_assert((3u ^ width) == 6u);
static_assert((width | 2u) == 7u);
static_assert((2u | width) == 7u);

constexpr Scalar<float> scale{0.5f};
static_assert(scale * 3.0f == 1.5f);
static_assert(3.0f / scale == 6.0f);
static_assert(__is_same(decltype(scale * 3.0f), float));
constexpr Scalar<unsigned char> small{3};
static_assert(__is_same(decltype(small << 1), int));
static_assert(__is_same(decltype(1u << small), unsigned));
static_assert(__is_same(decltype(small & small), int));

template <class T>
constexpr unsigned mix(T value) {
  return (value * 3u) ^ (1u << value);
}
static_assert(mix(shift) == 1u);

struct WithPointer {
  constexpr operator unsigned() const { return 5; }
  operator int*() const;
};
constexpr WithPointer withPointer{};
static_assert(withPointer * 3u == 15u);
static_assert((withPointer & 3u) == 1u);
static_assert((1u << withPointer) == 32u);

struct IntegralOrFloat {
  constexpr operator unsigned() const { return 5; }
  operator float() const;
};
constexpr IntegralOrFloat integralOrFloat{};
void integral_condition(IntegralOrFloat value) {
  switch (value) {
    default:
      break;
  }
}
void arithmetic_conversion_ambiguity(IntegralOrFloat value) {
  // expected-error@+1 {{invalid operands to binary expression ('::IntegralOrFloat' and 'unsigned int')}}
  value % 3u;
}

struct Overloaded {
  constexpr operator unsigned() const { return 5; }
  constexpr unsigned operator*(unsigned) const { return 17; }
  constexpr unsigned operator<<(unsigned) const { return 23; }
  constexpr unsigned operator&(unsigned) const { return 42; }
};
constexpr Overloaded overloaded{};
static_assert(overloaded * 3u == 17u);
static_assert((overloaded << 1u) == 23u);
static_assert((overloaded & 3u) == 42u);

struct Explicit {
  explicit operator unsigned() const;
};
void explicit_conversion(Explicit value) {
  // expected-error@+1 {{invalid operands to binary expression ('::Explicit' and 'unsigned int')}}
  value * 3u;
}
struct Ambiguous {
  operator unsigned() const;
  operator int() const;
};
void ambiguous_conversion(Ambiguous value) {
  // expected-error@+1 {{invalid operands to binary expression ('int' and '::Ambiguous')}}
  1 << value;
}

struct Deleted {
  operator unsigned() const = delete;
};
void deleted_conversion(Deleted value) {
  // expected-error@+1 {{use of deleted function 'operator unsigned int'}}
  1u << value;
  // expected-error@+1 {{use of deleted function 'operator unsigned int'}}
  value + 1u;
}
struct DeletedCondition {
  operator unsigned() const = delete;
};
void deleted_condition(DeletedCondition value) {
  // expected-error@+1 {{use of deleted function 'operator unsigned int'}}
  switch (value) {
    default:
      break;
  }
}
struct NonConst {
  operator unsigned();
};
void const_object(const NonConst value) {
  // expected-error@+1 {{invalid operands to binary expression ('const ::NonConst' and 'unsigned int')}}
  value % 3u;
}
