// RUN: %cxx -verify -fsyntax-only %s

// clang-format off

using Int4 = int __attribute__((ext_vector_type(4)));
using Complex = __complex__ double;

// expected-note@+1 {{candidate function not viable: no known conversion from '' to 'int __attribute__((ext_vector_type(4)))&' for argument 1}}
void consume_vector(Int4& value) {}

// expected-note@+1 {{candidate function not viable: no known conversion from '' to '_Complex double&' for argument 1}}
void consume_complex(Complex& value) {}

void reject_mutable_references() {
  // expected-error@+1 {{no matching function for call to 'consume_vector'}}
  consume_vector({1, 2, 3, 4});
  // expected-error@+1 {{no matching function for call to 'consume_complex'}}
  consume_complex({1.0, 2.0});
}
