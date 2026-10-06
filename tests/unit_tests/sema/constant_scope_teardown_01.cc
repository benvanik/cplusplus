// RUN: %cxx -verify -fsyntax-only %s

namespace constant_scope_teardown {
constexpr void append_digit(unsigned* output, unsigned digit) {
  *output = *output * 10 + digit;
}

struct Guard {
  unsigned* output;
  unsigned digit;
  constexpr ~Guard() { append_digit(output, digit); }
};

constexpr unsigned nested_blocks() {
  unsigned output = 1;
  {
    Guard outer{&output, 2};
    {
      Guard inner{&output, 3};
    }
  }
  return output;
}
static_assert(nested_blocks() == 132);

constexpr unsigned early_exit() {
  unsigned output = 4;
  while (true) {
    Guard outer{&output, 5};
    {
      Guard inner{&output, 6};
      break;
    }
  }
  return output;
}
static_assert(early_exit() == 465);
}  // namespace constant_scope_teardown
