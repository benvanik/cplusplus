// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

typedef int Int4 __attribute__((ext_vector_type(4)));
typedef float Float4 __attribute__((ext_vector_type(4)));

_Static_assert(__builtin_convertvector((Float4){1.75f, -2.75f, 257.5f, 0.0f},
                                       Int4)[0] == 1);
_Static_assert(__builtin_convertvector((Float4){1.75f, -2.75f, 257.5f, 0.0f},
                                       Int4)[1] == -2);
_Static_assert(__builtin_convertvector((Float4){1.75f, -2.75f, 257.5f, 0.0f},
                                       Int4)[2] == 257);
_Static_assert(__builtin_convertvector((Float4){1.75f, -2.75f, 257.5f, 0.0f},
                                       Int4)[3] == 0);
