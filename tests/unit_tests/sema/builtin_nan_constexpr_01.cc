// RUN: %cxx -verify -fsyntax-only %s

// Payloads remain readable after array-to-pointer conversion, including from
// a suffix of namespace-scope storage.
constexpr char global_payload[] = "x1";
static_assert(__builtin_bit_cast(unsigned,
                                 __builtin_nanf(global_payload + 1)) ==
              __builtin_bit_cast(unsigned, __builtin_nanf("1")));
static_assert(__builtin_bit_cast(unsigned, __builtin_nanf("1")) ==
              0x7fc00001u);
static_assert(__builtin_bit_cast(unsigned, __builtin_nanf("077")) ==
              0x7fc0003fu);

// Function-local arrays remain readable while their invocation is active.
static constexpr bool local_payload() {
  const char payload[] = "x0x123";
  return __builtin_bit_cast(unsigned long long,
                            __builtin_nan(payload + 1)) ==
         __builtin_bit_cast(unsigned long long, __builtin_nan("0x123"));
}
static_assert(local_payload());
static_assert(__builtin_bit_cast(unsigned long long,
                                 __builtin_nan("0x123")) ==
              0x7ff8000000000123ull);

// Long-double payloads use the same retained-address reader.
constexpr char long_double_payload[] = "x1";
static_assert(__builtin_nanl(long_double_payload + 1) !=
              __builtin_nanl(long_double_payload + 1));

// Storage that has left its automatic lifetime cannot provide a payload.
static constexpr double expired_payload() {
  const char* address = nullptr;
  {
    const char payload[] = "1";
    address = payload;
  }
  return __builtin_nan(address);
}
// expected-error@+1 {{constexpr variable must be initialized}}
constexpr double expired = expired_payload();

// A tag must be consumed completely by the integer grammar.
// expected-error@+1 {{constexpr variable must be initialized}}
constexpr float invalid_octal_payload = __builtin_nanf("08");
