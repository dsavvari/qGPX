import Map from "ol/Map.js";
import View from "ol/View.js";
import Feature from "ol/Feature.js";
import LineString from "ol/geom/LineString.js";
import Point from "ol/geom/Point.js";
import TileLayer from "ol/layer/Tile.js";
import VectorLayer from "ol/layer/Vector.js";
import VectorSource from "ol/source/Vector.js";
import OSM from "ol/source/OSM.js";
import { fromLonLat } from "ol/proj.js";
import { isEmpty } from "ol/extent.js";
import { Circle, Fill, Stroke, Style } from "ol/style.js";
import "ol/ol.css";
import "./style.css";
import { coverage, elevationRuns, formatDuration } from "./model.ts";
import type { Analysis, Measurements, Segment } from "./model.ts";
import { createReplay, replayPosition } from "./replay.ts";

function element<T extends Element>(id: string, type: new () => T): T {
  const value = document.getElementById(id);
  if (!(value instanceof type)) throw new Error(`Missing UI element: ${id}`);
  return value;
}

const fileInput = element("file", HTMLInputElement);
const downloadButton = element("download", HTMLButtonElement);
const trackSelect = element("track", HTMLSelectElement);
const segmentSelect = element("segment", HTMLSelectElement);
const status = element("status", HTMLParagraphElement);
const error = element("error", HTMLParagraphElement);
const description = element("description", HTMLParagraphElement);
const stats = element("stats", HTMLDListElement);
const profileNote = element("profile-note", HTMLParagraphElement);
const mapNote = element("map-note", HTMLParagraphElement);
const fitButton = element("fit", HTMLButtonElement);
const basemap = element("basemap", HTMLInputElement);
const dropZone = element("drop-zone", HTMLElement);
const profile = element("profile", SVGSVGElement);
const playButton = element("play", HTMLButtonElement);
const restartButton = element("restart", HTMLButtonElement);
const replaySpeed = element("replay-speed", HTMLSelectElement);
const follow = element("follow", HTMLInputElement);
const timeline = element("timeline", HTMLInputElement);
const replayMode = element("replay-mode", HTMLParagraphElement);
const replayReadout = element("replay-readout", HTMLOutputElement);
const replayMarker = new Feature(new Point([0, 0]));
const replaySource = new VectorSource({ features: [replayMarker], wrapX: false });
replayMarker.setStyle(new Style({
  image: new Circle({
    radius: 7,
    fill: new Fill({ color: "#e56b36" }),
    stroke: new Stroke({ color: "#fff", width: 3 }),
  }),
}));

const source = new VectorSource<Feature<LineString | Point>>({ wrapX: false });
const baseLayer = new TileLayer<OSM>({ visible: false });
const map = new Map({
  target: "map",
  layers: [
    baseLayer,
    new VectorLayer({
      source,
      style: new Style({
        stroke: new Stroke({ color: "#087f8c", width: 3 }),
        image: new Circle({
          radius: 5,
          fill: new Fill({ color: "#087f8c" }),
          stroke: new Stroke({ color: "#ffffff", width: 2 }),
        }),
      }),
    }),
    new VectorLayer({ source: replaySource }),
  ],
  view: new View({ center: fromLonLat([0, 25]), zoom: 2 }),
});

let analysis: Analysis | null = null;
let loading = false;
let exporting = false;
let loadedFile: File | null = null;
let filename = "";
let replay = createReplay([]);
let elapsed = 0;
let playing = false;
let frame = 0;
let lastFrame = 0;
let playbackRate = Number(replaySpeed.value);
let profileDistance = 0;
let profileOffset = 0;
const profileCursor = svgElement("line", {
  x1: "65", x2: "65", y1: "15", y2: "150", class: "replay-cursor",
});
replaySource.clear();

function updateReplayControls() {
  const playable = !loading && replay.duration > 0;
  playButton.disabled = !playable;
  restartButton.disabled = !playable;
  timeline.disabled = !playable;
  replaySpeed.disabled = !playable;
  follow.disabled = loading || replay.samples.length === 0;
  playButton.textContent = playing ? "Pause" : "Play";
  playButton.setAttribute("aria-pressed", String(playing));
}

function pauseReplay() {
  playing = false;
  cancelAnimationFrame(frame);
  updateReplayControls();
}

function renderReplayPosition() {
  const position = replayPosition(replay, elapsed);
  if (!position) return;
  const coordinates = fromLonLat([
    position.longitude, Math.max(-85.05112878, Math.min(85.05112878, position.latitude)),
  ]);
  replayMarker.getGeometry()?.setCoordinates(coordinates);
  if (follow.checked) map.getView().setCenter(coordinates);
  timeline.value = String(elapsed);
  const distance = position.distanceMeters - profileOffset;
  const x = 65 + distance / (profileDistance || 1) * 810;
  profileCursor.setAttribute("x1", String(x));
  profileCursor.setAttribute("x2", String(x));
  const elevation = position.elevationMeters === null ? "elevation unavailable" :
    `${position.elevationMeters.toFixed(0)} m elevation`;
  const speed = position.speedMetersPerSecond === null ? "speed unavailable" :
    `${(position.speedMetersPerSecond * 3.6).toFixed(1)} km/h (derived)`;
  replayReadout.textContent =
    `${formatDuration(Math.floor(elapsed))} / ${formatDuration(Math.floor(replay.duration))}` +
    ` | ${(distance / 1000).toFixed(2)} km | ${elevation} | ${speed}` +
    (position.inGap ? " | Segment gap: waiting, then jumping to the next segment." : "");
  timeline.setAttribute("aria-valuetext", replayReadout.textContent);
}

function advanceReplay(now: number) {
  elapsed = Math.min(replay.duration, elapsed + (now - lastFrame) / 1000 * playbackRate);
  lastFrame = now;
  renderReplayPosition();
  if (elapsed >= replay.duration) pauseReplay();
}

function animateReplay(now: number) {
  if (!playing) return;
  advanceReplay(now);
  if (playing) frame = requestAnimationFrame(animateReplay);
}

function resetReplay(segments: Segment[]) {
  pauseReplay();
  replay = createReplay(segments);
  elapsed = 0;
  timeline.max = String(replay.duration);
  timeline.value = "0";
  replaySource.clear();
  if (replay.samples.length) replaySource.addFeature(replayMarker);
  replayMode.textContent = replay.duration === 0 ?
    "No replay interval in this selection (single point or zero-distance preview)." :
    replay.recorded ?
      "Recorded timing. Segment gaps hold the last position, then jump." :
      "Constant-speed preview at 5 m/s: timestamps are missing, repeated, or out of order. Segment gaps jump instantly; this is not recorded timing.";
  updateReplayControls();
  renderReplayPosition();
}

function fitTrack() {
  const extent = source.getExtent();
  if (extent && !isEmpty(extent)) {
    map.getView().fit(extent, { padding: [45, 45, 45, 45], maxZoom: 15 });
  }
}

function svgElement<K extends keyof SVGElementTagNameMap>(
  tag: K,
  attributes: Record<string, string>,
  text?: string,
): SVGElementTagNameMap[K] {
  const node = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const [key, value] of Object.entries(attributes)) node.setAttribute(key, value);
  if (text !== undefined) node.textContent = text;
  return node;
}

function renderProfile(segments: Segment[]) {
  profile.replaceChildren();
  const runs = elevationRuns(segments);
  let min = Infinity;
  let max = -Infinity;
  const firstDistance = segments[0]?.points[0]?.distanceMeters ?? 0;
  const lastSegment = segments.at(-1);
  const maxDistance = (lastSegment?.points.at(-1)?.distanceMeters ?? firstDistance) - firstDistance;
  profileOffset = firstDistance;
  profileDistance = maxDistance;
  for (const run of runs) {
    for (const point of run) {
      min = Math.min(min, point.elevation);
      max = Math.max(max, point.elevation);
    }
  }
  if (!runs.length) {
    profileNote.textContent = "Elevation unavailable: these points have no elevation values.";
    return;
  }
  const padding = Math.max((max - min) * 0.1, 1);
  const low = min - padding;
  const high = max + padding;
  const x = (distance: number) => 65 + (distance / (maxDistance || 1)) * 810;
  const y = (height: number) => 150 - ((height - low) / (high - low)) * 130;
  for (let i = 0; i <= 4; i++) {
    const height = low + ((high - low) * i) / 4;
    profile.append(
      svgElement("line", { x1: "65", x2: "875", y1: `${y(height)}`, y2: `${y(height)}`, class: "grid" }),
      svgElement("text", { x: "55", y: `${y(height) + 4}`, "text-anchor": "end" }, height.toFixed(0)),
      svgElement("text", { x: `${65 + i * 202.5}`, y: "178", "text-anchor": "middle" }, ((maxDistance * i) / 4000).toFixed(2)),
    );
  }
  for (const run of runs) {
    profile.append(svgElement("polyline", {
      points: run.map((point) => `${x(point.distance)},${y(point.elevation)}`).join(" "),
      class: "elevation-line",
    }));
    if (run.length === 1 && run[0]) {
      profile.append(svgElement("circle", {
        cx: `${x(run[0].distance)}`, cy: `${y(run[0].elevation)}`, r: "3", class: "elevation-point",
      }));
    }
  }
  profileNote.textContent = "Gaps are preserved between segments and where elevation is missing.";
  profile.append(profileCursor);
}

function renderStats(measurements: Measurements, segments: Segment[]) {
  const data = coverage(segments);
  const meters = (value: number | null) => value === null ? "Unavailable" : `${value.toFixed(0)} m`;
  const entries: [string, string][] = [
    ["Distance", `${(measurements.distanceMeters / 1000).toFixed(2)} km`],
    ["Elevation gain", data.elevationPairs ? meters(measurements.elevationGainMeters) : "Unavailable"],
    ["Elevation loss", data.elevationPairs ? meters(measurements.elevationLossMeters) : "Unavailable"],
    ["Duration", formatDuration(measurements.durationSeconds)],
    ["Max speed (derived)", measurements.maxSpeedMetersPerSecond === null ? "Unavailable" :
      `${(measurements.maxSpeedMetersPerSecond * 3.6).toFixed(1)} km/h`],
    ["Segments / points", `${segments.length} / ${data.points.toLocaleString()}`],
    ["Elevation samples", `${data.elevations.toLocaleString()} / ${data.points.toLocaleString()}`],
    ["Timestamp samples", `${data.timestamps.toLocaleString()} / ${data.points.toLocaleString()}`],
  ];
  stats.replaceChildren();
  for (const [label, value] of entries) {
    const term = document.createElement("dt");
    const definition = document.createElement("dd");
    term.textContent = label;
    definition.textContent = value;
    stats.append(term, definition);
  }
  const note = document.createElement("p");
  note.className = "measurement-note";
  note.textContent = "Gain/loss use adjacent available elevations. Duration spans the first and last available timestamps, including segment gaps; it is not moving time. Speed is derived from adjacent timed points within segments, without smoothing; GPS spikes can inflate the maximum.";
  stats.after(note);
}

function renderSelection() {
  const track = analysis?.tracks[Number(trackSelect.value)];
  if (!track) return;
  const selected = segmentSelect.value === "all" ? null : track.segments[Number(segmentSelect.value)];
  const segments = selected ? [selected] : track.segments;
  source.clear();
  const features: Feature<LineString | Point>[] = [];
  for (const segment of segments) {
    const coordinates = segment.points.map((point) => fromLonLat([
      point.longitude, Math.max(-85.05112878, Math.min(85.05112878, point.latitude)),
    ]));
    const first = coordinates[0];
    if (!first) continue;
    const geometry = coordinates.length === 1 ? new Point(first) : new LineString(coordinates);
    features.push(new Feature(geometry));
  }
  source.addFeatures(features);
  fitButton.disabled = features.length === 0;
  description.textContent = track.description || "No track description.";
  document.querySelector(".measurement-note")?.remove();
  renderStats(selected ?? track, segments);
  renderProfile(segments);
  mapNote.textContent = "Geometry from the C++ parser. Segment breaks are not connected. Polar latitudes are clipped to the basemap's Mercator limits.";
  fitTrack();
  resetReplay(segments);
}

function selectTrack() {
  const track = analysis?.tracks[Number(trackSelect.value)];
  if (!track) return;
  segmentSelect.replaceChildren(new Option("All segments", "all"));
  track.segments.forEach((segment, index) => {
    segmentSelect.add(new Option(`Segment ${index + 1} (${segment.points.length} points)`, `${index}`));
  });
  renderSelection();
}

function updateFileControls() {
  fileInput.disabled = loading || exporting;
  downloadButton.disabled = loading || exporting || loadedFile === null;
  downloadButton.textContent = exporting ? "Preparing GPX..." : "Download GPX";
  dropZone.setAttribute("aria-busy", String(loading || exporting));
}

function setLoading(value: boolean) {
  if (value) pauseReplay();
  loading = value;
  updateFileControls();
  trackSelect.disabled = value || analysis === null;
  segmentSelect.disabled = value || analysis === null;
  updateReplayControls();
}

async function loadFile(file: File) {
  if (loading || exporting) return;
  error.hidden = true;
  setLoading(true);
  status.textContent = `Analysing ${file.name}...`;
  try {
    if (!file.name.toLowerCase().endsWith(".gpx")) throw new Error("Choose a .gpx track file.");
    if (file.size > 10 * 1024 * 1024) throw new Error("GPX upload exceeds the 10 MiB limit.");
    const response = await fetch("/api/analyse", {
      method: "POST",
      headers: { "Content-Type": "application/gpx+xml" },
      body: file,
    });
    if (!response.ok) {
      const problem: { error: string } = await response.json();
      throw new Error(problem.error || `Analysis failed (HTTP ${response.status}).`);
    }
    const result: Analysis = await response.json();
    if (result.schemaVersion !== 1 || !Array.isArray(result.tracks) || !result.tracks.length) {
      throw new Error("Unsupported or empty analysis response.");
    }
    analysis = result;
    loadedFile = file;
    filename = file.name;
    trackSelect.replaceChildren();
    analysis.tracks.forEach((track, index) => {
      trackSelect.add(new Option(track.name || `Track ${index + 1}`, `${index}`));
    });
    selectTrack();
    status.textContent = `${filename} | ${analysis.tracks.length} track(s) | creator: ${analysis.creator || "unspecified"}`;
  } catch (cause) {
    error.textContent = cause instanceof Error ? cause.message : String(cause);
    error.hidden = false;
    status.textContent = analysis ? `Load failed. Still showing ${filename}.` : "Load failed. No file loaded.";
  } finally {
    setLoading(false);
    fileInput.value = "";
  }
}

async function downloadGpx() {
  if (loading || exporting || !loadedFile) return;
  const file = loadedFile;
  exporting = true;
  error.hidden = true;
  updateFileControls();
  try {
    const response = await fetch("/api/export", {
      method: "POST",
      headers: { "Content-Type": "application/gpx+xml" },
      body: file,
    });
    if (!response.ok) {
      const problem: { error: string } = await response.json();
      throw new Error(problem.error || `GPX export failed (HTTP ${response.status}).`);
    }
    const url = URL.createObjectURL(await response.blob());
    const link = document.createElement("a");
    link.href = url;
    link.download = `${file.name.replace(/\.gpx$/i, "")}-export.gpx`;
    document.body.append(link);
    try {
      link.click();
    } finally {
      link.remove();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
    }
  } catch (cause) {
    error.textContent = cause instanceof Error ? cause.message : String(cause);
    error.hidden = false;
  } finally {
    exporting = false;
    updateFileControls();
  }
}

downloadButton.addEventListener("click", () => void downloadGpx());
fileInput.addEventListener("change", () => {
  const file = fileInput.files?.[0];
  if (file) void loadFile(file);
});
dropZone.addEventListener("keydown", (event) => {
  if (event.key === "Enter" || event.key === " ") {
    event.preventDefault();
    if (!loading && !exporting) fileInput.click();
  }
});
dropZone.addEventListener("dragover", (event) => {
  event.preventDefault();
  dropZone.classList.add("dragging");
});
dropZone.addEventListener("dragleave", () => dropZone.classList.remove("dragging"));
dropZone.addEventListener("drop", (event) => {
  event.preventDefault();
  dropZone.classList.remove("dragging");
  if (loading || exporting) return;
  const files = event.dataTransfer?.files;
  if (files?.length === 1 && files[0]) void loadFile(files[0]);
  else {
    error.textContent = "Drop one GPX file at a time.";
    error.hidden = false;
  }
});
trackSelect.addEventListener("change", selectTrack);
segmentSelect.addEventListener("change", renderSelection);
fitButton.addEventListener("click", () => {
  follow.checked = false;
  fitTrack();
});
playButton.addEventListener("click", () => {
  if (playing) {
    advanceReplay(performance.now());
    pauseReplay();
    return;
  }
  if (replay.duration <= 0 || loading) return;
  if (elapsed >= replay.duration) elapsed = 0;
  playing = true;
  lastFrame = performance.now();
  renderReplayPosition();
  updateReplayControls();
  frame = requestAnimationFrame(animateReplay);
});
restartButton.addEventListener("click", () => {
  pauseReplay();
  elapsed = 0;
  renderReplayPosition();
});
timeline.addEventListener("input", () => {
  pauseReplay();
  elapsed = Number(timeline.value);
  renderReplayPosition();
});
replaySpeed.addEventListener("change", () => {
  if (playing) advanceReplay(performance.now());
  playbackRate = Number(replaySpeed.value);
});
follow.addEventListener("change", renderReplayPosition);
map.on("pointerdrag", () => { follow.checked = false; });
document.addEventListener("visibilitychange", () => {
  if (document.hidden) pauseReplay();
});
basemap.addEventListener("change", () => {
  if (basemap.checked && !baseLayer.getSource()) {
    const tiles = new OSM();
    tiles.on("tileloaderror", () => {
      error.textContent = "Basemap tile failed to load. Check your connection or disable the basemap; track geometry remains available.";
      error.hidden = false;
    });
    baseLayer.setSource(tiles);
  }
  baseLayer.setVisible(basemap.checked);
});
