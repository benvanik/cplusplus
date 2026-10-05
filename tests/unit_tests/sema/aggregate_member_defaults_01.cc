// RUN: %cxx -std=c++23 -fsyntax-only -verify %s

struct Descriptor {
  unsigned elements = 32;
  unsigned words = this->elements / 8;
  unsigned pairs = words * 2;
};

static_assert(Descriptor{}.words == 4);
static_assert(Descriptor{64}.pairs == 16);
static_assert(Descriptor{.words = 7}.pairs == 14);
static_assert(Descriptor(48).words == 6);
static_assert(Descriptor().words == 4);

struct ZeroPhase {
  unsigned copy = later;
  unsigned later;
};

constexpr ZeroPhase zero = ZeroPhase();
static_assert(zero.copy == 0);
static_assert(zero.later == 0);

template <Descriptor Spec>
constexpr unsigned count() {
  return Spec.pairs;
}

static_assert(count<Descriptor{64}>() == 16);

struct Outer {
  unsigned elements;
  Descriptor payload{this->elements};
  unsigned words = payload.words;

  constexpr unsigned compute() const {
    Descriptor first{this->elements};
    Descriptor second{.elements = this->elements * 2};
    return first.words + second.words + this->elements;
  }
};

static_assert(Outer{24}.compute() == 33);
static_assert(Outer{24}.words == 3);

struct Batch {
  unsigned elements = 16;
  Descriptor descriptors[2]{{elements}, {2 * elements}};
  unsigned total = descriptors[0].words + descriptors[1].words;
};

static_assert(Batch{}.total == 6);
static_assert(Batch{.elements = 32}.total == 12);

struct WithMethod {
  unsigned elements = 32;
  constexpr unsigned wordCount() const { return elements / 8; }
  unsigned words = wordCount();
};

static_assert(WithMethod{64}.words == 8);

struct Base {
  unsigned elements;
};

struct Derived : Base {
  unsigned words = this->elements / 8;
};

static_assert(Derived{{48}}.words == 6);

struct Sequence {
  unsigned count = 4;
  unsigned next = ++count;
  unsigned twice = count * 2;
};

static_assert(Sequence{}.count == 5);
static_assert(Sequence{}.next == 5);
static_assert(Sequence{}.twice == 10);

struct Conditional {
  bool readLater;
  unsigned first = readLater ? second : 7;
  unsigned second = 9;
};

static_assert(Conditional{false}.first == 7);

struct Explicit {
  unsigned value = value;
};

static_assert(Explicit{.value = 11}.value == 11);

// expected-no-diagnostics
