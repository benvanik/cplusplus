// RUN: %cxx -emit-ir %s -o - | %filecheck %s

struct Leaf {
  char bytes[3];
  int value;
};

struct Middle {
  char lead;
  Leaf leaves[3];
};

struct Outer {
  short prefix;
  Middle middle[2];
};

auto compound_offset() {
  return __builtin_offsetof(Outer, middle[1].leaves[2].value);
}

// CHECK-LABEL: cxx.func @_Z15compound_offsetv
// CHECK: arith.constant 56 : i32
// CHECK-NOT: cxx.builtin.call "__builtin_offsetof"
