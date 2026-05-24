# Clone and Memory Stability

This document records the invariants and test strategy for the
`feature/clone-and-memory-stability` branch. The branch goal is to make clone
failure, actor recovery, local-object copying, minimodel allocation, integer-set
allocation, and search synchronization failures reproducible and maintainable.

## Review and Commit Shape

Each behavioral fix should follow this order:

1. Add or identify a focused test that fails on the pre-fix code.
2. Apply the smallest fix that addresses the failed invariant.
3. Run the focused test again and record the command/log.
4. Add nearby regression coverage when the bug could reappear through a related
   path.
5. For performance-sensitive changes, run a before/after timing check on the
   affected search or propagation path.

The fault-injection harness and sanitizer matrix are extended testing support.
They are not expected to run in the default developer test loop.

## External PR Triage

This branch ports fixes from the reviewed exception-safety PR only when they can
be stated as a local invariant and covered by a focused regression test. Do not
carry over broad cleanups or performance-sensitive changes just because they
appear in the source PR.

Currently ported areas:

- failed clone construction and recovery of source-space forwarding state
- failed clone disposal-array allocation
- failed dispose-notice array allocation
- integer-set range allocation failure
- minimodel expression allocation failure
- marked advisor subscriptions under UBSan

Currently deferred areas:

- Moving propagation-stat accounting behind a virtual hook. The reviewed change
  adds a virtual call on the propagation hot path without a local override or a
  failing test in the source PR.
- Reducing tracer lock scope. That is a separate thread-safety and performance
  change, and needs its own TSan-backed review rather than being bundled with
  clone and allocation recovery.

## Clone and Recovery Invariants

### Space cloning

- A clone must either finish construction or restore the source space to a
  usable state before the exception escapes.
- During clone construction, forwarding links in source propagators, branchers,
  advisors, variables, and local objects are temporary state.
- Recovery must undo temporary forwarding without assuming all target actors were
  successfully copied.
- Partially copied clone actors belong to the unfinished clone and must be
  disposed exactly once.
- The disposal array used for forced actor cleanup must be either completely
  initialized or treated as absent. Failed allocation of that array must not make
  clone destruction walk uninitialized memory.
- Clone constructors for derived `Space` classes can throw after the base
  `Space` copy has changed source forwarding state. The base clone object must
  remember enough source information to recover in its destructor.

### Actor lists

- `pl` and `bl` sentinels must be initialized before recovery can inspect clone
  actor lists.
- `b_status` and `b_commit` must always either point at a valid brancher or the
  brancher-list sentinel representation used by the current implementation.
- Propagator, brancher, and local-object forwarding must be restored even if
  copying fails in the middle of a list.
- The current UBSan vptr handling for intentional actor-link sentinel casts is
  accepted for this branch. Do not widen those annotations without a specific
  failing test.

### Local objects

- Local-object copy forwarding is part of the same clone transaction as actors.
- A failed local-object copy must not leave source local objects forwarded to a
  dead clone object.
- A partially copied local-object list in the clone must be disposed by the
  unfinished clone, not by the source.

### Minimodel and integer-set allocation

- Expression helper objects that allocate arrays must initialize ownership fields
  before any later allocation can throw.
- Destructors must tolerate partial construction.
- Integer-set construction must check width and size arithmetic before allocating
  or copying range arrays.

## Fault Injection Strategy

Fault injection should be deterministic and opt-in.

Recommended CMake option:

```bash
cmake -S . -B build/fault-ubsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGECODE_ENABLE_FAULT_INJECTION=ON \
  -DGECODE_SANITIZER=undefined
cmake --build build/fault-ubsan --target gecode-test -j
build/fault-ubsan/bin/gecode-test -iter 1 -threads 1 -stop true -test '^Fault::'
```

Fault phases should map to one ownership boundary:

- heap allocation used by support containers
- propagator copy
- brancher copy
- derived `Space` copy
- local-object copy
- clone disposal-array allocation
- integer-set range allocation
- minimodel expression allocation

Each phase should have one test that proves the source object remains usable
after the injected failure and that the next non-failing operation still works.

## Sanitizer Strategy

Use a known-good sanitizer toolchain. On macOS, the Xcode toolchain may work when
the command-line-tools sanitizer runtime does not.

### UBSan

UBSan is the first line for arithmetic, object-lifetime, and partial-construction
checks.

```bash
cmake -S . -B build/sanitizer-ubsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGECODE_SANITIZER=undefined
cmake --build build/sanitizer-ubsan --target gecode-test -j
build/sanitizer-ubsan/bin/gecode-test -iter 1 -threads 4 -stop true -test '^Search::'
```

Useful focused shards:

- `^Fault::`
- `^Search::`
- `^NoGoods::`
- `^FlatZinc::`
- `^Int::Linear::`
- `^Int::Extensional::`
- split `^Set::` by nearby prefixes when the full set suite is too slow

### ASan

ASan checks ownership fixes and partial-construction cleanup. Run the fault suite
and the search suite after clone/recovery changes.

```bash
cmake -S . -B build/xcode-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGECODE_SANITIZER=address
cmake --build build/xcode-asan --target gecode-test -j
build/xcode-asan/bin/gecode-test -iter 1 -threads 4 -stop true -test '^Search::'
```

For fault injection:

```bash
cmake -S . -B build/xcode-fault-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGECODE_ENABLE_FAULT_INJECTION=ON \
  -DGECODE_SANITIZER=address
cmake --build build/xcode-fault-asan --target gecode-test -j
build/xcode-fault-asan/bin/gecode-test -iter 1 -threads 1 -stop true -test '^Fault::'
```

### TSan

TSan is required for search stop objects, parallel engines, portfolio search,
shared no-goods, and default statistics objects.

```bash
cmake -S . -B build/xcode-tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGECODE_SANITIZER=thread
cmake --build build/xcode-tsan --target gecode-test -j
build/xcode-tsan/bin/gecode-test -iter 1 -threads 1 -stop true -test '^Search::'
build/xcode-tsan/bin/gecode-test -iter 1 -threads 4 -stop true -test '^Search::'
```

For fault injection:

```bash
cmake -S . -B build/xcode-fault-tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGECODE_ENABLE_FAULT_INJECTION=ON \
  -DGECODE_SANITIZER=thread
cmake --build build/xcode-fault-tsan --target gecode-test -j
build/xcode-fault-tsan/bin/gecode-test -iter 1 -threads 1 -stop true -test '^Fault::'
```

## Performance Gates

Changes in these areas need timing evidence:

- `Space::status`, `Space::clone`, `Space::commit`, and actor-list traversal
- variable subscription scheduling
- search engines, especially parallel DFS/BAB and restart/portfolio search
- stop-object checks in worker loops

For performance-sensitive fixes, compare a clean base build and the candidate
build with the same compiler, build type, and test set. A useful first pass is:

```bash
cmake -S . -B build/perf-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/perf-release --target gecode-test -j
time build/perf-release/bin/gecode-test -iter 3 -threads 1 -stop true -test '^Search::'
time build/perf-release/bin/gecode-test -iter 3 -threads 4 -stop true -test '^Search::'
```

Use domain-specific benchmarks when a change touches a narrower subsystem.
