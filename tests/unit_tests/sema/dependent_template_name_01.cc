// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

template <bool Value>
struct Boolean {
  static constexpr bool value = Value;
};
using False = Boolean<false>;

template <bool Continue>
struct Select;

template <>
struct Select<true> {
  template <class Result, class First, class... Rest>
  using type = typename Select<!First::value && sizeof...(Rest) != 0>::template
      type<First, Rest...>;
};

template <>
struct Select<false> {
  template <class Result, class...>
  using type = Result;
};

template <class T, class U>
using Equal = Boolean<__is_same(T, U)>;

// The qualifier and member template are known, but the specialization must
// wait for the arguments. Expanding a dependent pack here loses its identity.
template <class T, class First, class... Options>
using IsAny = typename Select<true>::template type<False, Equal<T, First>,
                                                 Equal<T, Options>...>;

static_assert(IsAny<int, float, int>::value);
static_assert(!IsAny<int, float, double>::value);

struct Item {
  using type = long long;
};
template <class T>
struct Box {
  using Alias = T;
  using type = typename Alias::type;
};
static_assert(__is_same(Box<Item>::type, long long));

template <class T>
using Identity = T;
template <class T>
using AliasType = typename Identity<T>::type;
static_assert(__is_same(AliasType<Item>, long long));

template <class T>
T source();
template <class T>
using DecltypeType = typename decltype(source<T>())::type;
static_assert(__is_same(DecltypeType<Item>, long long));

template <class T>
constexpr auto choose(T, int) -> typename T::type { return 1; }
template <class T>
constexpr int choose(T, ...) { return 2; }
static_assert(choose(Item{}, 0) == 1);
static_assert(choose(0, 0) == 2);
