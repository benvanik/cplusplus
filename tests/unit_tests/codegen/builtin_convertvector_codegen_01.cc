// RUN: %cxx -emit-ir %s -o - | %filecheck %s

using Int4 = int __attribute__((ext_vector_type(4)));
using UInt4 = unsigned __attribute__((ext_vector_type(4)));
using Short4 = short __attribute__((ext_vector_type(4)));
using SByte4 = signed char __attribute__((ext_vector_type(4)));
using UByte4 = unsigned char __attribute__((ext_vector_type(4)));
using Float4 = float __attribute__((ext_vector_type(4)));
using Bool4 = bool __attribute__((ext_vector_type(4)));
using Half4 = _Float16 __attribute__((ext_vector_type(4)));
using BFloat4 = __bf16 __attribute__((ext_vector_type(4)));
using Float8x4 = __float8_e4m3fn __attribute__((ext_vector_type(4)));

Float4 unsignedToFloat(UByte4 value) {
  return __builtin_convertvector(value, Float4);
}

Int4 floatToSigned(Float4 value) {
  return __builtin_convertvector(value, Int4);
}

UInt4 floatToUnsigned(Float4 value) {
  return __builtin_convertvector(value, UInt4);
}

Int4 signExtend(SByte4 value) {
  return __builtin_convertvector(value, Int4);
}

UInt4 zeroExtend(UByte4 value) {
  return __builtin_convertvector(value, UInt4);
}

Short4 truncate(Int4 value) {
  return __builtin_convertvector(value, Short4);
}

Bool4 toBool(Float4 value) {
  return __builtin_convertvector(value, Bool4);
}

Int4 fromBool(Bool4 value) {
  return __builtin_convertvector(value, Int4);
}

Half4 toHalf(BFloat4 value) {
  return __builtin_convertvector(value, Half4);
}

BFloat4 toBFloat(Half4 value) {
  return __builtin_convertvector(value, BFloat4);
}

Float8x4 toFloat8(Int4 value) {
  return __builtin_convertvector(value, Float8x4);
}

// CHECK-LABEL: cxx.func @_Z8toFloat8Dv4_i
// CHECK: arith.sitofp {{.*}} : vector<4xi32> to vector<4xf32>
// CHECK: arith.cmpf ogt, {{.*}} : vector<4xf32>
// CHECK: arith.cmpf olt, {{.*}} : vector<4xf32>
// CHECK: arith.truncf {{.*}} : vector<4xf32> to vector<4xf8E4M3FN>

// CHECK-LABEL: cxx.func @_Z8toBFloatDv4_DF16_
// CHECK: arith.extf {{.*}} : vector<4xf16> to vector<4xf32>
// CHECK-NEXT: arith.truncf {{.*}} : vector<4xf32> to vector<4xbf16>

// CHECK-LABEL: cxx.func @_Z6toHalfDv4_DF16b
// CHECK: arith.extf {{.*}} : vector<4xbf16> to vector<4xf32>
// CHECK-NEXT: arith.truncf {{.*}} : vector<4xf32> to vector<4xf16>

// CHECK-LABEL: cxx.func @_Z8fromBoolDv4_b
// CHECK: arith.extui {{.*}} : vector<4xi1> to vector<4xi32>

// CHECK-LABEL: cxx.func @_Z6toBoolDv4_f
// CHECK: arith.cmpf une, {{.*}} : vector<4xf32>

// CHECK-LABEL: cxx.func @_Z8truncateDv4_i
// CHECK: arith.trunci {{.*}} : vector<4xi32> to vector<4xi16>

// CHECK-LABEL: cxx.func @_Z10zeroExtendDv4_h
// CHECK: arith.extui {{.*}} : vector<4xi8> to vector<4xi32>

// CHECK-LABEL: cxx.func @_Z10signExtendDv4_a
// CHECK: arith.extsi {{.*}} : vector<4xi8> to vector<4xi32>

// CHECK-LABEL: cxx.func @_Z15floatToUnsignedDv4_f
// CHECK: arith.fptoui {{.*}} : vector<4xf32> to vector<4xi32>

// CHECK-LABEL: cxx.func @_Z13floatToSignedDv4_f
// CHECK: arith.fptosi {{.*}} : vector<4xf32> to vector<4xi32>

// CHECK-LABEL: cxx.func @_Z15unsignedToFloatDv4_h
// CHECK: arith.uitofp {{.*}} : vector<4xi8> to vector<4xf32>
