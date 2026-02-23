Applied an optimization pass focused on compressed hot paths, using the propagator/memory guidance you referenced.

What I changed

Added a compressed-only reverse index map in BitSet (word_index -> active_position+1) and enabled it only when posting compressed constraints:
extensional.hh
bit-set.hpp
Reworked aligned compressed ops to use the map (instead of repeated binary search per active word) in:
add_to_mask_aligned
intersects_aligned
ones_aligned
nand_with_mask_aligned
Kept dense/sparse runtime kernels unchanged; map is only constructed for compressed posting.
Wired compressed propagators to request aligned mode on table construction:
compact.hpp
Updated tiny-bitset constructor signature for compatibility:
tiny-bit-set.hpp
Validation

Build: make -j10 test passed.
Full extensional run:
DYLD_LIBRARY_PATH=. ./test/test -iter 1 -threads 1 -test '^Int::Extensional::' passed.
Timing (TupleSet-only extensional filters, -iter 1 -threads 1)

Dense:
before: 45.46s user
after: 45.14s user
DenseCompressed:
before: 139.85s user
after: 18.96s user
Sparse:
before: 201.44s user
after: 197.69s user
So DenseCompressed got a major speedup, with dense effectively unchanged.