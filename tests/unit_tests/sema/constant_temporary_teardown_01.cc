// RUN: %cxx -verify -fsyntax-only %s

namespace constant_temporary_teardown {
struct Guard {
  unsigned* output;
  unsigned digit;

  constexpr ~Guard() { *output = *output * 10 + digit; }
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
}  // namespace constant_temporary_teardown
