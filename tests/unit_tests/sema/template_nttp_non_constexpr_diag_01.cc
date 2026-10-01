// RUN: %cxx -verify -fsyntax-only -fstrict-templates %s

template <int N>
struct Box {};

int runtime();

// expected-error@+1 {{template argument is not a converted constant expression of type 'int'}}
template struct Box<runtime()>;
