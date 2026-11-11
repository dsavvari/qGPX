import { test } from "node:test";
import assert from "node:assert/strict";
import { createReplay, replayPosition } from "../src/replay.ts";
import type { Segment, TrackPoint } from "../src/model.ts";

function point(distance: number, time: number | null, longitude = 0, elevation: number | null = 0): TrackPoint {
  return { latitude: 0, longitude, distanceMeters: distance, timeUnixMs: time, elevationMeters: elevation, speedMetersPerSecond: null };
}

function segment(...points: TrackPoint[]): Segment {
  return { points, distanceMeters: 0, elevationGainMeters: 0, elevationLossMeters: 0, durationSeconds: null, maxSpeedMetersPerSecond: null };
}

test("recorded replay interpolates timing, distance, coordinates and elevation", () => {
  const replay = createReplay([segment(point(0, 1000, 10, 100), point(100, 11000, 20, 200))]);
  assert.equal(replay.recorded, true);
  assert.equal(replay.duration, 10);
  assert.deepEqual(replayPosition(replay, 5), {
    latitude: 0, longitude: 15, distanceMeters: 50, elevationMeters: 150, timeUnixMs: 6000, speedMetersPerSecond: null, inGap: false,
  });
  assert.equal(replayPosition(replay, -10)?.distanceMeters, 0);
  assert.equal(replayPosition(replay, 100)?.distanceMeters, 100);
});

test("recorded segment gaps hold then jump, never interpolate", () => {
  const replay = createReplay([
    segment(point(0, 0, 0), point(100, 10000, 1)),
    segment(point(100, 30000, 10), point(200, 40000, 11)),
  ]);
  assert.equal(replay.duration, 40);
  assert.equal(replayPosition(replay, 20)?.longitude, 1);
  assert.equal(replayPosition(replay, 20)?.inGap, true);
  assert.equal(replayPosition(replay, 30)?.longitude, 10);
  assert.equal(replayPosition(replay, 30)?.inGap, false);
});

test("missing, repeated and reversed timestamps use an explicit distance preview", () => {
  for (const times of [[null, null], [1000, null], [1000, 1000], [2000, 1000], [NaN, 1000]]) {
    const replay = createReplay([segment(point(0, times[0]!), point(100, times[1]!, 10))]);
    assert.equal(replay.recorded, false);
    assert.equal(replay.duration, 20);
    assert.equal(replayPosition(replay, 10)?.longitude, 5);
    assert.equal(replayPosition(replay, 10)?.timeUnixMs, null);
  }
});

test("preview skips segment gaps and stationary samples without inventing distance", () => {
  const replay = createReplay([
    segment(point(0, null), point(50, null, 1), point(50, null, 1)),
    segment(point(50, null, 20), point(100, null, 21)),
  ]);
  assert.equal(replay.duration, 20);
  assert.ok(Math.abs(replayPosition(replay, 9)!.longitude - 0.9) < 1e-10);
  assert.equal(replayPosition(replay, 10)?.longitude, 20);
  assert.equal(replayPosition(replay, 20)?.longitude, 21);
});

test("selected segment resets time and missing elevation is not interpolated", () => {
  const replay = createReplay([segment(point(100, 10000, 1, null), point(200, 20000, 2, 20))]);
  assert.equal(replay.duration, 10);
  assert.equal(replayPosition(replay, 5)?.elevationMeters, null);
  assert.equal(replayPosition(replay, 10)?.elevationMeters, 20);
});

test("empty, single point, and zero-distance previews have no playable interval", () => {
  assert.equal(replayPosition(createReplay([]), 0), null);
  for (const segments of [
    [segment(point(0, null))],
    [segment(point(0, null), point(0, null))],
    [segment(), segment(point(0, null))],
  ]) {
    const replay = createReplay(segments);
    assert.equal(replay.duration, 0);
    assert.equal(replayPosition(replay, 0)?.distanceMeters, 0);
  }
});

test("stationary recorded intervals retain their timing", () => {
  const replay = createReplay([segment(point(0, 0), point(0, 10000))]);
  assert.equal(replay.recorded, true);
  assert.equal(replay.duration, 10);
  assert.equal(replayPosition(replay, 5)?.distanceMeters, 0);
});

test("longitude interpolation takes the short path at the antimeridian", () => {
  const replay = createReplay([segment(point(0, 0, 179), point(100, 10000, -179))]);
  assert.equal(replayPosition(replay, 5)?.longitude, -180);
});

test("replay uses measured interval speed, not playback or preview speed", () => {
  const a = point(0, 0);
  const b = { ...point(100, 10000), speedMetersPerSecond: 10 };
  const c = point(100, 20000, 10);
  const replay = createReplay([segment(a, b), segment(c)]);
  assert.equal(replayPosition(replay, 5)?.speedMetersPerSecond, 10);
  assert.equal(replayPosition(replay, 15)?.speedMetersPerSecond, null);
  assert.equal(replayPosition(replay, 20)?.speedMetersPerSecond, null);
  const preview = createReplay([segment(point(0, null), point(100, null))]);
  assert.equal(replayPosition(preview, 5)?.speedMetersPerSecond, null);
});
