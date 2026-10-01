// RUN: %cxx -verify -fsyntax-only %s

void narrow(__float8_e4m3fn e4m3fn, __float8_e5m2 e5m2, _Float16 binary,
            __bf16 bfloat, float single) {
  // expected-error@1 {{narrowing conversion from '__float8_e5m2' to '__float8_e4m3fn' in braced-init-list}}
  __float8_e4m3fn e5_to_e4{e5m2};
  // expected-error@1 {{narrowing conversion from '__float8_e4m3fn' to '__float8_e5m2' in braced-init-list}}
  __float8_e5m2 e4_to_e5{e4m3fn};
  // expected-error@1 {{narrowing conversion from '_Float16' to '__float8_e4m3fn' in braced-init-list}}
  __float8_e4m3fn binary_to_e4{binary};
  // expected-error@1 {{narrowing conversion from '__bf16' to '__float8_e5m2' in braced-init-list}}
  __float8_e5m2 bfloat_to_e5{bfloat};
  // expected-error@1 {{narrowing conversion from 'float' to '__float8_e4m3fn' in braced-init-list}}
  __float8_e4m3fn single_to_e4{single};
}

auto mixed(__float8_e4m3fn left, __float8_e5m2 right) {
  // expected-error@1 {{invalid operands of types '__float8_e4m3fn' and '__float8_e5m2' to binary operator '+'}}
  return left + right;
}
