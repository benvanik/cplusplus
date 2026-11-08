// RUN: %cxx -std=c++23 -fsyntax-only -verify %s
// RUN: %cxx -std=c++23 -fsyntax-only -dump-symbols %s | %filecheck %s --check-prefix=NAMES

struct Parameters {
  unsigned scale;
  unsigned bias;

  constexpr const unsigned& get() const { return scale; }
  constexpr const unsigned& getExplicit() const { return this->scale; }
  constexpr const unsigned& select(int) const { return scale; }
  constexpr const unsigned& select(double) const { return scale; }

  template <typename T>
  constexpr const unsigned& selectTemplate(T) const {
    return scale;
  }

  constexpr operator const unsigned&() const { return scale; }
};

struct Pair {
  Parameters left;
  Parameters right;
};

struct ParametersLeft : Parameters {};
struct ParametersRight : Parameters {};
struct RepeatedParameters : ParametersLeft, ParametersRight {
  constexpr const unsigned& leftScale() const {
    return ParametersLeft::scale;
  }
  constexpr const unsigned& rightScale() const {
    return ParametersRight::scale;
  }
  constexpr const unsigned& leftGet() const {
    return ParametersLeft::get();
  }
  constexpr const unsigned& rightGet() const {
    return ParametersRight::get();
  }
  constexpr const unsigned* rightAddress() const {
    return &(ParametersRight::scale);
  }
  constexpr const unsigned& capturedRight() const {
    return [this]() -> const unsigned& { return ParametersRight::scale; }();
  }
};

struct VirtualParametersLeft : virtual Parameters {};
struct VirtualParametersRight : virtual Parameters {};
struct VirtualParameters : VirtualParametersLeft, VirtualParametersRight {
  constexpr const unsigned& leftScale() const {
    return VirtualParametersLeft::scale;
  }
  constexpr const unsigned& rightGet() const {
    return VirtualParametersRight::get();
  }
};

constexpr Parameters first{3, 5};
constexpr Parameters second{7, 11};
constexpr Pair pair{{3, 5}, {7, 11}};
constexpr Parameters table[2][2] = {{{3, 5}, {7, 11}},
                                     {{13, 17}, {19, 23}}};
extern Parameters external;
constexpr Parameters* external_pointer = &external;
extern RepeatedParameters repeated;
constexpr RepeatedParameters* repeated_pointer = &repeated;
extern VirtualParameters virtual_parameters;

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

static_assert(__is_same(Ref<table[1][0].get()>, Ref<table[1][0].scale>));
static_assert(!__is_same(Ref<table[0][1].scale>, Ref<table[1][0].scale>));

static_assert(__is_same(Ref<external.get()>, Ref<external.scale>));
static_assert(__is_same(Ref<external.getExplicit()>, Ref<external.scale>));
static_assert(__is_same(Ref<static_cast<const unsigned&>(external)>,
                        Ref<external.scale>));
static_assert(__is_same(Ref<external_pointer->get()>, Ref<external.scale>));

using RepeatedLeft = Ref<repeated.ParametersLeft::scale>;
using RepeatedRight = Ref<repeated.ParametersRight::scale>;
static_assert(!__is_same(RepeatedLeft, RepeatedRight));
static_assert(__is_same(Ref<repeated.ParametersLeft::get()>, RepeatedLeft));
static_assert(__is_same(Ref<repeated.ParametersRight::get()>, RepeatedRight));
static_assert(__is_same(Ref<repeated.leftScale()>, RepeatedLeft));
static_assert(__is_same(Ref<repeated.rightScale()>, RepeatedRight));
static_assert(__is_same(Ref<repeated.leftGet()>, RepeatedLeft));
static_assert(__is_same(Ref<repeated.rightGet()>, RepeatedRight));
static_assert(repeated.rightAddress() == &repeated.ParametersRight::scale);
static_assert(__is_same(Ref<repeated.capturedRight()>, RepeatedRight));
static_assert(__is_same(
    Ref<static_cast<const ParametersLeft&>(repeated).get()>, RepeatedLeft));
static_assert(__is_same(Ref<repeated_pointer->ParametersRight::get()>,
                        RepeatedRight));
static_assert(
    __is_same(Ref<repeated.ParametersLeft::select(0)>, RepeatedLeft));
static_assert(
    __is_same(Ref<repeated.ParametersRight::select(0.0)>, RepeatedRight));
static_assert(__is_same(Ref<repeated.ParametersLeft::selectTemplate(0)>,
                        RepeatedLeft));
static_assert(__is_same(Ref<repeated.ParametersRight::selectTemplate(0)>,
                        RepeatedRight));

using VirtualParametersValue =
    Ref<virtual_parameters.VirtualParametersLeft::scale>;
static_assert(__is_same(
    Ref<virtual_parameters.VirtualParametersLeft::get()>,
    VirtualParametersValue));
static_assert(__is_same(
    Ref<virtual_parameters.VirtualParametersRight::get()>,
    VirtualParametersValue));
static_assert(__is_same(Ref<virtual_parameters.leftScale()>,
                        VirtualParametersValue));
static_assert(__is_same(Ref<virtual_parameters.rightGet()>,
                        VirtualParametersValue));

struct AnonymousParameters {
  struct {
    struct {
      unsigned value;
    };
  };
  constexpr const unsigned& get() const { return value; }
};
extern AnonymousParameters anonymous_parameters;
using AnonymousParametersValue = Ref<anonymous_parameters.value>;
static_assert(__is_same(Ref<anonymous_parameters.get()>,
                        AnonymousParametersValue));

struct Bits {
  unsigned value : 4;
};
struct BitsLeft : Bits {};
struct BitsRight : Bits {};
struct RepeatedBits : BitsLeft, BitsRight {
  constexpr unsigned sum() const {
    return BitsLeft::value + BitsRight::value;
  }
  constexpr void update(unsigned left, unsigned right) {
    BitsLeft::value = left;
    BitsRight::value = right;
    ++BitsRight::value;
  }
};

constexpr bool updatesRepeatedBits() {
  RepeatedBits value{{{3}}, {{7}}};
  value.update(11, 13);
  return value.BitsLeft::value == 11 && value.BitsRight::value == 14 &&
         value.sum() == 25;
}
static_assert(updatesRepeatedBits());

template <typename T>
struct InheritedParameters : T {
  constexpr const unsigned& rightScale() const {
    return T::ParametersRight::scale;
  }
};
extern InheritedParameters<RepeatedParameters> inherited_parameters;
static_assert(__is_same(Ref<inherited_parameters.rightScale()>,
                        Ref<inherited_parameters.ParametersRight::scale>));

Ref<first.get()> first_ref;
Ref<pair.left.get()> pair_left_ref;
Ref<table[1][0].get()> table_one_ref;
Ref<external.get()> external_ref;
RepeatedLeft repeated_left_ref;
RepeatedRight repeated_right_ref;
VirtualParametersValue virtual_parameters_ref;
AnonymousParametersValue anonymous_parameters_ref;

// NAMES-DAG: class Ref<first.scale>
// NAMES-DAG: class Ref<pair.left.scale>
// NAMES-DAG: class Ref<table[1][0].scale>
// NAMES-DAG: class Ref<external.scale>
