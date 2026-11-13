// clang-format off
// RUN: %cxx -verify -fsyntax-only %s

// Emission targets introduced by builtin calls are not source declarations.
double builtin_root(double value) { return __builtin_sqrt(value); }
int builtin_magnitude(int value) { return __builtin_abs(value); }
float builtin_exponential(float value) { return __builtin_expf(value); }
double builtin_cube(double value) { return __builtin_cbrt(value); }

namespace library {
template <class = void> constexpr int sqrt(double) { return 17; }
template <class = void> constexpr int abs(int value) {
  return __builtin_abs(value);
}
}
using library::sqrt;
using library::abs;
static_assert(sqrt(4.0) == 17);
static_assert(abs(-7) == 7);

// Ordinary hidden functions do not become ADL candidates through a user's
// conversion operator. Actual hidden friends remain discoverable through ADL.
struct Number {
  operator double() const;
  friend constexpr int magnitude(Number) { return 23; }
};
template <class T> concept HasCube = requires(T value) { cbrt(value); };
static_assert(!HasCube<Number>);
static_assert(magnitude(Number{}) == 23);

float before_declaration(float value) {
  return expf(value); // expected-error {{use of undeclared identifier 'expf'}}
}

// An actual declaration makes the retained C symbol visible to source lookup.
extern "C" double sqrt(double value);
extern "C" int abs(int value);
extern "C" float expf(float value);
static_assert(__is_same(decltype(sqrt(4.0)), double));
double source_root(double value) { return sqrt(value); }
int source_magnitude(int value) { return abs(value); }
float source_exponential(float value) { return expf(value); }
