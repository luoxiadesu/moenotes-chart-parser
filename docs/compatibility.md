# Compatibility and Scope

MoeNotes Chart Parser reads SS JSON, optionally wrapped in a `score` object,
and gzip-compressed SS JSON. It is an offline parser and geometry foundation,
not a full gameplay, scoring, or rendering engine.

## Supported Input

The score object contains `events` (an object) and `notes` (an array).
Known note types include tap, flick, trace, long, guide, and line nodes.
Compact time signatures such as `"sig": [3, 4]` are supported, as are the
explicit `numerator`/`denominator` form. Unknown fields are ignored.

Convenience extensions include an unwrapped score object, an outer-begin long
form, and `line_indices: [0, ...]` for explicit Flick-to-slot associations.
The latter is **not a field observed in native SS JSON charts**. Standalone
Flicks do not automatically acquire every overlapping long line's slot.
Explicit hidden-node generation excludes the line's end tick.
Accepted schemas and rejected inputs are not a byte-for-byte emulation of
the native deserializer.

## Validation Scope

An external historical corpus contains 203 files: 139 SS JSON and 64 legacy
SUS-like text charts. All 139 JSON files pass the compatibility runner in base,
generated-note, mirrored, deterministic-rerun, and gzip modes, totaling 695
successful parses per run. The runner checks IDs, references, finite geometry,
determinism, gzip equivalence, and source-geometry mirror invariants.
The 64 text charts return `MOENOTES_ERR_UNSUPPORTED`; a SUS parser is not
implemented. No game charts or native binaries are distributed here.

Synthetic regressions cover automatic positions, separate edge easing,
compact signatures, events, relative combos, guide overlaps, shared endpoints,
explicit-slot hidden nodes, malformed input, gzip, and allocation failures.
Linux GCC/Clang builds and Clang ASan/UBSan have been exercised locally.
Windows and Android NDK builds have not been validated.

These tests establish implementation compatibility and robustness, not exact
native conversion. Earlier aggregate-count agreement was not a valid proof:
the prototype ignored compact signatures and used an incorrect combo grid.
The prototype must not be used as a correctness oracle.

## Known Boundaries

- Combo/ComboSkip generation remains experimental. Shared-begin branching,
  native creation order/IDs, unusual meter changes, and precise float32 timing
  boundaries have not been checked against execution of the complete native
  converter. Derived Full Combo counts are not authoritative game totals.
- Matching endpoints are coalesced, but complex shared graphs carry
  `MOENOTES_WARNING_SHARED_ENDPOINT` because complete native graph traversal
  and view ordering are unverified.
- Nonmonotonic line nodes are preserved rather than silently reordered, and
  flagged with `MOENOTES_WARNING_NONMONOTONIC_LINE`. Sampling and derived combos
  on these lines are not authoritative.
- Same-tick creator ordering, Flick-slot interactions, and the native pairing
  cache remain unverified. Pair links cover common simultaneous candidates,
  not every guide exception or native ordering rule.
- Tick-space rendering samples are distinct from time-space gameplay geometry.
  Same-time fallback behavior and all native mirror paths are not fully proven.
- The library does not implement judgement windows, skill execution, card/team
  bonuses, or final gameplay scoring. Commands are data projections only.
- There is no full native-converter differential harness or live-client
  equivalence guarantee. Algorithm corrections may change output; the public
  API compatibility policy is documented separately in [api.md](api.md).

## Resource Limits

Input and decompressed data are each limited to 64 MiB. Collections are limited
to one million records, and there are 20 concurrent line slots per kind.
Ticks and elapsed times must fit nonnegative int32 values. Gzip input must
contain exactly one member with no trailing data.

These are growth limits, not a bounded CPU-time or total-allocation guarantee.
Some graph operations have quadratic behavior. Isolate untrusted workloads
and impose external time/memory limits; fuzzing is not a security certification.
