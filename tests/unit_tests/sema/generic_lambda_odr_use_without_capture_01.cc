// RUN: %cxx -verify -fsyntax-only %s

struct two {
  constexpr operator int() const { return 2; }
};

template <class T>
constexpr int read(const T& value) {
  return value;
}

int by_reference_parameter() {
  constexpr int y = 1;
  auto f = [](auto n) {
    // expected-error@1 {{variable 'y' cannot be implicitly captured in a lambda with no capture-default specified}}
    return read(y) + n;
  };
  // expected-note@1 {{in instantiation of function template specialization 'operator ()<two>' requested here}}
  return f(two{});
}

int not_instantiated() {
  constexpr int y = 1;
  auto f = [](auto n) { return read(y) + n; };
  return 0;
}

int not_an_odr_use() {
  constexpr int y = 1;
  auto f = [](auto n) { return n + y; };
  return f(1);
}

int constant_eligibility() {
  int mutableValue = 1;
  // expected-error@+1 {{variable 'mutableValue' cannot be implicitly captured in a lambda with no capture-default specified}}
  auto readMutable = [] { return mutableValue; };

  const float floatingValue = 1.0f;
  // expected-error@+1 {{variable 'floatingValue' cannot be implicitly captured in a lambda with no capture-default specified}}
  auto readFloating = [] { return floatingValue; };

  const volatile int volatileValue = 1;
  // expected-error@+1 {{variable 'volatileValue' cannot be implicitly captured in a lambda with no capture-default specified}}
  auto readVolatile = [] { return volatileValue; };

  const int legacyIntegral = 2;
  auto readLegacyIntegral = [] { return legacyIntegral; };

  constexpr float constexprFloating = 3.0f;
  auto readConstexprFloating = [] { return constexprFloating; };

  return readMutable() + static_cast<int>(readFloating()) + readVolatile() +
         readLegacyIntegral() + static_cast<int>(readConstexprFloating());
}
