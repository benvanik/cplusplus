// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

template <class T = int>
void func();

template <class T>
void func() {}

template <class T = void>
struct S;

template <class T>
struct S {};

template <template <class> class C = S>
struct Meta {};

template <class... Ts>
struct Variadic {};

template <class T>
struct MemberDefaults {
  template <template <class> class C = S>
  MemberDefaults(T) {}

  template <template <class...> class C = Variadic>
  int value() {
    return 1;
  }
};

void test() {
  func<>();
  func<double>();
  S<> s;
  S<int> si;
  Meta<> m;
  MemberDefaults<int> memberDefaults(0);
  int value = memberDefaults.value();
}
