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

int main() {
  Both object{{{3}}, {{7}}};
  if (object.sum() != 10 || object.callSum() != 10) return 1;
  if (object.Left::value != 3 || object.Right::value != 7) return 2;
  if (object.Left::get() != 3 || object.Right::get() != 7) return 3;
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
  Virtual virtualObject;
  virtualObject.Base::value = 31;
  if (virtualBasePointer(&virtualObject)->value != 31) return 16;
  return 0;
}
