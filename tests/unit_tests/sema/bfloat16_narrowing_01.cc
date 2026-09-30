// RUN: %cxx -verify -fsyntax-only %s

void narrow(__bf16 bfloat, _Float16 binary) {
  // expected-error@1 {{narrowing conversion from '_Float16' to '__bf16' in braced-init-list}}
  __bf16 to_bfloat{binary};
  // expected-error@1 {{narrowing conversion from '__bf16' to '_Float16' in braced-init-list}}
  _Float16 to_binary{bfloat};
}
