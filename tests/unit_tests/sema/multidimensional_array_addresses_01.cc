// RUN: %cxx -std=c++23 -fsyntax-only -verify %s
// RUN: %cxx -std=c++23 -fsyntax-only -dump-symbols %s | %filecheck %s --check-prefix=NAMES

struct Cell {
  unsigned value;
};

struct Self {
  unsigned value = 9;
  const Self* self = this;
};

constexpr unsigned scalar_table[2][2] = {{3, 5}, {7, 11}};
static_assert(scalar_table[1][0] == 7);
static_assert(&scalar_table[0][1] != &scalar_table[1][0]);
static_assert(&scalar_table[0] + 1 == &scalar_table[1]);
constexpr const unsigned* scalar_pointer = scalar_table[1];
static_assert(scalar_pointer + 1 == &scalar_table[1][1]);

constexpr const unsigned& select_scalar(const unsigned (&values)[2][2]) {
  return values[1][0];
}
static_assert(&select_scalar(scalar_table) == &scalar_table[1][0]);

template <const unsigned&>
struct ScalarRef {};
static_assert(__is_same(ScalarRef<scalar_table[1][0]>,
                        ScalarRef<select_scalar(scalar_table)>));
static_assert(!__is_same(ScalarRef<scalar_table[0][1]>,
                         ScalarRef<scalar_table[1][0]>));
ScalarRef<scalar_table[1][0]> scalar_ref;

// NAMES-DAG: class ScalarRef<scalar_table[0][1]>
// NAMES-DAG: class ScalarRef<scalar_table[1][0]>
// NAMES-DAG: variable ::ScalarRef<scalar_table[1][0]> scalar_ref

constexpr bool automatic_scalars() {
  unsigned values[2][2] = {{13, 17}, {19, 23}};
  unsigned* selected = &values[1][0];
  *selected = 29;
  unsigned& referenced = values[0][1];
  referenced = 31;
  return values[1][0] == 29 && values[0][1] == 31 &&
         selected != &values[0][1];
}
static_assert(automatic_scalars());

constexpr Cell record_table[2][2] = {{{3}, {5}}, {{7}, {11}}};
static_assert(record_table[1][0].value == 7);
static_assert(&record_table[0][1] != &record_table[1][0]);
static_assert(&record_table[0][1].value != &record_table[1][0].value);

constexpr const Cell& select_record(const Cell (&values)[2][2]) {
  return values[1][0];
}
static_assert(&select_record(record_table) == &record_table[1][0]);

constexpr bool automatic_records() {
  Cell values[2][2] = {{{13}, {17}}, {{19}, {23}}};
  Cell* selected = &values[1][0];
  selected->value = 29;
  unsigned& member = values[0][1].value;
  member = 31;
  return values[1][0].value == 29 && values[0][1].value == 31 &&
         selected != &values[0][1];
}
static_assert(automatic_records());

constexpr Self self_table[2][2] = {};
static_assert(self_table[0][1].self == &self_table[0][1]);
static_assert(self_table[1][0].self == &self_table[1][0]);
static_assert(self_table[0][1].self != self_table[1][0].self);

constexpr unsigned cube[2][2][2] = {{{2, 3}, {5, 7}},
                                     {{11, 13}, {17, 19}}};
static_assert(cube[1][0][1] == 13);
static_assert(&cube[0][1][1] != &cube[1][0][0]);

constexpr const Cell (&temporary_records)[2][2] = {};
static_assert(temporary_records[1][0].value == 0);
static_assert(&temporary_records[0][1] != &temporary_records[1][0]);

// expected-no-diagnostics
