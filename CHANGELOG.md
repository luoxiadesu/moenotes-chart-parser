# Changelog

All notable project changes are recorded here. Versions use
`MAJOR.MINOR.PATCH`; see the [API compatibility policy](docs/api.md#version-and-stability).

## [Unreleased]

## [0.1.0] - 2026-09-23

First versioned release and public C API baseline. The interface is frozen for
the v0.1.x series; experimental derived values remain subject to corrections.

### Added

- Public `moenotes_*` API, version macros, and `moenotes_version_string()`.
- Line sampling, shared-endpoint memberships, warning flags, floating-point
  geometry, and source/generated metadata for preview renderers.
- Skill, fever, and call-event accessors, including call timing values.
- Experimental relative-eighth Combo/ComboSkip generation and explicit-slot
  Flick-hidden generation, both opt-in.
- Installable static library/header/CLI, CMake package, and the
  `moenotes::chart_parser` consumer target.
- CLI `--help` and `--version`, complete C example, API reference, compatibility
  documentation, and development/release guidance.
- API-contract and installed C/C++ consumer tests, expanded synthetic
  regressions, external-corpus runner, and optional Clang libFuzzer target.
- GitHub Actions checks for the public contract, packaging, and sanitizers.

### Changed

- **Breaking from the unversioned prototype:** renamed `music_score.h` to
  `moenotes_chart_parser.h`, `ms_*` to `moenotes_*`, and `MS_*` to `MOENOTES_*`,
  including CMake options. Public view structures gained fields. No aliases are
  retained; consumers must migrate and recompile.
- IDs are explicitly library-local, not native-client IDs. Count agreement is
  no longer presented as proof of gameplay equivalence.
- Legacy SUS-like text and unsupported combo units return an unsupported error.

### Fixed

- Compact time-signature parsing, timeline defaults, and relative combo placement.
- Tick-distance automatic-position interpolation, independent edge easing,
  width preservation, and mirror geometry.
- Flick-hidden association no longer attaches standalone Flicks to every
  overlapping long line.
- Allocation lifetime issues during note-array growth; gzip now uses caller
  allocator hooks and participates in allocation-failure tests.
- Unknown operation values no longer report themselves as judgement notes.
- Release builds retain regression assertions.

### Known Limitations

- Legacy SUS text is unsupported; 139 external JSON charts pass compatibility
  checks, while 64 legacy text charts are recognized as unsupported.
- Complex native graph ordering, pairing, exact derived numerical boundaries,
  and full native-converter differential verification remain incomplete.
- Windows and Android NDK builds are unvalidated. See
  [Compatibility](docs/compatibility.md) for the complete scope.

[Unreleased]: https://github.com/luoxiadesu/moenotes-chart-parser/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/luoxiadesu/moenotes-chart-parser/releases/tag/v0.1.0
