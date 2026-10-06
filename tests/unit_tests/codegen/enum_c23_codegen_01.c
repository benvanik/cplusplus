// RUN: %cxx -std=c23 -xc -emit-llvm %s -o - | %filecheck %s

enum Small { small = 1 };
enum Wide { wide = 1ULL << 40 };
enum Byte : unsigned char { byte = 255 };
enum __attribute__((packed)) Packed { packed = 255 };

enum Small small_object = small;
enum Wide wide_object = wide;
enum Byte byte_object = byte;
enum Packed packed_object = packed;

// CHECK-DAG: @small_object = global i32 1, align 4
// CHECK-DAG: @wide_object = global i64 1099511627776, align 8
// CHECK-DAG: @byte_object = global i8 -1, align 1
// CHECK-DAG: @packed_object = global i8 -1, align 1
