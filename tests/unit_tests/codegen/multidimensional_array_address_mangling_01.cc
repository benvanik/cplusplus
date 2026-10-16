// RUN: %cxx --target=x86_64-unknown-linux-gnu -std=c++23 -emit-llvm %s -o - | %filecheck %s

constexpr unsigned table[2][2] = {{3, 5}, {7, 11}};

template <const unsigned& value>
unsigned read() {
  return value;
}

unsigned read_zero_one() { return read<table[0][1]>(); }
unsigned read_one_zero() { return read<table[1][0]>(); }

template <const unsigned* value>
unsigned read_pointer() {
  return *value;
}

unsigned read_pointer_zero_one() { return read_pointer<&table[0][1]>(); }
unsigned read_pointer_one_zero() { return read_pointer<&table[1][0]>(); }

// CHECK-DAG: define linkonce_odr i32 @_Z4readIXsoKjL_ZL5tableE4EEEjv()
// CHECK-DAG: define linkonce_odr i32 @_Z4readIXsoKjL_ZL5tableE8EEEjv()
// CHECK-DAG: define linkonce_odr i32 @_Z12read_pointerIXadsoKjL_ZL5tableE4EEEjv()
// CHECK-DAG: define linkonce_odr i32 @_Z12read_pointerIXadsoKjL_ZL5tableE8EEEjv()
