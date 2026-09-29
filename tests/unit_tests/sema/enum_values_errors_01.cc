// RUN: %cxx -verify -fsyntax-only %s
// RUN: %cxx -toolchain macos -verify -fsyntax-only %s

enum class BadByte : unsigned char {
  bad_byte = 256,  // expected-error {{enumerator value is not representable in the underlying type}}
};

enum class BadUnsigned : unsigned {
  bad_unsigned = -1,  // expected-error {{enumerator value is not representable in the underlying type}}
};

enum class BadStep : unsigned char {
  last_byte = 255,
  next_byte,  // expected-error {{incremented enumerator value is not representable in the underlying type}}
};

enum class BadBool : bool {
  false_value,
  true_value,
  invalid_bool,  // expected-error {{incremented enumerator value is not representable in the underlying type}}
};

enum BadFloat {
  bad_float = 2.5,  // expected-error {{enumerator initializer must have integral or unscoped enumeration type}}
};

int runtime_value = 2;
enum NotConstant {
  not_constant = runtime_value,  // expected-error {{enumerator initializer is not an integral constant expression}}
};

enum class Scoped { first };
enum ScopedInitializer {
  scoped_initializer = Scoped::first,  // expected-error {{enumerator initializer must have integral or unscoped enumeration type}}
};

enum TooLarge {
  maximum = 0xffffffffffffffffULL,
  overflow,  // expected-error {{no integral type can represent the incremented enumerator value}}
};

enum TooWide {  // expected-error {{no integral type can represent all enumerator values}}
  negative = -1,
  positive = 0xffffffffffffffffULL,
};

template <class T, T Value>
constexpr T invalid_fixed_step() {
  enum class Kind : T {
    first = Value,
    next,  // expected-error {{incremented enumerator value is not representable in the underlying type}}
  };
  return T(Kind::next);
}

auto invalid_step = invalid_fixed_step<unsigned char, 255>();
