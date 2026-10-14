// RUN: %cxx -emit-ir %s -o - | %filecheck %s

extern "C" void observe(int);

struct Index {
  int value;
  operator int() const {
    observe(1);
    return value;
  }
};

struct Elements {
  int* value;
  operator int*() const {
    observe(2);
    return value;
  }
};

int direct(Elements elements, Index index) { return elements[index]; }
int commuted(Index index, Elements elements) { return index[elements]; }

// Preserve the written evaluation order while emitting the pointer and index
// operands in their semantic roles.
// CHECK-LABEL: cxx.func @_Z8commuted5Index8Elements
// CHECK: %[[INDEX:.+]] = cxx.call @_ZNK5IndexcviEv
// CHECK-NEXT: %[[ELEMENTS:.+]] = cxx.call @_ZNK8ElementscvPiEv
// CHECK-NEXT: cxx.ptradd %[[ELEMENTS]], %[[INDEX]]
// CHECK-LABEL: cxx.func @_Z6direct8Elements5Index
// CHECK: %[[ELEMENTS:.+]] = cxx.call @_ZNK8ElementscvPiEv
// CHECK-NEXT: %[[INDEX:.+]] = cxx.call @_ZNK5IndexcviEv
// CHECK-NEXT: cxx.ptradd %[[ELEMENTS]], %[[INDEX]]
