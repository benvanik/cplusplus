// RUN: %cxx -verify -fsyntax-only %s

struct SelfPointer {
  unsigned value = 9;
  const unsigned* address = &value;
};

struct ThisPointer {
  unsigned value = 9;
  const ThisPointer* self = this;
};

// ====
// Persistent objects retain the storage selected while their initializers run.
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

// ====
// Copying an object copies its pointer value. Guaranteed elision instead keeps
// the destination selected during construction.
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

// ====
// An unbound temporary stays readable through its complete expression.
static_assert(*SelfPointer{}.address == 9);
static_assert(ThisPointer{}.self->value == 9);

// ====
// A reference declaration owns its materialized temporary for the reference
// lifetime, including scalar and array values.
constexpr const unsigned& persistent_scalar_reference = 7u;
constexpr const ThisPointer& persistent_reference = ThisPointer{};
constexpr const ThisPointer (&persistent_array_reference)[2] = {};
static_assert(persistent_scalar_reference == 7);
static_assert(persistent_reference.self == &persistent_reference);
static_assert(persistent_array_reference[0].self ==
              &persistent_array_reference[0]);
static_assert(persistent_array_reference[0].self + 1 ==
              &persistent_array_reference[1]);

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

// ====
// A parameter borrows its temporary through the caller's complete expression.
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

// ====
// Borrowers stop reading a temporary at each C++ lifetime boundary.
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
