// RUN: %cxx -verify -fsyntax-only %s

// expected-no-diagnostics

#ifdef CXX_HAS_INT128

#include <limits.h>

constexpr bool unsigned_add() {
  unsigned long long result = 0;
  bool overflow = __builtin_uaddll_overflow(~0ull, 1ull, &result);
  return overflow && result == 0;
}

constexpr bool unsigned_add_fits() {
  unsigned long long result = 0;
  bool overflow = __builtin_uaddll_overflow(1ull, 2ull, &result);
  return !overflow && result == 3;
}

constexpr bool signed_add() {
  int result = 0;
  bool overflow = __builtin_sadd_overflow(INT_MAX, 1, &result);
  return overflow && result == INT_MIN;
}

constexpr bool signed_sub_long() {
  long result = 0;
  bool overflow = __builtin_ssubl_overflow(LONG_MIN, 1L, &result);
  return overflow && result == LONG_MAX;
}

constexpr bool unsigned_sub() {
  unsigned result = 0;
  bool overflow = __builtin_usub_overflow(0u, 1u, &result);
  return overflow && result == ~0u;
}

constexpr bool signed_mul_long_long() {
  long long result = 0;
  bool overflow =
      __builtin_smulll_overflow(1LL << 40, 1LL << 40, &result);
  return overflow && result == 0;
}

constexpr bool unsigned_mul_long() {
  unsigned long result = 0;
  bool overflow = __builtin_umull_overflow(6ul, 7ul, &result);
  return !overflow && result == 42;
}

constexpr bool narrow_result_type() {
  int result = 0;
  bool overflow = __builtin_smul_overflow(65536, 65536, &result);
  return overflow && result == 0;
}

static_assert(unsigned_add() && unsigned_add_fits() && signed_add());
static_assert(signed_sub_long() && unsigned_sub() && signed_mul_long_long());
static_assert(unsigned_mul_long() && narrow_result_type());

constexpr bool add_with_carry_family() {
  unsigned char byte_carry = 0;
  if (__builtin_addcb(UCHAR_MAX, 1, 0, &byte_carry) != 0 || byte_carry != 1)
    return false;

  unsigned short short_carry = 0;
  if (__builtin_addcs(USHRT_MAX, 1, 0, &short_carry) != 0 ||
      short_carry != 1)
    return false;

  unsigned carry = 0;
  if (__builtin_addc(1, 0, UINT_MAX, &carry) != 0 || carry != 1)
    return false;
  if (__builtin_addc(UINT_MAX, UINT_MAX, UINT_MAX, &carry) != UINT_MAX - 2 ||
      carry != 1)
    return false;

  unsigned long long_carry = 0;
  if (__builtin_addcl(ULONG_MAX, 1, 0, &long_carry) != 0 || long_carry != 1)
    return false;

  unsigned long long long_long_carry = 0;
  if (__builtin_addcll(ULLONG_MAX, 1, 0, &long_long_carry) != 0 ||
      long_long_carry != 1)
    return false;

  return true;
}

constexpr bool subtract_with_borrow_family() {
  unsigned char byte_borrow = 0;
  if (__builtin_subcb(0, 1, 0, &byte_borrow) != UCHAR_MAX || byte_borrow != 1)
    return false;

  unsigned short short_borrow = 0;
  if (__builtin_subcs(0, 1, 0, &short_borrow) != USHRT_MAX ||
      short_borrow != 1)
    return false;

  unsigned borrow = 0;
  if (__builtin_subc(0, UINT_MAX, UINT_MAX, &borrow) != 2 || borrow != 1)
    return false;
  if (__builtin_subc(UINT_MAX, 0, UINT_MAX, &borrow) != 0 || borrow != 0)
    return false;

  unsigned long long_borrow = 0;
  if (__builtin_subcl(0, 1, 0, &long_borrow) != ULONG_MAX || long_borrow != 1)
    return false;

  unsigned long long long_long_borrow = 0;
  if (__builtin_subcll(0, 1, 0, &long_long_borrow) != ULLONG_MAX ||
      long_long_borrow != 1)
    return false;

  return true;
}

static_assert(add_with_carry_family());
static_assert(subtract_with_borrow_family());

#endif
