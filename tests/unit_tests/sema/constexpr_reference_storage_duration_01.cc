// RUN: not %cxx -std=c++23 -fsyntax-only %s 2>&1 | %filecheck %s --check-prefix=CXX23
// RUN: not %cxx -std=c++26 -fsyntax-only %s 2>&1 | %filecheck %s --check-prefix=CXX26

// CXX23-COUNT-13: error: constexpr variable must be initialized
// CXX26-COUNT-2: error: constexpr variable must be initialized

struct Value {
  unsigned number = 9;
  unsigned lanes[2] = {3, 5};
};

struct PointerHolder {
  const Value* pointer;
};

struct ReferenceHolder {
  const Value& reference;
};

constexpr Value global{};
constexpr const Value& globalTemporary = Value{};
thread_local int threadObject;

constexpr bool staticReferents() {
  constexpr const Value& object = global;
  constexpr const unsigned& member = global.number;
  constexpr const unsigned& element = global.lanes[1];
  constexpr const Value& temporary = globalTemporary;
  return &object == &global && &member == &global.number &&
         &element == &global.lanes[1] && &temporary == &globalTemporary;
}
static_assert(staticReferents());

// C++26 makes automatic objects constexpr-referenceable within the same
// function parameter scope. C++23 rejects all of these declarations.
constexpr void temporaryReferents() {
  constexpr const unsigned& scalar = 7u;
  constexpr const Value& object = Value{};
  constexpr const unsigned& member = Value{}.number;
}

constexpr void automaticReferents() {
  Value mutableObject{};
  constexpr Value object{};
  constexpr const Value& mutableAlias = mutableObject;
  constexpr const Value& alias = object;
  constexpr const unsigned& member = object.number;
  constexpr const unsigned& element = object.lanes[1];
  constexpr const Value* pointer = &object;
  constexpr PointerHolder pointerHolder{&object};
  constexpr ReferenceHolder referenceHolder{object};
}

void nestedBlock() {
  int object = 0;
  {
    constexpr int& alias = object;
  }
}

// Crossing a function boundary or naming thread storage is invalid in both
// language modes.
void nestedFunction() {
  int object = 0;
  [&] {
    constexpr int& alias = object;
  }();
}

void threadReferent() { constexpr int& alias = threadObject; }
