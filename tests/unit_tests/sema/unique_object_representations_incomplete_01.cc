// RUN: %cxx -verify -fsyntax-only %s

struct Incomplete;

// expected-error@+1 {{incomplete type '::Incomplete' where a complete type is required}}
constexpr bool incomplete = __has_unique_object_representations(Incomplete);
