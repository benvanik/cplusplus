// RUN: %cxx -verify -fsyntax-only %s

namespace constant_scope_teardown {
constexpr void append_digit(unsigned* output, unsigned digit) {
  *output = *output * 10 + digit;
}

struct Guard {
  unsigned* output;
  unsigned digit;
  constexpr ~Guard() { append_digit(output, digit); }
};

constexpr unsigned block_fallthrough() {
  unsigned output = 1;
  {
    Guard first{&output, 2};
    Guard second{&output, 3};
  }
  return output;
}
static_assert(block_fallthrough() == 132);

constexpr unsigned loop_control() {
  unsigned output = 4;
  for (unsigned iteration = 0; iteration != 2; ++iteration) {
    Guard first{&output, 5};
    Guard second{&output, 6};
    if (iteration == 0) continue;
    break;
  }
  return output;
}
static_assert(loop_control() == 46565);

constexpr void return_from_scope(unsigned* output) {
  Guard first{output, 7};
  Guard second{output, 8};
  return;
}

constexpr unsigned early_return() {
  unsigned output = 6;
  return_from_scope(&output);
  return output;
}
static_assert(early_return() == 687);

constexpr unsigned statement_scopes() {
  unsigned output = 1;
  if (Guard ifGuard{&output, 2}; true) {
  }
  switch (Guard switchGuard{&output, 3}; 0) {
    default:
      break;
  }
  for (Guard forGuard{&output, 4}; false;) {
  }
  return output;
}
static_assert(statement_scopes() == 1234);
}  // namespace constant_scope_teardown
