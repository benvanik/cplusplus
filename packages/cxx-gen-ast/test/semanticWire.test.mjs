import assert from "node:assert/strict";
import test from "node:test";
import { substituteWireType } from "../src/semanticWire.ts";

const parameter = (index) => ({
  kind: "type-param",
  index,
  depth: 0,
  isPack: false,
});

const typeArgument = (type) => ({ kind: "type", text: "", type });

const classType = (name, arguments_) => ({
  kind: "class",
  name,
  isPolymorphic: false,
  arguments: arguments_.map(typeArgument),
});

test("wire substitution omits container implementation arguments", () => {
  for (const name of ["::std::basic_string", "::std::basic_string_view"]) {
    const string = classType(name, [parameter(0), parameter(1), parameter(2)]);
    assert.deepEqual(substituteWireType(string, []), {
      ...string,
      arguments: [],
    });
  }

  const vector = classType("::std::vector", [
    parameter(0),
    classType("::std::allocator", [parameter(0)]),
  ]);
  assert.deepEqual(
    substituteWireType(vector, [{ kind: "builtin", name: "int" }]),
    classType("::std::vector", [{ kind: "builtin", name: "int" }]),
  );

  const span = {
    ...classType("::std::span", []),
    arguments: [
      typeArgument(parameter(0)),
      { kind: "value", text: "42", value: 42 },
    ],
  };
  assert.deepEqual(
    substituteWireType(span, [{ kind: "builtin", name: "int" }]),
    classType("::std::span", [{ kind: "builtin", name: "int" }]),
  );

  const map = classType("::std::map", [
    parameter(0),
    parameter(1),
    classType("::std::less", [parameter(0)]),
    classType("::std::allocator", [
      classType("::std::pair", [parameter(0), parameter(1)]),
    ]),
  ]);
  assert.deepEqual(
    substituteWireType(map, [
      { kind: "builtin", name: "int" },
      { kind: "builtin", name: "double" },
    ]),
    classType("::std::map", [
      { kind: "builtin", name: "int" },
      { kind: "builtin", name: "double" },
    ]),
  );
});

test("wire substitution retains arguments owned by semantic types", () => {
  const semanticType = classType("::cxx::SemanticPair", [
    parameter(0),
    parameter(1),
  ]);
  assert.deepEqual(
    substituteWireType(semanticType, [
      { kind: "builtin", name: "int" },
      { kind: "builtin", name: "double" },
    ]),
    classType("::cxx::SemanticPair", [
      { kind: "builtin", name: "int" },
      { kind: "builtin", name: "double" },
    ]),
  );
});
