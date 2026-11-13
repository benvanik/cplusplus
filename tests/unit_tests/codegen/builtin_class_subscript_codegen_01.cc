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

struct __attribute__((packed)) Packed {
  char prefix;
  int values[2];
};
int packed_commuted(Index index, Packed* packed) {
  return index[packed->values];
}

struct Wrapper {
  int& operator[](char (&)[2]);
};
int overloaded(Wrapper& wrapper, char (&bytes)[2]) {
  return wrapper[bytes];
}

// Preserve written evaluation order while emitting the pointer and index in
// their semantic roles.
// CHECK-LABEL: cxx.func @_Z10overloadedR7WrapperRA2_c
// CHECK: %[[REFERENCE:.+]] = cxx.call @_ZN7WrapperixERA2_c
// CHECK-NEXT: cxx.load %[[REFERENCE]] {{.*}} align 4
// CHECK-LABEL: cxx.func @_Z15packed_commuted5IndexP6Packed
// CHECK: %[[ELEMENT:.+]] = cxx.subscript
// CHECK-NEXT: cxx.load %[[ELEMENT]] {{.*}} align 1
// CHECK-LABEL: cxx.func @_Z8commuted5Index8Elements
// CHECK: %[[INDEX:.+]] = cxx.call @_ZNK5IndexcviEv
// CHECK-NEXT: %[[ELEMENTS:.+]] = cxx.call @_ZNK8ElementscvPiEv
// CHECK-NEXT: cxx.ptradd %[[ELEMENTS]], %[[INDEX]]
// CHECK-LABEL: cxx.func @_Z6direct8Elements5Index
// CHECK: %[[ELEMENTS:.+]] = cxx.call @_ZNK8ElementscvPiEv
// CHECK-NEXT: %[[INDEX:.+]] = cxx.call @_ZNK5IndexcviEv
// CHECK-NEXT: cxx.ptradd %[[ELEMENTS]], %[[INDEX]]
