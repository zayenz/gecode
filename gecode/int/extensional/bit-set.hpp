/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Linnea Ingmar <linnea.ingmar@hotmail.com>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Linnea Ingmar, 2017
 *     Christian Schulte, 2017
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

namespace Gecode { namespace Int { namespace Extensional {

  forceinline const TupleSet::BitSetData*
  find_aligned_word(const TupleSet::CSupportWord* b,
                    const TupleSet::CSupportWord* e,
                    unsigned int widx) {
    while (b < e) {
      const TupleSet::CSupportWord* m = b + ((e-b) >> 1);
      if (widx < m->widx) {
        e = m;
      } else if (widx > m->widx) {
        b = m+1;
      } else {
        return &m->bits;
      }
    }
    return nullptr;
  }

  template<class IndexType>
  forceinline unsigned int
  BitSet<IndexType>::limit(void) const {
    return static_cast<unsigned int>(_limit);
  }
  
  template<class IndexType>
  forceinline bool
  BitSet<IndexType>::empty(void) const {
    return _limit == 0U;
  }
  
  template<class IndexType>
  forceinline unsigned int
  BitSet<IndexType>::words(void) const {
    return static_cast<unsigned int>(_limit);
  }

  template<class IndexType>
  forceinline unsigned int
  BitSet<IndexType>::size(void) const {
    return words();
  }
      
  template<class IndexType>
  forceinline unsigned int
  BitSet<IndexType>::width(void) const {
    assert(!empty());
    IndexType width = _index[0];
    for (IndexType i=1; i<_limit; i++)
      width = std::max(width,_index[i]);
    assert(static_cast<unsigned int>(width+1U) >= words());
    return static_cast<unsigned int>(width+1U);
  }

  template<class IndexType>
  forceinline
  BitSet<IndexType>::BitSet(Space& home, unsigned int n, bool aligned)
    : _limit(static_cast<IndexType>(n)),
      _capacity(static_cast<IndexType>(n)),
      _index(home.alloc<IndexType>(n)),
      _bits(home.alloc<BitSetData>(n)),
      _pos(aligned ? home.alloc<IndexType>(n) : nullptr) {
    // Set all bits in all words (including the last)
    for (IndexType i=0; i<_limit; i++) {
      _bits[i].init(true);
      _index[i] = i;
      if (_pos != nullptr)
        _pos[i] = i+1;
    }
  }

  template<class IndexType>
  template<class OldIndexType>
  forceinline
  BitSet<IndexType>::BitSet(Space& home,
                            const BitSet<OldIndexType>& bs)
    : _limit(static_cast<IndexType>(bs._limit)),
      _capacity(static_cast<IndexType>(bs.width())),
      _index(home.alloc<IndexType>(_limit)),
      _bits(home.alloc<BitSetData>(_limit)),
      _pos((bs._pos != nullptr) ? home.alloc<IndexType>(_capacity) : nullptr) {
    assert(_limit > 0U);
    for (IndexType i=0; i<_limit; i++) {
      _bits[i] = bs._bits[i];
      _index[i] = static_cast<IndexType>(bs._index[i]);
    }
    if (_pos != nullptr) {
      for (IndexType i=0; i<_capacity; i++)
        _pos[i] = 0;
      for (IndexType i=0; i<_limit; i++)
        _pos[_index[i]] = i+1;
    }
  }

  template<class IndexType>
  forceinline void
  BitSet<IndexType>::flush(void) {
    _limit = 0U;
    assert(empty());
  }
    
  template<class IndexType>
  forceinline
  BitSet<IndexType>::BitSet(Space&, const TinyBitSet<1U>&)
    : _limit(0U), _capacity(0U), _index(nullptr), _bits(nullptr), _pos(nullptr) {
    GECODE_NEVER;
  }
  template<class IndexType>
  forceinline
  BitSet<IndexType>::BitSet(Space&, const TinyBitSet<2U>&)
    : _limit(0U), _capacity(0U), _index(nullptr), _bits(nullptr), _pos(nullptr) {
    GECODE_NEVER;
  }
  template<class IndexType>
  forceinline
  BitSet<IndexType>::BitSet(Space&, const TinyBitSet<3U>&)
    : _limit(0U), _capacity(0U), _index(nullptr), _bits(nullptr), _pos(nullptr) {
    GECODE_NEVER;
  }
  template<class IndexType>
  forceinline
  BitSet<IndexType>::BitSet(Space&, const TinyBitSet<4U>&)
    : _limit(0U), _capacity(0U), _index(nullptr), _bits(nullptr), _pos(nullptr) {
    GECODE_NEVER;
  }

  template<class IndexType>
  forceinline void
  BitSet<IndexType>::replace_and_decrease(IndexType i, BitSetData w) {
    assert(_limit > 0U);
    BitSetData w_i = _bits[i];
    if (w != w_i) {
      _bits[i] = w;
      if (w.none()) {
        assert(_bits[i].none());
        const IndexType removed_idx = _index[i];
        _limit--;
        const IndexType moved_idx = _index[_limit];
        _bits[i] = _bits[_limit];
        _index[i] = moved_idx;
        if (_pos != nullptr) {
          _pos[removed_idx] = 0;
          if (i < _limit)
            _pos[moved_idx] = i+1;
        }
      }
    }
  }

  template<class IndexType>
  forceinline void
  BitSet<IndexType>::clear_mask(BitSetData* mask) const {
    assert(_limit > 0U);
    for (IndexType i=0; i<_limit; i++) {
      mask[i].init(false);
      assert(mask[i].none());
    }
  }
  
  template<class IndexType>
  forceinline void
  BitSet<IndexType>::add_to_mask(const BitSetData* b, BitSetData* mask) const {
    assert(_limit > 0U);
    for (IndexType i=0; i<_limit; i++)
      mask[i] = BitSetData::o(mask[i],b[_index[i]]);
  }

  template<class IndexType>
  forceinline void
  BitSet<IndexType>::add_to_mask_aligned(const TupleSet::CSupportWord* b,
                                         const TupleSet::CSupportWord* e,
                                         BitSetData* mask) const {
    if ((_limit == 0U) || (b >= e))
      return;
    if (_pos == nullptr) {
      for (IndexType i=0; i<_limit; i++) {
        const BitSetData* s = find_aligned_word(b,e,_index[i]);
        if (s != nullptr)
          mask[i] = BitSetData::o(mask[i],*s);
      }
      return;
    }
    for (const TupleSet::CSupportWord* s=b; s<e; ++s) {
      if (s->widx >= static_cast<unsigned int>(_capacity))
        continue;
      const IndexType p = _pos[s->widx];
      if (p == 0U)
        continue;
      mask[p-1] = BitSetData::o(mask[p-1],s->bits);
    }
  }

  template<class IndexType>
  template<bool sparse>
  forceinline void
  BitSet<IndexType>::intersect_with_mask(const BitSetData* mask) {
    assert(_limit > 0U);
    if (sparse) {
      for (IndexType i = _limit; i--; ) {
        assert(!_bits[i].none());
        BitSetData w_i = _bits[i];
        BitSetData w_a = BitSetData::a(w_i, mask[_index[i]]);
        replace_and_decrease(i,w_a);
        assert(i == _limit || !_bits[i].none());
      }
    } else { // The same except different _indexing in mask
      for (IndexType i = _limit; i--; ) {
        assert(!_bits[i].none());
        BitSetData w_i = _bits[i];
        BitSetData w_a = BitSetData::a(w_i, mask[i]);
        replace_and_decrease(i,w_a);
        assert(i == _limit || !_bits[i].none());
      }
    }
  }

  template<class IndexType>
  forceinline void
  BitSet<IndexType>::intersect_with_mask_aligned
  (const TupleSet::CSupportWord* b, const TupleSet::CSupportWord* e) {
    if (_limit == 0U)
      return;
    for (IndexType i = _limit; i--; ) {
      assert(!_bits[i].none());
      const BitSetData* s = find_aligned_word(b,e,_index[i]);
      BitSetData w;
      if (s == nullptr) {
        w.init(false);
      } else {
        w = BitSetData::a(_bits[i],*s);
      }
      replace_and_decrease(i,w);
      assert(i == _limit || !_bits[i].none());
    }
  }
  
  template<class IndexType>
  forceinline void
  BitSet<IndexType>::intersect_with_masks(const BitSetData* a,
                                          const BitSetData* b) {
    assert(_limit > 0U);
    for (IndexType i = _limit; i--; ) {
      assert(!_bits[i].none());
      BitSetData w_i = _bits[i];
      IndexType offset = _index[i];
      BitSetData w_o = BitSetData::o(a[offset], b[offset]);
      BitSetData w_a = BitSetData::a(w_i,w_o);
      replace_and_decrease(i,w_a);
      assert(i == _limit || !_bits[i].none());
    }
  }

  template<class IndexType>
  forceinline void
  BitSet<IndexType>::intersect_with_masks_aligned
  (const TupleSet::CSupportWord* ab, const TupleSet::CSupportWord* ae,
   const TupleSet::CSupportWord* bb, const TupleSet::CSupportWord* be) {
    if (_limit == 0U)
      return;
    for (IndexType i = _limit; i--; ) {
      assert(!_bits[i].none());
      const BitSetData* sa = find_aligned_word(ab,ae,_index[i]);
      const BitSetData* sb = find_aligned_word(bb,be,_index[i]);
      BitSetData m;
      if (sa != nullptr) {
        m = *sa;
      } else {
        m.init(false);
      }
      if (sb != nullptr)
        m = BitSetData::o(m,*sb);
      BitSetData w = BitSetData::a(_bits[i],m);
      replace_and_decrease(i,w);
      assert(i == _limit || !_bits[i].none());
    }
  }
  
  template<class IndexType>
  forceinline void
  BitSet<IndexType>::nand_with_mask(const BitSetData* b) {
    assert(_limit > 0U);
    for (IndexType i = _limit; i--; ) {
      assert(!_bits[i].none());
      BitSetData w = BitSetData::a(_bits[i],~(b[_index[i]]));
      replace_and_decrease(i,w);
      assert(i == _limit || !_bits[i].none());
    }
  }

  template<class IndexType>
  forceinline void
  BitSet<IndexType>::nand_with_mask_aligned(const TupleSet::CSupportWord* b,
                                            const TupleSet::CSupportWord* e) {
    if ((_limit == 0U) || (b >= e))
      return;
    if (_pos == nullptr) {
      for (IndexType i = _limit; i--; ) {
        assert(!_bits[i].none());
        const BitSetData* s = find_aligned_word(b,e,_index[i]);
        if (s != nullptr) {
          BitSetData w = BitSetData::a(_bits[i],~(*s));
          replace_and_decrease(i,w);
          assert(i == _limit || !_bits[i].none());
        }
      }
      return;
    }
    for (const TupleSet::CSupportWord* s=b; s<e; ++s) {
      if (s->widx >= static_cast<unsigned int>(_capacity))
        continue;
      const IndexType p = _pos[s->widx];
      if (p == 0U)
        continue;
      const IndexType i = p-1;
      BitSetData w = BitSetData::a(_bits[i],~(s->bits));
      replace_and_decrease(i,w);
    }
  }

  template<class IndexType>
  forceinline bool
  BitSet<IndexType>::intersects(const BitSetData* b) const {
    for (IndexType i=0; i<_limit; i++)
      if (!BitSetData::a(_bits[i],b[_index[i]]).none())
        return true;
    return false;
  }

  template<class IndexType>
  forceinline bool
  BitSet<IndexType>::intersects_aligned(const TupleSet::CSupportWord* b,
                                        const TupleSet::CSupportWord* e) const {
    if ((_limit == 0U) || (b >= e))
      return false;
    if (_pos == nullptr) {
      for (IndexType i=0; i<_limit; i++) {
        const BitSetData* s = find_aligned_word(b,e,_index[i]);
        if ((s != nullptr) && !BitSetData::a(_bits[i],*s).none())
          return true;
      }
      return false;
    }
    for (const TupleSet::CSupportWord* s=b; s<e; ++s) {
      if (s->widx >= static_cast<unsigned int>(_capacity))
        continue;
      const IndexType p = _pos[s->widx];
      if ((p != 0U) && !BitSetData::a(_bits[p-1],s->bits).none())
        return true;
    }
    return false;
  }
    
  template<class IndexType>
  forceinline unsigned long long int
  BitSet<IndexType>::ones(const BitSetData* b) const {
    unsigned long long int o = 0U;
    for (IndexType i=0; i<_limit; i++)
      o += static_cast<unsigned long long int>
        (BitSetData::a(_bits[i],b[_index[i]]).ones());
    return o;
  }

  template<class IndexType>
  forceinline unsigned long long int
  BitSet<IndexType>::ones_aligned(const TupleSet::CSupportWord* b,
                                  const TupleSet::CSupportWord* e) const {
    unsigned long long int o = 0U;
    if ((_limit == 0U) || (b >= e))
      return 0U;
    if (_pos == nullptr) {
      for (IndexType i=0; i<_limit; i++) {
        const BitSetData* s = find_aligned_word(b,e,_index[i]);
        if (s != nullptr)
          o += static_cast<unsigned long long int>
            (BitSetData::a(_bits[i],*s).ones());
      }
      return o;
    }
    for (const TupleSet::CSupportWord* s=b; s<e; ++s) {
      if (s->widx >= static_cast<unsigned int>(_capacity))
        continue;
      const IndexType p = _pos[s->widx];
      if (p == 0U)
        continue;
      o += static_cast<unsigned long long int>
        (BitSetData::a(_bits[p-1],s->bits).ones());
    }
    return o;
  }
    
  template<class IndexType>
  forceinline unsigned long long int
  BitSet<IndexType>::ones(void) const {
    unsigned long long int o = 0U;
    for (IndexType i=0; i<_limit; i++)
      o += static_cast<unsigned long long int>(_bits[i].ones());
    return o;
  }
    
  template<class IndexType>
  forceinline unsigned long long int
  BitSet<IndexType>::bits(void) const {
    return (static_cast<unsigned long long int>(_limit) *
            static_cast<unsigned long long int>(BitSetData::bpb));
  }

}}}

// STATISTICS: int-prop
