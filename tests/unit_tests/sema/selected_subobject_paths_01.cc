// RUN: %cxx -std=c++23 -fsyntax-only -verify %s
// RUN: %cxx -std=c++23 -fsyntax-only -dump-symbols %s | %filecheck %s --check-prefix=NAMES

struct Base {
  unsigned value;

  constexpr const unsigned& get() const { return value; }
};

struct Left : Base {};
struct Right : Base {};
struct Both : Left, Right {};
extern Both repeated;

template <const unsigned& Value>
struct Ref {};

using RepeatedLeft = Ref<repeated.Left::value>;
using RepeatedRight = Ref<repeated.Right::value>;
static_assert(!__is_same(RepeatedLeft, RepeatedRight));

using ConvertedLeft = Ref<static_cast<const Left&>(repeated).value>;
static_assert(__is_same(ConvertedLeft, RepeatedLeft));

constexpr const unsigned& leftValue(const Both& object) {
  return static_cast<const Left&>(object).get();
}
using ReturnedLeft = Ref<leftValue(repeated)>;
static_assert(__is_same(ReturnedLeft, RepeatedLeft));

struct Unique : Left {};
extern Unique unique;
using UniqueInherited = Ref<unique.value>;
using UniqueQualified = Ref<unique.Left::value>;
static_assert(__is_same(UniqueInherited, UniqueQualified));

struct Selected : Left, Right {
  using Left::value;
};
extern Selected selected;
using UsingSelected = Ref<selected.value>;
using SelectedLeft = Ref<selected.Left::value>;
using SelectedRight = Ref<selected.Right::value>;
static_assert(__is_same(UsingSelected, SelectedLeft));
static_assert(!__is_same(UsingSelected, SelectedRight));
static_assert(&selected.value == &selected.Left::value);
static_assert(&selected.value != &selected.Right::value);

struct Anonymous {
  struct {
    struct {
      unsigned value;
    };
  };
};
extern Anonymous anonymous;
using AnonymousValue = Ref<anonymous.value>;

constexpr Both initialized{{{3}}, {{7}}};
static_assert(initialized.Left::value == 3);
static_assert(initialized.Right::value == 7);
static_assert(static_cast<const Left&>(initialized).value == 3);
static_assert(leftValue(initialized) == 3);

RepeatedLeft repeatedLeft;
RepeatedRight repeatedRight;
UsingSelected usingSelected;
AnonymousValue anonymousValue;

// NAMES-DAG: class Ref<repeated.Left.Base.value>
// NAMES-DAG: class Ref<repeated.Right.Base.value>
// NAMES-DAG: class Ref<selected.Left.Base.value>
// NAMES-DAG: class Ref<anonymous.value>

// expected-no-diagnostics
