/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Linnea Ingmar <linnea.ingmar@hotmail.com>
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Linnea Ingmar, 2017
 *     Mikael Lagerkvist, 2007
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

#include <gecode/int.hh>
#include <algorithm>
#include <limits>
#include <vector>

namespace Gecode { namespace Int { namespace Extensional {

  /// Import tuple type
  typedef ::Gecode::TupleSet::Tuple Tuple;

  /// Tuple comparison
  class TupleCompare {
  private:
    /// The arity of the tuples to compare
    int arity;
  public:
    /// Initialize with arity \a a
    TupleCompare(int a);
    /// Comparison of tuples \a a and \a b
    bool operator ()(const Tuple& a, const Tuple& b);
  };

  /// Tuple comparison by position
  class PosCompare {
  private:
    /// The position of the tuples to compare
    int p;
  public:
    /// Initialize with position \a p
    PosCompare(int p);
    /// Comparison of tuples \a a and \a b
    bool operator ()(const Tuple& a, const Tuple& b);
  };


  forceinline
  TupleCompare::TupleCompare(int a) : arity(a) {}

  forceinline bool
  TupleCompare::operator ()(const Tuple& a, const Tuple& b) {
    for (int i=0; i<arity; i++)
      if (a[i] < b[i])
        return true;
      else if (a[i] > b[i])
        return false;
    return false;
  }


  forceinline
  PosCompare::PosCompare(int p0) : p(p0) {}

  forceinline bool
  PosCompare::operator ()(const Tuple& a, const Tuple& b) {
    return a[p] < b[p];
  }


}}}

namespace Gecode {

  /*
   * Tuple set data
   *
   */
  void
  TupleSet::Data::finalize(ExtensionalPropKind epk) {
    using namespace Int::Extensional;
    assert(!finalized());
    // Mark as finalized
    n_free = -1;

    // Initialization
    if (n_tuples == 0) {
      heap.rfree(td);
      td=nullptr;
      return;
    }

    // Compact and copy data
    Region r;
    // Set up tuple pointers
    Tuple* tuple = r.alloc<Tuple>(n_tuples);
    {
      for (int t=0; t<n_tuples; t++)
        tuple[t] = td + t*arity;
      TupleCompare tc(arity);
      Support::quicksort(tuple, n_tuples, tc);
      // Remove duplicates
      int j=1;
      for (int t=1; t<n_tuples; t++) {
        for (int a=0; a<arity; a++)
          if (tuple[t-1][a] != tuple[t][a])
            goto notsame;
        goto same;
      notsame: ;
        tuple[j++] = tuple[t];
      same: ;
      }
      assert(j <= n_tuples);
      n_tuples=j;
      // Initialize hash key
      key = static_cast<std::size_t>(n_tuples);
      cmb_hash(key, arity);
      // Copy into now possibly smaller area
      const unsigned long n_tcells =
        static_cast<unsigned long>(n_tuples) *
        static_cast<unsigned long>(arity);
      int* new_td = heap.alloc<int>(n_tcells);
      for (int t=0; t<n_tuples; t++) {
        for (int a=0; a<arity; a++) {
          new_td[t*arity+a] = tuple[t][a];
          cmb_hash(key,tuple[t][a]);
        }
        tuple[t] = new_td + t*arity;
      }
      heap.rfree(td);
      td = new_td;
    }
    
    // Only now compute how many tuples are needed!
    n_words = BitSetData::data(static_cast<unsigned int>(n_tuples));
    sparse = false;

    // Compute range information
    {
      /*
       * Pass one: compute how many values and ranges are needed
       */
      // How many values
      unsigned int n_vals = 0U;
      // How many ranges
      unsigned int n_ranges = 0U;
      for (int a=0; a<arity; a++) {
        // Sort tuple according to position
        PosCompare pc(a);
        Support::quicksort(tuple, n_tuples, pc);
        // Scan values
        {
          int max=tuple[0][a];
          n_vals++; n_ranges++;
          for (int i=1; i<n_tuples; i++) {
            assert(tuple[i-1][a] <= tuple[i][a]);
            if (max+1 == tuple[i][a]) {
              n_vals++;
              max=tuple[i][a];
            } else if (max+1 < tuple[i][a]) {
              n_vals++; n_ranges++;
              max=tuple[i][a];
            } else {
              assert(max == tuple[i][a]);
            }
          }
        }
      }
      const unsigned long long n_support_entries64 =
        static_cast<unsigned long long>(n_words) *
        static_cast<unsigned long long>(n_vals);
      const unsigned long long n_support_bits =
        n_support_entries64 *
        static_cast<unsigned long long>(BitSetData::bpb);
      const unsigned long long n_support_ones =
        static_cast<unsigned long long>(arity) *
        static_cast<unsigned long long>(n_tuples);
      const bool support_overflow =
        (n_support_entries64 >
         static_cast<unsigned long long>(std::numeric_limits<unsigned int>::max()));
      // Prefer sparse representation for very large and very sparse tables.
      const unsigned long long sparse_bytes_threshold =
        256ULL * 1024ULL * 1024ULL;
      const bool support_large =
        (n_support_entries64 >
         sparse_bytes_threshold /
         static_cast<unsigned long long>(sizeof(BitSetData)));
      const bool support_very_sparse =
        (n_support_bits > 0ULL) && (n_support_ones * 100ULL < n_support_bits);
      const bool auto_sparse =
        support_overflow || (support_large && support_very_sparse);
      switch (epk) {
      case EPK_AUTO:
        sparse = auto_sparse;
        break;
      case EPK_DENSE:
        if (support_overflow)
          throw Int::OutOfLimits("TupleSet::finalize()");
        sparse = false;
        break;
      case EPK_SPARSE:
        sparse = true;
        break;
      default:
        GECODE_NEVER;
      }
      const bool build_sparse = sparse;
      const bool build_dense =
        !support_overflow &&
        ((epk == EPK_DENSE) || !sparse || !support_large);
      /*
       * Pass 2: allocate memory and fill data structures
       */
      // Allocate memory for ranges
      Range* cr = range = heap.alloc<Range>(n_ranges);
      // Allocate and initialize memory for supports unless sparse mode is active.
      support = nullptr;
      sparse_n_vals = 0U;
      sparse_offsets = nullptr;
      sparse_tuples = nullptr;
      sparse_tv = nullptr;
      BitSetData* cs = nullptr;
      unsigned int n_support_entries_u32 = 0U;
      if (build_dense) {
        assert((n_words == 0U) ||
               (n_vals <= std::numeric_limits<unsigned int>::max() / n_words));
        n_support_entries_u32 = n_words * n_vals;
        cs = support = heap.alloc<BitSetData>(n_support_entries_u32);
        for (unsigned int i=0; i<n_support_entries_u32; i++)
          cs[i].init();
      }
      for (int a=0; a<arity; a++) {
        // Set range pointer
        vd[a].r = cr;
        // Sort tuple according to position
        PosCompare pc(a);
        Support::quicksort(tuple, n_tuples, pc);
        // Update min and max
        min = std::min(min,tuple[0][a]);
        max = std::max(max,tuple[n_tuples-1][a]);
        // Compress into non-overlapping ranges
        {
          unsigned int j=0U;
          vd[a].r[0].max=vd[a].r[0].min=tuple[0][a];
          for (int i=1; i<n_tuples; i++) {
            assert(tuple[i-1][a] <= tuple[i][a]);
            if (vd[a].r[j].max+1 == tuple[i][a]) {
              vd[a].r[j].max=tuple[i][a];
            } else if (vd[a].r[j].max+1 < tuple[i][a]) {
              j++; vd[a].r[j].min=vd[a].r[j].max=tuple[i][a];
            } else {
              assert(vd[a].r[j].max == tuple[i][a]);
            }
          }
          vd[a].n = j+1U;
          cr += j+1U;
        }
        // Set support pointer and set bits
        for (unsigned int i=0U; i<vd[a].n; i++) {
          vd[a].r[i].s = cs;
          vd[a].r[i].sparse_base = 0U;
          if (build_dense)
            cs += n_words * vd[a].r[i].width();
        }
        if (build_dense) {
          int j=0;
          for (int i=0; i<n_tuples; i++) {
            while (tuple[i][a] > vd[a].r[j].max)
              j++;
            set(const_cast<BitSetData*>
                (vd[a].r[j].supports(n_words,tuple[i][a])),
                tuple2idx(tuple[i]));
          }
        }
      }
      if (build_sparse) {
        // Build sparse support lists indexed by (position,value).
        sparse_n_vals = n_vals;
        sparse_offsets = heap.alloc<unsigned int>(sparse_n_vals+1U);
        for (unsigned int i=0U; i<=sparse_n_vals; i++)
          sparse_offsets[i] = 0U;

        unsigned int gid_base = 0U;
        for (int a=0; a<arity; a++) {
          for (unsigned int i=0U; i<vd[a].n; i++) {
            vd[a].r[i].sparse_base = gid_base;
            gid_base += vd[a].r[i].width();
          }
        }
        assert(gid_base == sparse_n_vals);

        const unsigned long long n_tcells64 =
          static_cast<unsigned long long>(n_tuples) *
          static_cast<unsigned long long>(arity);
        if (n_tcells64 >
            static_cast<unsigned long long>(std::numeric_limits<unsigned int>::max()))
          throw Int::OutOfLimits("TupleSet::finalize()");
        const unsigned int n_tcells = static_cast<unsigned int>(n_tcells64);

        sparse_tv = heap.alloc<unsigned int>(n_tcells);
        for (int i=0; i<n_tuples; i++) {
          const unsigned int tid = tuple2idx(tuple[i]);
          for (int a=0; a<arity; a++) {
            const unsigned int r = vd[a].start(tuple[i][a]);
            const unsigned int gid =
              vd[a].r[r].sparse_base +
              static_cast<unsigned int>(tuple[i][a] - vd[a].r[r].min);
            sparse_tv[tid*arity+a] = gid;
            sparse_offsets[gid+1U]++;
          }
        }

        for (unsigned int i=1U; i<=sparse_n_vals; i++)
          sparse_offsets[i] += sparse_offsets[i-1U];

        sparse_tuples = heap.alloc<unsigned int>(n_tcells);
        unsigned int* next = r.alloc<unsigned int>(sparse_n_vals);
        for (unsigned int i=0U; i<sparse_n_vals; i++)
          next[i] = sparse_offsets[i];
        for (int i=0; i<n_tuples; i++) {
          const unsigned int tid = tuple2idx(tuple[i]);
          for (int a=0; a<arity; a++) {
            const unsigned int gid = sparse_tv[tid*arity+a];
            sparse_tuples[next[gid]++] = tid;
          }
        }
      }
      if (build_dense)
        assert(cs == support + n_support_entries_u32);
      assert(cr == range + n_ranges);
    }
    if ((min < Int::Limits::min) || (max > Int::Limits::max))
      throw Int::OutOfLimits("TupleSet::finalize()");
    assert(finalized());
  }

  void
  TupleSet::Data::resize(void) {
    assert(n_free == 0);
    int n = static_cast<int>(1+n_tuples*1.5);
    td = heap.realloc<int>(td, n_tuples * arity, n * arity);
    n_free = n - n_tuples;
  }

  TupleSet::Data::~Data(void) {
    heap.rfree(td);
    heap.rfree(vd);
    heap.rfree(range);
    heap.rfree(support);
    heap.rfree(sparse_offsets);
    heap.rfree(sparse_tuples);
    heap.rfree(sparse_tv);
  }


  /*
   * Tuple set
   *
   */
  TupleSet::TupleSet(int a)
    : SharedHandle(new Data(a)) {}
  void
  TupleSet::init(int a) {
    object(new Data(a));
  }
  TupleSet::TupleSet(const TupleSet& ts)
    : SharedHandle(ts) {}
  TupleSet&
  TupleSet::operator =(const TupleSet& ts) {
    (void) SharedHandle::operator =(ts);
    return *this;
  }

  TupleSet::TupleSet(int a, const Gecode::DFA& dfa) {
    /// Edges in layered graph
    struct Edge {
      int i_state; ///< Number of in-state
      int o_state; ///< Number of out-state
    };
    /// State in layered graph
    struct State {
      int i_deg; ///< In-degree (number of incoming arcs)
      int o_deg; ///< Out-degree (number of outgoing arcs)
      int n_tuples; ///< Number of tuples
      int* tuples; ///< The tuples
    };
    /// Support for a value
    struct Support {
      int val; ///< Supported value
      int n_edges; ///< Number of supporting edges
      Edge* edges; ///< Supporting edges
    };
    /// Layer in layered graph
    struct Layer {
      State* states; ///< States
      Support* supports; ///< Supported values
      int n_supports; ///< Number of supported values
    };
    // Initialize
    object(new Data(a));

    Region r;
    // Number of states
    int max_states = dfa.n_states();
    // Allocate memory for all layers and states
    Layer* layers = r.alloc<Layer>(a+1);
    State* states = r.alloc<State>(max_states*(a+1));

    for (int i=0; i<max_states*(a+1); i++) {
      states[i].i_deg = 0; states[i].o_deg = 0;
      states[i].n_tuples = 0;
      states[i].tuples = nullptr;
    }
    for (int i=0; i<a+1; i++) {
      layers[i].states = states + i*max_states;
      layers[i].n_supports = 0;
    }

    // Mark initial state as being reachable
    layers[0].states[0].i_deg = 1;
    layers[0].states[0].n_tuples = 1;
    layers[0].states[0].tuples = r.alloc<int>(1);
    assert(layers[0].states[0].tuples != nullptr);
  
    // Allocate temporary memory for edges and supports
    Edge* edges = r.alloc<Edge>(dfa.max_degree());
    Support* supports = r.alloc<Support>(dfa.n_symbols());
  
    // Forward pass: accumulate
    for (int i=0; i<a; i++) {
      int n_supports=0;
      for (DFA::Symbols s(dfa); s(); ++s) {
        int n_edges=0;
        for (DFA::Transitions t(dfa,s.val()); t(); ++t) {
          if (layers[i].states[t.i_state()].i_deg != 0) {
            // Create edge
            edges[n_edges].i_state = t.i_state();
            edges[n_edges].o_state = t.o_state();
            n_edges++;
            // Adjust degrees
            layers[i].states[t.i_state()].o_deg++;
            layers[i+1].states[t.o_state()].i_deg++;
            // Adjust number of tuples
            layers[i+1].states[t.o_state()].n_tuples
              += layers[i].states[t.i_state()].n_tuples;
          }
          assert(static_cast<unsigned int>(n_edges) <= dfa.max_degree());
        }
        // Found a support for the value
        if (n_edges > 0) {
          Support& support = supports[n_supports++];
          support.val = s.val();
          support.n_edges = n_edges;
          support.edges = Heap::copy(r.alloc<Edge>(n_edges),edges,n_edges);
        }
      }
      // Create supports
      if (n_supports > 0) {
        layers[i].supports =
          Heap::copy(r.alloc<Support>(n_supports),supports,n_supports);
        layers[i].n_supports = n_supports;
      } else {
        finalize();
        return;
      }
    }

    // Mark final states as being reachable
    for (int s=dfa.final_fst(); s<dfa.final_lst(); s++) {
      if (layers[a].states[s].i_deg != 0U)
        layers[a].states[s].o_deg = 1U;
    }

    // Backward pass: validate
    for (int i=a; i--; ) {
      for (int j = layers[i].n_supports; j--; ) {
        Support& s = layers[i].supports[j];
        for (int k = s.n_edges; k--; ) {
          int i_state = s.edges[k].i_state;
          int o_state = s.edges[k].o_state;
          // State is unreachable
          if (layers[i+1].states[o_state].o_deg == 0) {
            // Adjust degree
            --layers[i+1].states[o_state].i_deg;
            --layers[i].states[i_state].o_deg;
            // Remove edge
            assert(s.n_edges > 0);
            s.edges[k] = s.edges[--s.n_edges];
          }
        }
        // Lost support
        if (s.n_edges == 0)
          layers[i].supports[j] = layers[i].supports[--layers[i].n_supports];
      }
      if (layers[i].n_supports == 0U) {
        finalize();
        return;
      }
    }

    // Generate tuples
    for (int i=0; i<a; i++) {
      for (int j = layers[i].n_supports; j--; ) {
        Support& s = layers[i].supports[j];
        for (int k = s.n_edges; k--; ) {
          int i_state = s.edges[k].i_state;
          int o_state = s.edges[k].o_state;
          // Allocate memory for tuples if not done
          if (layers[i+1].states[o_state].tuples == nullptr) {
            int n_tuples = layers[i+1].states[o_state].n_tuples;
            layers[i+1].states[o_state].tuples = r.alloc<int>((i+1)*n_tuples);
            layers[i+1].states[o_state].n_tuples = 0;
          }
          int n = layers[i+1].states[o_state].n_tuples;
          // Write tuples
          for (int t=0; t < layers[i].states[i_state].n_tuples; t++) {
            // Copy the first i number of digits from the previous layer
            Heap::copy(&layers[i+1].states[o_state].tuples[n*(i+1)+t*(i+1)],
                       &layers[i].states[i_state].tuples[t*i], i);
            // Write the last digit
            layers[i+1].states[o_state].tuples[n*(i+1)+t*(i+1)+i] = s.val;
          }
          layers[i+1].states[o_state].n_tuples
            += layers[i].states[i_state].n_tuples;
        }
      }
    }

    // Add tuples to tuple set
    for (int s = dfa.final_fst(); s < dfa.final_lst(); s++) {
      for (int i=0; i<layers[a].states[s].n_tuples; i++) {
        int* tuple = &layers[a].states[s].tuples[i*a];
        add(IntArgs(a,tuple));
      }
    }
  
    finalize();
  } 

  DFA
  TupleSet::dfa(void) const {
    if (!*this)
      throw Int::UninitializedTupleSet("TupleSet::dfa()");
    if (!finalized())
      throw Int::NotYetFinalized("TupleSet::dfa()");

    const int a = arity();
    const int n = tuples();

    if (n == 0) {
      DFA::Transition t[1];
      t[0] = DFA::Transition(-1,0,0);
      int f[1] = {-1};
      return DFA(0,t,f,false);
    }

    const unsigned long long max_transitions =
      static_cast<unsigned long long>(n) *
      static_cast<unsigned long long>(a);
    if (max_transitions >
        static_cast<unsigned long long>(std::numeric_limits<int>::max()-1))
      throw Int::OutOfLimits("TupleSet::dfa()");

    std::vector<DFA::Transition> transitions;
    transitions.reserve(static_cast<std::size_t>(max_transitions + 1ULL));
    std::vector<int> finals;
    finals.reserve(2);

    if (a == 0) {
      finals.push_back(0);
    } else {
      const int final_state = 1;
      int next_state = 2;
      std::vector<int> state_at_depth(static_cast<std::size_t>(a),0);
      std::vector<int> prev_prefix(static_cast<std::size_t>(a-1),0);
      bool has_prev = false;

      for (int i=0; i<n; i++) {
        Tuple c = (*this)[i];

        int lcp = 0;
        if (has_prev) {
          while ((lcp < a-1) && (prev_prefix[lcp] == c[lcp]))
            lcp++;
        }

        for (int d=lcp; d<a-1; d++) {
          if (next_state == std::numeric_limits<int>::max())
            throw Int::OutOfLimits("TupleSet::dfa()");
          const int from = state_at_depth[d];
          const int to = next_state++;
          transitions.push_back(DFA::Transition(from,c[d],to));
          state_at_depth[d+1] = to;
        }

        transitions.push_back
          (DFA::Transition(state_at_depth[a-1],c[a-1],final_state));

        for (int d=0; d<a-1; d++)
          prev_prefix[d] = c[d];
        has_prev = true;
      }

      finals.push_back(final_state);
    }

    transitions.push_back(DFA::Transition(-1,0,0));
    finals.push_back(-1);
    return DFA(0,&transitions[0],&finals[0],false);
  }

  bool
  TupleSet::equal(const TupleSet& t) const {
    assert(tuples() == t.tuples());
    assert(arity() == t.arity());
    assert(min() == t.min());
    assert(max() == t.max());
    for (int i=0; i<tuples(); i++)
      for (int j=0; j<arity(); j++)
        if ((*this)[i][j] != t[i][j])
          return false;
    return true;
  }

  void
  TupleSet::_add(const IntArgs& t) {
    if (!*this)
      throw Int::UninitializedTupleSet("TupleSet::add()");
    if (raw().finalized())
      throw Int::AlreadyFinalized("TupleSet::add()");
    if (t.size() != raw().arity)
      throw Int::ArgumentSizeMismatch("TupleSet::add()");
    Tuple a = raw().add();
    for (int i=0; i<t.size(); i++)
      a[i]=t[i];
  }

}

// STATISTICS: int-prop
