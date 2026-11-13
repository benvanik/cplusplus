// RUN: %cxx -verify -fsyntax-only %s

struct Incomplete;
using Alias = const Incomplete;

// expected-error@+1 {{parameter has incomplete class type '::Incomplete'}}
void named(Incomplete value) {}
// expected-error@+1 {{parameter has incomplete class type '::Incomplete'}}
void unnamed(Incomplete) {}
// expected-error@+1 {{parameter has incomplete class type 'const ::Incomplete'}}
void qualified(Alias value) {}
// expected-error@+1 {{return type is an incomplete class type '::Incomplete'}}
Incomplete returned() { for (;;) {} }

struct Methods {
  // expected-error@+1 {{parameter has incomplete class type '::Incomplete'}}
  void member(Incomplete value) {}
  // expected-error@+1 {{parameter has incomplete class type '::Incomplete'}}
  Methods(Incomplete value) {}
};

// expected-error@+1 {{parameter has incomplete class type '::Incomplete'}}
auto lambda = [](Incomplete value) {};

struct Abstract { virtual void operation() = 0; };
// expected-error@+1 {{parameter has abstract class type '::Abstract'}}
void abstract_parameter(Abstract value) {}
// expected-error@+1 {{return type is an abstract class type '::Abstract'}}
Abstract abstract_return() { for (;;) {} }

template <class T>
// expected-error@+1 {{parameter has incomplete class type '::Incomplete'}}
void independent(Incomplete value, T) {}

template <class T>
// expected-error@+1 {{parameter has incomplete class type '::Incomplete'}}
void dependent(T value) {}
// expected-note@+1 {{in instantiation of function template specialization 'dependent <::Incomplete>' requested here}}
template void dependent<Incomplete>(Incomplete);

template <class T> struct Member {
  // expected-error@+1 {{parameter has incomplete class type '::Incomplete'}}
  static void consume(T value) {}
};
// expected-note@+1 {{in instantiation of member function '::Member<::Incomplete>::consume' requested here}}
auto member_pointer = &Member<Incomplete>::consume;

template <class T>
// expected-error@+1 {{return type is an incomplete class type '::Incomplete'}}
T dependent_return() { for (;;) {} }
// expected-note@+1 {{in instantiation of function template specialization 'dependent_return <::Incomplete>' requested here}}
template Incomplete dependent_return<Incomplete>();

template <class... T>
// expected-error@+1 {{parameter has incomplete class type '::Incomplete'}}
void pack(T... values) {}
// expected-note@+1 {{in instantiation of function template specialization 'pack <int, ::Incomplete>' requested here}}
template void pack<int, Incomplete>(int, Incomplete);
