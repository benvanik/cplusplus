// REQUIRES: native-x86_64-linux
// RUN: %cxx --target=x86_64-unknown-linux-gnu -std=c++23 %s -o %t
// RUN: %t

struct Base {
  unsigned value;
  unsigned bits : 4;
  unsigned get() const { return value; }
};
struct Left : Base {};
struct Right : Base {};
struct Both : Left, Right {
  unsigned sum() const { return Left::value + Right::value; }
  unsigned callSum() const { return Left::get() + Right::get(); }
  unsigned bitSum() const { return Left::bits + Right::bits; }
  unsigned& right() { return Right::value; }
  void update(unsigned left, unsigned right) {
    Left::value = left;
    Right::value = right;
    Left::bits = left;
    Right::bits = right;
  }
  unsigned capturedSum() const {
    return [this] { return Left::value + Right::value; }();
  }
};

struct Other { unsigned value; };
struct Selected : Other, Right {
  using Right::value;
  unsigned get() const { return value; }
};

struct Anonymous {
  struct {
    struct {
      unsigned value;
    };
  };
  unsigned get() const { return value; }
};

template <typename T>
struct Derived : T {
  unsigned inherited() const { return T::sum() + T::Right::value; }
};

struct Adjusted : Other, Left { unsigned tail; };
Base* basePointer(Adjusted* value) { return value; }
Adjusted* derivedPointer(Base* value) { return static_cast<Adjusted*>(value); }
Adjusted& derivedReference(Base& value) { return static_cast<Adjusted&>(value); }
Adjusted&& derivedRvalue(Base& value) { return static_cast<Adjusted&&>(value); }

struct VirtualLeft : virtual Base {};
struct Virtual : Other, VirtualLeft {};
Base* virtualBasePointer(Virtual* value) { return value; }

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

int main() {
  Both object{{{3}}, {{7}}};
  if (object.sum() != 10 || object.callSum() != 10) return 1;
  if (object.Left::value != 3 || object.Right::value != 7) return 2;
  if (object.Left::get() != 3 || object.Right::get() != 7) return 3;
  Both copied = object;
  if (copied.Left::value != 3 || copied.Right::value != 7) return 21;
  Both assigned{{{0}}, {{0}}};
  assigned = object;
  if (assigned.Left::value != 3 || assigned.Right::value != 7) return 22;
  Both movedBases = static_cast<Both&&>(copied);
  if (movedBases.Left::value != 3 || movedBases.Right::value != 7) return 23;
  Both moveAssigned{{{0}}, {{0}}};
  moveAssigned = static_cast<Both&&>(movedBases);
  if (moveAssigned.Left::value != 3 || moveAssigned.Right::value != 7)
    return 24;
  if (object.capturedSum() != 10) return 4;
  if (&object.right() != &object.Right::value) return 5;
  object.update(11, 13);
  object.right() += 1;
  if (object.Left::value != 11 || object.Right::value != 14) return 6;
  Selected selected{{17}, {{19}}};
  if (selected.get() != 19 || selected.value != 19) return 7;
  Anonymous anonymous{{{23}}};
  if (anonymous.get() != 23 || anonymous.value != 23) return 8;
  Derived<Both> derived{{{{3}}, {{7}}}};
  if (derived.inherited() != 17) return 9;
  if (object.bitSum() != 24) return 10;
  Adjusted adjusted{{17}, {{19}}, 23};
  auto* base = basePointer(&adjusted);
  if (base->value != 19 || derivedPointer(base) != &adjusted) return 11;
  if (basePointer(nullptr) != nullptr) return 12;
  if (derivedPointer(nullptr) != nullptr) return 13;
  derivedReference(*base).tail = 29;
  Adjusted&& moved = derivedRvalue(*base);
  if (moved.tail != 29 || adjusted.tail != 29) return 14;
  if (virtualBasePointer(nullptr) != nullptr) return 15;
  Virtual virtualObject{};
  virtualObject.Base::value = 31;
  if (virtualBasePointer(&virtualObject)->value != 31) return 16;
  Virtual copiedVirtual = virtualObject;
  if (copiedVirtual.Base::value != 31) return 25;
  Virtual assignedVirtual{};
  assignedVirtual = virtualObject;
  if (assignedVirtual.Base::value != 31) return 26;
  Virtual movedVirtual = static_cast<Virtual&&>(copiedVirtual);
  if (movedVirtual.Base::value != 31) return 27;
  Virtual moveAssignedVirtual{};
  moveAssignedVirtual = static_cast<Virtual&&>(movedVirtual);
  if (moveAssignedVirtual.Base::value != 31) return 28;
  InstantiatedPaths<double> paths{{{1, 2, 3}}, {37}};
  if (paths.explicitCall() != 37 || paths.implicitCall() != 37) return 17;
  if (paths.explicitValue() != 37 || paths.implicitValue() != 37) return 18;
  if (paths.basePointer() != static_cast<TemplateValue*>(&paths)) return 19;
  if (&paths.baseReference() != static_cast<TemplateValue*>(&paths)) return 20;
  return 0;
}
