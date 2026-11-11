# qGPX

qGPX is a C++17 library for parsing GPX track data and calculating track distance,
elevation gain and loss, and duration. It provides both a C++ API and a C API.

## Dependencies

- CMake 3.20 or newer
- A C++17 compiler
- Expat
- GoogleTest is fetched by CMake when tests are enabled (the default)

On macOS, install Expat with `brew install expat`. On Ubuntu or Debian, install it
with `sudo apt install libexpat1-dev`.

## Build and test

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

To build the library without tests:

```sh
cmake -S . -B build -DBUILD_TESTS=OFF
cmake --build build --parallel
```

## Code formatting

Use `clang-format` with the repository's `.clang-format` rules to format the
maintained C++ files. The commands below cover the public headers, implementation,
and tests:

```sh
clang-format -i include/*.h include/*.hpp src/*.cpp src/viewer/*.cpp src/viewer/*.hpp test/*.cpp test/unit/*.cpp test/viewer/*.cpp
```

Check formatting without changing files:

```sh
clang-format --dry-run --Werror include/*.h include/*.hpp src/*.cpp src/viewer/*.cpp src/viewer/*.hpp test/*.cpp test/unit/*.cpp test/viewer/*.cpp
```

Formatting is checked in CI, but is not part of the CMake build or configured to
run on save. CI installs the Ubuntu-provided `clang-format` package; its version is
not pinned.

To enable the repository's pre-commit formatting check in this clone, run once:

```sh
git config core.hooksPath .githooks
```

The hook checks staged C++ files and blocks commits with formatting issues. It
requires `clang-format` to be installed.

## Optional browser viewer

The [local development viewer](viewer/README.md) provides GPX upload, track and
segment selection, a map, statistics, and an elevation profile. It uses the C++
library through a loopback-only HTTP adapter; the library itself remains
frontend-independent.

```sh
npm --prefix viewer ci
npm --prefix viewer run build
cmake -S . -B build -DBUILD_VIEWER=ON
cmake --build build --parallel
./build/qgpx-viewer
```

Requires Node.js 24 or newer in addition to the core build dependencies.
Open <http://127.0.0.1:8080>. The optional external basemap is disabled by default.
`BUILD_VIEWER` defaults to `OFF`; core-only consumers do not need Node or the
viewer dependencies. See [third-party licenses](third_party/README.md) for
dependency sources and notices.

## Continuous integration

GitHub Actions runs on pushes and pull requests targeting `main`. It configures and
builds the project with the optional viewer, checks C++ formatting, builds and tests
the frontend, and runs the registered tests with CTest.

## Public headers

- `GPXParser.hpp` parses GPX XML strings and files into the `qGpx::core` data
	model.
- `GPXAnalytics.hpp` calculates distance, elevation change, and duration.
- `GPXModels.hpp` defines the GPX data, track, segment, point, and result types.
- `GPXCore.h` exposes the C API.

## License

See [LICENSE](LICENSE).
