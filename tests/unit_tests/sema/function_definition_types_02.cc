// RUN: %cxx -verify -fsyntax-only %s

struct Incomplete;
Incomplete declared(Incomplete);
void deleted(Incomplete) = delete;
Incomplete deleted_return() = delete;
void indirect(Incomplete*, Incomplete&, Incomplete&&, Incomplete[]) {}

struct Complete;
Complete forward(Complete);
struct Complete { int value; };
Complete forward(Complete value) { return value; }

struct Self {
  Self copy(Self value) { return value; }
  struct Nested;
  void delayed(Nested value) {}
  struct Nested {};
};

struct Abstract { virtual void operation() = 0; };
void abstract_deleted(Abstract) = delete;
Abstract abstract_deleted_return() = delete;
void abstract_indirect(Abstract&, Abstract*) {}

template <class T> struct Box { T value; };
void completed_specialization(Box<int> value) {}

template <class T> constexpr T identity(T value) { return value; }
static_assert(identity(7) == 7);
static_assert(identity(Complete{9}).value == 9);

template <class T> void unused(T) {}
template <class T> struct Unused { void consume(T value) {} };
Unused<Incomplete> declaration_only;

auto inferred() { return Complete{11}; }
auto lambda = [](Complete value) { return value.value; };
static_assert(lambda(Complete{13}) == 13);

template <class... T> void pack(T... values) {}
template void pack<int, Complete>(int, Complete);
