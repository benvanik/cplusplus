// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

_Static_assert(sizeof(__float8_e4m3fn) == 1, "E4M3FN size");
_Static_assert(_Alignof(__float8_e4m3fn) == 1, "E4M3FN alignment");
_Static_assert(sizeof(__float8_e5m2) == 1, "E5M2 size");
_Static_assert(_Alignof(__float8_e5m2) == 1, "E5M2 alignment");
_Static_assert(_Generic((__float8_e4m3fn)0, __float8_e4m3fn: 1, default: 0),
               "E4M3FN type");
_Static_assert(_Generic((__float8_e5m2)0, __float8_e5m2: 1, default: 0),
               "E5M2 type");
_Static_assert(_Generic((__float8_e4m3fn)0 + 1, __float8_e4m3fn: 1,
                        default: 0),
               "E4M3FN arithmetic type");
_Static_assert(_Generic(1 + (__float8_e5m2)0, __float8_e5m2: 1, default: 0),
               "E5M2 arithmetic type");

__float8_e4m3fn add_e4m3fn(__float8_e4m3fn left,
                           __float8_e4m3fn right) {
  return left + right;
}

__float8_e5m2 add_e5m2(__float8_e5m2 left, __float8_e5m2 right) {
  return left + right;
}
