// RUN: %cxx -std=c++23 -fsyntax-only -verify %s

unsigned factor();

unsigned local_self_call() {
  // expected-error@+1 {{called object of type 'const unsigned int' is not a function or function pointer}}
  const unsigned factor = factor();
  return factor;
}

struct Parameters {
  // expected-error@+1 {{called object of type 'const int' is not a function or function pointer}}
  static const int factor = factor();
};

template <typename T>
int template_self_call() {
  // expected-error@+1 {{called object of type 'const int' is not a function or function pointer}}
  const int factor = factor();
  return factor;
}

static constexpr int discarded_self_reference() {
  const int value = false ? value : 7;
  return value;
}

static_assert(discarded_self_reference() == 7);

template <int Count>
static constexpr int dependent_self_reference() {
  const int value = false ? value : Count;
  return value;
}

static_assert(dependent_self_reference<7>() == 7);
static_assert(dependent_self_reference<11>() == 11);
