// RUN: %cxx -verify -fsyntax-only %s

namespace constant_temporary_subobject_reference {
struct Direct {
  unsigned value = 7;
};

struct Nested {
  Direct inner;
  unsigned values[2] = {3, 5};
};

// Named objects and whole scalar or array temporaries remain readable.
constexpr Direct named{};
constexpr const unsigned& named_member = named.value;
constexpr const unsigned& scalar = 11u;
constexpr const unsigned (&array)[2] = {13, 17};
static_assert(named_member == 7);
static_assert(scalar == 11);
static_assert(array[1] == 17);

// An automatic reference reads its extended temporary during the same constant
// evaluation.
constexpr unsigned automatic_member() {
  const unsigned& value = Direct{}.value;
  return value;
}
static_assert(automatic_member() == 7);

// Persistent references retain these addresses, but the temporary subobjects
// are not readable by a later constant evaluation.
constexpr const unsigned& member = Direct{}.value;
constexpr const Direct& nested_record = Nested{}.inner;
constexpr const unsigned& nested_member = Nested{}.inner.value;
constexpr const unsigned& array_element = Nested{}.values[1];
constexpr const unsigned* escaped_pointer = &member;
static_assert(escaped_pointer == &member);

struct Static {
  inline static constexpr const unsigned& member = Direct{}.value;
};

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(member == 7);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(nested_record.value == 7);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(nested_member == 7);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(array_element == 5);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(*escaped_pointer == 7);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(Static::member == 7);
}  // namespace constant_temporary_subobject_reference
