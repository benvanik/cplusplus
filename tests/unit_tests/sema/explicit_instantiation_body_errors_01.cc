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
