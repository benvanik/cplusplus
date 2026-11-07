// RUN: %cxx -verify -fsyntax-only -fvalidate-ast %s

template <class T>
struct box {
  T value;
  constexpr T get() const { return value; }
};

template <class T>
constexpr box<T> make_box(T value) {
  return {value};
}

template <class T>
struct pointer_box {
  pointer_box(T*);
  T value;
};

template <class T>
void check_dependent_ctad(T* value) {
  pointer_box deferred(value);
  (void)deferred.value;
}

// expected-error@+1 {{use of undeclared identifier 'missing'}}
constexpr auto unresolved = missing;
static_assert(make_box(unresolved).get() == 42);

// expected-error@+1 {{variable with 'auto' type must be initialized}}
auto without_initializer;
static_assert(without_initializer == 42);

// expected-error@+1 {{cannot deduce 'auto*' from an initializer of type 'int'}}
auto* incompatible = 42;
static_assert(*incompatible == 42);

// expected-error@+1 {{no viable constructor or deduction guide for deduction of template arguments of 'pointer_box'}}
pointer_box failed_ctad(42);
static_assert(failed_ctad.value == 42);

void instantiate_dependent_ctad(int* value) {
  check_dependent_ctad(value);
}
