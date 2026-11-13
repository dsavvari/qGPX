import { test } from "node:test";
import assert from "node:assert/strict";
import { coverage, elevationRuns, formatDuration, sensorRuns } from "../src/model.ts";
import type { Segment, TrackPoint } from "../src/model.ts";

function point(distance: number, elevation: number | null): TrackPoint {
  return {
    latitude: 0,
    longitude: 0,
    distanceMeters: distance,
    elevationMeters: elevation,
    heartRateBpm: null,
    cadenceRpm: null,
    temperatureCelsius: null,
    timeUnixMs: null,
    speedMetersPerSecond: null,
  };
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

test("sensor profiles preserve missing samples, segment gaps, and zero values", () => {
  const first = point(0, null);
  first.heartRateBpm = 120;
  const second = point(100, null);
  second.heartRateBpm = 125;
  second.cadenceRpm = 0;
  second.temperatureCelsius = -2.5;
  const gap = point(45, null);
  const third = point(80, null);
  third.heartRateBpm = 122;
  const segments = [segment([first, gap, third]), segment([second])];

  assert.deepEqual(sensorRuns(segments, "heartRateBpm"), [
    [{ distance: 0, value: 120 }],
    [{ distance: 80, value: 122 }],
    [{ distance: 100, value: 125 }],
  ]);
  assert.deepEqual(sensorRuns(segments, "cadenceRpm"), [
    [{ distance: 100, value: 0 }],
  ]);
  assert.deepEqual(sensorRuns(segments, "temperatureCelsius"), [
    [{ distance: 100, value: -2.5 }],
  ]);
});
