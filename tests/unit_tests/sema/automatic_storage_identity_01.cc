// RUN: %cxx -verify -fsyntax-only %s

#include <initializer_list>

namespace recursive_frames {
constexpr bool preserves_distinct_storage(unsigned depth,
                                          const unsigned* outerScalar,
                                          const char* outerArray) {
  unsigned scalar = depth + 7;
  char array[] = {static_cast<char>('a' + depth), 0};
  if (!depth) {
    return outerScalar && outerArray && *outerScalar == 8 &&
           *outerArray == 'b' && &scalar != outerScalar &&
           array != outerArray;
  }
  return preserves_distinct_storage(depth - 1, &scalar, array);
}
static_assert(preserves_distinct_storage(1, nullptr, nullptr));

constexpr unsigned preserves_reference_to_caller(unsigned depth,
                                                 const unsigned& outer) {
  unsigned value = depth + 7;
  if (!depth) return outer;
  return preserves_reference_to_caller(depth - 1, value);
}
static_assert(preserves_reference_to_caller(1, 0) == 8);
}  // namespace recursive_frames

namespace lexical_lifetimes {
constexpr unsigned outer_declaration_survives_inner_block() {
  unsigned value = 9;
  const unsigned* address = nullptr;
  { address = &value; }
  return *address;
}
static_assert(outer_declaration_survives_inner_block() == 9);

constexpr unsigned read_expired_scalar() {
  const unsigned* address = nullptr;
  {
    unsigned value = 9;
    address = &value;
  }
  return *address;
}

constexpr char read_expired_array() {
  const char* address = nullptr;
  {
    char values[] = {'a', 0};
    address = values;
  }
  return *address;
}

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(read_expired_scalar() == 9);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(read_expired_array() == 'a');
}  // namespace lexical_lifetimes

namespace loop_lifetimes {
constexpr unsigned for_initializer_lives_across_iterations() {
  const unsigned* address = nullptr;
  for (unsigned value = 9, index = 0; index < 2; ++index) {
    if (!index)
      address = &value;
    else
      return *address;
  }
  return 0;
}
static_assert(for_initializer_lives_across_iterations() == 9);

constexpr unsigned read_expired_for_condition() {
  const unsigned* address = nullptr;
  unsigned index = 0;
  for (; unsigned value = index < 2 ? index + 1 : 0; ++index) {
    if (!index)
      address = &value;
    else
      return *address;
  }
  return 0;
}

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(read_expired_for_condition() == 1);

constexpr unsigned range_reference_keeps_array_storage() {
  unsigned values[] = {7, 9};
  const unsigned* address = nullptr;
  unsigned index = 0;
  for (unsigned& value : values) {
    unsigned& alias = value;
    if (!index++)
      address = &alias;
    else
      return *address;
  }
  return 0;
}
static_assert(range_reference_keeps_array_storage() == 7);

constexpr unsigned read_expired_range_value() {
  unsigned values[] = {7, 9};
  const unsigned* address = nullptr;
  unsigned index = 0;
  for (unsigned value : values) {
    unsigned& alias = value;
    if (!index++)
      address = &alias;
    else
      return *address;
  }
  return 0;
}

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(read_expired_range_value() == 7);
}  // namespace loop_lifetimes

namespace reference_identity {
struct Value {
  constexpr Value& operator*() { return *this; }
  constexpr Value& operator=(unsigned newValue) {
    value = newValue;
    return *this;
  }

  unsigned value = 0;
};

constexpr bool operators_preserve_receiver_identity() {
  Value object;
  Value& dereferenced = *object;
  Value& assigned = (dereferenced = 7);
  return &dereferenced == &object && &assigned == &object &&
         object.value == 7;
}
static_assert(operators_preserve_receiver_identity());

constexpr unsigned& identity(unsigned& value) { return value; }
constexpr void add(unsigned& value) { value += 3; }

constexpr unsigned mutate_caller_storage() {
  unsigned value = 4;
  identity(value) += 5;
  add(value);
  return identity(value);
}
static_assert(mutate_caller_storage() == 12);

constexpr unsigned read_expired_reference() {
  const unsigned* address = nullptr;
  {
    unsigned value = 9;
    unsigned& alias = value;
    address = &alias;
  }
  return *address;
}

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(read_expired_reference() == 9);
}  // namespace reference_identity

namespace materialized_ranges {
constexpr unsigned initializer_list_lives_for_range_statement() {
  const unsigned* address = nullptr;
  unsigned index = 0;
  for (const unsigned& value : {7u, 9u}) {
    if (!index++)
      address = &value;
    else
      return *address;
  }
  return 0;
}
static_assert(initializer_list_lives_for_range_statement() == 7);
}  // namespace materialized_ranges

namespace escaped_storage {
constexpr const unsigned* parameter(unsigned value) { return &value; }

constexpr const unsigned& local() {
  unsigned value = 9;
  return value;
}

struct Aggregate {
  unsigned value;
};

constexpr const unsigned& subobject() {
  Aggregate object{9};
  return object.value;
}

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(*parameter(9) == 9);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(local() == 9);
// expected-error@+1 {{constexpr variable must be initialized}}
constexpr const unsigned& invalidSubobject = subobject();
}  // namespace escaped_storage
