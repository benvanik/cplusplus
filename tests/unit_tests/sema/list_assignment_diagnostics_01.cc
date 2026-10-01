// RUN: %cxx -verify -fsyntax-only %s

using Int4 = int __attribute__((ext_vector_type(4)));

void vector_element(Int4* output, const int* pointer) {
  // expected-error@+1 {{cannot initialize vector element of type 'int' with expression of type 'const int*'}}
  *output = {pointer};
}

void complex_element(_Complex int* output, const int* pointer) {
  // expected-error@+1 {{cannot initialize complex element of type 'int' with expression of type 'const int*'}}
  *output = {pointer, 0};
}

void nested_element(Int4* output, const int* pointer) {
  // expected-error@+1 {{cannot initialize type 'int' with expression of type 'const int*'}}
  *output = {{pointer}};
}

void scalar_element(int* output, const int* pointer) {
  // expected-error@+1 {{cannot initialize type 'int' with expression of type 'const int*'}}
  *output = {pointer};
}

void array_element(const int* pointer) {
  // expected-error@+1 {{cannot initialize array element of type 'int' with expression of type 'const int*'}}
  int values[] = {pointer};
}

void independent_errors(int* output, const int* pointer) {
  // expected-error@+1 {{cannot initialize type 'int' with expression of type 'const int*'}}
  *output = {pointer};
  // expected-error@+1 {{cannot assign expression of type 'const int*' to 'int'}}
  *output = pointer;
  // expected-error@+1 {{cannot assign to an rvalue of type 'int'}}
  1 = {2};
}

void scalar_narrowing(int* output, float value) {
  // expected-error@+1 {{narrowing conversion from 'float' to 'int' in braced-init-list}}
  *output = {value};
}

void scalar_excess(int* output) {
  // expected-error@+1 {{excess elements in scalar initializer}}
  *output = {1, 2};
}

constexpr int valid_assignments() {
  int scalar = 0;
  scalar = {3};
  Int4 vector{};
  vector = {scalar, 7};
  return vector[0] + vector[1] + vector[2] + vector[3];
}
static_assert(valid_assignments() == 10);
