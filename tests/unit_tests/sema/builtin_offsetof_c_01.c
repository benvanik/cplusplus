// RUN: %cxx -verify -fsyntax-only -fvalidate-ast %s

struct Leaf {
  char bytes[3];
  int value;
};

struct Owner {
  char lead;
  struct Leaf leaves[3];
};

_Static_assert(__builtin_offsetof(struct Owner, leaves[1].value) == 16,
               "compound member offset");
_Static_assert(__builtin_offsetof(struct Owner, leaves[2].bytes[1]) == 21,
               "compound array offset");

int runtime_index;
unsigned long runtime_offset =
    __builtin_offsetof(struct Owner, leaves[runtime_index].value); // expected-error {{offsetof index must be an integral constant expression}}
