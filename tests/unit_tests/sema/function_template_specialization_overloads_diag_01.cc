// RUN: %cxx -verify -fsyntax-only %s

template <class T>
int select(T) = delete;
template <class T>
int select(T*) = delete;

template <>
int select<unsigned>(unsigned) { return 1; }  // expected-note {{previous definition is here}}
template <>
int select<unsigned>(unsigned*) { return 2; }  // expected-note {{previous definition is here}}

template <>
int select<unsigned>(unsigned) { return 3; }  // expected-error {{redefinition of 'select<>'}}
template <>
int select<unsigned>(unsigned*) { return 4; }  // expected-error {{redefinition of 'select<>'}}
