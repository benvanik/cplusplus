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
  return 0;
}
