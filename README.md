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
clang-format -i include/*.h include/*.hpp src/*.cpp test/*.cpp test/unit/*.cpp
```

Check formatting without changing files:

```sh
clang-format --dry-run --Werror include/*.h include/*.hpp src/*.cpp test/*.cpp test/unit/*.cpp
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

## Continuous integration

GitHub Actions runs on pushes and pull requests targeting `main`. It configures and
builds the project, checks C++ formatting, and runs the registered tests with CTest.

## Public headers

- `GPXParser.hpp` parses GPX XML strings and files into the `visualgpx::core` data
	model.
- `GPXAnalytics.hpp` calculates distance, elevation change, and duration.
- `GPXModels.hpp` defines the GPX data, track, segment, point, and result types.
- `GPXCore.h` exposes the C API.

## License

See [LICENSE](LICENSE).
