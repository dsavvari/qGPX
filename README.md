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

## Public headers

- `GPXParser.hpp` parses GPX XML strings and files into the `visualgpx::core` data
	model.
- `GPXAnalytics.hpp` calculates distance, elevation change, and duration.
- `GPXModels.hpp` defines the GPX data, track, segment, point, and result types.
- `GPXCore.h` exposes the C API.

## License

See [LICENSE](LICENSE).
