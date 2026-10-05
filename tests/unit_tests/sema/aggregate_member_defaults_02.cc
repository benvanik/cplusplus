// RUN: %cxx -std=c++23 -fsyntax-only -verify %s

struct Later {
  unsigned first = second;
  unsigned second = 9;
};

// expected-error@+1 {{constexpr variable must be initialized}}
constexpr Later later{};

struct Self {
  unsigned value = value;
};

// expected-error@+1 {{constexpr variable must be initialized}}
constexpr Self self{};

struct Conditional {
  bool readLater;
  unsigned first = readLater ? second : 7;
  unsigned second = 9;
};

// expected-error@+1 {{constexpr variable must be initialized}}
constexpr Conditional conditional{true};

extern unsigned runtimeValue;

struct Runtime {
  unsigned value = runtimeValue;
};

// expected-error@+1 {{constexpr variable must be initialized}}
constexpr Runtime runtime{};
