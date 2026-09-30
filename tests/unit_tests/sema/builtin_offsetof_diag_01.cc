// RUN: %cxx -verify -fsyntax-only %s

struct BitFieldOwner {
  unsigned bits : 3;
};

auto bit_field = __builtin_offsetof(BitFieldOwner, bits); // expected-error {{offsetof requires an addressable data member}}

struct StaticOwner {
  static int value;
};

auto static_member = __builtin_offsetof(StaticOwner, value); // expected-error {{offsetof requires an addressable data member}}

struct PointerOwner {
  int* values;
};

auto pointer_element = __builtin_offsetof(PointerOwner, values[2]); // expected-error {{offsetof requires an array type, but got 'int*'}}

enum class Index { one = 1 };

struct ArrayOwner {
  int values[4];
};

auto scoped_index = __builtin_offsetof(ArrayOwner, values[Index::one]); // expected-error {{offsetof index must be an integral constant expression}}
auto negative_index = __builtin_offsetof(ArrayOwner, values[-1]); // expected-error {{offsetof index must be a nonnegative integral constant expression}}

int index;
auto dynamic_index = __builtin_offsetof(ArrayOwner, values[index]); // expected-error {{offsetof index must be an integral constant expression}}

struct ScalarOwner {
  int value;
};

auto scalar_member = __builtin_offsetof(ScalarOwner, value.missing); // expected-error {{offsetof requires a class or union type, but got 'int'}}
auto scalar_subscript = __builtin_offsetof(ScalarOwner, value[0]); // expected-error {{offsetof requires an array type, but got 'int'}}
auto missing_member = __builtin_offsetof(ScalarOwner, absent); // expected-error {{no member named 'absent' in 'ScalarOwner'}}

struct Incomplete;
auto incomplete_member = __builtin_offsetof(Incomplete, value); // expected-error {{offsetof requires a complete class or union type}}

struct OverflowOwner {
  int values[1];
};

auto overflowing_offset =
    __builtin_offsetof(OverflowOwner, values[0x40000000]); // expected-error {{offsetof result exceeds the target size type}}

struct VirtualBase {
  int value;
};

struct VirtualDerived : virtual VirtualBase {};

auto virtual_base_member = __builtin_offsetof(VirtualDerived, value); // expected-error {{offsetof cannot name a member of a virtual base}}

struct AmbiguousBase {
  int value;
};

struct AmbiguousLeft : AmbiguousBase {};
struct AmbiguousRight : AmbiguousBase {};
struct AmbiguousOwner : AmbiguousLeft, AmbiguousRight {};

auto ambiguous_member = __builtin_offsetof(AmbiguousOwner, value); // expected-error {{member 'value' found in multiple base classes of '::AmbiguousOwner'}}

class PrivateOwner {
  int value;
};

auto private_member = __builtin_offsetof(PrivateOwner, value); // expected-error {{'value' is a private member of '::PrivateOwner'}}
