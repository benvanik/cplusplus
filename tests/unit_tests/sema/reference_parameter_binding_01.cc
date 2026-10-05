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
