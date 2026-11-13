// RUN: %cxx -std=c++23 -fsyntax-only -verify %s
// expected-no-diagnostics

struct Scaled {
  unsigned value;
  constexpr Scaled(unsigned source = 7, unsigned scale = 2)
      : value(source * scale) {}
};

constexpr unsigned constructed_references() {
  const Scaled& direct{3};
  const Scaled& copy = {4};
  Scaled&& moved{5};
  const Scaled& empty{};
  const Scaled& multiple{6, 3};
  return direct.value + copy.value + moved.value + empty.value + multiple.value;
}
static_assert(constructed_references() == 56);

template <unsigned Scale>
constexpr unsigned instantiated(unsigned source) {
  const Scaled& value{source, Scale};
  return value.value;
}
static_assert(instantiated<3>(9) == 27);

struct Pair { unsigned first; unsigned second; };
constexpr unsigned aliased_referent() {
  unsigned source = 3;
  const unsigned& alias{source};
  source = 9;
  return alias;
}
static_assert(aliased_referent() == 9);

constexpr unsigned converted_referent() {
  float value = 5.0f;
  const double& converted{value};
  value = 8.0f;
  return static_cast<unsigned>(converted);
}
static_assert(converted_referent() == 5);

constexpr unsigned value_initialized_referent() {
  const unsigned& zero{};
  return zero;
}
static_assert(value_initialized_referent() == 0);

constexpr unsigned aggregate_referent() {
  const Pair& aggregate{7, 11};
  return aggregate.first + aggregate.second;
}
static_assert(aggregate_referent() == 18);

constexpr unsigned array_referent() {
  const unsigned (&array)[]{13, 17};
  return array[0] + array[1];
}
static_assert(array_referent() == 30);
