// RUN: %cxx --target=x86_64-unknown-linux-gnu -verify -fsyntax-only -fvalidate-ast %s

typedef unsigned * __attribute__((aligned(16))) P, Slots[alignof(P)];
static_assert(alignof(P) == 16);
static_assert(sizeof(P) == 8 && alignof(__remove_pointer(P)) == 4);
static_assert(sizeof(Slots) == 64);

struct Packet {
  char tag;
  unsigned * const __attribute__((aligned(16))) pointer, *sibling;
  unsigned payload;
};
static_assert(__builtin_offsetof(Packet, pointer) == 16);
static_assert(__builtin_offsetof(Packet, sibling) == 24);
static_assert(__builtin_offsetof(Packet, payload) == 32);
static_assert(sizeof(Packet) == 48);

struct Members {
  unsigned (* __attribute__((aligned(16))) function)();
  unsigned Members::* __attribute__((aligned(32))) member;
  unsigned (* __attribute__((aligned(16))) array)[4];
};
static_assert(__builtin_offsetof(Members, member) == 32);
static_assert(__builtin_offsetof(Members, array) == 48);

typedef unsigned * __attribute__((aligned(2))) UnalignedPointer;
struct Packed {
  char tag;
  unsigned * __attribute__((packed, aligned(2))) pointer;
};
static_assert(alignof(UnalignedPointer) == 2);
static_assert(__builtin_offsetof(Packed, pointer) == 2 && sizeof(Packed) == 10);

template<class T, int N> struct PacketOf {
  typedef T * __attribute__((aligned(N))) Pointer;
  char tag;
  Pointer pointer;
  T * __attribute__((aligned(N * 2))) *indirect;
};
using Wide = PacketOf<unsigned, 16>;
static_assert(alignof(Wide::Pointer) == 16);
static_assert(__builtin_offsetof(Wide, pointer) == 16);
static_assert(__builtin_offsetof(Wide, indirect) == 32);

extern unsigned * volatile __attribute__((aligned(16))) const qualified;
extern unsigned & __restrict __attribute__((aligned(16))) reference;
static_assert(__alignof__(qualified) == 16);
static_assert(__alignof__(*qualified) == 4);
static_assert(__alignof__(reference) == 16);

using Plain = unsigned * __attribute__((aligned(16)));
static_assert(alignof(Plain) == alignof(unsigned*));
// expected-warning@+1 {{'gnu::aligned' ignored on a declarator type; place it on the declaration}}
extern unsigned * [[gnu::aligned(16)]] scoped;
static_assert(__alignof__(scoped) == alignof(unsigned*));

void catch_pointer() {
  try {} catch (unsigned * __attribute__((aligned(16))) p) {
    static_assert(__alignof__(p) == 16 && __alignof__(*p) == alignof(unsigned));
  }
}
