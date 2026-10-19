// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

struct G {
  constexpr int operator()(int x) { return x; }
  constexpr int operator()(int x) const { return x + 100; }
};

static_assert(G{}(1) == 1);
constexpr G const_g;
static_assert(const_g(1) == 101);

void test_const_overload() {
  const G cg;
  G mg;
  static_assert(__is_same(decltype(cg(1)), int));
  static_assert(__is_same(decltype(mg(1)), int));
}

struct MutableCall {};
struct ConstCall {};
struct VolatileCall {};
struct ConstVolatileCall {};

struct CvQualified {
  MutableCall operator()();
  ConstCall operator()() const;
  VolatileCall operator()() volatile;
  ConstVolatileCall operator()() const volatile;
};

extern CvQualified mutable_call;
extern const CvQualified const_call;
extern volatile CvQualified volatile_call;
extern const volatile CvQualified const_volatile_call;

static_assert(__is_same(decltype(mutable_call()), MutableCall));
static_assert(__is_same(decltype(const_call()), ConstCall));
static_assert(__is_same(decltype(volatile_call()), VolatileCall));
static_assert(
    __is_same(decltype(const_volatile_call()), ConstVolatileCall));

struct LvalueCall {};
struct ConstLvalueCall {};
struct RvalueCall {};
struct ConstRvalueCall {};

struct RefQualified {
  LvalueCall operator()() &;
  ConstLvalueCall operator()() const&;
  RvalueCall operator()() &&;
  ConstRvalueCall operator()() const&&;
};

extern RefQualified lvalue_call;
extern const RefQualified const_lvalue_call;
auto mutable_rvalue_call() -> RefQualified&&;
auto const_rvalue_call() -> const RefQualified&&;

static_assert(__is_same(decltype(lvalue_call()), LvalueCall));
static_assert(__is_same(decltype(const_lvalue_call()), ConstLvalueCall));
static_assert(__is_same(decltype(mutable_rvalue_call()()), RvalueCall));
static_assert(__is_same(decltype(const_rvalue_call()()), ConstRvalueCall));
