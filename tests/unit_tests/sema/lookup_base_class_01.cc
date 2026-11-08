// clang-format off
// RUN: %cxx -verify -fsyntax-only %s

// Base class member lookup: names inherited from base classes
// should be found by unqualified lookup in derived classes.

struct Base {
  int value = 42;
  static int static_value;

  int get() const { return value; }
};

int Base::static_value = 10;

struct Derived : Base {
  int use_base() {
    int a = value;
    int b = static_value;
    int c = get();
    return a + b + c;
  }
};

// Diamond hierarchy - name found through multiple paths
struct A {
  using type = int;
};

struct B : A {};
struct C : A {};
struct D : B, C {
  // Qualified lookup should resolve through a specific path
  using type_b = B::type;
  using type_c = C::type;
};

struct RepeatedMember {
  int value;
  int get() const { return value; }
  static int get(int value) { return value; }
};

struct RepeatedLeft : RepeatedMember {};
struct RepeatedRight : RepeatedMember {};

struct UsingDoesNotSelectStorage : RepeatedLeft, RepeatedRight {
  using RepeatedLeft::get;
  using RepeatedLeft::value;

  int read() const {
    return value; // expected-error {{member 'value' found in multiple base classes of 'UsingDoesNotSelectStorage'}}
  }

  int call() const {
    return get(); // expected-error {{member 'get' found in multiple base classes of 'UsingDoesNotSelectStorage'}}
  }

  int call_static() const { return get(17); }
};

int ambiguous_value(UsingDoesNotSelectStorage& object) {
  return object.value; // expected-error {{member 'value' found in multiple base classes of 'UsingDoesNotSelectStorage'}}
}

int ambiguous_call(UsingDoesNotSelectStorage& object) {
  return object.get(); // expected-error {{member 'get' found in multiple base classes of 'UsingDoesNotSelectStorage'}}
}

int static_call(UsingDoesNotSelectStorage& object) { return object.get(19); }

int RepeatedMember::* member_pointer = &UsingDoesNotSelectStorage::value;
