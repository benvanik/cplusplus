// clang-format off
// RUN: %cxx -verify -fsyntax-only %s

// Dependent arguments do not supply a missing namespace or template name.
template <int N>
auto missing_namespace() -> missing::type<N>; // expected-error {{nested name specifier must be a class or namespace}}
template <int N>
auto missing_plain_type() -> missing::type; // expected-error {{nested name specifier must be a class or namespace}}
template <int N>
auto missing_template() -> Missing<N>; // expected-error {{'Missing<>' is not a type}}

namespace known {
struct Type {};
}
template <int N>
auto missing_member_template() -> known::missing<N>; // expected-error {{'missing<>' is not a type}}
template <int N>
auto missing_nested_scope() -> known::missing::type<N>; // expected-error {{nested name specifier must be a class or namespace}}
template <int N>
auto missing_class_member() -> known::Type::missing<N>; // expected-error {{'missing<>' is not a type}}

// Neither disambiguator changes lookup of a nondependent name.
template <int N>
using MissingNamespace = typename missing::template type<N>; // expected-error {{nested name specifier must be a class or namespace}}
template <int N>
using MissingMember = typename known::template missing<N>; // expected-error {{no type named 'missing' in 'known'}}
template <int N>
using NotATemplate = typename known::template Type<N>; // expected-error {{no type named 'Type' in 'known'}}
template <int N>
using MissingTemplateScope = typename Missing<N>::type; // expected-error {{nested name specifier must be a class or namespace}}
template <int N>
using MissingMemberScope = typename known::template missing<N>::type; // expected-error {{nested name specifier must be a class or namespace}}

using MissingPlainScope = typename missing::type; // expected-error {{nested name specifier must be a class or namespace}}
using MissingPlainMember = typename known::missing; // expected-error {{no type named 'missing' in 'known'}}

// A declared value or scalar alias is not a dependent scope.
constexpr int value = 0;
using Scalar = int;
template <int N>
auto value_scope() -> value::type<N>; // expected-error {{nested name specifier must be a class or namespace}}
template <int N>
using ScalarScope = typename Scalar::type; // expected-error {{nested name specifier must be a class or namespace}}
template <int N>
using DecltypeScope = typename decltype(0)::type; // expected-error {{nested name specifier must be a class or namespace}}

template <int N>
int missing_value() {
  return missing::value + N; // expected-error {{nested name specifier must be a class or namespace}}
}

template <class T>
struct MissingBase : missing::Base {}; // expected-error {{nested name specifier must be a class or namespace}}
template <class T>
struct MissingUsing {
  using missing::value; // expected-error {{nested name specifier must be a class or namespace}}
};
