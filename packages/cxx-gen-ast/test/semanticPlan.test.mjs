import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import { loadModel } from "../src/parseModel.ts";
import { PlanBuilder } from "../src/semanticPlan.ts";

const source = fs.readFileSync(
  new URL("../semantic-model.json", import.meta.url),
  "utf8",
);
const plan = new PlanBuilder(loadModel(source)).build();

function reportsFor(owner, name) {
  return plan.report.filter(
    (field) => field.owner === owner && field.name === name,
  );
}

test("persisted field plans retain their binding rationale", () => {
  const fields = reportsFor("::cxx::Symbol", "abiTags_");
  assert.ok(fields.length > 0);
  for (const field of fields) {
    assert.equal(field.cls, "P");
    assert.equal(field.why, "abiTags() answers a span over the interned list");
  }
});

test("constant address parents are persisted with their rationale", () => {
  const fields = reportsFor("::cxx::ConstAddress", "parent_");
  assert.equal(fields.length, 1);
  assert.equal(fields[0].cls, "P");
  assert.equal(
    fields[0].why,
    "subobject identity includes the immutable containing address",
  );
});

test("semantic plan substitution diagnostics identify their owner", () => {
  const model = JSON.parse(source);
  const missingParameter = {
    kind: "type-param",
    index: 99,
    depth: 0,
    isPack: false,
  };

  const maybeTemplate = model.classes.find(
    (entry) => entry.name === "::cxx::MaybeTemplate",
  );
  assert.ok(maybeTemplate);
  const templateDeclaration = maybeTemplate.methods.find(
    (method) => method.name === "templateDeclaration",
  );
  assert.ok(templateDeclaration);
  templateDeclaration.returnType = missingParameter;

  const control = model.classes.find(
    (entry) => entry.name === "::cxx::Control",
  );
  assert.ok(control);
  const getIdentifier = control.methods.find(
    (method) => method.name === "getIdentifier",
  );
  assert.ok(getIdentifier);
  assert.ok(
    getIdentifier.parameters[0]?.typeName.includes("basic_string_view"),
  );
  getIdentifier.parameters[0].type = missingParameter;

  const diagnostics = new PlanBuilder(loadModel(JSON.stringify(model))).build()
    .diagnostics;
  assert.ok(
    diagnostics.includes(
      "::cxx::MaybeTemplate::templateDeclaration: no template argument for type-param<99, 0>",
    ),
  );
  assert.ok(
    diagnostics.includes(
      "::cxx::Identifier(name): no template argument for type-param<99, 0>",
    ),
  );
});
