# Local GPX viewer

An optional development UI, not a hosted service. The browser uploads GPX to a
loopback-only C++ server, which uses the real qGPX parser and analytics. HTTP and
JSON code live in `src/viewer/`; this frontend contains no GPX parser.

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

## First iteration

- File picker or drag/drop, one `.gpx` file at a time, at most **10 MiB**.
- Track and segment selection, interactive map with fit/zoom/pan.
- Distance, elevation gain/loss, duration, and sample coverage from the library.
- Elevation profile with gaps at segment boundaries and missing elevations.
- Visible upload/parser errors; a failed upload leaves the previous view intact.
- Missing elevation/time values remain distinct from zero measurements.
- Optional OpenStreetMap tiles, **off by default**. With tiles disabled, assets
  and track processing need no internet connection after the build.

Uploads are analysed in memory, not saved. There is no database, account system,
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

## API

`POST /api/analyze`, with raw GPX XML and `Content-Type: application/gpx+xml`.
No multipart wrapper or filename is required.

```sh
curl -H 'Content-Type: application/gpx+xml' \
  --data-binary @test/activity_22044819196.gpx \
  http://127.0.0.1:8080/api/analyze
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
    "durationSeconds": null,
    "segments": [{
      "distanceMeters": 0,
      "elevationGainMeters": 0,
      "elevationLossMeters": 0,
      "durationSeconds": null,
      "points": [{
        "latitude": 38,
        "longitude": 23,
        "elevationMeters": null,
        "timeUnixMs": null,
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
clang-format --dry-run --Werror src/viewer/*.cpp src/viewer/*.hpp test/viewer/*.cpp
```

See [third-party dependencies](../third_party/README.md). Every production
frontend build includes `third-party-licenses.txt`, accessible from the UI.
