import assert from "node:assert/strict";
import test from "node:test";
import { TraceEmitter } from "../dist/TraceEmitter.js";

test("same-width floating formats retain distinct type identities", () => {
  const emitter = new TraceEmitter();

  const binary16 = emitter.floatingType("Half");
  const bfloat16 = emitter.floatingType("BFloat");
  const e4m3fn = emitter.floatingType("Float8E4M3FN");
  const e5m2 = emitter.floatingType("Float8E5M2");

  assert.notEqual(binary16, bfloat16);
  assert.notEqual(e4m3fn, e5m2);
});
