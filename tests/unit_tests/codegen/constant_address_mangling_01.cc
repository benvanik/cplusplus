// RUN: %cxx --target=x86_64-unknown-linux-gnu -std=c++23 -emit-llvm %s -o - | %filecheck %s

constexpr unsigned table[2][2] = {{3, 5}, {7, 11}};
inline constexpr unsigned shared_table[2][2] = {{13, 17}, {19, 23}};

template <const unsigned& value>
unsigned read() {
  return value;
}

unsigned read_zero_one() { return read<table[0][1]>(); }
unsigned read_one_zero() { return read<table[1][0]>(); }
unsigned read_shared() { return read<shared_table[0][1]>(); }

template <const unsigned* value>
unsigned read_pointer() {
  return *value;
}

unsigned read_pointer_zero_one() { return read_pointer<&table[0][1]>(); }
unsigned read_pointer_one_zero() { return read_pointer<&table[1][0]>(); }
unsigned read_pointer_shared() {
  return read_pointer<&shared_table[0][1]>();
}

static unsigned internal_increment(unsigned value) { return value + 1; }
inline unsigned shared_increment(unsigned value) { return value + 2; }

template <unsigned (*function)(unsigned)>
unsigned invoke(unsigned value) {
  return function(value);
}

unsigned invoke_internal(unsigned value) {
  return invoke<internal_increment>(value);
}
unsigned invoke_shared(unsigned value) {
  return invoke<shared_increment>(value);
}

struct Descriptor {
  const unsigned* pointer;
};

template <Descriptor descriptor>
unsigned read_descriptor() {
  return descriptor.pointer ? *descriptor.pointer : 0;
}

unsigned read_internal_descriptor() {
  return read_descriptor<Descriptor{&table[0][0]}>();
}
unsigned read_shared_descriptor() {
  return read_descriptor<Descriptor{&shared_table[0][0]}>();
}

unsigned read_null_descriptor() { return read_descriptor<Descriptor{nullptr}>(); }

template <const unsigned*... pointers>
unsigned count_values() {
  return sizeof...(pointers);
}

unsigned count_internal_values() {
  return count_values<&table[0][0], &shared_table[0][0]>();
}
unsigned count_shared_values() {
  return count_values<&shared_table[0][0], &shared_table[0][1]>();
}

// CHECK-DAG: define internal i32 @_Z4readIXsoKjL_ZL5tableE4EEEjv()
// CHECK-DAG: define internal i32 @_Z4readIXsoKjL_ZL5tableE8EEEjv()
// CHECK-DAG: define internal i32 @_Z12read_pointerIXadsoKjL_ZL5tableE4EEEjv()
// CHECK-DAG: define internal i32 @_Z12read_pointerIXadsoKjL_ZL5tableE8EEEjv()
// CHECK-DAG: define linkonce_odr i32 @_Z4readIXsoKjL_Z12shared_tableE4EEEjv()
// CHECK-DAG: define linkonce_odr i32 @_Z12read_pointerIXadsoKjL_Z12shared_tableE4EEEjv()
// CHECK-DAG: define internal i32 @_Z6invokeIXadL_ZL18internal_incrementjEEEjj(i32
// CHECK-DAG: define linkonce_odr i32 @_Z6invokeIXadL_Z16shared_incrementjEEEjj(i32
// CHECK-DAG: define internal i32 @_Z15read_descriptorIXtl10DescriptoradsoKjL_ZL5tableEEEEEjv()
// CHECK-DAG: define linkonce_odr i32 @_Z15read_descriptorIXtl10DescriptoradsoKjL_Z12shared_tableEEEEEjv()
// CHECK-DAG: define linkonce_odr i32 @_Z15read_descriptorIXtl10DescriptorEEEjv()
// CHECK-DAG: define internal i32 @_Z12count_valuesIJXadsoKjL_ZL5tableEEEXadsoS0_L_Z12shared_tableEEEEEjv()
// CHECK-DAG: define linkonce_odr i32 @_Z12count_valuesIJXadsoKjL_Z12shared_tableEEEXadsoS0_L_Z12shared_tableE4EEEEjv()
