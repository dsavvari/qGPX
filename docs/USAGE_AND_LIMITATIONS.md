# Usage and limitations

## Loading and saving GPX

The C++ API lives in `qGpx::core`. `GPXParser::parse` reads XML in memory;
`parseFile` loads an accessible local file. `GPXWriter::serialize` produces XML
in memory; `saveFile` validates the data and then overwrites a local file.
Both use `Result<T>`: check `success` before accessing `value`, and report
`errorMessage` on failure.

```cpp
#include <GPXParser.hpp>
#include <GPXWriter.hpp>
#include <iostream>

qGpx::core::GPXParser parser;
auto loaded = parser.parseFile("input.gpx");
if (!loaded.success) {
    std::cerr << loaded.errorMessage << '\n';
} else {
    qGpx::core::GPXWriter writer;
    auto saved = writer.saveFile(*loaded.value, "output.gpx");
    if (!saved.success) {
        std::cerr << saved.errorMessage << '\n';
    }
}
```

## Output compatibility

Output follows [GPX 1.1](https://www.topografix.com/GPX/1/1/): UTF-8 XML, the
standard namespace, required version and creator attributes, schema-ordered
track elements, decimal measurements and UTC timestamps with fractional seconds.
An empty creator is written as `qGPX`. Coordinates must use WGS84 degrees;
latitude is in `[-90, 90]` and longitude in `[-180, 180)` as required by the schema.
Elevation is in meters. Non-finite measurements and invalid UTF-8/XML characters
are rejected. Standards compliance targets interoperability, but does not guarantee
acceptance by every application.

## Saving limitations

Saving is **not a lossless round trip**: only creator, track names/descriptions,
segments, coordinates, elevations and timestamps are written. Unsupported metadata,
waypoints, routes and extensions are not preserved; computed analytics are not
written. Empty GPX documents, tracks and segments can be serialized, but the existing
parser drops empty segments/tracks and requires at least one track containing points.
Writes are **not atomic**: open/write/close errors are reported,
but a write failure can leave a partial destination file.

## C API ownership and errors

The C API in [GPXCore.h](../include/GPXCore.h) exposes `gpxParseFile`, `gpxSerialize`
and `gpxSaveFile`.
Free serialized strings and error strings with `gpxStringFree`; free loaded data
with `gpxDataFree`. New operations set `outError` to NULL on success, or an allocated
error on failure when supplied. Serialization returns NULL on failure; saving
returns false.

### Setting the creator

GPX's `creator` identifies the software producing the file, not the person recording
the activity. C++ callers can assign `GPXData::creator` directly. C and Swift callers
can use `gpxDataSetCreator(handle, "My GPX app", &error)` before serialization or saving.
The setter copies the string, so the caller retains ownership of the input.
It returns false for a NULL handle or creator without changing the data; errors
follow the ownership rules above. Invalid UTF-8/XML text is rejected when serializing
or saving, not by the setter.

An empty string selects the writer's `qGPX` fallback. A pointer previously returned
by `gpxDataGetCreator` is no longer guaranteed valid after setting the creator.

## iOS integration

For iOS, compile the library and Expat for the selected device/simulator target
using Apple's toolchain. The C API can be called from Swift. Keep document selection,
security-scoped URL access and coordinated file operations in the Apple application
layer; pass XML to `gpxParse` and obtain XML with `gpxSerialize`. Path-based functions
are conveniences for accessible local files, not document-provider or permission APIs.
iOS builds are not currently exercised by this repository's tests.
