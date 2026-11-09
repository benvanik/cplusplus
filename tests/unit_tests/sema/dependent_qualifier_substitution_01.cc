// RUN: %cxx -verify -fsyntax-only %s
// expected-no-diagnostics

template <long A, long B>
constexpr long gcd = gcd<B, A % B>;
template <long A>
constexpr long gcd<A, 0> = A;

template <long N, long D>
struct Ratio {
  static constexpr long num = N;
  static constexpr long den = D;
};

template <class Left, class Right>
struct Divide {
  static constexpr long factor = gcd<Left::num, Right::num>;
  using type = Ratio<Left::num / factor, Right::num / factor>;
};

template <class Left, class Right>
using DivideAlias = typename Divide<Left, Right>::type;

// Instantiating the enclosing template does not resolve the member's template
// parameters. Their qualifiers must remain dependent throughout substitution.
template <class Tag>
struct Converter {
  template <class Left, class Right,
            class Result = DivideAlias<typename Left::period,
                                       typename Right::period>>
  using result = Result;
};

struct Left {
  using period = Ratio<6, 1>;
};
struct Right {
  using period = Ratio<4, 1>;
};
using Result = Converter<void>::result<Left, Right>;
static_assert(Result::num == 3);
static_assert(Result::den == 2);
