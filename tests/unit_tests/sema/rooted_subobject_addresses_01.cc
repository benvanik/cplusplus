// RUN: %cxx -std=c++23 -fsyntax-only -verify %s
// RUN: %cxx -std=c++23 -fsyntax-only -dump-symbols %s | %filecheck %s --check-prefix=NAMES

struct Parameters {
  unsigned scale;
  unsigned bias;

  constexpr const unsigned& get() const { return scale; }
  constexpr const unsigned& getExplicit() const { return this->scale; }
  constexpr operator const unsigned&() const { return scale; }
};

struct Pair {
  Parameters left;
  Parameters right;
};

constexpr Parameters first{3, 5};
constexpr Parameters second{7, 11};
constexpr Pair pair{{3, 5}, {7, 11}};
constexpr Parameters table[2] = {{3, 5}, {7, 11}};
extern Parameters external;
constexpr Parameters* external_pointer = &external;

template <const unsigned& value>
struct Ref {};

static_assert(__is_same(Ref<first.get()>, Ref<first.scale>));
static_assert(__is_same(Ref<first.getExplicit()>, Ref<first.scale>));
static_assert(__is_same(Ref<static_cast<const unsigned&>(first)>,
                        Ref<first.scale>));
static_assert(!__is_same(Ref<first.scale>, Ref<second.scale>));
static_assert(!__is_same(Ref<first.scale>, Ref<first.bias>));

static_assert(__is_same(Ref<pair.left.get()>, Ref<pair.left.scale>));
static_assert(!__is_same(Ref<pair.left.scale>, Ref<pair.right.scale>));

static_assert(__is_same(Ref<table[1].get()>, Ref<table[1].scale>));
static_assert(!__is_same(Ref<table[0].scale>, Ref<table[1].scale>));

static_assert(__is_same(Ref<external.get()>, Ref<external.scale>));
static_assert(__is_same(Ref<external.getExplicit()>, Ref<external.scale>));
static_assert(__is_same(Ref<static_cast<const unsigned&>(external)>,
                        Ref<external.scale>));
static_assert(__is_same(Ref<external_pointer->get()>, Ref<external.scale>));

Ref<first.get()> first_ref;
Ref<pair.left.get()> pair_left_ref;
Ref<table[1].get()> table_one_ref;
Ref<external.get()> external_ref;

// NAMES-DAG: class Ref<first.scale>
// NAMES-DAG: class Ref<pair.left.scale>
// NAMES-DAG: class Ref<table[1].scale>
// NAMES-DAG: class Ref<external.scale>
