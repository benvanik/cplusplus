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

struct Aggregate {
  int member;
  int values[2];
};

struct Base {
  int value;
};
struct Left : Base {};
struct Right : Base {};
struct Repeated : Left, Right {};
extern Repeated repeated;

struct VirtualLeft : virtual Base {};
struct VirtualRight : virtual Base {};
struct Diamond : VirtualLeft, VirtualRight {};
extern Diamond diamond;

constexpr Aggregate aggregate{1, {1, 1}};
constexpr int matrix[2][2] = {{1, 1}, {1, 1}};

template <const int&>
struct Ref {};

template <const int*>
struct Pointer {};

Ref<left> refLeft;
Ref<right> refRight;
Ref<values[1]> refElement;
Ref<scope::value> refNamespace;
Ref<Holder::value> refMember;
Ref<aggregate.member> refSubobject;
Ref<aggregate.values[1]> refNestedElement;
Ref<matrix[1][0]> refMultidimensionalElement;
Ref<repeated.Left::value> refLeftBase;
Ref<repeated.Right::value> refRightBase;
Ref<diamond.VirtualLeft::value> refVirtualBase;
Ref<diamond.VirtualRight::value> refVirtualBaseAgain;
Pointer<&left> pointerLeft;
Pointer<&values[1]> pointerElement;
Pointer<&left + 1> pointerOnePastObject;
Pointer<values + 2> pointerOnePastArray;
Pointer<&aggregate.member> pointerSubobject;
Pointer<&matrix[1][0]> pointerMultidimensionalElement;

// CHECK:      [specializations]
// CHECK-NEXT:   class Ref<left>
// CHECK:        class Ref<right>
// CHECK:        class Ref<values[1]>
// CHECK:        class Ref<scope::value>
// CHECK:        class Ref<Holder::value>
// CHECK:        class Ref<aggregate.member>
// CHECK:        class Ref<aggregate.values[1]>
// CHECK:        class Ref<matrix[1][0]>
// CHECK:        class Ref<repeated.Left::Base::value>
// CHECK:        class Ref<repeated.Right::Base::value>
// CHECK:        class Ref<diamond.Base::value>
// CHECK:      [specializations]
// CHECK-NEXT:   class Pointer<&left>
// CHECK:        class Pointer<&values[1]>
// CHECK:        class Pointer<(&left + 1)>
// CHECK:        class Pointer<(values + 2)>
// CHECK:        class Pointer<&aggregate.member>
// CHECK:        class Pointer<&matrix[1][0]>
