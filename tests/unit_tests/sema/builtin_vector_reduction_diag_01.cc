// RUN: %cxx -verify -fsyntax-only %s

using Int4 = int __attribute__((ext_vector_type(4)));
using Float4 = float __attribute__((ext_vector_type(4)));

int noArguments() {
  // expected-error@+1 {{'__builtin_reduce_and' takes exactly one argument}}
  return __builtin_reduce_and();
}

int tooManyArguments(Int4 value) {
  // expected-error@+1 {{'__builtin_reduce_or' takes exactly one argument}}
  return __builtin_reduce_or(value, value);
}

int scalarArgument(int value) {
  // expected-error@+1 {{the operand of '__builtin_reduce_and' must be a vector of integers (was 'int')}}
  return __builtin_reduce_and(value);
}

float floatingArgument(Float4 value) {
  // expected-error@+1 {{the operand of '__builtin_reduce_or' must be a vector of integers (was 'float __attribute__((ext_vector_type(4)))')}}
  return __builtin_reduce_or(value);
}
