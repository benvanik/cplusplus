// RUN: %cxx -std=c++23 -fsyntax-only -verify %s

extern int opaque;
constexpr int readable = 11;

constexpr bool has_address(const int& value, const int* expected) {
  return &value == expected;
}

static_assert(has_address(opaque, &opaque));

constexpr int rebind_to_opaque(const int& value, bool recurse) {
  if (recurse) return rebind_to_opaque(opaque, false);
  return value;
}

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(rebind_to_opaque(readable, true) == 11);

namespace readable_arrays {
constexpr int first(const int (&values)[2]) { return values[0]; }

constexpr int global_values[2] = {1, 2};
static_assert(first(global_values) == 1);

constexpr int read_local_array() {
  int values[2] = {3, 4};
  values[0] = 5;
  return first(values);
}
static_assert(read_local_array() == 5);

constexpr const int (&global_alias)[2] = global_values;
static_assert(first(global_alias) == 1);

// libc++ classifies comparison categories by visiting a constexpr enum array
// through a reference and subscripting a local count array.
enum Category : unsigned { None, Partial, Weak, Strong, CategoryCount };

template <unsigned Size>
constexpr Category classify(const Category (&types)[Size]) {
  int seen[CategoryCount] = {};
  for (auto type : types) ++seen[type];
  if (seen[None]) return None;
  if (seen[Partial]) return Partial;
  if (seen[Weak]) return Weak;
  return Strong;
}

constexpr Category strong_categories[] = {Strong, Strong};
constexpr Category weak_categories[] = {Strong, Weak};
constexpr Category partial_categories[] = {Weak, Partial};
constexpr Category invalid_categories[] = {Strong, None};
static_assert(classify(strong_categories) == Strong);
static_assert(classify(weak_categories) == Weak);
static_assert(classify(partial_categories) == Partial);
static_assert(classify(invalid_categories) == None);

int mutable_values[2] = {7, 9};
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(first(mutable_values) == 7);
}  // namespace readable_arrays
