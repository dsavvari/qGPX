import { test } from "node:test";
import assert from "node:assert/strict";
import { coverage, elevationRuns, formatDuration } from "../src/model.ts";
import type { Segment, TrackPoint } from "../src/model.ts";

function point(distance: number, elevation: number | null): TrackPoint {
  return { latitude: 0, longitude: 0, distanceMeters: distance, elevationMeters: elevation, timeUnixMs: null, speedMetersPerSecond: null };
}

function segment(points: TrackPoint[]): Segment {
  return { points, distanceMeters: 0, elevationGainMeters: 0, elevationLossMeters: 0, durationSeconds: null, maxSpeedMetersPerSecond: null };
}

test("profile preserves segment and missing-elevation gaps", () => {
  const segments = [
    segment([point(0, 100), point(10, null), point(20, 120)]),
    segment([point(20, 200), point(30, 210)]),
  ];
  assert.deepEqual(elevationRuns(segments), [
    [{ distance: 0, elevation: 100 }],
    [{ distance: 20, elevation: 120 }],
    [{ distance: 20, elevation: 200 }, { distance: 30, elevation: 210 }],
  ]);
  assert.deepEqual(coverage(segments), { points: 5, elevations: 4, timestamps: 0, elevationPairs: 1 });
});

test("selected segment profile starts at zero distance", () => {
  assert.deepEqual(elevationRuns([segment([point(100, 0), point(150, -10)])]), [
    [{ distance: 0, elevation: 0 }, { distance: 50, elevation: -10 }],
  ]);
});

test("missing and zero elevation are distinct", () => {
  assert.deepEqual(elevationRuns([segment([point(0, null)])]), []);
  assert.deepEqual(coverage([segment([point(0, 0), point(1, 0)])]), {
    points: 2, elevations: 2, timestamps: 0, elevationPairs: 1,
  });
  assert.deepEqual(elevationRuns([]), []);
});

test("duration keeps missing, zero, and negative measurements distinct", () => {
  assert.equal(formatDuration(null), "Unavailable");
  assert.equal(formatDuration(0), "0h 0m 0s");
  assert.equal(formatDuration(3661), "1h 1m 1s");
  assert.equal(formatDuration(-60), "-0h 1m 0s");
});
