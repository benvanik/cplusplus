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
