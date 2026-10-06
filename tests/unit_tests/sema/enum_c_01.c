// RUN: %cxx -verify -fsyntax-only -dump-symbols -xc %s | %filecheck %s

enum X {
  A,
  B = 1,
  C = B,
  D,
};

static_assert(A == 0);
static_assert(B == 1);
static_assert(C == 1);
static_assert(D == 2);

enum X x;

//      CHECK:namespace
// CHECK-NEXT:  enum X : unsigned int
// CHECK-NEXT:  enumerator int A = 0
// CHECK-NEXT:  enumerator int B = 1
// CHECK-NEXT:  enumerator int C = 1
// CHECK-NEXT:  enumerator int D = 2
// CHECK-NEXT:  variable extern "C" ::X x
