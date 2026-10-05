// RUN: %cxx -std=c++23 -fsyntax-only -verify %s

struct VirtualBase {
  unsigned value;
};
struct VirtualLeft : virtual VirtualBase {};
struct VirtualRight : virtual VirtualBase {};
struct Diamond : VirtualLeft, VirtualRight {};
extern Diamond diamond;

template <const unsigned& Value>
struct Ref {};

// ConstObject has no single storage node for a shared virtual base. Reject the
// selected path instead of assigning it to either inheritance branch.
// expected-error@+1 {{template argument is not a converted constant expression of type 'const unsigned int&'}}
Ref<diamond.VirtualLeft::value> unsupportedVirtualBase;
