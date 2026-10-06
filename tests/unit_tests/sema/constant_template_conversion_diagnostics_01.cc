// RUN: %cxx -std=c++26 -verify -fsyntax-only %s

template <unsigned char N>
struct Byte {};

// expected-error@+1 {{narrowing conversion from 'int' to 'unsigned char' in converted constant expression: integer constant 256 is outside the range of 'unsigned char'}}
Byte<256> byte;

template <_Float16 N>
struct Half {};

// expected-error@+1 {{narrowing conversion from 'float' to '_Float16' in converted constant expression: constant value 65520 rounds to infinity in '_Float16'; maximum finite value is 65504}}
Half<65520.0f> half;

template <float N>
struct Float {};

// expected-error@+1 {{narrowing conversion from 'double' to 'float' in converted constant expression}}
Float<0.1> floating;
