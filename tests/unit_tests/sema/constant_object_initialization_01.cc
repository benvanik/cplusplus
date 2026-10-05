// RUN: %cxx -verify -fsyntax-only %s

namespace nested_defaults {
// Nested implicit constructors run after the outer zero-initialization phase.
struct Inner { unsigned value = 9; };
struct Outer { Inner inner; };
static_assert(Outer().inner.value == 9);
}  // namespace nested_defaults

namespace defaulted_constructors {
// Explicitly defaulted constructors follow the same rule.
struct Inner { unsigned value = 9; constexpr Inner() = default; };
struct Outer { Inner inner; constexpr Outer() = default; };
static_assert(Outer().inner.value == 9);
}  // namespace defaulted_constructors

namespace array_defaults {
// Every array element executes its default initialization.
struct Inner { unsigned value = 9; };
struct Outer { Inner inner[2]; };
static_assert(Outer().inner[0].value == 9);
static_assert(Outer().inner[1].value == 9);
}  // namespace array_defaults

namespace zero_before_defaults {
// A nested constructor sees zeroed storage before later member defaults run.
struct Inner { unsigned copy = later; unsigned later = 9; };
struct Outer { Inner inner; };
static_assert(Outer().inner.copy == 0);
static_assert(Outer().inner.later == 9);
}  // namespace zero_before_defaults

namespace nested_user_constructor {
// A nested user-provided constructor still runs in the zeroed outer object.
struct Inner { unsigned value; constexpr Inner() : value(9) {} };
struct Outer { Inner inner; };
static_assert(Outer().inner.value == 9);
}  // namespace nested_user_constructor

namespace base_defaults {
// Base construction keeps the zero-initialized storage and applies defaults.
struct Base { unsigned value = 9; unsigned other; };
struct Outer : Base { unsigned copy = value; };
static_assert(Outer().value == 9);
static_assert(Outer().other == 0);
static_assert(Outer().copy == 9);
}  // namespace base_defaults

namespace nested_zero_phase {
// Nested user-provided construction does not erase the outer zero phase.
struct Inner { unsigned value; constexpr Inner() {} };
struct Outer { Inner inner; };
static_assert(Outer().inner.value == 0);
}  // namespace nested_zero_phase

namespace scalar_arrays {
// Scalar arrays retain their zeroed elements during default initialization.
struct Outer { unsigned values[2][2]; };
static_assert(Outer().values[1][1] == 0);
}  // namespace scalar_arrays

namespace union_default {
// The selected union default may belong to a later member.
union Choice { unsigned first; unsigned selected = 9; };
static_assert(Choice().selected == 9);
struct Outer { Choice choice; };
static_assert(Outer().choice.selected == 9);
}  // namespace union_default

namespace union_zero_phase {
// Value initialization of a trivial union initializes its first member.
union Choice { unsigned first; unsigned second; };
static_assert(Choice().first == 0);
}  // namespace union_zero_phase

namespace inactive_union_activation {
// Assignment activates a member of a default-initialized automatic union.
union Choice { unsigned first; unsigned second; };
static constexpr unsigned compute() {
  Choice choice;
  choice.second = 9;
  return choice.second;
}
static_assert(compute() == 9);
}  // namespace inactive_union_activation

namespace direct_member_constructor {
// Direct member initializers call their selected constructors.
struct Inner { unsigned value; constexpr Inner(unsigned value) : value(value) {} };
struct Outer { Inner inner{9}; };
static_assert(Outer().inner.value == 9);
}  // namespace direct_member_constructor

namespace uninitialized_user_constructor {
// A user-provided complete-object constructor has no implicit zero phase.
struct Inner { unsigned value; constexpr Inner() {} };
// expected-error@+1 {{constexpr variable must be initialized}}
constexpr Inner invalid = Inner();
}  // namespace uninitialized_user_constructor

namespace uninitialized_default_constructor {
// Ordinary default initialization cannot manufacture zero scalar members.
struct Inner { unsigned value; };
struct Outer { Inner inner; };
constexpr unsigned read() { Outer value; return value.inner.value; }
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(read() == 0);
}  // namespace uninitialized_default_constructor

namespace local_value_initialization {
struct Inner { unsigned value; constexpr Inner() {} };
struct Outer { Inner inner; constexpr Outer() = default; };
static constexpr unsigned read() { Outer value = Outer(); return value.inner.value; }
static_assert(read() == 0);
}  // namespace local_value_initialization

namespace braced_value_initialization {
struct Inner { unsigned value; constexpr Inner() {} };
struct Outer { Inner inner; constexpr Outer() = default; };
static_assert(Outer{}.inner.value == 0);
constexpr Outer value{};
static_assert(value.inner.value == 0);
}  // namespace braced_value_initialization

namespace member_value_initialization {
struct Inner { unsigned copy = later; unsigned later = 9; constexpr Inner() = default; };
struct Outer { Inner inner{}; };
static_assert(Outer().inner.copy == 0);
static_assert(Outer().inner.later == 9);
}  // namespace member_value_initialization

namespace user_constructor_defaults {
struct Inner { unsigned value = 9; };
struct Outer { Inner inner; constexpr Outer() {} };
static_assert(Outer().inner.value == 9);
}  // namespace user_constructor_defaults

namespace deep_zero_phase {
struct Inner { unsigned value; constexpr Inner() {} };
struct Middle { Inner inner; constexpr Middle() {} };
struct Outer { Middle middle; };
static_assert(Outer().middle.inner.value == 0);
}  // namespace deep_zero_phase

namespace base_zero_phase {
struct Base { unsigned value; constexpr Base() {} };
struct Middle : Base { constexpr Middle() {} };
struct Outer { Middle middle; };
static_assert(Outer().middle.value == 0);
}  // namespace base_zero_phase

namespace array_zero_phase {
struct Inner { unsigned value; constexpr Inner() {} };
struct Middle { Inner inner[2]; constexpr Middle() {} };
struct Outer { Middle middle; };
static_assert(Outer().middle.inner[1].value == 0);
}  // namespace array_zero_phase

namespace nonconstant_default {
unsigned runtime_value();
struct Inner { unsigned value = runtime_value(); };
struct Outer { Inner inner; };
// expected-error@+1 {{constexpr variable must be initialized}}
constexpr Outer invalid = Outer();
}  // namespace nonconstant_default

namespace move_delegation {
// Delegating to a move constructor initializes the original destination.
struct Moved {
  unsigned value;
  constexpr Moved() : Moved(Moved(7)) {}
  constexpr Moved(unsigned value) : value(value) {}
};
static_assert(Moved().value == 7);
}  // namespace move_delegation

namespace delegation_zero_phase {
// Delegation preserves storage zeroed by the containing object's value phase.
struct Inner {
  unsigned value;
  constexpr Inner() : Inner(0) {}
  constexpr Inner(unsigned) {}
};
struct Outer { Inner inner; };
static_assert(Outer().inner.value == 0);
}  // namespace delegation_zero_phase

namespace invalid_arithmetic_default {
struct Inner {
  unsigned divisor;
  // expected-warning@+1 {{invalid binary expression}}
  unsigned quotient = 32 / divisor;
};
struct Outer { Inner inner; };
// expected-error@+1 {{constexpr variable must be initialized}}
constexpr Outer invalid = Outer();
}  // namespace invalid_arithmetic_default

namespace temporary_reference_parameter {
// Reference parameters retain the address of their call-owned temporary.
struct Payload { unsigned words; };
static constexpr unsigned read(const Payload& payload) { return payload.words; }
static_assert(read(Payload{7}) == 7);
static_assert(read(Payload{9}) == 9);
}  // namespace temporary_reference_parameter

namespace selected_member_copy {
// Generated move construction executes a member's selected copy constructor.
struct Value {
  unsigned value;
  constexpr Value(unsigned value) : value(value) {}
  constexpr Value(const Value& other) : value(other.value + 1) {}
};
struct Wrapper {
  Value inner;
  constexpr Wrapper(unsigned value) : inner(value) {}
  constexpr Wrapper() : Wrapper(Wrapper(7)) {}
};
static_assert(Wrapper().inner.value == 8);
}  // namespace selected_member_copy
