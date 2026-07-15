import { readFileSync } from "node:fs";
import { performance } from "node:perf_hooks";

import { loadTest } from "../.test-dist/test/generated/test.pc.js";

const { Main } = await loadTest({
  wasm: new Uint8Array(
    readFileSync(new URL("../dist/protocache.wasm", import.meta.url)),
  ),
});

const fixture = new Uint8Array(
  readFileSync(new URL("../.benchmark-fixtures/test.pc", import.meta.url)),
);
const iterations = Number.parseInt(process.env.BENCH_N ?? "50000", 10);
const rounds = Number.parseInt(process.env.BENCH_ROUNDS ?? "7", 10);
const warmup = Number.parseInt(
  process.env.BENCH_WARMUP_N ?? String(Math.min(5000, iterations)),
  10,
);

if (
  !Number.isSafeInteger(iterations) || iterations <= 0 ||
  !Number.isSafeInteger(rounds) || rounds <= 0 ||
  !Number.isSafeInteger(warmup) || warmup < 0
) {
  throw new RangeError("BENCH_N, BENCH_ROUNDS and BENCH_WARMUP_N must be valid");
}

function median(values) {
  const sorted = [...values].sort((left, right) => left - right);
  const middle = Math.floor(sorted.length / 2);
  return sorted.length % 2 === 0
    ? (sorted[middle - 1] + sorted[middle]) / 2
    : sorted[middle];
}

function relativeStandardDeviation(values) {
  const mean = values.reduce((sum, value) => sum + value, 0) / values.length;
  const variance = values.reduce(
    (sum, value) => sum + (value - mean) ** 2,
    0,
  ) / values.length;
  return Math.sqrt(variance) / mean * 100;
}

let checksum = 0;
for (let index = 0; index < warmup; index += 1) {
  const root = Main.deserialize(fixture);
  checksum += root.i32 + root.index.size + root.objectv.length;
}

const samples = [];
const heapDeltas = [];
for (let round = 0; round < rounds; round += 1) {
  globalThis.gc?.();
  const beforeHeap = process.memoryUsage().heapUsed;
  const started = performance.now();
  for (let index = 0; index < iterations; index += 1) {
    const root = Main.deserialize(fixture);
    checksum += root.i32 + root.index.size + root.objectv.length;
  }
  const elapsedMs = performance.now() - started;
  samples.push(elapsedMs * 1_000_000 / iterations);
  heapDeltas.push(process.memoryUsage().heapUsed - beforeHeap);
}

const bestNs = Math.min(...samples);
const medianNs = median(samples);

console.log(
  JSON.stringify(
    {
      fixtureBytes: fixture.byteLength,
      iterations,
      rounds,
      warmup,
      bestNsPerOperation: bestNs,
      medianNsPerOperation: medianNs,
      relativeStandardDeviationPercent: relativeStandardDeviation(samples),
      bestOperationsPerSecond: 1_000_000_000 / bestNs,
      maxHeapDeltaBytes: Math.max(...heapDeltas),
      samplesNsPerOperation: samples,
      checksum,
    },
    null,
    2,
  ),
);
