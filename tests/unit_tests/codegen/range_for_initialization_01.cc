// REQUIRES: native-x86_64-linux
// RUN: %cxx --target=x86_64-unknown-linux-gnu -std=c++23 %s -o %t
// RUN: %t

int copies = 0;
int destroyed = 0;

struct Sentinel {
  int limit;
};

struct Iterator {
  int value;

  Iterator(int value) : value(value) {}
  Iterator(const Iterator& other) : value(other.value) { ++copies; }
  ~Iterator() { ++destroyed; }

  int operator*() const { return value; }
  void operator++() { ++value; }
  bool operator!=(Sentinel end) const { return value != end.limit; }
};

struct Range {
  Iterator first{0};

  Iterator& begin() { return first; }
  Sentinel end() { return {4}; }
};

template <class T, unsigned N>
int sum(T (&values)[N]) {
  int result = 0;
  for (auto value : values) result += value;
  return result;
}

int main() {
  const int values[3] = {2, 3, 7};
  if (sum(values) != 12) return 1;

  int rows[2][2] = {{1, 2}, {3, 4}};
  for (auto& [a, b] : rows) {
    a += 10;
    b *= 2;
  }
  if (rows[0][0] != 11 || rows[1][1] != 8) return 2;

  int result = 0;
  {
    Range range;
    for (int value : range) {
      if (value == 1) continue;
      if (value == 3) break;
      result += value;
    }
    if (result != 2 || range.first.value != 0 || copies != 1) return 3;
    if (destroyed != 1) return 4;
  }
  if (destroyed != 2) return 5;
  return 0;
}
