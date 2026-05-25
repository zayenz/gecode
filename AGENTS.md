# Agent Notes

This branch is for clone, memory, and sanitizer stability work on top of
`origin/build-modernization`.

Read `docs/clone-memory-stability.md` before changing clone, recovery, actor
lists, local objects, no-goods, search stop objects, sanitizer configuration, or
fault-injection tests.

## Branch Discipline

- Keep changes split by cause and effect: first add or identify a failing test,
  then apply the smallest fix, then record the verification.
- Do not fold unrelated UBSan, TSan, ASan, and exception-safety fixes into one
  patch unless a shared invariant truly requires it.
- Generated files must stay synchronized with their generators. In particular,
  changes to `gecode/kernel/var-imp.hpp` that come from
  `misc/genvarimp.py` must update the generator as well.
- Gecode does not preserve ABI compatibility between patch releases, so layout
  changes are allowed when they are the right fix. Still measure hot paths when
  touching search, propagation, branching, or actor-list traversal.

## Verification

Use normal builds for quick iteration, then use the extended test matrix in
`docs/clone-memory-stability.md` for this branch. Sanitizer and fault-injection
logs should be stored under `findings/` or another clearly named local results
directory.
