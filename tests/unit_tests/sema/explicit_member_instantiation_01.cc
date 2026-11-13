// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

template <bool, class T = int> struct Enable {};
template <class T> struct Enable<true, T> { using type = T; };

struct Operations {
  template <class T, typename Enable<sizeof(T) == 8>::type = 0>
  void apply(T) { static_assert(sizeof(T) == 0, "unselected overload"); }
  template <class T> void apply(T) { static_assert(sizeof(T) == 4); }
  template <class T> void external() {
    static_assert(sizeof(T) == 0, "extern template body");
  }
  template <class T> Operations(T) { static_assert(sizeof(T) == 4); }
  template <class T> int operator()(T) const { return sizeof(T); }
  template <class T> operator T() const { return T{}; }
};
template void Operations::apply(int);
extern template void Operations::external<int>();
template Operations::Operations(int);
template int Operations::operator()<int>(int) const;
template Operations::operator int() const;

template <class T> struct Members {
  Members(T) { static_assert(sizeof(T) == 4); }
  ~Members() { static_assert(sizeof(T) == 4); }
  void apply(T) { static_assert(sizeof(T) == 4); }
  template <class U, typename Enable<sizeof(U) == 8>::type = 0>
  void apply(U) { static_assert(sizeof(U) == 0, "unselected overload"); }
  void external() { static_assert(sizeof(T) == 0, "extern member body"); }
};
template Members<int>::Members(int);
template Members<int>::~Members();
extern template void Members<int>::apply(int);
template void Members<int>::apply(int);
extern template void Members<int>::external();

template <class T> struct QualifiedMembers {
  using Value = T;
  void constrained(Value) requires(sizeof(T) == 4) {}
};
template void QualifiedMembers<int>::constrained(Value);

template <class T> concept AnyObject = sizeof(T) > 0;
template <class T> concept FourByteObject = AnyObject<T> && sizeof(T) == 4;
template <class T> struct OrderedMembers {
  void select() requires AnyObject<T> {
    static_assert(sizeof(T) == 0, "less constrained overload");
  }
  void select() requires FourByteObject<T> { static_assert(sizeof(T) == 4); }
};
template void OrderedMembers<int>::select();

namespace Library {
template <class T> int read(T) { return sizeof(T); }
}
template int Library::read(int);

struct Specialized {
  template <class T> void apply() {
    static_assert(sizeof(T) == 0, "specialized member template");
  }
};
template <> void Specialized::apply<int>();
extern template void Specialized::apply<int>();
template void Specialized::apply<int>();

template <class T> struct SpecializedMembers {
  void apply() { static_assert(sizeof(T) == 0, "specialized member"); }
  void defined();
};
template <class T> void SpecializedMembers<T>::defined() {
  static_assert(sizeof(T) == 0, "specialized out-of-class member");
}
template <> void SpecializedMembers<int>::apply();
template <> void SpecializedMembers<int>::defined() {}
extern template void SpecializedMembers<int>::apply();
template void SpecializedMembers<int>::apply();
template struct SpecializedMembers<int>;
void use_specializations(SpecializedMembers<int>& object) {
  object.apply();
  object.defined();
}

// Specialized constructors, conversions, and operators are ordinary call
// candidates, not templates with another empty argument list to instantiate.
template <class T> struct Value {
  int value;
  constexpr Value(T input) : value(input) { static_assert(sizeof(T) == 0); }
  constexpr operator T() const { static_assert(sizeof(T) == 0); return 0; }
  constexpr T operator()(T) const { static_assert(sizeof(T) == 0); return 0; }
  constexpr T operator+(T) const { static_assert(sizeof(T) == 0); return 0; }
};
template <> constexpr Value<int>::Value(int input) : value(input) {}
template <> constexpr Value<int>::operator int() const { return value + 1; }
template <> constexpr int Value<int>::operator()(int input) const { return value + input; }
template <> constexpr int Value<int>::operator+(int input) const { return value - input; }
template Value<int>::Value(int);
template Value<int>::operator int() const;
template int Value<int>::operator()(int) const;
template int Value<int>::operator+(int) const;
constexpr Value<int> value(5);
static_assert(static_cast<int>(value) == 6);
static_assert(value(2) == 7);
static_assert(value + 2 == 3);
constexpr int converted = value;
static_assert(converted == 6);

struct Derived : Value<int> { using Value<int>::Value; };
constexpr Derived derived(9);
static_assert(static_cast<int>(derived) == 10);
