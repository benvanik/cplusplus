// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

using Int4 = int __attribute__((ext_vector_type(4)));
using UInt4 = unsigned __attribute__((vector_size(16)));
using Float4 = float __attribute__((ext_vector_type(4)));
using Complex = __complex__ double;

struct State {
  Int4 value;
};

void assign_pointer(Int4* output, int first) {
  *output = {first, 1, 2, 3};
}

Int4 assign_local(int first) {
  Int4 value{};
  value = {first};
  return value;
}

State assign_member(int first) {
  State state{};
  state.value = {first, 9};
  return state;
}

void clear_vector(volatile UInt4* output) { *output = {}; }

void assign_complex(Complex* output, double real, double imaginary) {
  *output = {real, imaginary};
}

constexpr int sum(Int4 value) {
  return value[0] + value[1] + value[2] + value[3];
}

constexpr int sum_gnu(UInt4 value) {
  return value[0] + value[1] + value[2] + value[3];
}

static constexpr int choose(Int4 value) { return 1; }
static constexpr int choose(Float4 value) { return 2; }

static_assert(sum({1, 2, 3, 4}) == 10);
static_assert(sum({7}) == 7);
static_assert(sum({}) == 0);
static_assert(sum_gnu({7}) == 7);
static_assert(choose({1, 2, 3, 4}) == 1);
static_assert(choose({1.0f, 2.0f, 3.0f, 4.0f}) == 2);

constexpr int assigned_lane(unsigned lane) {
  Int4 value{11, 12, 13, 14};
  value = {7};
  return value[lane];
}

static_assert(assigned_lane(0) == 7);
static_assert(assigned_lane(3) == 0);

constexpr int referenced_lane(const Int4& value, unsigned lane) {
  return value[lane];
}

static_assert(referenced_lane({1, 2, 3, 4}, 3) == 4);

int rvalue_lane(Int4&& value) { return value[2]; }

int pass_rvalue_reference() { return rvalue_lane({1, 2, 3, 4}); }

Complex identity_complex(Complex value) { return value; }

void consume_complex(const Complex& value) {}

Complex pass_complex(double real, double imaginary) {
  consume_complex({real, imaginary});
  return identity_complex({real, imaginary});
}
