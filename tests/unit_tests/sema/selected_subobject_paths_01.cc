// RUN: %cxx -std=c++23 -fsyntax-only -verify %s
// RUN: %cxx -std=c++23 -fsyntax-only -dump-symbols %s | %filecheck %s --check-prefix=NAMES

struct Base {
  unsigned value;
  unsigned bits : 4;

  constexpr const unsigned& get() const { return value; }
};

struct Left : Base {};
struct Right : Base {};
struct Both : Left, Right {
  constexpr unsigned sum() const { return Left::value + Right::value; }
  constexpr unsigned callSum() const { return Left::get() + Right::get(); }
  constexpr unsigned bitSum() const { return Left::bits + Right::bits; }
  constexpr const unsigned& right() const { return Right::get(); }
  constexpr const unsigned* rightAddress() const { return &(Right::value); }
  constexpr void update(unsigned left, unsigned right) {
    Left::value = left;
    Right::value = right;
    ++Right::value;
    Left::bits = left;
    Right::bits = right;
  }
  constexpr unsigned capturedSum() const {
    return [this] { return Left::value + Right::value; }();
  }
  constexpr const unsigned& capturedRight() const {
    return [this]() -> const unsigned& { return Right::value; }();
  }
  constexpr const unsigned* capturedAddress() const {
    return [this] { return &this->Right::value; }();
  }
};
extern Both repeated;

template <const unsigned& Value>
struct Ref {};

using RepeatedLeft = Ref<repeated.Left::value>;
using RepeatedRight = Ref<repeated.Right::value>;
static_assert(!__is_same(RepeatedLeft, RepeatedRight));
using ImplicitRight = Ref<repeated.right()>;
static_assert(__is_same(ImplicitRight, RepeatedRight));
static_assert(repeated.rightAddress() == &repeated.Right::value);
using CapturedRight = Ref<repeated.capturedRight()>;
static_assert(__is_same(CapturedRight, RepeatedRight));
static_assert(repeated.capturedAddress() == &repeated.Right::value);

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

struct Other { unsigned value; };
struct Selected : Left, Other {
  using Left::value;
  constexpr const unsigned& currentValue() const { return value; }
};
extern Selected selected;
using UsingSelected = Ref<selected.value>;
using SelectedLeft = Ref<selected.Left::value>;
using SelectedRight = Ref<selected.Other::value>;
static_assert(__is_same(UsingSelected, SelectedLeft));
static_assert(!__is_same(UsingSelected, SelectedRight));
static_assert(&selected.value == &selected.Left::value);
static_assert(&selected.value != &selected.Other::value);
using ImplicitSelected = Ref<selected.currentValue()>;
static_assert(__is_same(ImplicitSelected, SelectedLeft));

struct Anonymous {
  struct {
    struct {
      unsigned value;
    };
  };
  constexpr const unsigned& get() const { return value; }
};
extern Anonymous anonymous;
using AnonymousValue = Ref<anonymous.value>;
using AnonymousReturned = Ref<anonymous.get()>;
static_assert(__is_same(AnonymousValue, AnonymousReturned));

constexpr Both initialized{{{3}}, {{7}}};
static_assert(initialized.Left::value == 3);
static_assert(initialized.Right::value == 7);
static_assert(static_cast<const Left&>(initialized).value == 3);
static_assert(leftValue(initialized) == 3);
static_assert(initialized.sum() == 10);
static_assert(initialized.callSum() == 10);
static_assert(initialized.capturedSum() == 10);
static_assert(initialized.right() == 7);
static_assert(*initialized.rightAddress() == 7);

constexpr bool updateRepeated() {
  Both value{{{3}}, {{7}}};
  value.update(11, 13);
  return value.Left::value == 11 && value.Right::value == 14 &&
         value.bitSum() == 24;
}
static_assert(updateRepeated());

template <typename T>
struct Derived : T {
  constexpr unsigned inherited() const {
    return T::sum() + T::Right::value;
  }
};
constexpr Derived<Both> instantiated{{{{3}}, {{7}}}};
static_assert(instantiated.inherited() == 17);

struct TemplateOffset {
  unsigned padding[3];
};
struct TemplateValue {
  unsigned value;
  constexpr unsigned get() const { return value; }
};
template <typename T>
struct InstantiatedPaths : TemplateOffset, TemplateValue {
  constexpr unsigned explicitCall() const { return this->get(); }
  constexpr unsigned implicitCall() const { return get(); }
  constexpr unsigned explicitValue() const { return this->value; }
  constexpr unsigned implicitValue() const { return value; }
  constexpr const TemplateValue* basePointer() const { return this; }
  constexpr const TemplateValue& baseReference() const { return *this; }
};
constexpr InstantiatedPaths<double> instantiatedPaths{{{1, 2, 3}}, {37}};
static_assert(instantiatedPaths.explicitCall() == 37);
static_assert(instantiatedPaths.implicitCall() == 37);
static_assert(instantiatedPaths.explicitValue() == 37);
static_assert(instantiatedPaths.implicitValue() == 37);
static_assert(instantiatedPaths.basePointer() ==
              static_cast<const TemplateValue*>(&instantiatedPaths));
static_assert(&instantiatedPaths.baseReference() ==
              static_cast<const TemplateValue*>(&instantiatedPaths));

// Class adjustments preserve the complete object behind the selected base.
struct Adjusted : Other, Left { unsigned tail; };
constexpr Adjusted adjusted{{17}, {{19}}, 23};
constexpr const Base* adjustedBase = &adjusted;
static_assert(adjustedBase->value == 19);
static_assert(static_cast<const Adjusted*>(adjustedBase) == &adjusted);
static_assert(static_cast<const Adjusted&>(*adjustedBase).tail == 23);
static_assert(static_cast<const Adjusted&&>(*adjustedBase).tail == 23);
static_assert(static_cast<const Base&&>(adjusted).value == 19);
template <typename T>
constexpr const T* recover(const Base* value) { return static_cast<const T*>(value); }
static_assert(recover<Adjusted>(adjustedBase) == &adjusted);

constexpr const Right* selectedRight = &initialized;
constexpr const Base* selectedLeaf = selectedRight;
static_assert(static_cast<const Right*>(selectedLeaf) == selectedRight);
static_assert(static_cast<const Both*>(selectedRight) == &initialized);

extern Adjusted externalAdjusted;
static_assert(static_cast<Adjusted*>(static_cast<Base*>(&externalAdjusted)) == &externalAdjusted);
using AdjustedMember = Ref<static_cast<const Adjusted&>(*adjustedBase).tail>;
using DirectAdjustedMember = Ref<adjusted.tail>;
static_assert(__is_same(AdjustedMember, DirectAdjustedMember));

constexpr Adjusted adjustedArray[] = {{{1}, {{2}}, 3}, {{4}, {{5}}, 6}};
constexpr const Base* arrayBase = &adjustedArray[1];
static_assert(static_cast<const Adjusted*>(arrayBase) == &adjustedArray[1]);
static_assert(static_cast<const Adjusted&>(*arrayBase).tail == 6);

constexpr Adjusted* nullAdjusted = nullptr;
constexpr Base* nullBase = nullptr;
static_assert(static_cast<Base*>(nullAdjusted) == nullptr);
static_assert(static_cast<Adjusted*>(nullBase) == nullptr);
static_assert(static_cast<const Base*>(nullAdjusted) == nullptr);
static_assert(static_cast<const Adjusted*>(nullBase) == nullptr);

constexpr bool updateAdjusted() {
  Adjusted value{{1}, {{2}}, 3};
  Base& base = value;
  static_cast<Adjusted&>(base).tail = 7;
  Adjusted&& moved = static_cast<Adjusted&&>(base);
  moved.tail += 2;
  return value.Other::value == 1 && base.value == 2 && value.tail == 9;
}
static_assert(updateAdjusted());

RepeatedLeft repeatedLeft;
RepeatedRight repeatedRight;
UsingSelected usingSelected;
AnonymousValue anonymousValue;

// NAMES-DAG: class Ref<repeated.Left.Base.value>
// NAMES-DAG: class Ref<repeated.Right.Base.value>
// NAMES-DAG: class Ref<selected.Left.Base.value>
// NAMES-DAG: class Ref<anonymous.value>

// expected-no-diagnostics
