import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import { loadModel } from "../src/parseModel.ts";
import { PlanBuilder } from "../src/semanticPlan.ts";
import { gen_semantic_codec } from "../src/gen_semantic_codec.ts";

const source = fs.readFileSync(
  new URL("../semantic-model.json", import.meta.url),
  "utf8",
);
const index = loadModel(source);
const plan = new PlanBuilder(index).build();

test("semantic decoding derives nested container types from their accessor", () => {
  assert.deepEqual(plan.diagnostics, []);
  const address = plan.structs.find(
    (entity) => entity.short === "ConstAddress",
  );
  const field = address.fields.find((field) => field.name === "subobjectPath_");
  assert.equal(field.typeExpression, "$->subobjectPath()");

  const output = fs.mkdtempSync(path.join(os.tmpdir(), "cxx-semantic-"));
  try {
    const sourceOutput = path.join(output, "semantic_codec.cc");
    gen_semantic_codec({
      plan,
      headerOutput: path.join(output, "semantic_codec.h"),
      sourceOutput,
    });
    const source = fs.readFileSync(sourceOutput, "utf8");
    const decoder = source.match(
      /void SemanticDecoder::readcxxConstAddress\([^]*?\n\}/,
    )[0];
    assert.match(
      decoder,
      /std::remove_cvref_t<decltype\(self->subobjectPath\(\)\)>/,
    );
    assert.match(decoder, /decltype\(value\d+\)::value_type element\d+/);
    assert.doesNotMatch(decoder, /unsigned long/);
  } finally {
    fs.rmSync(output, { recursive: true, force: true });
  }
});

test("semantic decoding keeps owning types when accessors return pointers or views", () => {
  const parameter = plan.symbols.find(
    (entity) => entity.short === "ParameterSymbol",
  );
  const pending = parameter.fields.find(
    (field) => field.name === "pendingDefaultArgument_",
  );
  assert.equal(pending.wire.k, "unique");
  assert.equal(pending.typeExpression, undefined);
  assert.equal(pending.cppType, "std::unique_ptr<cxx::PendingInstantiation>");

  const scope = plan.symbolBases.find(
    (entity) => entity.short === "ScopeSymbol",
  );
  const members = scope.fields.find((field) => field.name === "members_");
  assert.equal(members.typeExpression, undefined);
  assert.equal(members.cppType, "std::vector<cxx::Symbol*>");
});

test("semantic encoding preserves parameter reflections as a distinct variant", () => {
  assert.deepEqual(plan.diagnostics, []);
  const meta = plan.structs.find((entity) => entity.name === "::cxx::Meta");
  const value = meta.fields.find((field) => field.name === "value");
  assert.equal(value.wire.k, "variant");
  assert.equal(value.wire.alternatives.length, 5);
  assert.equal(value.wire.alternatives[3].k, "struct");
  assert.equal(value.wire.alternatives[3].cpp, "cxx::Meta::Parameter");
  const parameter = plan.structs.find(
    (entity) => entity.name === "::cxx::Meta::Parameter",
  );
  assert.deepEqual(
    parameter.fields.map((field) => field.name),
    ["function", "index"],
  );
});

test("persisted field plans retain their binding rationale", () => {
  const expected = [
    [
      "::cxx::Symbol",
      "abiTags_",
      "abiTags() answers a span over the interned list",
    ],
    [
      "::cxx::NamespaceSymbol",
      "anonNamespaceIndex_",
      "the accessor answers nullopt for a named namespace",
    ],
    [
      "::cxx::MaybeRedecl",
      "canonical_",
      "canonical() answers `this` when the field is null",
    ],
    [
      "::cxx::ClassSymbol",
      "minimumAlignmentShift_",
      "the accessor represents the encoded request as byte alignment",
    ],
  ];
  for (const [owner, name, why] of expected) {
    const fields = plan.report.filter(
      (field) => field.owner === owner && field.name === name,
    );
    assert.ok(fields.length > 0, `${owner}::${name}`);
    for (const field of fields) {
      assert.equal(field.cls, "P");
      assert.equal(field.why, why);
    }
  }
});

test("semantic plan substitution diagnostics identify their owner", () => {
  const model = JSON.parse(source);
  const missingParameter = {
    kind: "type-param",
    index: 99,
    depth: 0,
    isPack: false,
  };

  const classNamed = (name) => {
    const entry = model.classes.find((entry) => entry.name === name);
    assert.ok(entry, name);
    return entry;
  };
  const methodNamed = (entry, name) => {
    const method = entry.methods.find((method) => method.name === name);
    assert.ok(method, `${entry.name}::${name}`);
    return method;
  };
  const fieldNamed = (entry, name) => {
    const field = entry.fields.find((field) => field.name === name);
    assert.ok(field, `${entry.name}::${name}`);
    return field;
  };

  methodNamed(
    classNamed("::cxx::MaybeTemplate"),
    "templateDeclaration",
  ).returnType = missingParameter;
  methodNamed(
    classNamed("::cxx::Control"),
    "getIdentifier",
  ).parameters[0].type = missingParameter;
  methodNamed(classNamed("::cxx::ConstAddress"), "subobjectPath").returnType =
    missingParameter;
  fieldNamed(classNamed("::cxx::Symbol"), "location_").type = missingParameter;

  const diagnostics = new PlanBuilder(loadModel(JSON.stringify(model))).build()
    .diagnostics;
  const message = "no template argument for type-param<99, 0>";
  for (const context of [
    "::cxx::MaybeTemplate::templateDeclaration",
    "::cxx::Identifier(name)",
    "::cxx::ConstAddress::subobjectPath_",
    "::cxx::Symbol::location_",
  ]) {
    assert.ok(diagnostics.includes(`${context}: ${message}`), context);
  }
});
