# Third-party dependencies

Third-party code is kept out of the maintained library and viewer source.
Vendored files are unmodified upstream downloads; retain their copyright and
license notices when redistributing them. The viewer is optional and does not
add HTTP, JSON, or frontend dependencies to the `qGPX` library target.

## Vendored C++ headers (viewer only)

| Component | Pinned release | License | Source | Local notice |
|---|---|---|---|---|
| cpp-httplib | v0.66.0 | MIT | [upstream](https://github.com/yhirose/cpp-httplib/tree/v0.66.0) | [LICENSE](cpp-httplib/LICENSE) |
| nlohmann/json | v3.12.0 | MIT, with embedded component notices below | [upstream](https://github.com/nlohmann/json/tree/v3.12.0) | [LICENSE.MIT](nlohmann-json/LICENSE.MIT), [NOTICE.txt](nlohmann-json/NOTICE.txt) |

The JSON header includes MIT-licensed UTF-8/Grisu2 code, CC0-1.0 Hedley code, and
Apache-2.0 Abseil compatibility code. Supplemental texts are retained in
[LICENSES/](nlohmann-json/LICENSES/). The Apache text comes from the same
upstream JSON tag; the CC0 text comes from
[Creative Commons](https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt).
Our [NOTICE.txt](nlohmann-json/NOTICE.txt) records the embedded attributions
documented by upstream; it is not an upstream file.

SHA-256 of the vendored headers:

```text
51ddd1482c67acb31f3064e48329aade14acc7dd0fab31ee5397193697ea1c35  cpp-httplib/httplib.h
aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63  nlohmann-json/json.hpp
```

For updates, download each header and its license from an explicit upstream
tag, review embedded notices, update these versions/checksums and the viewer's
notice-generation configuration, and rerun viewer tests. Do not format vendored
headers.

## Frontend packages

Frontend dependencies are isolated under `viewer/node_modules/`, not copied into
application source or committed. `viewer/package-lock.json` pins exact versions
and integrity hashes.

| Direct dependency | Role | License |
|---|---|---|
| OpenLayers (`ol`) | Map rendering | BSD-2-Clause |
| TypeScript | Development: type checking | Apache-2.0 |
| Vite | Development: bundling | MIT |

The production build generates `viewer/dist/third-party-licenses.txt` using the
packages in Vite's module graph, retaining their full license files as well as the
vendored C++ notices above. The UI links to this file. Generation fails if a
bundled package is missing its license file. Preserve this generated file with
any distributed frontend build. Unused OpenLayers format readers and their
dependencies are not intentionally bundled in this first iteration.

Transitive package license identifiers are recorded in the lockfile; original
notices remain in each installed package. Build-tool dependencies are not
distributed as part of the production frontend.

## Existing core/test dependencies

- **Expat:** MIT, system-provided; [upstream license](https://github.com/libexpat/libexpat/blob/master/COPYING).
- **GoogleTest v1.14.0:** BSD-3-Clause, fetched into the CMake build directory only
  for tests; [upstream license](https://github.com/google/googletest/blob/v1.14.0/LICENSE).

## Map data

The optional basemap uses OpenStreetMap's public tile service. OpenLayers
displays its attribution when enabled. OSM data is under
[ODbL 1.0](https://www.openstreetmap.org/copyright); access to the public tile
service is subject to its separate
[tile usage policy](https://operations.osmfoundation.org/policies/tiles/).
This viewer does not bulk-download or provide offline tiles. A distributed or
hosted application should select a provider appropriate for its usage.
