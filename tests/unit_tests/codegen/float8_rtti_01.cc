// RUN: %cxx -emit-ir %s -o - | %filecheck %s

namespace std {
class type_info {};
}  // namespace std

const std::type_info& e4_type_info() { return typeid(__float8_e4m3fn); }
const std::type_info& e5_type_info() { return typeid(__float8_e5m2); }

// The C++ ABI runtime cannot supply type-info objects for CXX's vendor types.
// Emit each object and its name with ODR linkage in the translation unit.

// CHECK: cxx.global @_ZTIu13__float8_e5m2 linkonce_odr constant
// CHECK: cxx.address_of @_ZTVN10__cxxabiv123__fundamental_type_infoE
// CHECK: cxx.global @_ZTSu13__float8_e5m2 linkonce_odr constant
// CHECK-LABEL: cxx.func @_Z12e5_type_infov
// CHECK: cxx.address_of @_ZTIu13__float8_e5m2

// CHECK: cxx.global @_ZTIu15__float8_e4m3fn linkonce_odr constant
// CHECK: cxx.address_of @_ZTVN10__cxxabiv123__fundamental_type_infoE
// CHECK: cxx.global @_ZTSu15__float8_e4m3fn linkonce_odr constant
// CHECK-LABEL: cxx.func @_Z12e4_type_infov
// CHECK: cxx.address_of @_ZTIu15__float8_e4m3fn
