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

The current external corpus contains 356 SS JSON charts (340 song charts and
16 auxiliary charts). All pass the compatibility runner in base, generated-note,
mirrored, deterministic-rerun, and gzip modes: 1,780 parses per build. Checks
include IDs, references, geometry, source and generated mirror invariants,
canonical branch endpoints, and line memberships. Four additional SUS-like
text assets return `MOENOTES_ERR_UNSUPPORTED`.

With derived combos enabled, all 340 song totals match the available package
master after the v0.2.0 timing correction (v0.1.0 matched 321). This aggregate
comparison is supplementary evidence: the package master and downloaded chart
versions are not independently bound, and equal counts do not prove equal nodes.
146 JSON inputs retain shared-endpoint warnings.

A separate historical corpus had 139 JSON charts and 64 legacy text charts;
that was the v0.1.0 baseline, not an additional current-corpus test or a claim
that all historical formats are supported. Real game charts and native binaries
are not distributed here.

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
- Pairing now follows the six-type candidate cache, begin/hidden/other creation
  priorities, and bar/progress equality. Three-way simultaneous links can be
  asymmetric. Full native collection/alias ordering and Flick-slot interactions
  have not been replayed end to end.
- Tick-space rendering samples are distinct from creator geometry. Mirror
  preserves source easing for the latter and reflects independent edges for
  rendering. Equal-millisecond bar/progress fallback is implemented. Neither
  sampler is a complete native renderer or gameplay hitbox evaluator.
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
