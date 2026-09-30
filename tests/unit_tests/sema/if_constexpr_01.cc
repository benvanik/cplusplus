// RUN: %cxx -std=c++23 -fsyntax-only -verify %s

extern const int external_value;

int external_condition() {
  // expected-error@+1 {{condition of 'if constexpr' is not a constant expression}}
  if constexpr (external_value == 4) return 11;
  return 22;
}

int runtime_condition(int value) {
  // expected-error@+1 {{condition of 'if constexpr' is not a constant expression}}
  if constexpr (value) return 11;
  return 22;
}

template <typename T>
int dependent_condition(T value) {
  // expected-error@+1 {{condition of 'if constexpr' is not a constant expression}}
  if constexpr (value) return 11;
  return 22;
}

// expected-note@+1 {{in instantiation of function template specialization 'dependent_condition <int>' requested here}}
int instantiate_dependent_condition() { return dependent_condition(3); }

template <typename T>
int nondependent_condition(int value) {
  // expected-error@+1 {{condition of 'if constexpr' is not a constant expression}}
  if constexpr (value) return 11;
  return 22;
}

template <bool Value>
constexpr int select() {
  if constexpr (Value) return 11;
  return 22;
}

static_assert(select<true>() == 11);
static_assert(select<false>() == 22);

template <typename T>
constexpr int discard_dependent_arm() {
  if constexpr (true) return 33;
  else if constexpr (T::missing) return 44;
}

static_assert(discard_dependent_arm<int>() == 33);

constexpr int initialized() {
  int visits = 0;
  if constexpr (++visits; false) visits += 100;
  else visits += 10;
  return visits;
}

static_assert(initialized() == 11);
