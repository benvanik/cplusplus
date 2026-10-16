// RUN: %cxx -verify -fsyntax-only %s

namespace constexpr_reference_storage_duration {
struct Value {
  unsigned number = 9;
  unsigned lanes[2] = {3, 5};
};

constexpr Value global{};
constexpr const Value& global_temporary = Value{};
constexpr unsigned answer() { return 42; }

// An automatic constexpr reference can retain an address whose referent has
// static storage duration, including a lifetime-extended temporary.
constexpr bool static_referents() {
  constexpr const Value& object = global;
  constexpr const unsigned& member = global.number;
  constexpr const unsigned& element = global.lanes[1];
  constexpr const Value& extended = global_temporary;
  constexpr auto& function = answer;
  constexpr const char (&text)[2] = "x";
  return &object == &global && &member == &global.number &&
         &element == &global.lanes[1] && &extended == &global_temporary &&
         function() == 42 && text[0] == 'x';
}
static_assert(static_referents());

// Ordinary automatic references retain invocation-local objects and
// temporaries while their lifetimes remain active.
constexpr bool ordinary_automatic_references() {
  Value object{};
  const unsigned& scalar = 7u;
  const Value& temporary = Value{};
  const unsigned& member = object.number;
  return scalar == 7 && temporary.number == 9 && member == 9;
}
static_assert(ordinary_automatic_references());

constexpr void reject_scalar_temporary() {
  // expected-error@+1 {{constexpr variable must be initialized}}
  constexpr const unsigned& value = 7u;
}

constexpr void reject_class_temporary() {
  // expected-error@+1 {{constexpr variable must be initialized}}
  constexpr const Value& value = Value{};
}

constexpr void reject_temporary_subobject() {
  // expected-error@+1 {{constexpr variable must be initialized}}
  constexpr const unsigned& value = Value{}.number;
}

constexpr void reject_named_automatic() {
  constexpr Value object{};
  // expected-error@+1 {{constexpr variable must be initialized}}
  constexpr const Value& value = object;
}

constexpr void reject_named_subobject() {
  constexpr Value object{};
  // expected-error@+1 {{constexpr variable must be initialized}}
  constexpr const unsigned& value = object.number;
}

constexpr void reject_named_array_element() {
  constexpr Value object{};
  // expected-error@+1 {{constexpr variable must be initialized}}
  constexpr const unsigned& value = object.lanes[1];
}
}  // namespace constexpr_reference_storage_duration
