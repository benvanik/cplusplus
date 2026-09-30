// RUN: %cxx -verify -fsyntax-only %s

constexpr bool bounded(unsigned first, unsigned second, unsigned third,
                       unsigned fourth, unsigned fifth, unsigned sixth) {
  return first < 256u && second < 256u && third < 256u && fourth < 256u &&
         fifth < 256u && sixth < 256u;
}

static_assert(bounded(0, 1, 2, 3, 4, 255));
static_assert(!bounded(0, 1, 2, 3, 4, 256));

namespace bounds {
constexpr unsigned first = 1;
constexpr unsigned second = 2;
}  // namespace bounds

static_assert(bounds::first < 2u && bounds::second < 3u);

struct Bounds {
  unsigned first;
  unsigned second;
};

constexpr Bounds pair{1, 2};
static_assert(pair.first < 2u && pair.second < 3u);

template <unsigned Limit>
constexpr bool below(unsigned value) {
  return value < Limit && Limit < 256u;
}

static_assert(below<16>(15));
static_assert(!below<16>(16));

constexpr int increment(int value) { return value + 10; }

template <class T>
constexpr T increment(T value) {
  return T(value) + T{1};
}

static_assert(increment(2) == 12);
static_assert(increment<unsigned>(2) == 3);

template <class T>
struct Box {
  template <class U>
  struct Rebind {
    using type = U;
  };

  template <class U>
  constexpr U get() const {
    return U{3};
  }
};

template <class T, class U>
constexpr U extract(const Box<T>& box) {
  return box.template get<U>();
}

constexpr Box<float> box;
static_assert(extract<float, int>(box) == 3);

template <class T>
struct Alias {
  using type = typename T::template Rebind<int>::type;
};

static_assert(sizeof(Alias<Box<int>>::type) == sizeof(int));
