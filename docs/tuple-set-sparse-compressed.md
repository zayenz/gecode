# Sparse-Compressed TupleSet Design Note

## Idea

A sparse-compressed TupleSet representation would keep the sparse incremental
propagator algorithm, but replace each `(position,value) -> tuple_id[]` support
list with compressed support-word lists like those used by dense-compressed
posting:

```text
(position,value) -> [(tuple_word_index, support_bits), ...]
```

The propagator would still keep the sparse tuple-to-value-id map:

```text
(tuple_id,position) -> support_value_id
```

That map is required when a tuple is deactivated: every value used by that
tuple must have its live support count decremented. Without it, the sparse
support-count update would need to recover the tuple's values by another lookup,
which would lose the main advantage of the sparse algorithm.

## Algorithm

The propagator would otherwise be the existing sparse incremental propagator:

1. Keep active tuple ids and live support counts for every support value.
2. When a variable loses a value, fetch that value's support.
3. Enumerate the tuple ids in the support and deactivate them.
4. For each deactivated tuple, use `sparse_tv` to decrement all affected
   support counts.
5. Queue positive-table values whose support count reaches zero.

The only changed operation is step 3. Plain sparse scans an array of tuple ids.
Sparse-compressed would scan compressed support words and enumerate set bits.

## Expected Trade-Off

Sparse-compressed is not automatically "best of both worlds".

Plain sparse support-list cost is roughly:

```text
4 bytes * number_of_tuple_cells
```

Sparse-compressed support-list cost is roughly:

```text
sizeof(CSupportWord) * number_of_nonzero_tuple_words
```

On this platform `CSupportWord` is substantially larger than a single tuple id:
it stores a tuple-word index plus a full support word. The compressed list wins
only when a support list has several tuple ids in the same tuple-word block. If
tuple ids are scattered, it can use more memory and adds bit-enumeration
overhead.

This means the representation is most plausible when tuple ordering clusters
supports, or when values have many supporting tuples per tuple-word block. It is
less attractive for singleton-like supports, such as a word-id column.

## Initial Microbenchmark

Temporary prototype benchmark:

```text
/tmp/tuple_epk_bench <variant> <arity> <domain> <tuples> <repetitions>
```

Each run built a random tuple set, finalized it with the selected
representation, posted one positive extensional constraint, and enumerated all
solutions. Values below are medians of five runs from the prototype.

| Case | Sparse ms | Sparse-compressed ms | Dense-compressed ms |
|---|---:|---:|---:|
| arity 6, domain 16, tuples 512, reps 20 | 35.979 | 36.298 | 26.013 |
| arity 8, domain 32, tuples 2000, reps 8 | 123.719 | 127.148 | 110.463 |
| arity 10, domain 64, tuples 4000, reps 4 | 249.400 | 253.527 | 260.002 |

The prototype was close to sparse on these random cases, but not faster. The
large generated `gecode-test` cases were also noticeably slower than plain
sparse.

Based on this first pass, sparse-compressed should not be kept as a public
TupleSet representation and should not be selected by `EPK_AUTO`. It is a useful
design to remember, but it needs a benchmark family with clustered tuple ids and
a clear speed or memory win before it is worth exposing.
