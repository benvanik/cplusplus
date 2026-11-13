// RUN: %cxx -verify -fsyntax-only %s

template <class T> int function(T value) {
  // expected-error@+1 {{"function body"}}
  static_assert(sizeof(T) == 0, "function body");
  return 0;
}
// expected-note@+1 {{in instantiation of function template specialization 'function <int>' requested here}}
template int function<int>(int);

template <class T> struct Member {
  void function() {
    // expected-error@+1 {{"member body"}}
    static_assert(sizeof(T) == 0, "member body");
  }
};
template struct Member<int>;

template <class T> int called(T value) {
  // expected-error@+1 {{"called body"}}
  static_assert(sizeof(T) == 0, "called body");
  return 0;
}
// expected-note@+1 {{in instantiation of function template specialization 'called <int>' requested here}}
int use() { return called(1); }

struct Operations {
  template <class T> void apply() {
    // expected-error@+1 {{"member template body"}}
    static_assert(sizeof(T) == 0, "member template body");
  }
  template <class T> static void static_apply() {
    // expected-error@+1 {{"static member template body"}}
    static_assert(sizeof(T) == 0, "static member template body");
  }
  template <class T> void deduced(T) {
    // expected-error@+1 {{"deduced member template body"}}
    static_assert(sizeof(T) == 0, "deduced member template body");
  }
  template <class T> void qualified() const & {
    // expected-error@+1 {{"qualified member template body"}}
    static_assert(sizeof(T) == 0, "qualified member template body");
  }
};
// expected-note@+1 {{in instantiation of function template specialization 'apply <int>' requested here}}
template void Operations::apply<int>();
// expected-note@+1 {{in instantiation of function template specialization 'static_apply <int>' requested here}}
template void Operations::static_apply<int>();
// expected-note@+1 {{in instantiation of function template specialization 'deduced <int>' requested here}}
template void Operations::deduced(int);
// expected-note@+1 {{in instantiation of function template specialization 'qualified <int>' requested here}}
template void Operations::qualified<int>() const &;

template <class T> struct Container {
  template <class U> void convert() {
    // expected-error@+1 {{"nested member template body"}}
    static_assert(sizeof(U) == 0, "nested member template body");
  }
  void store() {
    // expected-error@+1 {{"class specialization member body"}}
    static_assert(sizeof(T) == 0, "class specialization member body");
  }
};
// expected-note@+1 {{in instantiation of function template specialization 'convert <int>' requested here}}
template void Container<int>::convert<int>();
// expected-note@+1 {{in instantiation of member function '::Container<int>::store' requested here}}
template void Container<int>::store();

struct Missing {
  template <class T> void apply(T) {}
};
// expected-error@+1 {{explicit instantiation of 'apply' does not refer to a function template specialization}}
template void Missing::apply(int, int);
// expected-error@+1 {{explicit instantiation of 'absent' does not refer to a function template specialization}}
template void Missing::absent();

struct Ordinary { void apply() {} };
// expected-error@+1 {{explicit instantiation requires a member of a class template specialization}}
template void Ordinary::apply();
