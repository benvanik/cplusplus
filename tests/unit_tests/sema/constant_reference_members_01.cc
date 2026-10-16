// RUN: %cxx -verify -fsyntax-only %s

namespace constant_reference_members {
constexpr unsigned seed = 9;
constexpr unsigned values[3] = {2, 3, 5};

// The zero-initialization phase leaves reference members for their default
// member initializers to bind.
struct DefaultScalar {
  const unsigned& value = seed;
};
constexpr DefaultScalar defaultScalar = DefaultScalar();
static_assert(defaultScalar.value == 9);
static_assert(&defaultScalar.value == &seed);

struct ExplicitlyDefaulted {
  const unsigned& value = seed;
  constexpr ExplicitlyDefaulted() = default;
};
constexpr ExplicitlyDefaulted explicitlyDefaulted = ExplicitlyDefaulted();
static_assert(explicitlyDefaulted.value == 9);
static_assert(&explicitlyDefaulted.value == &seed);

struct RvalueReference {
  const unsigned&& value = static_cast<const unsigned&&>(seed);
};
constexpr RvalueReference rvalueReference = RvalueReference();
static_assert(rvalueReference.value == 9);
static_assert(&rvalueReference.value == &seed);

// A reference to an earlier member retains the completed object's identity.
struct SelfReference {
  unsigned value = 7;
  const unsigned& alias = value;
};
constexpr SelfReference self = SelfReference();
static_assert(self.alias == 7);
static_assert(&self.alias == &self.value);

// Copying a reference member copies its referent rather than rebinding it to
// the corresponding member in the destination.
constexpr SelfReference copied = self;
static_assert(copied.alias == 7);
static_assert(&copied.alias == &self.value);

// Nested and base subobjects apply the same two initialization phases.
struct Nested {
  SelfReference inner;
};
constexpr Nested nested = Nested();
static_assert(nested.inner.alias == 7);
static_assert(&nested.inner.alias == &nested.inner.value);

struct BaseReference {
  const unsigned& alias = seed;
};
struct DerivedReference : BaseReference {
  unsigned value = 11;
};
constexpr DerivedReference derived = DerivedReference();
static_assert(derived.alias == 9 && derived.value == 11);
static_assert(&derived.alias == &seed);

// A reference member can preserve an entire array referent.
struct ArrayReference {
  const unsigned (&array)[3] = values;
};
constexpr ArrayReference array = ArrayReference();
static_assert(array.array[0] == 2 && array.array[2] == 5);
static_assert(&array.array == &values);

// Explicit construction and mutable automatic aliases remain controls for the
// paths that do not require class zero-initialization.
struct ConstructorReference {
  const unsigned& value;
  constexpr ConstructorReference(const unsigned& source) : value(source) {}
};
constexpr ConstructorReference constructed(seed);
static_assert(constructed.value == 9);
static_assert(&constructed.value == &seed);

constexpr unsigned mutate() {
  unsigned local = 11;
  struct MutableReference {
    unsigned& value;
  };
  MutableReference reference(local);
  reference.value = 13;
  return local;
}
static_assert(mutate() == 13);

// Value initialization cannot manufacture a referent when no initializer
// exists; the source constructor remains deleted.
struct InvalidReference {
  const unsigned& value;
};
// expected-error@+2 {{call to implicitly-deleted default constructor of 'InvalidReference'}}
// expected-error@+1 {{constexpr variable must be initialized}}
constexpr InvalidReference invalid = InvalidReference();
}  // namespace constant_reference_members
