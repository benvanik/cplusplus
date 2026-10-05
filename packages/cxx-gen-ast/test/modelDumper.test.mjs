import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { loadCxx, Parser } from "cxx-frontend";
import { dumpModel } from "../src/modelDumper.ts";

const wasm = await readFile(
  new URL("../../cxx-frontend/dist/wasm/cxx-js.wasm", import.meta.url),
);

await loadCxx({ wasm });

test("model fields and methods use the class-key default access", async () => {
  await using parser = await Parser.parse({
    path: "/project/src/parser/cxx/default_access.h",
    source: `
class ClassDefault {
  int hiddenField;
  void hiddenMethod();

 public:
  int visibleField;
  void visibleMethod();
};

struct StructDefault {
  int visibleField;
  void visibleMethod();

 private:
  int hiddenField;
  void hiddenMethod();
};
`,
  });

  assert.deepEqual(parser.diagnostics, []);
  const model = dumpModel(parser);
  const classDefault = model.classes.find(
    (entry) => entry.name === "::ClassDefault",
  );
  const structDefault = model.classes.find(
    (entry) => entry.name === "::StructDefault",
  );

  assert.ok(classDefault);
  assert.ok(structDefault);

  assert.deepEqual(
    classDefault.fields.map((field) => [field.name, field.access]),
    [
      ["hiddenField", "private"],
      ["visibleField", "public"],
    ],
  );
  assert.deepEqual(
    classDefault.methods.map((method) => [method.name, method.access]),
    [
      ["hiddenMethod", "private"],
      ["visibleMethod", "public"],
    ],
  );
  assert.deepEqual(
    structDefault.fields.map((field) => [field.name, field.access]),
    [
      ["visibleField", "public"],
      ["hiddenField", "private"],
    ],
  );
  assert.deepEqual(
    structDefault.methods.map((method) => [method.name, method.access]),
    [
      ["visibleMethod", "public"],
      ["hiddenMethod", "private"],
    ],
  );
});
