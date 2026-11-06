// RUN: %cxx -emit-ir %s -o - | %filecheck %s

using Int2 = int __attribute__((ext_vector_type(2)));
using Int3 = int __attribute__((ext_vector_type(3)));
using Int4 = int __attribute__((ext_vector_type(4)));

int scalar(Int4 value) { return value.z; }

Int2 reverse(Int4 value) { return value.yx; }

void assign(Int4& value, Int2 replacement) { value.yx = replacement; }

Int2 odd(Int3 value) { return value.odd; }

void assignHigh(Int3& value, Int2 replacement) { value.hi = replacement; }

// The padded lane in an odd-width high selection is never written.
// CHECK-LABEL: cxx.func @_Z10assignHighRDv3_iDv2_i
// CHECK: %[[HIGH_REPLACEMENT:.+]] = cxx.load {{.*}} -> vector<2xi32>
// CHECK: %[[THREE_LANES:.+]] = cxx.load {{.*}} -> vector<3xi32>
// CHECK: %[[HIGH_SOURCE_INDEX:.+]] = arith.index_castui %c0_i32
// CHECK: %[[HIGH_SOURCE:.+]] = vector.extract %[[HIGH_REPLACEMENT]][%[[HIGH_SOURCE_INDEX]]]
// CHECK: %[[HIGH_TARGET_INDEX:.+]] = arith.index_castui %c2_i32
// CHECK: %[[HIGH_RESULT:.+]] = vector.insert %[[HIGH_SOURCE]], %[[THREE_LANES]] [%[[HIGH_TARGET_INDEX]]]
// CHECK-NOT: vector.insert
// CHECK: cxx.store %[[HIGH_RESULT]]

// CHECK-LABEL: cxx.func @_Z3oddDv3_i
// CHECK: %[[ODD_SOURCE:.+]] = cxx.load {{.*}} -> vector<3xi32>
// CHECK: %[[ODD_PADDING:.+]] = cxx.undef : vector<3xi32>
// CHECK: %[[ODD_RESULT:.+]] = vector.shuffle %[[ODD_SOURCE]], %[[ODD_PADDING]] [1, 3]

// CHECK-LABEL: cxx.func @_Z6assignRDv4_iDv2_i
// CHECK: %[[REPLACEMENT:.+]] = cxx.load {{.*}} -> vector<2xi32>
// CHECK: %[[ORIGINAL:.+]] = cxx.load {{.*}} -> vector<4xi32>
// CHECK: %[[SOURCE_ZERO:.+]] = arith.index_castui %c0_i32
// CHECK: %[[FIRST:.+]] = vector.extract %[[REPLACEMENT]][%[[SOURCE_ZERO]]]
// CHECK: %[[TARGET_ONE:.+]] = arith.index_castui %c1_i32
// CHECK: %[[FIRST_INSERT:.+]] = vector.insert %[[FIRST]], %[[ORIGINAL]] [%[[TARGET_ONE]]]
// CHECK: %[[SOURCE_ONE:.+]] = arith.index_castui %c1_i32_0
// CHECK: %[[SECOND:.+]] = vector.extract %[[REPLACEMENT]][%[[SOURCE_ONE]]]
// CHECK: %[[TARGET_ZERO:.+]] = arith.index_castui %c0_i32_1
// CHECK: %[[ASSIGN_RESULT:.+]] = vector.insert %[[SECOND]], %[[FIRST_INSERT]] [%[[TARGET_ZERO]]]

// CHECK-LABEL: cxx.func @_Z7reverseDv4_i
// CHECK: %[[REVERSE_SOURCE:.+]] = cxx.load {{.*}} -> vector<4xi32>
// CHECK: %[[REVERSE_PADDING:.+]] = cxx.undef : vector<4xi32>
// CHECK: %[[REVERSE_RESULT:.+]] = vector.shuffle %[[REVERSE_SOURCE]], %[[REVERSE_PADDING]] [1, 0]

// CHECK-LABEL: cxx.func @_Z6scalarDv4_i
// CHECK: %[[SCALAR_SOURCE:.+]] = cxx.load {{.*}} -> vector<4xi32>
// CHECK: %[[SCALAR_INDEX:.+]] = arith.index_castui %c2_i32
// CHECK: %[[SCALAR_RESULT:.+]] = vector.extract %[[SCALAR_SOURCE]][%[[SCALAR_INDEX]]]
