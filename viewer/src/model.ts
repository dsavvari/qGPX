export interface TrackPoint {
  latitude: number;
  longitude: number;
  elevationMeters: number | null;
  timeUnixMs: number | null;
  distanceMeters: number;
  speedMetersPerSecond: number | null;
}

export interface Measurements {
  maxSpeedMetersPerSecond: number | null;
  distanceMeters: number;
  elevationGainMeters: number | null;
  elevationLossMeters: number | null;
  durationSeconds: number | null;
}

export interface Segment extends Measurements {
  points: TrackPoint[];
}

export interface Track extends Measurements {
  name: string;
  description: string;
  pointCount: number;
  segments: Segment[];
}

export interface Analysis {
  schemaVersion: 1;
  creator: string;
  tracks: Track[];
}

export interface ProfilePoint {
  distance: number;
  elevation: number;
}

export function elevationRuns(segments: Segment[]): ProfilePoint[][] {
  const runs: ProfilePoint[][] = [];
  const offset = segments[0]?.points[0]?.distanceMeters ?? 0;
  for (const segment of segments) {
    let run: ProfilePoint[] = [];
    for (const point of segment.points) {
      if (point.elevationMeters === null) {
        if (run.length) runs.push(run);
        run = [];
      } else {
        run.push({
          distance: point.distanceMeters - offset,
          elevation: point.elevationMeters,
        });
      }
    }
    if (run.length) runs.push(run);
  }
  return runs;
}

export function coverage(segments: Segment[]) {
  let points = 0;
  let elevations = 0;
  let timestamps = 0;
  let elevationPairs = 0;
  for (const segment of segments) {
    let previousElevation: number | null = null;
    for (const point of segment.points) {
      points++;
      if (point.elevationMeters !== null) elevations++;
      if (point.timeUnixMs !== null) timestamps++;
      if (previousElevation !== null && point.elevationMeters !== null) elevationPairs++;
      previousElevation = point.elevationMeters;
    }
  }
  return { points, elevations, timestamps, elevationPairs };
}

export function formatDuration(seconds: number | null): string {
  if (seconds === null) return "Unavailable";
  const sign = seconds < 0 ? "-" : "";
  const total = Math.abs(seconds);
  const hours = Math.floor(total / 3600);
  const minutes = Math.floor((total % 3600) / 60);
  return `${sign}${hours}h ${minutes}m ${total % 60}s`;
}
