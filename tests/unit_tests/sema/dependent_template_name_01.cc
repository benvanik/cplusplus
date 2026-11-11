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

// Concrete arguments do not make a template-template parameter's specialization
// nondependent. Outer instantiation must preserve the member template's names.
namespace template_template_qualifiers {
template <class... Ts>
struct All {
  static constexpr bool value = (Ts::value && ...);
  using type = int;
};

template <class... Ts>
using AllAlias = All<Ts...>;

template <bool, class T>
struct EnableIf {};
template <class T>
struct EnableIf<true, T> {
  using type = T;
};

template <class... Ts>
struct Tuple {
  template <template <class...> class A = All,
            bool = A<Boolean<true>>::value>
  constexpr Tuple(const Ts&...) : count(sizeof...(Ts)) {}
  int count;
};
static_assert(Tuple<int>(1).count == 1);
static_assert(Tuple<int, double>(1, 2.0).count == 2);

template <class... Ts>
struct ConstrainedTuple {
  template <template <class...> class A = AllAlias,
            typename EnableIf<A<Boolean<true>>::value, int>::type = 0>
  constexpr ConstrainedTuple(const Ts&...) : count(sizeof...(Ts)) {}
  int count;
};
static_assert(ConstrainedTuple<int>(1).count == 1);

template <class... Ts>
struct ShadowedTuple {
  template <template <class...> class All = All,
            bool = All<Boolean<true>>::value>
  constexpr ShadowedTuple(const Ts&...) : count(sizeof...(Ts)) {}
  int count;
};
static_assert(ShadowedTuple<int>(1).count == 1);

template <class T>
struct Nested {
  template <template <class...> class A>
  using Type = typename A<Boolean<true>>::type;

  template <template <class...> class A>
  static constexpr bool value() {
    return A<>::value;
  }
};
static_assert(__is_same(Nested<int>::Type<AllAlias>, int));
static_assert(Nested<int>::value<All>());
}  // namespace template_template_qualifiers
