// RUN: %cxx -verify -fsyntax-only %s

struct SelfPointer {
  unsigned value = 9;
  const unsigned* address = &value;
};

struct ThisPointer {
  unsigned value = 9;
  const ThisPointer* self = this;
};

constexpr SelfPointer persistent{};
static_assert(*persistent.address == 9);
static_assert(persistent.address == &persistent.value);

constexpr ThisPointer direct{};
static_assert(direct.self == &direct);

struct Nested {
  SelfPointer inner;
};
constexpr Nested nested{};
static_assert(nested.inner.address == &nested.inner.value);

constexpr ThisPointer elements[2]{};
static_assert(elements[0].self == &elements[0]);
static_assert(elements[0].self + 1 == &elements[1]);

struct ExplicitConstructor {
  unsigned value;
  const ExplicitConstructor* self;
  constexpr ExplicitConstructor(unsigned value = 9)
      : value(value), self(this) {}
};
constexpr ExplicitConstructor explicit_constructor{};
static_assert(explicit_constructor.self == &explicit_constructor);

struct BasePointer {
  const BasePointer* self = this;
};
struct DerivedPointer : BasePointer {
  unsigned value = 9;
};
constexpr DerivedPointer derived{};
static_assert(derived.self == static_cast<const BasePointer*>(&derived));
static_assert(derived.self->self == derived.self);

union UnionPointer {
  unsigned value;
  const UnionPointer* self;
  constexpr UnionPointer() : self(this) {}
};
constexpr UnionPointer selected_union{};
static_assert(selected_union.self == &selected_union);

constexpr bool copy_identity() {
  SelfPointer source{};
  SelfPointer copy = source;
  return copy.address == &source.value && copy.address != &copy.value;
}

constexpr bool copy_observes_original() {
  SelfPointer source{};
  SelfPointer copy = source;
  source.value = 11;
  return *copy.address == 11;
}

constexpr SelfPointer make_self_pointer() { return SelfPointer{}; }
constexpr SelfPointer elided = make_self_pointer();
static_assert(copy_identity());
static_assert(copy_observes_original());
static_assert(elided.address == &elided.value);

constexpr bool move_identity() {
  SelfPointer source{};
  SelfPointer destination = static_cast<SelfPointer&&>(source);
  return destination.address == &source.value &&
         destination.address != &destination.value;
}
static_assert(move_identity());

static_assert(*SelfPointer{}.address == 9);
static_assert(ThisPointer{}.self->value == 9);

constexpr const unsigned& persistent_scalar_reference = 7u;
constexpr const ThisPointer& persistent_reference = ThisPointer{};
constexpr const ThisPointer (&persistent_array_reference)[2] = {};
static_assert(persistent_scalar_reference == 7);
static_assert(persistent_reference.self == &persistent_reference);
static_assert(persistent_array_reference[0].self ==
              &persistent_array_reference[0]);
static_assert(persistent_array_reference[0].self + 1 ==
              &persistent_array_reference[1]);

namespace persistent_temporary_reads {
struct Base {
  unsigned base = 3;
};

struct Element : Base {
  unsigned value = 5;
};

struct Container {
  Element inner;
  unsigned values[2] = {7, 11};
};

struct AlternateContainer {
  unsigned prefix = 13;
  Element inner;
};

using Pair = unsigned[2];

// A const lvalue reference makes the whole temporary, including a selected
// base subobject, readable in later constant evaluations.
constexpr const Element& whole = Element{};
const Element& constant_initialized = Element{};
constexpr const Base& base = Element{};
constexpr const Pair& array = Pair{17, 19};
static_assert(whole.value == 5);
static_assert(constant_initialized.value == 5);
static_assert(base.base == 3);
static_assert(array[1] == 19);

// Other persistent bindings retain their addresses, but not permission to
// read the temporary in a later constant evaluation.
constexpr Element&& rvalue = Element{};
constexpr const Element& member = Container{}.inner;
constexpr const unsigned& nested_member = Container{}.inner.value;
constexpr const unsigned& array_element = Container{}.values[1];
constexpr const unsigned& direct_array_element = Pair{23, 29}[1];
constexpr const unsigned* escaped_pointer = &nested_member;
constexpr bool choose_first = true;
constexpr const Element& conditional_member =
    choose_first ? Container{}.inner : AlternateContainer{}.inner;
static_assert(escaped_pointer == &nested_member);

struct Static {
  inline static constexpr const unsigned& member = Container{}.inner.value;
};

constexpr unsigned automatic_conditional(bool first) {
  const Element& selected =
      first ? Container{}.inner : AlternateContainer{}.inner;
  return selected.value;
}
static_assert(automatic_conditional(true) == 5);
static_assert(automatic_conditional(false) == 5);

// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(rvalue.value == 5);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(member.value == 5);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(nested_member == 5);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(array_element == 11);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(direct_array_element == 29);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(*escaped_pointer == 5);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(conditional_member.value == 5);
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(Static::member == 5);
}  // namespace persistent_temporary_reads

constexpr bool automatic_references() {
  const unsigned& scalar = 7u;
  const ThisPointer& object = ThisPointer{};
  const ThisPointer (&array)[2] = {};
  const unsigned& member = ThisPointer{}.value;
  return scalar == 7 && object.self == &object &&
         array[0].self == &array[0] && array[0].self + 1 == &array[1] &&
         member == 9;
}
static_assert(automatic_references());

constexpr bool inspect(const ThisPointer& object) {
  return object.self == &object && object.self->value == 9;
}

constexpr unsigned inspect_member(const unsigned& value) { return value; }

constexpr const ThisPointer* return_pointer(const ThisPointer& object) {
  return object.self;
}

static_assert(inspect(ThisPointer{}));
static_assert(inspect_member(ThisPointer{}.value) == 9);
static_assert(return_pointer(ThisPointer{})->value == 9);

constexpr bool expired_declaration() {
  const ThisPointer* pointer = return_pointer(ThisPointer{});
  return pointer->value == 9;
}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(expired_declaration());

constexpr void save_pointer(const ThisPointer& object,
                            const ThisPointer*& output) {
  output = object.self;
}

constexpr bool expired_expression_statement() {
  const ThisPointer* pointer = nullptr;
  save_pointer(ThisPointer{}, pointer);
  return pointer->value == 9;
}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(expired_expression_statement());

constexpr bool save_pointer_condition(const ThisPointer& object,
                                      const ThisPointer*& output) {
  output = object.self;
  return true;
}

constexpr bool expired_condition() {
  const ThisPointer* pointer = nullptr;
  if (save_pointer_condition(ThisPointer{}, pointer)) {}
  return pointer->value == 9;
}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(expired_condition());

constexpr bool expired_reference_scope() {
  const ThisPointer* pointer = nullptr;
  {
    const ThisPointer& object = ThisPointer{};
    pointer = object.self;
  }
  return pointer->value == 9;
}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(expired_reference_scope());

constexpr bool expired_subobject_reference_scope() {
  const unsigned* pointer = nullptr;
  {
    const unsigned& value = ThisPointer{}.value;
    pointer = &value;
  }
  return *pointer == 9;
}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(expired_subobject_reference_scope());

constexpr const ThisPointer* escaped_return_expression() {
  return return_pointer(ThisPointer{});
}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(escaped_return_expression()->value == 9);
