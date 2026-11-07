// RUN: %cxx -verify -fsyntax-only %s

constexpr int incomplete_while() {
  while (true)
  // expected-error@1 {{expected a statement}}
}

// expected-error@1 {{static assertion expression is not an integral constant expression}}
static_assert(incomplete_while() == 0);

constexpr int incomplete_do() {
  do
  // expected-error@2 {{expected a statement}}
  // expected-warning@1 {{non-void function does not return a value of type 'int' on every path}}
}

// expected-error@1 {{static assertion expression is not an integral constant expression}}
static_assert(incomplete_do() == 0);

constexpr int incomplete_for() {
  for (;;)
  // expected-error@1 {{expected a statement}}
}

// expected-error@1 {{static assertion expression is not an integral constant expression}}
static_assert(incomplete_for() == 0);

constexpr int incomplete_range_for() {
  int values[1]{};
  for (int value : values)
  // expected-error@2 {{expected a statement}}
  // expected-warning@1 {{non-void function does not return a value of type 'int' on every path}}
}

// expected-error@1 {{static assertion expression is not an integral constant expression}}
static_assert(incomplete_range_for() == 0);
