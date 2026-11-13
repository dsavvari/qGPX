# Local GPX viewer

An optional development UI, not a hosted service. The browser uploads GPX to a
loopback-only C++ server, which uses the real qGPX parser and analytics. HTTP and
JSON code and the application's `main()` live in `viewer/server/`, separate from
the reusable core in `src/`. This frontend contains no GPX parser.
The viewer links `qGPX::qGPX`; the core never links the viewer or HTTP adapter.
With `BUILD_VIEWER=OFF`, none of the HTTP/application sources are compiled.

## Build and run

In addition to the core build dependencies, use Node.js 24 or newer and npm.
From the repository root:

```sh
npm --prefix viewer ci
npm --prefix viewer run build
cmake -S . -B build -DBUILD_VIEWER=ON
cmake --build build --parallel
./build/qgpx-viewer
```

Open <http://127.0.0.1:8080>. Stop the server with Ctrl-C.
For another port or a relocated frontend:

```sh
./build/qgpx-viewer 8081 /absolute/path/to/viewer/dist
```

The compiled default asset path points to this checkout's `viewer/dist`.
Rebuild the frontend after changing UI source, then refresh the page.
CMake does not invoke npm, fetch these vendored headers, or require Node for a
core-only build. `BUILD_VIEWER` defaults to `OFF`.

For troubleshooting, see [Common issues and how-to guides](../docs/COMMON_ISSUES_AND_HOW_TO.md).

## First iteration

- File picker or drag/drop, one `.gpx` file at a time, at most **10 MiB**.
- Download all loaded tracks as GPX 1.1 through the core writer.
- Track and segment selection, interactive map with fit/zoom/pan.
- Distance, elevation gain/loss, duration, and sample coverage from the library.
- Aligned heart-rate, cadence, and temperature profiles for the selected track or
  segment, with independent scales and gaps for missing samples or segment breaks.
  Other imported GPX extensions are preserved for export but are not interpreted.
- Derived travel speed in the replay and maximum speed for the selected track or
  segment, displayed in km/h.
- Elevation profile with gaps at segment boundaries and missing elevations.
- 2D replay: play/pause, restart, speed selection, timeline scrubbing, optional
  follow-camera, and synchronised dashed cursors across the elevation and sensor
  profiles.
- Visible upload/parser errors; a failed upload leaves the previous view intact.
- Missing elevation/time values remain distinct from zero measurements.
- Optional OpenStreetMap tiles, **off by default**. With tiles disabled, assets
  and track processing need no internet connection after the build.

Uploads are analysed and exported in memory, not saved on the server. There is no database, account system,
or arbitrary server-side file-path API. The server binds to `127.0.0.1`, rejects
foreign Host/Origin headers, and serves the frontend and API from one origin.
Do not expose it through a public proxy; deployment/authentication is out of scope.
Enabling the basemap sends tile requests for the viewed area to OpenStreetMap,
but does not upload GPX files there.

Only the library's **tracks/segments/points** are supported. GPX routes, standalone
waypoints, FIT, TCX, KML, GeoJSON imports, offline maps, editing, and moving-time
calculations are not part of this iteration. The map uses Web Mercator and clips
polar latitudes to its limits; antimeridian-spanning tracks are not unwrapped.
Elevation gain/loss use adjacent points with elevation; incomplete coverage is
shown. Duration uses the first and last available timestamps, including gaps
between segments, rather than summing moving time. Missing or invalid timestamps
appear unavailable according to the current core parser's behavior.

## Downloading GPX

After a successful upload, **Download GPX** downloads `<original-name>-export.gpx`.
The export includes **all tracks and segments**, regardless of the current selection.
The viewer retains the last successfully loaded file in browser memory and sends it
to `/api/export` on demand; the server parses it and uses the core GPX 1.1 writer.
The output creator is `qGPX Track lab`.

This is a modeled-data export, not a copy of the original file: unsupported metadata,
waypoints and routes are omitted. Recognized Garmin sensor values and retained GPX
extension trees are preserved by the core model; analytics and replay positions are
not written. See [Usage and limitations](../docs/USAGE_AND_LIMITATIONS.md) for output
validation rules and other limitations.

A failed upload leaves the previous file available for download. Export errors are
displayed without clearing the loaded track. Downloads are initiated by the browser;
the viewer cannot confirm that the file was saved to disk.

## Replay

Replay uses the selected track or segment. When every point has a finite,
strictly increasing timestamp, playback follows recorded timing. Between
segments it holds the last position until the next timestamp, then jumps;
it never interpolates a route across a segment gap.

Otherwise the entire selection uses a clearly labelled **constant-speed preview
at 5 m/s**, based on the library's cumulative distance. This does not claim
recorded timing; segment transitions are instantaneous. Speed selection
(1x to 60x, default 10x) multiplies either timeline. Missing elevation remains
unavailable, not zero or bridged over.

Scrubbing pauses playback. Restart pauses and returns to the start; Play at the
end starts again. Changing track/segment resets playback, and an upload pauses
it (a failed upload keeps the previous replay). Switching to a background tab
pauses playback. Follow-camera is optional; dragging the map or using Fit track
turns follow off. Single points and zero-duration previews show a position but
have no playable interval. All replay state is frontend-only; playback adds no
core dependency. Derived speed comes from the core analytics described below.

### Derived speed

The core calculates horizontal speed as Haversine distance divided by the
fractional-second time difference between adjacent points in the same segment.
Missing or non-increasing timestamps and non-finite results are excluded, never
bridged. Maximum speed is the largest valid interval speed in the selection;
zero is valid for a stationary interval, while no valid intervals means unavailable.
These are **unsmoothed estimates**, not device-reported instantaneous speed:
GPS noise/spikes can inflate the maximum.

Replay shows the speed of the interval currently being traversed, independent
of playback acceleration. At segment gaps or after the final point, speed is
unavailable rather than an invented zero. A constant-speed preview does not
become measured speed: intervals without valid recorded timing stay unavailable.
Even when the whole selection needs preview timing, any valid timed interval
still uses its actual derived speed.

## API

`POST /api/analyse`, with raw GPX XML and `Content-Type: application/gpx+xml`.
No multipart wrapper or filename is required.

```sh
curl -H 'Content-Type: application/gpx+xml' \
  --data-binary @test/activity_22044819196.gpx \
  http://127.0.0.1:8080/api/analyse
```

A successful response is JSON:

```json
{
  "schemaVersion": 1,
  "creator": "example",
  "tracks": [{
    "name": "Example track",
    "description": "",
    "pointCount": 1,
    "distanceMeters": 0,
    "elevationGainMeters": 0,
    "elevationLossMeters": 0,
    "maxSpeedMetersPerSecond": null,
    "durationSeconds": null,
    "segments": [{
      "distanceMeters": 0,
      "elevationGainMeters": 0,
      "elevationLossMeters": 0,
      "maxSpeedMetersPerSecond": null,
      "durationSeconds": null,
      "points": [{
        "latitude": 38,
        "longitude": 23,
        "elevationMeters": null,
        "heartRateBpm": null,
        "cadenceRpm": null,
        "temperatureCelsius": null,
        "timeUnixMs": null,
        "speedMetersPerSecond": null,
        "distanceMeters": 0
      }]
    }]
  }]
}
```

Point distance is cumulative **within its track**, without connecting separate
segments; it resets for each track. Coordinates are degrees, lengths/elevations
are metres, durations are seconds, and timestamps are Unix milliseconds.
Missing point elevation/time and unavailable duration are JSON `null`.
`heartRateBpm`, `cadenceRpm`, and `temperatureCelsius` are the recognized Garmin
point measurements, or `null` when unavailable. The UI plots available sensor
measurements against a shared distance axis, using independent vertical scales to
avoid mixing units.
`speedMetersPerSecond` describes the interval **ending at** that point and is
`null` at each segment's first point or for an invalid interval.
`maxSpeedMetersPerSecond` is available on both tracks and segments; it is `null`
when no valid interval exists. Speed values are m/s in the API, converted to km/h
only by the UI. These fields are additive to development schema version 1.
Zero elevation gain/loss follows the core API; the UI also checks sample coverage
and displays unavailable when there are no adjacent elevation pairs.

Errors use `{"error":"message"}` with HTTP 403 (origin/host), 413 (upload limit),
415 (content type), 422 (parse/measurement error), or 500 (unexpected server error,
also logged). Missing resources return 404. This is a development API, not a
stability guarantee for future public services.

## Validation and licenses

```sh
npm --prefix viewer test
npm --prefix viewer run build
ctest --test-dir build -R '^ViewerTest\.' --output-on-failure
clang-format --dry-run --Werror viewer/server/*.cpp viewer/server/*.hpp test/viewer/*.cpp
```

See [third-party dependencies](../third_party/README.md). Every production
frontend build includes `third-party-licenses.txt`, accessible from the UI.
