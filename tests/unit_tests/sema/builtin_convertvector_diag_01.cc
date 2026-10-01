// RUN: %cxx -verify -fsyntax-only %s

using Int2 = int __attribute__((ext_vector_type(2)));
using Int4 = int __attribute__((ext_vector_type(4)));

Int4 fromScalar(int value) {
  // expected-error@1 {{__builtin_convertvector requires source and destination vector types}}
  return __builtin_convertvector(value, Int4);
}

int toScalar(Int4 value) {
  // expected-error@1 {{__builtin_convertvector requires source and destination vector types}}
  return __builtin_convertvector(value, int);
}

Int2 changeLaneCount(Int4 value) {
  // expected-error@1 {{__builtin_convertvector requires the same number of elements}}
  return __builtin_convertvector(value, Int2);
}
