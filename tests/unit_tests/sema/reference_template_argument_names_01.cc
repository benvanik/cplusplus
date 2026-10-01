// RUN: %cxx -fsyntax-only -dump-symbols %s | %filecheck %s

constexpr int left = 1;
constexpr int right = 1;
constexpr int values[2] = {1, 1};

namespace scope {
constexpr int value = 1;
}  // namespace scope

struct Holder {
  static constexpr int value = 1;
};

template <const int&>
struct Ref {};

template <const int*>
struct Pointer {};

Ref<left> refLeft;
Ref<right> refRight;
Ref<values[1]> refElement;
Ref<scope::value> refNamespace;
Ref<Holder::value> refMember;
Pointer<&left> pointerLeft;
Pointer<&values[1]> pointerElement;

// CHECK:      [specializations]
// CHECK-NEXT:   class Ref<left>
// CHECK:        class Ref<right>
// CHECK:        class Ref<values[1]>
// CHECK:        class Ref<scope::value>
// CHECK:        class Ref<Holder::value>
// CHECK:      [specializations]
// CHECK-NEXT:   class Pointer<&left>
// CHECK:        class Pointer<&values[1]>
