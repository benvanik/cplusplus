// RUN: %cxx -verify -fsyntax-only %s

int sum_array() {
  int arr[4] = {1, 2, 3, 4};
  int total = 0;
  for (int x : arr) {
    total = total + x;
  }
  return total;
}

void scalar_range(int value) {
  // expected-error@+1 {{range-based for requires an array or class range}}
  for (int element : value) {}
}

void pointer_range(int* values) {
  // expected-error@+1 {{range-based for requires an array or class range}}
  for (int element : values) {}
}

void unbounded_range(int (&values)[]) {
  // expected-error@+1 {{range-based for requires an array with a known bound}}
  for (int element : values) {}
}
