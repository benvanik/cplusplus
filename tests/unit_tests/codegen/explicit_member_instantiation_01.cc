// RUN: %cxx -emit-llvm %s -o - | %filecheck %s

struct Operations {
  template <class T> static T apply(T value) { return value + 1; }
};
template int Operations::apply(int);

template <class T> struct Container {
  static T apply(T value) { return value + 2; }
  static T external(T value) { return value + 3; }
  static T specialized(T value) { return value + 4; }
};
template int Container<int>::apply(int);
extern template int Container<int>::external(int);
template <> int Container<int>::specialized(int value) { return value + 5; }
template int Container<int>::specialized(int);

int entry(int value) { return Container<int>::external(value); }

// The definitions are emitted even without calls. The extern declaration
// suppresses only its selected member; the specialization keeps normal linkage.
// CHECK-DAG: define weak_odr i32 @_ZN10Operations5applyIiEET_S1_(i32
// CHECK-DAG: define weak_odr i32 @_ZN9ContainerIiE5applyEi(i32
// CHECK-DAG: define i32 @_ZN9ContainerIiE11specializedEi(i32
// CHECK-DAG: declare i32 @_ZN9ContainerIiE8externalEi(i32
