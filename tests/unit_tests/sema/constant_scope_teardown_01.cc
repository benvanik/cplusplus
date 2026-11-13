// RUN: %cxx -verify -fsyntax-only %s

namespace constant_scope_teardown {
constexpr void append_digit(unsigned* output, unsigned digit) {
  *output = *output * 10 + digit;
}

struct Guard {
  unsigned* output;
  unsigned digit;
  constexpr explicit operator bool() const { return true; }
  constexpr ~Guard() { append_digit(output, digit); }
};

struct GuardArray {
  Guard values[2];
};

struct Envelope {
  Guard guard;
};

constexpr Guard make_guard(unsigned* output, unsigned digit) {
  return Guard{output, digit};
}

constexpr void borrow_guard(const Guard&) {}

constexpr void borrow_digit(const unsigned&) {}

constexpr void take_guard(Guard) {}

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

constexpr unsigned discarded_temporary() {
  unsigned output = 0;
  Guard{&output, 1};
  return output;
}
static_assert(discarded_temporary() == 1);

constexpr unsigned returned_temporary() {
  unsigned output = 0;
  make_guard(&output, 2);
  return output;
}
static_assert(returned_temporary() == 2);

constexpr unsigned reference_parameter_temporary() {
  unsigned output = 0;
  borrow_guard(Guard{&output, 3});
  return output;
}
static_assert(reference_parameter_temporary() == 3);

constexpr unsigned value_parameter_temporary() {
  unsigned output = 0;
  take_guard(Guard{&output, 4});
  return output;
}
static_assert(value_parameter_temporary() == 4);

constexpr unsigned conditional_temporary(bool first) {
  unsigned output = 0;
  first ? Guard{&output, 5} : Guard{&output, 6};
  return output;
}
static_assert(conditional_temporary(true) == 5);
static_assert(conditional_temporary(false) == 6);

constexpr unsigned condition_full_expression() {
  unsigned output = 0;
  if (Guard{&output, 7}) {
    if (output != 7) return 99;
  }
  return output;
}
static_assert(condition_full_expression() == 7);

constexpr unsigned temporary_array_order() {
  unsigned output = 0;
  GuardArray{{Guard{&output, 1}, Guard{&output, 2}}};
  return output;
}
static_assert(temporary_array_order() == 21);

constexpr unsigned extended_object_lifetime() {
  unsigned output = 0;
  {
    const Guard& guard = Guard{&output, 3};
    if (output != 0 || guard.digit != 3) return 99;
  }
  return output;
}
static_assert(extended_object_lifetime() == 3);

constexpr unsigned extended_array_lifetime() {
  unsigned output = 0;
  {
    const GuardArray& guards =
        GuardArray{{Guard{&output, 4}, Guard{&output, 5}}};
    if (output != 0 || guards.values[1].digit != 5) return 99;
  }
  return output;
}
static_assert(extended_array_lifetime() == 54);

constexpr unsigned extended_subobject_lifetime() {
  unsigned output = 0;
  {
    const unsigned& digit = Envelope{Guard{&output, 9}}.guard.digit;
    if (output != 0 || digit != 9) return 99;
  }
  return output;
}
static_assert(extended_subobject_lifetime() == 9);

constexpr unsigned extended_array_element_lifetime() {
  unsigned output = 0;
  {
    const Guard& guard =
        GuardArray{{Guard{&output, 1}, Guard{&output, 2}}}.values[0];
    if (output != 0 || guard.digit != 1) return 99;
  }
  return output;
}
static_assert(extended_array_element_lifetime() == 21);

constexpr unsigned extended_conditional_lifetime(bool first) {
  unsigned output = 0;
  {
    const Guard& guard = first ? Guard{&output, 1} : Guard{&output, 2};
    if (output != 0 || guard.digit != (first ? 1u : 2u)) return 99;
  }
  return output;
}
static_assert(extended_conditional_lifetime(true) == 1);
static_assert(extended_conditional_lifetime(false) == 2);

constexpr unsigned parameter_subobject_lifetime() {
  unsigned output = 0;
  borrow_digit(Envelope{Guard{&output, 3}}.guard.digit);
  return output;
}
static_assert(parameter_subobject_lifetime() == 3);

constexpr unsigned nested_boundaries() {
  unsigned output = 0;
  Guard{&output, 6};
  {
    const Guard& extended = Guard{&output, 7};
    Guard{&output, 8};
    if (output != 68 || extended.digit != 7) return 99;
  }
  return output;
}
static_assert(nested_boundaries() == 687);
}  // namespace constant_scope_teardown
