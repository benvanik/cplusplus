// RUN: %cxx -emit-ir %s -o - | %filecheck %s

__float8_e4m3fn add_e4m3fn(__float8_e4m3fn left,
                           __float8_e4m3fn right) {
  return left + right;
}

__float8_e5m2 multiply_e5m2(__float8_e5m2 left, __float8_e5m2 right) {
  return left * right;
}

__float8_e4m3fn to_e4m3fn(float value) { return value; }
__float8_e4m3fn double_to_e4m3fn(double value) { return value; }
float from_e4m3fn(__float8_e4m3fn value) { return value; }

__float8_e4m3fn from_signed(int value) { return value; }
__float8_e4m3fn from_unsigned(unsigned value) { return value; }
__float8_e4m3fn construct_signed(int value) {
  return __float8_e4m3fn(value);
}
__float8_e5m2 construct_unsigned_e5m2(unsigned value) {
  return __float8_e5m2(value);
}

__float8_e5m2 to_e5m2(__float8_e4m3fn value) {
  return (__float8_e5m2)value;
}

__float8_e4m3fn to_e4m3fn(__float8_e5m2 value) {
  return (__float8_e4m3fn)value;
}

void increment(__float8_e4m3fn* value) {
  ++*value;
  (*value)++;
}

using E4M3FN4 = __float8_e4m3fn __attribute__((ext_vector_type(4)));
E4M3FN4 add_vector(E4M3FN4 left, E4M3FN4 right) { return left + right; }

// CHECK-LABEL: cxx.func @_Z10add_vectorDv4_u15__float8_e4m3fnS_
// CHECK: arith.extf {{.*}} : vector<4xf8E4M3FN> to vector<4xf32>
// CHECK: arith.addf {{.*}} : vector<4xf32>
// CHECK: arith.constant 4.480000e+02 : f32
// CHECK: arith.constant -4.480000e+02 : f32
// CHECK: vector.broadcast {{.*}} : f32 to vector<4xf32>
// CHECK: vector.broadcast {{.*}} : f32 to vector<4xf32>
// CHECK: arith.cmpf ogt, {{.*}} : vector<4xf32>
// CHECK: arith.select {{.*}} : vector<4xi1>, vector<4xf32>
// CHECK: arith.cmpf olt, {{.*}} : vector<4xf32>
// CHECK: arith.select {{.*}} : vector<4xi1>, vector<4xf32>
// CHECK: arith.truncf {{.*}} : vector<4xf32> to vector<4xf8E4M3FN>

// CHECK-LABEL: cxx.func @_Z9incrementPu15__float8_e4m3fn
// CHECK: arith.addf {{.*}} : f32
// CHECK: arith.cmpf ogt, {{.*}} : f32
// CHECK: arith.cmpf olt, {{.*}} : f32
// CHECK: arith.truncf {{.*}} : f32 to f8E4M3FN
// CHECK: arith.addf {{.*}} : f32
// CHECK: arith.cmpf ogt, {{.*}} : f32
// CHECK: arith.cmpf olt, {{.*}} : f32
// CHECK: arith.truncf {{.*}} : f32 to f8E4M3FN

// CHECK-LABEL: cxx.func @_Z9to_e4m3fnu13__float8_e5m2
// CHECK: arith.extf {{.*}} : f8E5M2 to f32
// CHECK: arith.cmpf ogt, {{.*}} : f32
// CHECK: arith.cmpf olt, {{.*}} : f32
// CHECK: arith.truncf {{.*}} : f32 to f8E4M3FN

// CHECK-LABEL: cxx.func @_Z7to_e5m2u15__float8_e4m3fn
// CHECK: arith.extf {{.*}} : f8E4M3FN to f32
// CHECK-NEXT: arith.truncf {{.*}} : f32 to f8E5M2

// CHECK-LABEL: cxx.func @_Z23construct_unsigned_e5m2j
// CHECK: arith.uitofp {{.*}} : i32 to f8E5M2

// CHECK-LABEL: cxx.func @_Z16construct_signedi
// CHECK: arith.sitofp {{.*}} : i32 to f32
// CHECK: arith.cmpf ogt, {{.*}} : f32
// CHECK: arith.cmpf olt, {{.*}} : f32
// CHECK: arith.truncf {{.*}} : f32 to f8E4M3FN

// CHECK-LABEL: cxx.func @_Z13from_unsignedj
// CHECK: arith.uitofp {{.*}} : i32 to f32
// CHECK: arith.cmpf ogt, {{.*}} : f32
// CHECK: arith.cmpf olt, {{.*}} : f32
// CHECK: arith.truncf {{.*}} : f32 to f8E4M3FN

// CHECK-LABEL: cxx.func @_Z11from_signedi
// CHECK: arith.sitofp {{.*}} : i32 to f32
// CHECK: arith.cmpf ogt, {{.*}} : f32
// CHECK: arith.cmpf olt, {{.*}} : f32
// CHECK: arith.truncf {{.*}} : f32 to f8E4M3FN

// CHECK-LABEL: cxx.func @_Z11from_e4m3fnu15__float8_e4m3fn
// CHECK: arith.extf {{.*}} : f8E4M3FN to f32

// CHECK-LABEL: cxx.func @_Z16double_to_e4m3fnd
// CHECK-NOT: arith.truncf {{.*}} : f64 to f32
// CHECK: arith.constant 4.480000e+02 : f64
// CHECK: arith.constant -4.480000e+02 : f64
// CHECK: arith.cmpf ogt, {{.*}} : f64
// CHECK: arith.cmpf olt, {{.*}} : f64
// CHECK: arith.truncf {{.*}} : f64 to f8E4M3FN

// CHECK-LABEL: cxx.func @_Z9to_e4m3fnf
// CHECK: arith.cmpf ogt, {{.*}} : f32
// CHECK: arith.cmpf olt, {{.*}} : f32
// CHECK: arith.truncf {{.*}} : f32 to f8E4M3FN

// CHECK-LABEL: cxx.func @_Z13multiply_e5m2u13__float8_e5m2u13__float8_e5m2
// CHECK: arith.extf {{.*}} : f8E5M2 to f32
// CHECK: arith.mulf {{.*}} : f32
// CHECK: arith.truncf {{.*}} : f32 to f8E5M2

// CHECK-LABEL: cxx.func @_Z10add_e4m3fnu15__float8_e4m3fnu15__float8_e4m3fn
// CHECK: arith.extf {{.*}} : f8E4M3FN to f32
// CHECK: arith.addf {{.*}} : f32
// CHECK: arith.constant 4.480000e+02 : f32
// CHECK: arith.constant -4.480000e+02 : f32
// CHECK: arith.cmpf ogt, {{.*}} : f32
// CHECK: arith.select {{.*}} : f32
// CHECK: arith.cmpf olt, {{.*}} : f32
// CHECK: arith.select {{.*}} : f32
// CHECK: arith.truncf {{.*}} : f32 to f8E4M3FN
