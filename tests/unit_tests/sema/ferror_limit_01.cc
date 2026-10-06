// RUN: %cxx -verify -fsyntax-only -ferror-limit 2 %s

template <int>
struct S;

// expected-error@+2 {{template argument is not a converted constant expression of type 'int'}}
// expected-error@+1 {{expected a declarator}}
S<"bad">;
// No more errors expected - limit reached
S<"bad">;
S<"bad">;
S<"bad">;
