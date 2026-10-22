# Dashed — Final Engineering and Performance Report

- **Date:** October 22, 2026
- **Project:** Dashed, bounded string variables and constraints for Gecode
- **Release integration commit:** `6db6169ff`
- **Historical baseline:** `3b0c40d7e0713c84c7eee2d790819061fd6b9e47` (`Port G-Strings to Gecode 6.4`)

## Executive summary

Dashed is the completed refactoring and modernization of G-Strings for Gecode
6.4. It preserves the established Gecode-facing API and library names for
compatibility while substantially improving correctness, Unicode support,
branching robustness, memory ownership, automata processing, packaging, and
cross-platform validation.

Against the first Gecode 6.4 port, the final core delivers:

- **1.774x geometric-mean speedup** across the nine reliable workloads;
- **43.8% lower aggregate median runtime**, from 5.330 s to 2.995 s;
- **no stable runtime regression above 5%**;
- **no significant peak-memory regression**;
- up to **6.562x speedup** on match-heavy workloads;
- identical correctness signatures across all measured workloads.

The production refactoring was finalized in `9aace0a3e`. The subsequent commits
through `6db6169ff` address tests, CI, cross-platform linkage, build systems,
packaging, and documentation; they do not alter the solver algorithms measured
by the final benchmark.

## Main engineering improvements

### Unicode-safe value and alphabet model

Dashed separates solver symbols from byte encodings. `StringSymbol` represents
a Unicode scalar value and `StringVal` provides an encoding-neutral concrete
value type.

The public boundary now includes:

- strict UTF-8 and UTF-16 decoding and encoding;
- rejection of malformed sequences, surrogate scalar values, and values above
  `U+10FFFF`;
- explicit legacy byte conversions that reject, rather than truncate,
  out-of-range values;
- Unicode-aware FlatZinc literals, escapes, output, regular expressions, and
  active alphabets;
- symbolic wildcard and complemented-character-class handling without
  enumerating the Unicode space.

Legacy byte-oriented interfaces remain available for compatibility, with their
one-byte-per-symbol contract made explicit.

### Correct and recomputation-safe branching

The original block-relative replay could reinterpret a decision incorrectly
after cloning or recomputation if the dashed-domain segmentation had changed.
Dashed introduces hybrid semantic replay:

- the direct block path is retained for domains where it is unambiguous and
  fast;
- ambiguous decompositions fall back to a position-based semantic decision;
- archived choices contain scalar decision data, never pointers into a space
  or domain;
- overlapping-block tests verify disjoint solutions without duplicates;
- branching combinations that are not implemented now fail explicitly instead
  of silently installing no brancher.

The refactoring also removed mutations performed from `choice()`, which could
make valid solutions disappear, corrected several selector calculations, and
made preferred-symbol selection local and allocation-free. No process-global
branching state remains.

FlatZinc keeps `-c-d 1` as its production recomputation setting. Both benchmark
revisions were explicitly measured with this same value.

### Clear DS/NS ownership boundaries

The two dashed-domain representations were retained deliberately:

- `DS*` objects are owned by a Gecode `Space`, use its allocation model, and
  participate in cloning;
- `NS*` objects have ordinary C++ lifetimes and support temporary analysis and
  transactional refinement.

Read-only algorithms were generalized to accept either representation.
Conversion to space-owned storage now occurs only at the final commit. This
removed temporary `DSBlock` and `DSIntSet` allocations that could not be
reclaimed at C++ scope boundaries.

Additional changes include:

- direct initialization of space-owned ranges and blocks;
- direct construction of `DSBlocks` from `NSBlocks`;
- DFA and Match analysis operating directly on temporary `NSBlock` values;
- correct return of removed range nodes to the Gecode space free-list;
- removal of redundant set copies during regular-expression processing.

### Transactional propagation and event reporting

Multi-domain refinements now follow an explicit analyze-mutate-notify contract.
The variable implementation snapshots relevant state, commits a complete
refinement, and emits at most one aggregate modification event.

This prevents:

- partially updated domains after an intermediate failure;
- stale change markers;
- duplicate or inconsistent scheduling;
- propagators observing a half-committed dashed-domain transformation.

Sweep-based constraints analyze proposed refinements before applying them, and
symmetric constraints preserve a defined directional commit order.

### More efficient regular and Match automata

Regular-expression and Match processing now preserve symbolic character-set
labels through automata construction and propagation.

The implementation:

- partitions regex symbols before determinization;
- coalesces transitions between the same state pair into range sets;
- builds block-filtered reverse tables once per pass;
- appends transitions directly to caller-owned frontier sets;
- avoids per-state temporary frontier allocations;
- shares immutable automata safely between cloned propagators;
- uses scalar-aware fixed-prefix and Match strategy calculations.

These changes are central to the largest measured speedups.

### Cloning, sharing, and memory discipline

Space-owned string domains are copied exactly once during cloning. Views install
the copied variable implementation rather than copying the domain again.
Immutable automata and GCC mappings use typed shared ownership between clones,
while mutable frontier storage remains local.

The String brancher clones its complete view array and retains only scalar
auxiliary state. Choices never archive space-local addresses.

### Integration and compatibility

Dashed remains source- and binary-compatible with the established Gecode String
module naming:

- header: `<gecode/string.hh>`;
- library: `gecodestring`;
- CMake component: `string`;
- CMake target: `Gecode::gecodestring`.

The CMake component closure and Autoconf linkage now include the real String
dependencies on Int, Set, and MiniModel. Installed-package consumer tests cover
Linux, Windows, and vcpkg.

## Benchmark methodology

- **Baseline:** `3b0c40d7e0713c84c7eee2d790819061fd6b9e47`
- **Final measured core:** `2458d467d+final-refactor`
- **Production core commit:** `9aace0a3e`
- **Release integration commit:** `6db6169ff`
- **Samples:** one warm-up and nine measured runs per revision and workload
- **Execution order:** alternating baseline/current for every sample pair
- **CPU affinity:** logical CPU 0
- **CPU governor:** `powersave`
- **Compiler:** GCC 13.3.0
- **Build:** shared libraries, `-O3 -DNDEBUG -march=native`
- **Disabled optional features:** Qt, Gist, MPFR, and CPProfiler
- **FlatZinc recomputation distance:** explicit `-c-d 1` for both revisions

The benchmark records wall, user, and system time; peak RSS; normalized output;
correctness signatures; and solver statistics. Hardware performance counters
were unavailable because the host used `perf_event_paranoid=4`.

## Performance results

### Aggregate

- Reliable workloads: **9 of 16**
- Geometric-mean speedup: **1.774x**
- Sum of reliable median runtimes: **5.330 s → 2.995 s**
- Aggregate runtime reduction: **43.8%**
- Stable regressions above 5%: **0**
- Stable improvements above 5%: **5**
- Correctness-signature differences: **0**
- Significant peak-RSS regressions: **0**

### Detailed results

| Workload | Category | Baseline | Dashed | Delta | Speedup | Assessment |
|---|---|---:|---:|---:|---:|---|
| `anbn_n10000` | construction | 0.0106 s | 0.0105 s | -0.7% | 1.007x | Too fast |
| `chunk_split_n10000` | concatenation | 0.0115 s | 0.0117 s | +1.8% | 0.982x | Too fast |
| `fzn_dna_l512_k15` | FlatZinc Match | 0.6967 s | 0.3559 s | -48.9% | 1.958x | Improvement |
| `fzn_inglenook` | FlatZinc model | 0.9696 s | 0.9759 s | +0.7% | 0.994x | Neutral |
| `fzn_latin_n14` | FlatZinc Match | 0.9401 s | 0.2167 s | -77.0% | 4.339x | Improvement |
| `gray` | relation/search | 0.0421 s | 0.0418 s | -0.7% | 1.007x | Neutral, confirmed |
| `gsql_n500` | GCC | 0.0127 s | 0.0124 s | -2.5% | 1.026x | Too fast |
| `hamming_mod_n10000` | combined | 0.0118 s | 0.0113 s | -4.0% | 1.042x | Too fast |
| `levenshtein_n10000` | Element | 0.0116 s | 0.0111 s | -4.1% | 1.043x | Too fast |
| `longest_match` | Match/search | 1.8822 s | 0.6874 s | -63.5% | 2.738x | Improvement |
| `minimum_superstring` | find/search | 0.6239 s | 0.6359 s | +1.9% | 0.981x | Neutral |
| `palindrome` | relation/search | 0.0333 s | 0.0310 s | -7.0% | 1.075x | Neutral, confirmed |
| `shortest_match` | Match/search | 0.1040 s | 0.0158 s | -84.8% | 6.562x | Improvement |
| `sql_n10000` | regex/domain | 0.0506 s | 0.0508 s | +0.3% | 0.997x | Noisy |
| `sql_n2500` | regex/domain | 0.0379 s | 0.0352 s | -7.2% | 1.077x | Improvement |
| `string_replace_n10000` | combined | 0.0122 s | 0.0125 s | +2.2% | 0.978x | Too fast |

### In-process confirmation

Gray and Palindrome were retested using 100 solves per process, one warm-up,
nine alternating measured samples, and the same CPU affinity. This isolates
solver cost from process startup and scheduler noise.

| Workload | Baseline | Dashed | Delta | Baseline/Dashed MAD |
|---|---:|---:|---:|---:|
| `gray` | 0.0247 s | 0.0254 s | +3.0% | 2.8% / 1.4% |
| `palindrome` | 0.0166 s | 0.0170 s | +2.2% | 4.1% / 4.1% |

Both confirmed changes remain below the 5% reporting threshold.

## Memory results

No workload showed a significant peak-memory regression. Process-level RSS
improved substantially for:

- `longest_match`: **-87.8%**
- `shortest_match`: **-45.2%**

Valgrind Massif was rerun with `--stacks=yes` and explicit `-c-d 1` for both
revisions:

| Workload | Baseline peak heap | Dashed peak heap | Delta |
|---|---:|---:|---:|
| `dna` | 142.6 MiB | 140.3 MiB | -1.6% |
| `latin` | 50.6 MiB | 48.0 MiB | -5.1% |

An earlier diagnostic had accidentally compared different implicit clone
distances. Pinning both revisions to the production-safe value removed that
methodological mismatch and showed that Dashed reduces peak heap in both cases.

## Correctness and interpretation

Correctness hashes exclude runtime, solver counters, and pointer addresses.
Workloads with non-unique solutions use assertions and successful exit status.
FlatZinc signatures compare solve status and final objective, allowing different
valid solutions.

`chunk_split_n10000` and Inglenook selected different valid solutions between
the two revisions. Inglenook retained the same optimum. These output differences
are reported separately and are not correctness failures.

A runtime improvement or regression requires:

- a median change above 5%;
- at least one revision with a runtime of 20 ms or more;
- relative median absolute deviation no greater than 10%.

An RSS signal requires both a change above 25% and an absolute difference above
4 MiB.

## Validation and release readiness

The final code was validated beyond the performance suite:

- complete GCC and Clang builds;
- complete C++ String test suites;
- complete MiniZinc and FlatZinc suites;
- ASan and UBSan validation;
- LeakSanitizer validation of the String suite;
- staged installation and external installed-package consumer;
- Doxygen generation with zero warnings and errors;
- ABI export comparison and bidirectional header/library smoke tests;
- Linux, macOS, Windows, MSYS2, and vcpkg CI;
- 14 required GitHub Actions jobs passing on the release integration branch.

The String library keeps the SONAME `libgecodestring.so.51`. The final ABI
comparison found no removed exported symbol. New headers with the previous
library and previous headers with the new library both passed compatibility
smoke tests.

## Conclusion

Dashed completes the transition from the original G-Strings port to a
production-ready String module for Gecode 6.4. The refactoring improves the
largest Match and regular-language workloads by factors of roughly 2x to 6.5x,
reduces aggregate runtime by 43.8%, and introduces no stable performance or
memory regression above the reporting thresholds.

The performance gains are accompanied by stronger correctness guarantees,
encoding-neutral Unicode support, recomputation-safe branching, disciplined
space ownership, transactional propagation, explicit dependency packaging, and
cross-platform release validation.

## Supporting data

The complete machine-readable benchmark artifacts are archived separately from
the repository. They include raw per-run results, CSV summaries, workload
definitions, the benchmark environment, in-process confirmations, and Massif
measurements.
