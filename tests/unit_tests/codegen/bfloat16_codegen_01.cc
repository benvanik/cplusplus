// RUN: %cxx -emit-ir %s -o - | %filecheck %s

__bf16 add(__bf16 left, __bf16 right) { return left + right; }

_Float16 to_binary(__bf16 value) { return value; }

__bf16 to_bfloat(_Float16 value) { return value; }

_Float16 construct_binary(__bf16 value) { return _Float16(value); }

__bf16 construct_bfloat(_Float16 value) { return __bf16(value); }

_Float16 mixed(__bf16 left, _Float16 right) { return left + right; }

void increment(__bf16* value) {
  ++*value;
  (*value)++;
}

// CHECK-LABEL: cxx.func @_Z9incrementPDF16b
// CHECK: arith.constant 1.000000e+00 : bf16
// CHECK: arith.addf {{.*}} : bf16
// CHECK: arith.constant 1.000000e+00 : bf16
// CHECK: arith.addf {{.*}} : bf16

// CHECK-LABEL: cxx.func @_Z5mixedDF16bDF16_
// CHECK: arith.extf {{.*}} : bf16 to f32
// CHECK-NEXT: arith.truncf {{.*}} : f32 to f16
// CHECK: arith.addf {{.*}} : f16

// CHECK-LABEL: cxx.func @_Z16construct_bfloatDF16_
// CHECK: arith.extf {{.*}} : f16 to f32
// CHECK-NEXT: arith.truncf {{.*}} : f32 to bf16

// CHECK-LABEL: cxx.func @_Z16construct_binaryDF16b
// CHECK: arith.extf {{.*}} : bf16 to f32
// CHECK-NEXT: arith.truncf {{.*}} : f32 to f16

// CHECK-LABEL: cxx.func @_Z9to_bfloatDF16_
// CHECK: arith.extf {{.*}} : f16 to f32
// CHECK-NEXT: arith.truncf {{.*}} : f32 to bf16

// CHECK-LABEL: cxx.func @_Z9to_binaryDF16b
// CHECK: arith.extf {{.*}} : bf16 to f32
// CHECK-NEXT: arith.truncf {{.*}} : f32 to f16

// CHECK-LABEL: cxx.func @_Z3addDF16bDF16b
// CHECK: arith.addf {{.*}} : bf16
