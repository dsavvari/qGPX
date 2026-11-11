# Agent Instructions

## Project structure

- Keep public C++ and C headers in `include/`, implementation files in `src/`, and tests in `test/`.
- The C++ API is in the `visualgpx::core` namespace; `GPXCore.h` is the C API.
- The CMake library target is `qGPX`. Keep source paths and target references aligned when editing CMake.

## Changes and validation

- Keep changes focused and follow the existing C++17 style and APIs.
- Use camelCase for functions and variables (including the C API in `GPXCore.h`, e.g. `gpxParse`); constants, types, and enum values use PascalCase (first character uppercase).
- Add or update tests in the relevant `test/` directory when behavior changes.
- Use the repository's `.clang-format` rules. Check formatting with:

  ```sh
  clang-format --dry-run --Werror include/*.h include/*.hpp src/*.cpp test/*.cpp test/unit/*.cpp
  ```

- Validate changes with the project build and tests:

  ```sh
  cmake -S . -B build
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```

- CMake requires Expat and fetches GoogleTest when `BUILD_TESTS` is enabled. Keep CI changes consistent with `.github/workflows/ci.yml`.
- Do not add generated build output from `build/` to source control.