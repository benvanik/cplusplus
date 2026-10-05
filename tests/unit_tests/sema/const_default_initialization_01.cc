// RUN: %cxx -verify -fsyntax-only %s

namespace union_requires_initializer {
union Choice { unsigned first; unsigned second; };
// expected-error@+1 {{requires a const-default-constructible type}}
constexpr Choice value;
}  // namespace union_requires_initializer

namespace nested_union_requires_initializer {
union Choice { unsigned first; unsigned second; };
struct Configuration { Choice choice; };
// expected-error@+1 {{requires a const-default-constructible type}}
constexpr Configuration value;
}  // namespace nested_union_requires_initializer

namespace defaulted_union_requires_initializer {
union Choice { unsigned first; unsigned second; constexpr Choice() = default; };
// expected-error@+1 {{requires a const-default-constructible type}}
constexpr Choice value;
}  // namespace defaulted_union_requires_initializer

namespace array_of_unions {
union Choice { unsigned first; unsigned second; };
struct Configuration { Choice choices[2]; };
// expected-error@+1 {{requires a const-default-constructible type}}
constexpr Configuration value;
}  // namespace array_of_unions

namespace uninitialized_base {
struct Base { unsigned value; };
struct Derived : Base {};
// expected-error@+1 {{requires a const-default-constructible type}}
constexpr Derived value;
}  // namespace uninitialized_base

namespace selected_union_default {
union Choice { unsigned first; unsigned second = 9; };
constexpr Choice value;
static_assert(value.second == 9);
struct Configuration { Choice choices[2]; };
constexpr Configuration config;
static_assert(config.choices[1].second == 9);
}  // namespace selected_union_default

namespace user_provided_union {
// An inactive union is a valid constant when its constructor is user-provided.
union Choice { unsigned value; constexpr Choice() {} };
constexpr Choice value;
struct Configuration { Choice choice; };
constexpr Configuration config;
}  // namespace user_provided_union

namespace empty_and_base_defaults {
// Empty classes and default-initialized bases need no synthesized scalar zero.
struct Empty {};
constexpr Empty empty;
struct Base { unsigned value = 9; };
struct Derived : Base {};
constexpr Derived derived;
static_assert(derived.value == 9);
}  // namespace empty_and_base_defaults

namespace activate_automatic_union {
// Ordinary automatic storage can activate an initially inactive union member.
union Choice { unsigned first; unsigned second; };
static constexpr unsigned compute() {
  Choice choice;
  choice.second = 9;
  return choice.second;
}
static_assert(compute() == 9);
}  // namespace activate_automatic_union

namespace const_automatic_union {
// Const automatic storage has the same declaration requirement.
union Choice { unsigned first; unsigned second; };
static void invalid() {
  // expected-error@+1 {{requires a const-default-constructible type}}
  const Choice value;
}
}  // namespace const_automatic_union

namespace unnamed_bitfield {
// Unnamed bit-fields contribute padding rather than a required initializer.
struct Padded { unsigned : 3; unsigned value = 9; };
constexpr Padded value;
static_assert(value.value == 9);
constexpr Padded copied = value;
static_assert(copied.value == 9);
struct UserPadded {
  unsigned : 3;
  unsigned value = 11;
  constexpr UserPadded() {}
};
constexpr UserPadded user;
static_assert(user.value == 11);
constexpr UserPadded user_copy = user;
static_assert(user_copy.value == 11);
struct PaddingOnly { unsigned : 3; };
constexpr PaddingOnly padding;
constexpr PaddingOnly padding_copy = padding;
}  // namespace unnamed_bitfield

namespace explicit_union_initializer {
// An explicit initializer supplies the value-initialization phase.
union Choice { unsigned first; unsigned second; };
constexpr Choice value{};
static_assert(value.first == 0);
}  // namespace explicit_union_initializer
