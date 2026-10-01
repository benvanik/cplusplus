// RUN: %cxx -std=c++26 -verify -fsyntax-only %s
// expected-no-diagnostics

template <class T>
struct Scalar {
  T value;
  constexpr operator T() const { return value; }
};

constexpr Scalar<unsigned> count{5};
constexpr unsigned direct = 9;

template <unsigned N>
constexpr unsigned choose() {
  return N;
}

static_assert(choose<count>() == 5);
static_assert(choose<direct>() == 9);

template <unsigned N = count>
constexpr unsigned defaulted() {
  return N;
}

static_assert(defaulted<>() == 5);

template <unsigned... N>
constexpr unsigned sum() {
  return (N + ...);
}

static_assert(sum<count, 7>() == 12);

template <class T, T N>
constexpr T converted() {
  return N;
}

static_assert(converted<unsigned, count>() == 5);

constexpr Scalar<int> bias{-3};
static_assert(converted<int, bias>() == -3);

template <auto N>
constexpr auto deduced() {
  return N;
}

static_assert(deduced<5u>() == 5u);
static_assert(__is_same(decltype(deduced<5u>()), unsigned));

static_assert(converted<bool, 1>());
static_assert(!converted<bool, 0>());
constexpr char small = 7;
static_assert(converted<int, small>() == 7);
static_assert(converted<signed char, -128>() == -128);
static_assert(converted<long long, 0x7fffffffffffffffULL>() ==
              0x7fffffffffffffffLL);

struct Floating {
  constexpr operator float() const { return 1.5f; }
};

struct ExactFloatingConversion {
  constexpr operator double() const { return 1.5; }
};

struct InexactFloatingConversion {
  constexpr operator double() const { return 0.1; }
};

constexpr Floating floating{};
constexpr float promoted = 1.5f;
static_assert(converted<float, floating>() == 1.5f);
static_assert(converted<float, ExactFloatingConversion{}>() == 1.5f);
static_assert(converted<float, 1.5>() == 1.5f);
static_assert(converted<double, promoted>() == 1.5);

template <class T>
concept Accepted = requires { choose<T{}>(); };

struct Runtime {
  operator unsigned() const { return 5; }
};

struct Explicit {
  explicit constexpr operator unsigned() const { return 5; }
};

struct Deleted {
  constexpr operator unsigned() const = delete;
};

struct Private {
 private:
  constexpr operator unsigned() const { return 5; }
};

static_assert(!Accepted<Runtime>);
static_assert(!Accepted<Explicit>);
static_assert(!Accepted<Deleted>);
static_assert(!Accepted<Private>);

template <class T>
unsigned runtimeValue();

template <class T>
concept AcceptsRuntimeValue = requires { choose<runtimeValue<T>()>(); };

static_assert(!AcceptsRuntimeValue<int>);

template <auto N>
concept FitsByte = requires { converted<unsigned char, N>(); };

static_assert(FitsByte<255>);
static_assert(!FitsByte<256>);
static_assert(!FitsByte<-1>);

template <auto N>
concept FitsBool = requires { converted<bool, N>(); };

static_assert(FitsBool<0>);
static_assert(FitsBool<1>);
static_assert(!FitsBool<2>);
static_assert(!FitsBool<1.0f>);

struct Wide {
  constexpr operator unsigned() const { return 256; }
};

template <class T>
concept FitsConvertedByte = requires { converted<unsigned char, T{}>(); };

static_assert(!FitsConvertedByte<Wide>);

template <auto N>
concept FitsFloat = requires { converted<float, N>(); };

template <class T>
concept FitsConvertedFloat = requires { converted<float, T{}>(); };

static_assert(!FitsFloat<0.1>);
static_assert(!FitsFloat<1>);
static_assert(!FitsConvertedFloat<InexactFloatingConversion>);

template <auto N>
concept FitsInt = requires { converted<int, N>(); };

static_assert(!FitsInt<1.0f>);

static_assert(converted<int*, nullptr>() == nullptr);

template <auto N>
concept FitsPointer = requires { converted<int*, N>(); };

static_assert(!FitsPointer<0>);

int object;
static_assert(converted<const int*, &object>() == &object);

constexpr int array[1] = {};
static_assert(converted<const int*, array>() == array);

void function() noexcept {}
using FunctionPointer = void (*)();
static_assert(converted<FunctionPointer, function>() == function);

void overloaded() {}
void overloaded(int) {}
static_assert(converted<FunctionPointer, overloaded>() ==
              static_cast<FunctionPointer>(overloaded));
