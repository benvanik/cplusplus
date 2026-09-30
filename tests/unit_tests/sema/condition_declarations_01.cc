// RUN: %cxx -std=c++23 -fsyntax-only -verify %s

constexpr int if_condition() {
  if (int value = 3) return value;
  return 0;
}

constexpr int switch_condition() {
  switch (int value = 2) {
    case 2:
      return value;
    default:
      return 0;
  }
}

constexpr int while_condition() {
  int cursor = 0;
  int total = 0;
  while (int remaining = 3 - cursor) {
    total += remaining;
    ++cursor;
  }
  return total;
}

constexpr int for_condition() {
  int total = 0;
  for (int cursor = 0; int remaining = 3 - cursor;
       total += remaining, ++cursor) {}
  return total;
}

constexpr int reference_condition() {
  int input = 4;
  if (const int& value = input) return value;
  return 0;
}

struct Guard {
  int value;
  int* destructionCount;

  constexpr explicit operator bool() const { return value != 0; }
  constexpr ~Guard() { ++*destructionCount; }
};

constexpr int condition_lifetime() {
  int cursor = 0;
  int destructionCount = 0;
  while (Guard guard{2 - cursor, &destructionCount}) {
    ++cursor;
  }
  return cursor * 10 + destructionCount;
}

template <int Value>
constexpr int template_condition() {
  if (int value = Value) return value;
  return 0;
}

template <int Value>
constexpr int template_self_reference_condition() {
  if (int value = ((void)&value, Value)) return value;
  return 0;
}

constexpr int constexpr_if_condition() {
  if constexpr (const int value = 7) return value;
  return 0;
}

static_assert(if_condition() == 3);
static_assert(switch_condition() == 2);
static_assert(while_condition() == 6);
static_assert(for_condition() == 6);
static_assert(reference_condition() == 4);
static_assert(condition_lifetime() == 23);
static_assert(template_condition<5>() == 5);
static_assert(template_self_reference_condition<6>() == 6);
static_assert(constexpr_if_condition() == 7);
