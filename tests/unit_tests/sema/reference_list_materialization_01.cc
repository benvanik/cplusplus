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
constexpr unsigned list_referents() {
  unsigned source = 3;
  const unsigned& alias{source};
  float value = 5.0f;
  const double& converted{value};
  const unsigned& zero{};
  const Pair& aggregate{7, 11};
  const unsigned (&array)[]{13, 17};
  source = 9;
  value = 8.0f;
  return alias + static_cast<unsigned>(converted) + zero + aggregate.first +
         aggregate.second + array[0] + array[1];
}
static_assert(list_referents() == 62);
