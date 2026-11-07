// RUN: %cxx -verify -fsyntax-only %s

using Alias = int;
int alias_value = Alias;  // expected-error {{'Alias' does not refer to a value}}

struct Record {};
int record_value = Record;  // expected-error {{'Record' does not refer to a value}}

enum Enumeration {};
int enum_value = Enumeration;  // expected-error {{'Enumeration' does not refer to a value}}

namespace ns {}
int namespace_value = ns;  // expected-error {{'ns' does not refer to a value}}

template <class T>
T invalid_value() {
  return T;  // expected-error {{'T' does not refer to a value}}
}

int instantiated = invalid_value<int>();
