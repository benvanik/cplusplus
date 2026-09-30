// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

_Static_assert(sizeof(__bf16) == 2, "bfloat16 size");
_Static_assert(_Alignof(__bf16) == 2, "bfloat16 alignment");
_Static_assert(_Generic((__bf16)0, __bf16: 1, default: 0), "bfloat16 type");
_Static_assert(1.00390625bf16 == 1.0, "bfloat16 literal rounding");

__bf16 add(__bf16 left, __bf16 right) { return left + right; }
