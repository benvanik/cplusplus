// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

using Bool4 = bool __attribute__((ext_vector_type(4)));
using Char4 = char __attribute__((ext_vector_type(4)));
using UInt4 = unsigned __attribute__((ext_vector_type(4)));

static_assert(__has_builtin(__builtin_reduce_add));
static_assert(__has_builtin(__builtin_reduce_and));
static_assert(__has_builtin(__builtin_reduce_mul));
static_assert(__has_builtin(__builtin_reduce_or));
static_assert(__has_builtin(__builtin_reduce_xor));

static_assert(__is_same(decltype(__builtin_reduce_add(UInt4{})), unsigned));
static_assert(__is_same(decltype(__builtin_reduce_and(Bool4{})), bool));
static_assert(__is_same(decltype(__builtin_reduce_and(UInt4{})), unsigned));
static_assert(__is_same(decltype(__builtin_reduce_mul(Char4{})), char));
static_assert(__is_same(decltype(__builtin_reduce_or(Char4{})), char));
static_assert(__is_same(decltype(__builtin_reduce_xor(UInt4{})), unsigned));

static_assert(__builtin_reduce_and(Bool4{true, true, true, true}));
static_assert(!__builtin_reduce_and(Bool4{true, true, false, true}));
static_assert(__builtin_reduce_or(Bool4{false, false, true, false}));
static_assert(!__builtin_reduce_or(Bool4{false, false, false, false}));

static_assert(__builtin_reduce_and(UInt4{15, 7, 3, 1}) == 1u);
static_assert(__builtin_reduce_or(UInt4{1, 2, 4, 8}) == 15u);
static_assert(__builtin_reduce_xor(UInt4{1, 2, 3, 4}) == 4u);
static_assert(__builtin_reduce_and(Char4{-1, 7, 3, 1}) == 1);

template <typename T>
constexpr auto reduceOr(T value) {
  return __builtin_reduce_or(value);
}

static_assert(reduceOr(UInt4{1, 2, 4, 8}) == 15u);
