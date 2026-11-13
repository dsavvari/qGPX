import type { Segment, TrackPoint } from "./model.ts";

export interface ReplaySample {
  point: TrackPoint;
  segment: number;
  seconds: number;
}

export interface Replay {
  samples: ReplaySample[];
  duration: number;
  recorded: boolean;
}

export interface ReplayPosition
  extends Omit<TrackPoint, "heartRateBpm" | "cadenceRpm" | "temperatureCelsius"> {
  inGap: boolean;
}

export function createReplay(segments: Segment[]): Replay {
  const samples = segments.flatMap((segment, index) =>
    segment.points.map((point) => ({ point, segment: index, seconds: 0 })),
  );
  const firstTime = samples[0]?.point.timeUnixMs;
  const recorded = samples.length > 1 && samples.every((sample, index) => {
    const time = sample.point.timeUnixMs;
    const previous = samples[index - 1]?.point.timeUnixMs;
    return time !== null && Number.isFinite(time) &&
      (index === 0 || (previous !== null && previous !== undefined && time > previous));
  });
  let seconds = 0;
  for (let i = 0; i < samples.length; i++) {
    const sample = samples[i];
    if (!sample) continue;
    const previous = samples[i - 1];
    const segment = segments[sample.segment];
    if (recorded && sample.point.timeUnixMs !== null && firstTime != null) {
      seconds = (sample.point.timeUnixMs - firstTime) / 1000;
    } else if (segment && segment.durationSeconds !== null && segment.durationSeconds !== undefined) {
      // Use the C++ calculated moving duration if available, otherwise use elapsed duration
      const durationToUse = segment.movingDurationSeconds !== null && segment.movingDurationSeconds !== undefined
        ? segment.movingDurationSeconds
        : segment.durationSeconds;
      // Distribute the duration by point index within the segment
      const segmentPoints = segment.points;
      const pointIndex = segmentPoints.indexOf(sample.point);
      if (pointIndex >= 0 && segmentPoints.length > 1) {
        seconds = (pointIndex / (segmentPoints.length - 1)) * durationToUse;
      } else if (pointIndex === 0) {
        seconds = 0;
      }
    } else if (previous && sample.segment === previous.segment) {
      seconds += Math.max(0, sample.point.distanceMeters - previous.point.distanceMeters) / 5;
    }
    sample.seconds = seconds;
  }
  return { samples, duration: seconds, recorded };
}

export function replayPosition(replay: Replay, seconds: number): ReplayPosition | null {
  if (!replay.samples.length) return null;
  const time = Math.max(0, Math.min(replay.duration, seconds));
  let low = 0;
  let high = replay.samples.length;
  // Upper bound also skips instantaneous transitions at segment boundaries.
  while (low < high) {
    const middle = Math.floor((low + high) / 2);
    if (replay.samples[middle]!.seconds <= time) low = middle + 1;
    else high = middle;
  }
  const current = replay.samples[Math.max(0, low - 1)]!;
  const next = replay.samples[low];
  if (!next || next.segment !== current.segment) {
    return {
      latitude: current.point.latitude,
      longitude: current.point.longitude,
      distanceMeters: current.point.distanceMeters,
      elevationMeters: current.point.elevationMeters,
      timeUnixMs: current.point.timeUnixMs,
      speedMetersPerSecond: null,
      inGap: Boolean(next),
    };
  }
  const fraction = (time - current.seconds) / (next.seconds - current.seconds);
  const interpolate = (a: number, b: number) => a + (b - a) * fraction;
  const a = current.point;
  const b = next.point;
  const longitudeDelta = ((b.longitude - a.longitude + 540) % 360) - 180;
  return {
    latitude: interpolate(a.latitude, b.latitude),
    longitude: ((a.longitude + longitudeDelta * fraction + 540) % 360) - 180,
    distanceMeters: interpolate(a.distanceMeters, b.distanceMeters),
    speedMetersPerSecond: b.speedMetersPerSecond,
    elevationMeters: fraction === 0 ? a.elevationMeters :
      a.elevationMeters !== null && b.elevationMeters !== null ?
        interpolate(a.elevationMeters, b.elevationMeters) : null,
    timeUnixMs: replay.recorded && a.timeUnixMs !== null && b.timeUnixMs !== null ?
      interpolate(a.timeUnixMs, b.timeUnixMs) : null,
    inGap: false,
  };
}
