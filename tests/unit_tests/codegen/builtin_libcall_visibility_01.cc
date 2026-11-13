// RUN: %cxx -emit-ir %s -o - | %filecheck %s

double builtin_root(double value) { return __builtin_sqrt(value); }
double another_builtin_root(double value) { return __builtin_sqrt(value); }
double builtin_cube(double value) { return __builtin_cbrt(value); }

extern "C" double sqrt(double value);
double source_root(double value) { return sqrt(value); }

// CHECK-LABEL: cxx.func @_Z11source_rootd(
// CHECK: cxx.call @sqrt({{%[0-9]+}}) : (f64) -> f64
// CHECK: cxx.func @cbrt(f64) -> f64
// CHECK-LABEL: cxx.func @_Z12builtin_cubed(
// CHECK: cxx.call @cbrt({{%[0-9]+}}) : (f64) -> f64
// CHECK-LABEL: cxx.func @_Z20another_builtin_rootd(
// CHECK: cxx.call @sqrt({{%[0-9]+}}) : (f64) -> f64
// CHECK: cxx.func @sqrt(f64) -> f64
// CHECK-LABEL: cxx.func @_Z12builtin_rootd(
// CHECK: cxx.call @sqrt({{%[0-9]+}}) : (f64) -> f64
