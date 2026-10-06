// RUN: %cxx -verify -fsyntax-only %s

constexpr unsigned left = 5, right = 5;
constexpr const unsigned& alias = left;
template <const unsigned& value>
struct Ref {};
static_assert(__is_same(Ref<left>, Ref<left>));
static_assert(__is_same(Ref<left>, Ref<alias>));
static_assert(!__is_same(Ref<left>, Ref<right>));

// expected-note@+3 {{candidate function not viable: substitution failure: template argument is not a constant expression}}
// expected-note@+2 {{candidate function not viable: substitution failure: template argument is not a converted constant expression of type 'const unsigned int&'}}
template <const unsigned& value>
constexpr const unsigned* address() {
  return &value;
}
static_assert(address<left>() == &left);
static_assert(address<right>() == &right);
static_assert(address<left>() != address<right>());
template <const unsigned& value>
constexpr unsigned read() {
  return value;
}
static_assert(read<left>() == 5);

template <const unsigned* value>
struct Pointer {};
static_assert(__is_same(Pointer<&left>, Pointer<&left>));
static_assert(!__is_same(Pointer<&left>, Pointer<&right>));
static_assert(__is_same(Pointer<nullptr>,
                        Pointer<static_cast<const unsigned*>(nullptr)>));
constexpr unsigned table[2] = {5, 5};
static_assert(read<table[1]>() == 5);
static_assert(!__is_same(Pointer<&table[0]>, Pointer<&table[1]>));
static_assert(__is_same(Pointer<&table[1]>, Pointer<&table[1]>));
static_assert(address<table[0]>() != address<table[1]>());
Pointer<&left + 1> onePastObject;
Pointer<table + 2> onePastArray;
static_assert(!__is_same(Pointer<&left>, Pointer<&left + 1>));
// expected-error@+1 {{template argument is not a constant expression}}
Ref<*(&left + 1)> onePastObjectReference;
// expected-error@+1 {{template argument is not a constant expression}}
Ref<*(table + 2)> onePastArrayReference;
// expected-error@+1 {{template argument is not a constant expression}}
Pointer<&left + 2> pastObjectBounds;
// expected-error@+1 {{template argument is not a constant expression}}
Pointer<table + 3> pastArrayBounds;

template <const char*>
struct StringPointer {};
// expected-error@+1 {{template argument is not a constant expression}}
StringPointer<"text"> stringPointer;

extern unsigned object;
template <const unsigned& value = object>
constexpr const unsigned* defaultAddress() {
  return &value;
}
static_assert(defaultAddress<>() == &object);
static_assert(defaultAddress<object>() == &object);

template <const unsigned&... values>
constexpr unsigned count() {
  return sizeof...(values);
}
static_assert(count<left, right, object>() == 3);
template <const unsigned&... values>
struct References {};
static_assert(__is_same(References<left, right>, References<left, right>));
static_assert(!__is_same(References<left, right>, References<right, left>));

template <auto& value>
constexpr auto deducedAddress() {
  return &value;
}
static_assert(__is_same(decltype(deducedAddress<object>()), unsigned*));
static_assert(deducedAddress<object>() == &object);
static_assert(__is_same(decltype(deducedAddress<left>()), const unsigned*));

extern unsigned array[2];
template <unsigned (&value)[2]>
constexpr unsigned (*arrayAddress())[2] {
  return &value;
}
static_assert(arrayAddress<array>() == &array);

constexpr unsigned function() { return 5; }
template <unsigned (&value)()>
constexpr auto functionAddress() {
  return &value;
}
static_assert(functionAddress<function>() == &function);

using BeforeRedeclaration = Ref<object>;
extern unsigned object;
static_assert(__is_same(BeforeRedeclaration, Ref<object>));

void automatic() {
  constexpr unsigned local = 5;
  // expected-error@+1 {{no matching function for call to 'address'}}
  address<local>();
}

void incompatible() {
  // expected-error@+1 {{no matching function for call to 'address'}}
  address<5u>();
}

constexpr const auto& tableAlias = table;
constexpr const unsigned* elementPointer = table + 1;
static_assert(tableAlias[1] == 5);
static_assert(__is_same(Ref<tableAlias[1]>, Ref<table[1]>));
static_assert(__is_same(Ref<elementPointer[0]>, Ref<table[1]>));
static_assert(__is_same(Ref<*elementPointer>, Ref<table[1]>));
static_assert(__is_same(Ref<*(table + 1)>, Ref<table[1]>));
static_assert(__is_same(Ref<true ? table[0] : table[1]>, Ref<table[0]>));
static_assert(!__is_same(Ref<elementPointer[0]>, Ref<table[0]>));
constexpr const unsigned& secondElement() { return table[1]; }
static_assert(__is_same(Ref<secondElement()>, Ref<table[1]>));

struct StaticTable {
  static constexpr unsigned value = 5;
};
static_assert(__is_same(Ref<StaticTable::value>, Ref<StaticTable::value>));

const float floatingObject = 1.5f;
template <const float& value>
constexpr float readFloating() {
  return value;
}
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(readFloating<floatingObject>() == 1.5f);

struct FloatingSettings {
  inline static const float scale = 1.5f;
};
// expected-error@+1 {{static assertion expression is not an integral constant expression}}
static_assert(readFloating<FloatingSettings::scale>() == 1.5f);

extern const unsigned declared;
constexpr unsigned declared = 5;
static_assert(*(&declared) == 5);

constexpr unsigned addressSideEffects() {
  unsigned index = 0;
  const unsigned* pointer = &table[++index];
  return index;
}
static_assert(addressSideEffects() == 1);
