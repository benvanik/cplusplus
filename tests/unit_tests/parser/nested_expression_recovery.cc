// RUN: %cxx -verify -fsyntax-only %s

template <class T>
concept arithmetic = requires(T a, T b) {
  // expected-error@1 {{expected ')'}}
  a (((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((* b;
};
