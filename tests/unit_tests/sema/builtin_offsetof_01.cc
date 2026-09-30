// RUN: %cxx -verify -fsyntax-only -fvalidate-ast %s

// expected-no-diagnostics

struct Leaf {
  char bytes[3];
  int value;
};

struct Middle {
  char lead;
  Leaf leaves[3];
};

struct Outer {
  short prefix;
  Middle middle[2];
};

static_assert(__builtin_offsetof(Outer, prefix) == 0);
static_assert(__builtin_offsetof(Outer, middle) == 4);
static_assert(__builtin_offsetof(Outer, middle[1]) == 32);
static_assert(__builtin_offsetof(Outer, middle[1].leaves[2].value) == 56);
static_assert(__builtin_offsetof(Outer, middle[0].leaves[1].bytes[2]) == 18);

template <typename T>
consteval auto nested_offset() {
  return __builtin_offsetof(T, middle[1].leaves[2].value);
}

static_assert(nested_offset<Outer>() == 56);

template <int MiddleIndex, int LeafIndex>
consteval auto indexed_offset() {
  return __builtin_offsetof(Outer,
                            middle[MiddleIndex].leaves[LeafIndex].value);
}

static_assert(indexed_offset<1, 2>() == 56);

enum Index { first = 1, second = 2 };
enum class ScopedIndex { first = 1 };

struct Indexed {
  char lead;
  int values[4];
  short matrix[2][3];
};

static_assert(__builtin_offsetof(Indexed, values[second]) == 12);
static_assert(__builtin_offsetof(
                  Indexed, values[static_cast<int>(ScopedIndex::first)]) == 8);
static_assert(__builtin_offsetof(Indexed, matrix[1][2]) == 30);
static_assert(__builtin_offsetof(Indexed, values[8]) == 36);

struct FirstBase {
  int first;
};

struct SecondBase {
  char lead;
  int inherited;
};

struct Derived : FirstBase, SecondBase {
  int own;
};

struct Holder {
  char prefix;
  Derived value;
};

static_assert(__builtin_offsetof(Derived, inherited) == 8);
static_assert(__builtin_offsetof(Holder, value.inherited) == 12);

struct AnonymousOwner {
  char lead;
  union {
    int direct;
    struct {
      short nested;
      char tail;
    };
  };
};

static_assert(__builtin_offsetof(AnonymousOwner, direct) == 4);
static_assert(__builtin_offsetof(AnonymousOwner, nested) == 4);
static_assert(__builtin_offsetof(AnonymousOwner, tail) == 6);
