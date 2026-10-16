// RUN: %cxx -std=c++23 -fsyntax-only -verify %s

constexpr const int& identity(const int& value) { return value; }

// A reference parameter borrows a materialized argument for the caller's
// complete expression, including the returned reference read.
static_assert(identity(3) == 3);

constexpr int read_temporary() { return identity(5); }
static_assert(read_temporary() == 5);

struct array_bound {
  int values[identity(7)];
};
static_assert(sizeof(array_bound::values) == 7 * sizeof(int));

template <const int& Value>
struct reference_argument {
  static constexpr int value = Value;
};

constexpr int persistent = 11;
static_assert(reference_argument<identity(persistent)>::value == 11);

// The borrowed temporary expires after the complete expression containing the
// call rather than following the returned reference into a later expression.
constexpr const int* escape(const int& value) { return &value; }
constexpr int read_expired_temporary() {
  const int* pointer = escape(13);
  return *pointer;
}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(read_expired_temporary() == 13);
