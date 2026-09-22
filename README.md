# MoeNotes Chart Parser

MoeNotes Chart Parser is a small, embeddable C17 library and command-line tool
for parsing the recovered MoeNotes live-chart format. It is designed as a
foundation for offline chart inspection, note-command generation, and future
score simulation. The project has no network, device, game-client, or
credential-handling component.

## Features

- Parses UTF-8 chart JSON and gzip-wrapped chart JSON in memory.
- Expands tap, flick, trace, long, and guide notes into stable note records.
- Converts ticks to millisecond and bar positions using BPM and time-signature
  segments.
- Supports mirror-lane transformation and flick direction mirroring.
- Exposes pair-note and line membership metadata through read-only accessors.
- Optionally derives slide Combo and ComboSkip records.
- Optionally materializes bookkeeping Hidden records associated with flicks on
  active long-note lines.
- Exposes judgement filtering, Full Combo Count calculation, and score-command
  adaptation.
- Uses opaque score handles, explicit result codes, and caller-provided
  allocator hooks.
- Includes a JSON CLI suitable for scripts and regression comparisons.

The parser is intentionally offline. It does not fetch charts, call service
APIs, access Android devices, or process authentication material.

## Build

Requirements: C17 compiler, CMake 3.16 or newer, and zlib.

```sh
cmake -S . -B build -DMS_BUILD_TESTS=ON -DMS_BUILD_CLI=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Build a static library with the Android NDK:

```sh
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DMS_BUILD_CLI=OFF \
  -DMS_BUILD_TESTS=OFF
cmake --build build-android
```

## CLI

```sh
./build/moenotes-chart-parser parse chart.json
./build/moenotes-chart-parser parse chart.json --mirror --combo-unit 8
```

JSON is written to stdout. A short human-readable summary and diagnostics are
written to stderr. The library API is declared in
`include/music_score.h`.

## Library API

The public API uses an opaque `ms_score_t` handle. Callers pass a byte buffer to
`ms_score_parse`, inspect records with read-only accessors, and release the
result with `ms_score_free`. The parser accepts optional `ms_allocator_t`
callbacks so an embedding application can control memory ownership.

The parser returns explicit `ms_result_t` values for invalid arguments, memory
allocation failures, gzip failures, JSON errors, schema errors, and range
errors. No exceptions or global mutable parser state are used.

## Validation

An offline golden chart fixture is used for regression validation. With
`--combo-unit 8`, the current implementation produces:

| Metric | Result |
| --- | ---: |
| Base nodes | 522 |
| Base judgement nodes | 514 |
| Combo nodes | 96 |
| ComboSkip nodes | 3 |
| Final judgement count | 610 |

The C implementation matches the established offline reference for these
counts. The test suite also covers gzip input, mirror lanes, line accessors,
and allocator-safe cleanup. AddressSanitizer and UndefinedBehaviorSanitizer
are used during local verification.

## Scope and limitations

This is an offline parser, not a complete gameplay or score simulator. The
current implementation preserves the `pos:auto` marker but does not yet apply
multi-point interpolation for it. Malformed-chart diagnostics are deliberately
minimal and may be expanded as more chart variants are validated.

`third_party/yyjson.{c,h}` is vendored under its MIT license. See the license
notice at the top of those files. Project source is licensed under the MIT
license in `LICENSE`.

## License

The project source is released under the MIT License. The vendored yyjson
source retains its original copyright and license notice.
