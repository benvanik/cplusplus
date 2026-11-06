// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

using Int2 = int __attribute__((ext_vector_type(2)));
using Int3 = int __attribute__((ext_vector_type(3)));
using Int4 = int __attribute__((ext_vector_type(4)));
using Int16 = int __attribute__((ext_vector_type(16)));

static_assert(__is_same(decltype(Int4{}.x), int));
static_assert(__is_same(decltype(Int4{}.xy), Int2));
static_assert(__is_same(decltype((Int4{}.x)), int));
static_assert(__is_same(decltype((Int4{}.xy)), Int2));
static_assert(Int4{1, 2, 3, 4}.w == 4);

Int4&& vectorRvalue();
static_assert(__is_same(decltype((vectorRvalue().x)), int&&));
static_assert(__is_same(decltype((vectorRvalue().xy)), Int2&&));

void select(Int4& value, const Int4& constant, Int4* pointer, Int16& wide) {
  static_assert(__is_same(decltype(value.x), int&));
  static_assert(__is_same(decltype(value.xy), Int2&));
  static_assert(__is_same(decltype(constant.r), const int&));
  static_assert(__is_same(decltype(constant.rg), const Int2&));
  static_assert(__is_same(decltype(constant.rr), const Int2));
  static_assert(__is_same(decltype(pointer->w), int&));
  static_assert(__is_same(decltype(value.xx), Int2));
  static_assert(__is_same(decltype(value.lo), Int2&));
  static_assert(__is_same(decltype(wide.s0F), Int2&));

  value.x = pointer->w;
  value.xy = value.yx;
  value.rgba = value.abgr;
  value.s30 = value.S12;
  value.hi = value.lo;
  value.even = value.odd;
}

void selectOddWidth(Int3& value) {
  static_assert(__is_same(decltype(value.lo), Int2&));
  static_assert(__is_same(decltype(value.hi), Int2&));
  static_assert(__is_same(decltype(value.even), Int2&));
  static_assert(__is_same(decltype(value.odd), Int2&));
}

struct PointerWrapper {
  Int4* operator->();
};

void selectThroughOperatorArrow(PointerWrapper value) { value->z = 1; }

template <class Vector>
auto selectSecond(Vector& value) -> int {
  return value.y;
}

void instantiateDependentSelection(Int4& value) {
  int second = selectSecond(value);
  (void)second;
}
