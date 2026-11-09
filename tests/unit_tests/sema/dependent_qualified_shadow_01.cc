// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

struct Deep {
  using type = float;
};

struct Item {
  struct Inner {
    struct Deep {
      using type = long long;
      static constexpr int value = 17;
    };
  };
};

// An unresolved prefix still qualifies every following component.
template <class T>
using Result = typename T::Inner::Deep::type;
static_assert(__is_same(Result<Item>, long long));

template <class T>
struct Extract {
  using type = typename T::Inner::Deep::type;
  static constexpr int value = T::Inner::Deep::value;
};
static_assert(__is_same(Extract<Item>::type, long long));
static_assert(Extract<Item>::value == 17);

template <class T>
using Identity = T;
template <class T>
using AliasResult = typename Identity<T>::Inner::Deep::type;
static_assert(__is_same(AliasResult<Item>, long long));

// Resolving one template level must not capture names from the next level.
template <class T>
struct Outer {
  template <class U>
  using Result = typename U::Inner::Deep::type;
};
static_assert(__is_same(Outer<int>::Result<Item>, long long));

template <class T>
struct Provider {
  template <class U>
  using Rebind = U;
  template <class U>
  struct Nested : U {};
};
template <class T, class U>
using Rebound = typename T::template Rebind<U>::Inner::Deep::type;
static_assert(__is_same(Rebound<Provider<int>, Item>, long long));

template <class T, class U>
using NestedResult = typename T::template Nested<U>::Inner::Deep::type;
static_assert(__is_same(NestedResult<Provider<int>, Item>, long long));

// A fully qualified, nondependent name keeps its ordinary meaning.
template <class T>
using Global = typename ::Deep::type;
static_assert(__is_same(Global<Item>, float));
