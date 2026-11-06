// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

namespace different_arity {
template <class T>
constexpr int rotation(int) = delete;
template <>
constexpr int rotation<unsigned>(int value) {
  return value + 1;
}
template <>
constexpr int rotation<unsigned long long>(int value) {
  return value + 3;
}

template <class T>
constexpr int rotation(int, int) = delete;
template <>
constexpr int rotation<unsigned>(int lhs, int rhs) {
  return lhs + rhs;
}
template <>
constexpr int rotation<unsigned long long>(int lhs, int rhs) {
  return lhs + rhs + 1;
}

static_assert(rotation<unsigned>(4) == 5);
static_assert(rotation<unsigned long long>(4) == 7);
static_assert(rotation<unsigned>(4, 2) == 6);
static_assert(rotation<unsigned long long>(4, 2) == 7);
}  // namespace different_arity

namespace same_arity {
template <class T>
constexpr int classify(T*) = delete;
template <class T>
constexpr int classify(T) = delete;

template <>
constexpr int classify<unsigned>(unsigned*) {
  return 11;
}
template <>
constexpr int classify<unsigned>(unsigned) {
  return 22;
}

static_assert(classify<unsigned>(static_cast<unsigned*>(nullptr)) == 11);
static_assert(classify<unsigned>(0u) == 22);
static_assert(classify<unsigned>(0) == 11);
}  // namespace same_arity

namespace qualified_definition {
template <class T>
constexpr unsigned combine(T) = delete;
template <class T>
constexpr unsigned combine(T, unsigned) = delete;
template <>
constexpr unsigned combine<unsigned>(unsigned, unsigned);
template <>
constexpr unsigned combine<unsigned>(unsigned);
}  // namespace qualified_definition

template <>
constexpr unsigned qualified_definition::combine<unsigned>(unsigned value) {
  return value + 7;
}
template <>
constexpr unsigned qualified_definition::combine<unsigned>(unsigned value,
                                                           unsigned bias) {
  return value + bias + 13;
}
static_assert(qualified_definition::combine(11u) == 18);
static_assert(qualified_definition::combine(11u, 3u) == 27);
