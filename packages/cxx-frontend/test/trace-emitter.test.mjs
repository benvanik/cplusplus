import assert from "node:assert/strict";
import test from "node:test";
import { TraceEmitter } from "../dist/TraceEmitter.js";

test("same-width floating formats retain distinct type identities", () => {
  const emitter = new TraceEmitter();

  const binary16 = emitter.floatingType("Half");
  const bfloat16 = emitter.floatingType("BFloat");

  assert.notEqual(binary16, bfloat16);
});
