// RUN: %cxx --target=x86_64-pc-windows-msvc -verify -fsyntax-only %s
// RUN: %cxx --target=aarch64-pc-windows-msvc -verify -fsyntax-only %s

struct OrdinaryRecord {
  unsigned value;
};

static_assert(sizeof(OrdinaryRecord) == 4);

// expected-error@+1 {{Microsoft ABI bit-field layout is not supported}}
struct UnsupportedBitfieldRecord {
  unsigned bits : 3;
};
