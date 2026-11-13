// RUN: %cxx -emit-ir %s -o - | %filecheck %s

unsigned add(unsigned lhs, unsigned rhs, unsigned carry_in,
             unsigned* carry_out) {
  return __builtin_addc(lhs, rhs, carry_in, carry_out);
}

unsigned subtract(unsigned lhs, unsigned rhs, unsigned borrow_in,
                  unsigned* borrow_out) {
  return __builtin_subc(lhs, rhs, borrow_in, borrow_out);
}

// CHECK-LABEL: cxx.func @_Z8subtractjjjPj
// CHECK: %[[LHS:.+]] = arith.extui %{{.+}} : i32 to i65
// CHECK: %[[RHS:.+]] = arith.extui %{{.+}} : i32 to i65
// CHECK: %[[FIRST:.+]] = arith.subi %[[LHS]], %[[RHS]] : i65
// CHECK: %[[FIRST_RESULT:.+]] = arith.trunci %[[FIRST]] : i65 to i32
// CHECK: %[[FIRST_RESTORED:.+]] = arith.extui %[[FIRST_RESULT]] : i32 to i65
// CHECK: %[[FIRST_BORROW:.+]] = arith.cmpi ne, %[[FIRST]], %[[FIRST_RESTORED]] : i65
// CHECK: %[[FIRST_WIDE:.+]] = arith.extui %[[FIRST_RESULT]] : i32 to i65
// CHECK: %[[BORROW_IN:.+]] = arith.extui %{{.+}} : i32 to i65
// CHECK: %[[SECOND:.+]] = arith.subi %[[FIRST_WIDE]], %[[BORROW_IN]] : i65
// CHECK: %[[RESULT:.+]] = arith.trunci %[[SECOND]] : i65 to i32
// CHECK: %[[SECOND_RESTORED:.+]] = arith.extui %[[RESULT]] : i32 to i65
// CHECK: %[[SECOND_BORROW:.+]] = arith.cmpi ne, %[[SECOND]], %[[SECOND_RESTORED]] : i65
// CHECK: %[[BORROW:.+]] = arith.ori %[[FIRST_BORROW]], %[[SECOND_BORROW]] : i1
// CHECK: %[[STORED_BORROW:.+]] = arith.extui %[[BORROW]] : i1 to i32
// CHECK: cxx.store %[[STORED_BORROW]], %{{.+}} : i32, !{{.+}} align 4
// CHECK: cxx.store %[[RESULT]], %{{.+}} : i32, !{{.+}} align 4

// CHECK-LABEL: cxx.func @_Z3addjjjPj
// CHECK: %[[LHS:.+]] = arith.extui %{{.+}} : i32 to i65
// CHECK: %[[RHS:.+]] = arith.extui %{{.+}} : i32 to i65
// CHECK: %[[FIRST:.+]] = arith.addi %[[LHS]], %[[RHS]] : i65
// CHECK: %[[FIRST_RESULT:.+]] = arith.trunci %[[FIRST]] : i65 to i32
// CHECK: %[[FIRST_RESTORED:.+]] = arith.extui %[[FIRST_RESULT]] : i32 to i65
// CHECK: %[[FIRST_CARRY:.+]] = arith.cmpi ne, %[[FIRST]], %[[FIRST_RESTORED]] : i65
// CHECK: %[[FIRST_WIDE:.+]] = arith.extui %[[FIRST_RESULT]] : i32 to i65
// CHECK: %[[CARRY_IN:.+]] = arith.extui %{{.+}} : i32 to i65
// CHECK: %[[SECOND:.+]] = arith.addi %[[FIRST_WIDE]], %[[CARRY_IN]] : i65
// CHECK: %[[RESULT:.+]] = arith.trunci %[[SECOND]] : i65 to i32
// CHECK: %[[SECOND_RESTORED:.+]] = arith.extui %[[RESULT]] : i32 to i65
// CHECK: %[[SECOND_CARRY:.+]] = arith.cmpi ne, %[[SECOND]], %[[SECOND_RESTORED]] : i65
// CHECK: %[[CARRY:.+]] = arith.ori %[[FIRST_CARRY]], %[[SECOND_CARRY]] : i1
// CHECK: %[[STORED_CARRY:.+]] = arith.extui %[[CARRY]] : i1 to i32
// CHECK: cxx.store %[[STORED_CARRY]], %{{.+}} : i32, !{{.+}} align 4
// CHECK: cxx.store %[[RESULT]], %{{.+}} : i32, !{{.+}} align 4
