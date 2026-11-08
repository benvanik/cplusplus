// clang-format off
// RUN: %cxx -verify -fsyntax-only %s

// expected-no-diagnostics

// Basic arithmetic conversions
static_assert(__is_convertible(int, long), "int to long");
static_assert(__is_convertible(int, double), "int to double");
static_assert(__is_convertible(float, double), "float to double");
static_assert(__is_convertible(char, int), "char to int");

// Same type
static_assert(__is_convertible(int, int), "same type");

// Void conversions
static_assert(__is_convertible(void, void), "void to void");
static_assert(!__is_convertible(void, int), "void to int");
static_assert(!__is_convertible(int, void), "int to void");

// Bool conversions
static_assert(__is_convertible(int, bool), "int to bool");
static_assert(__is_convertible(double, bool), "double to bool");

// Pointer conversions
static_assert(__is_convertible(int*, void*), "ptr to void ptr");
static_assert(!__is_convertible(void*, int*), "void ptr to ptr");

// Null pointer
static_assert(__is_convertible(decltype(nullptr), int*), "nullptr to ptr");

// __is_convertible_to is the same
static_assert(__is_convertible_to(int, long), "is_convertible_to also works");

// Class hierarchy
struct Base {};
struct Derived : Base {};

static_assert(__is_convertible(Derived, Base), "derived to base");
static_assert(__is_convertible(Derived*, Base*), "derived ptr to base ptr");
static_assert(!__is_convertible(Base, Derived), "base to derived");

// Nothrow conversions
static_assert(__is_nothrow_convertible(int, long), "arithmetic conversion");
static_assert(__is_nothrow_convertible(void, void), "void to void");
static_assert(!__is_nothrow_convertible(void, int), "void to int");

struct NothrowConversion {
  operator int() noexcept;
};

struct ThrowingConversion {
  operator int() noexcept(false);
};

static_assert(__is_nothrow_convertible(NothrowConversion, int),
              "nothrow conversion function");
static_assert(!__is_nothrow_convertible(ThrowingConversion, int),
              "throwing conversion function");

struct NothrowDestination {
  NothrowDestination(int) noexcept;
};

struct ThrowingDestination {
  ThrowingDestination(int) noexcept(false);
};

static_assert(__is_nothrow_convertible(int, NothrowDestination),
              "nothrow converting constructor");
static_assert(!__is_nothrow_convertible(int, ThrowingDestination),
              "throwing converting constructor");

struct ThrowingDestructor {
  ThrowingDestructor(int) noexcept;
  ~ThrowingDestructor() noexcept(false);
};

static_assert(!__is_nothrow_convertible(int, ThrowingDestructor),
              "throwing destination destructor");

template <class From, class To>
inline constexpr bool is_nothrow_convertible_v =
    __is_nothrow_convertible(From, To);

static_assert(is_nothrow_convertible_v<int, long>, "template type operands");
