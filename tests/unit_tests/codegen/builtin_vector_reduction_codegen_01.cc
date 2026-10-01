// RUN: %cxx -emit-ir %s -o - | %filecheck %s

using Bool4 = bool __attribute__((ext_vector_type(4)));
using Int4 = int __attribute__((ext_vector_type(4)));

int sum(Int4 value) { return __builtin_reduce_add(value); }

bool all(Bool4 value) { return __builtin_reduce_and(value); }

int product(Int4 value) { return __builtin_reduce_mul(value); }

bool any(Bool4 value) { return __builtin_reduce_or(value); }

int bitwiseAnd(Int4 value) { return __builtin_reduce_and(value); }

int bitwiseOr(Int4 value) { return __builtin_reduce_or(value); }

int parity(Int4 value) { return __builtin_reduce_xor(value); }

// CHECK-LABEL: cxx.func @_Z6parityDv4_i
// CHECK: vector.reduction <xor>

// CHECK-LABEL: cxx.func @_Z9bitwiseOrDv4_i
// CHECK: vector.reduction <or>

// CHECK-LABEL: cxx.func @_Z10bitwiseAndDv4_i
// CHECK: vector.reduction <and>

// CHECK-LABEL: cxx.func @_Z3anyDv4_b
// CHECK: vector.reduction <or>

// CHECK-LABEL: cxx.func @_Z7productDv4_i
// CHECK: vector.reduction <mul>

// CHECK-LABEL: cxx.func @_Z3allDv4_b
// CHECK: vector.reduction <and>

// CHECK-LABEL: cxx.func @_Z3sumDv4_i
// CHECK: vector.reduction <add>
