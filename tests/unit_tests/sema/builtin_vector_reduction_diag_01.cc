// RUN: %cxx -verify -fsyntax-only %s

using Int4 = int __attribute__((ext_vector_type(4)));
using Float4 = float __attribute__((ext_vector_type(4)));

int noArguments() {
  // expected-error@1 {{'__builtin_reduce_and' requires one argument}}
  return __builtin_reduce_and();
}

int tooManyArguments(Int4 value) {
  // expected-error@1 {{'__builtin_reduce_or' requires one argument}}
  return __builtin_reduce_or(value, value);
}

int scalarArgument(int value) {
  // expected-error@1 {{argument to '__builtin_reduce_and' must be an integer vector}}
  return __builtin_reduce_and(value);
}

float floatingArgument(Float4 value) {
  // expected-error@1 {{argument to '__builtin_reduce_or' must be an integer vector}}
  return __builtin_reduce_or(value);
}
