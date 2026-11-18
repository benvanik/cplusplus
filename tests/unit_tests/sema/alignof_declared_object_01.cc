// RUN: %cxx --target=x86_64-unknown-linux-gnu -verify -fsyntax-only -fvalidate-ast %s

constexpr unsigned parameter(unsigned * __attribute__((aligned(16))) p) {
  static_assert(alignof(decltype(p)) == alignof(unsigned*));
  static_assert(__alignof__(*p) == alignof(unsigned));
  return __alignof__(p);
}
constexpr unsigned reduced(unsigned *p __attribute__((aligned(2)))) {
  return __alignof__(p);
}
static_assert(parameter(nullptr) == 16);
static_assert(reduced(nullptr) == 2);

extern unsigned * __attribute__((aligned(2))) object;
static_assert(__alignof__(object) == 2);
static_assert(alignof(decltype(object)) == alignof(unsigned*));

extern unsigned *redeclaration;
extern unsigned * __attribute__((aligned(16))) redeclaration;
extern unsigned * __attribute__((aligned(2))) redeclaration;
extern unsigned *redeclaration;
static_assert(__alignof__(redeclaration) == 16);

#pragma pack(push, 1)
struct Packet {
  unsigned *pointer;
  static unsigned * __attribute__((aligned(16))) shared;
  static unsigned * __attribute__((aligned(2))) reduced;
};
#pragma pack(pop)
static_assert(__alignof__(Packet::pointer) == 1);
static_assert(__alignof__(Packet::shared) == 16);
static_assert(__alignof__(Packet::reduced) == 2);

struct Defined { static unsigned *out_of_class; };
unsigned * __attribute__((aligned(32))) Defined::out_of_class;
static_assert(__alignof__(Defined::out_of_class) == 32);

template<class T, int N> constexpr unsigned parameter_of(T * __attribute__((aligned(N))) p) {
  return __alignof__(p);
}
template<class T> constexpr unsigned member_of() { return __alignof__(T::pointer); }
static_assert(parameter_of<unsigned, 32>(nullptr) == 32);
static_assert(member_of<Packet>() == 1);

// expected-error@+1 {{invalid use of non-static data member 'pointer'}}
unsigned *evaluated() { return Packet::pointer; }
