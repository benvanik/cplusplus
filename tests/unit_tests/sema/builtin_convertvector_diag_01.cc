// RUN: %cxx -verify -fsyntax-only %s

using Int2 = int __attribute__((ext_vector_type(2)));
using Int4 = int __attribute__((ext_vector_type(4)));

Int4 fromScalar(int value) {
  // expected-error@+1 {{the operand of '__builtin_convertvector' must be a vector (was 'int')}}
  return __builtin_convertvector(value, Int4);
}

int toScalar(Int4 value) {
  // expected-error@+1 {{the target of '__builtin_convertvector' must be a vector type (was 'int')}}
  return __builtin_convertvector(value, int);
}

Int2 changeLaneCount(Int4 value) {
  // expected-error@+1 {{'__builtin_convertvector' requires vectors with the same number of elements ('int __attribute__((ext_vector_type(4)))' and 'int __attribute__((ext_vector_type(2)))')}}
  return __builtin_convertvector(value, Int2);
}
