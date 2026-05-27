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

#include <gecode/int/extensional.hh>

namespace Gecode { namespace Int { namespace Extensional {

  template<class View, bool pos>
  class SparseInc : public Propagator {
  protected:
    class SparseAdvisor : public ViewAdvisor<View> {
    public:
      using ViewAdvisor<View>::view;
    protected:
      int i;
    public:
      SparseAdvisor(Space& home, Propagator& p, Council<SparseAdvisor>& c,
                    View x0, int i0)
        : ViewAdvisor<View>(home,p,c,x0), i(i0) {}
      SparseAdvisor(Space& home, SparseAdvisor& a)
        : ViewAdvisor<View>(home,a), i(a.i) {}
      int index(void) const {
        return i;
      }
      void dispose(Space& home, Council<SparseAdvisor>& c) {
        (void) ViewAdvisor<View>::dispose(home,c);
      }
    };

    ViewArray<View> x;
    TupleSet ts;
    Council<SparseAdvisor> c;
    int arity;
    unsigned int n_tuples;
    unsigned int n_vals;
    unsigned int* active_ids;
    unsigned int* pos_in_active;
    unsigned int active_limit;
    unsigned int* support_count;
    int* gid_var;
    int* gid_val;
    unsigned int* zero_queue;
    unsigned int zero_queue_size;
    unsigned char* queued;
    const unsigned int* tv;
    bool in_propagate;

    forceinline const unsigned int*
    tuple_gids(unsigned int tid) const {
      const unsigned long long idx =
        static_cast<unsigned long long>(tid) *
        static_cast<unsigned long long>(arity);
      return tv + idx;
    }

    void
    enqueue_zero(unsigned int gid) {
      if ((gid < n_vals) && (queued[gid] == 0U)) {
        queued[gid] = 1U;
        zero_queue[zero_queue_size++] = gid;
      }
    }

    void
    init_gid_maps(void) {
      for (unsigned int i=0U; i<n_vals; i++) {
        gid_var[i] = -1;
        gid_val[i] = 0;
      }
      for (int a=0; a<x.size(); a++) {
        for (const TupleSet::Range* r=ts.fst(a); r<=ts.lst(a); r++) {
          int n = r->min;
          while (true) {
            const unsigned int gid =
              r->sparse_base + static_cast<unsigned int>(n-r->min);
            assert(gid < n_vals);
            gid_var[gid] = a;
            gid_val[gid] = n;
            if (n == r->max)
              break;
            n++;
          }
        }
      }
    }

    void
    init_support_counts(void) {
      assert(tv != nullptr);
      const unsigned int* offsets = TupleSetAccess::sparse_support_offsets(ts);
      if (offsets != nullptr) {
        for (unsigned int i=0U; i<n_vals; i++)
          support_count[i] = offsets[i+1U] - offsets[i];
      } else {
        for (unsigned int i=0U; i<n_vals; i++)
          support_count[i] = 0U;
        const unsigned long long n_tv =
          static_cast<unsigned long long>(n_tuples) *
          static_cast<unsigned long long>(arity);
        for (unsigned long long i=0ULL; i<n_tv; i++) {
          const unsigned int gid = tv[i];
          assert(gid < n_vals);
          support_count[gid]++;
        }
      }
    }

    void
    deactivate_tuple(unsigned int tid) {
      if (tid >= n_tuples)
        return;
      const unsigned int p = pos_in_active[tid];
      if ((p >= active_limit) || (active_ids[p] != tid))
        return;

      const unsigned int last = active_limit - 1U;
      const unsigned int last_tid = active_ids[last];
      active_ids[p] = last_tid;
      pos_in_active[last_tid] = p;
      active_limit = last;

      const unsigned int* row = tuple_gids(tid);
      for (int a=0; a<arity; a++) {
        const unsigned int gid = row[a];
        assert(gid < n_vals);
        assert(support_count[gid] > 0U);
        support_count[gid]--;
        if (pos && (support_count[gid] == 0U))
          enqueue_zero(gid);
      }
    }

    unsigned int
    tuple_gid(unsigned int tid, int a) const {
      const unsigned int gid = tuple_gids(tid)[a];
      assert(gid < n_vals);
      return gid;
    }

    void
    deactivate_value_support(int i, int n) {
      const unsigned int* b = nullptr;
      const unsigned int* e = nullptr;
      unsigned int gid = 0U;
      if (TupleSetAccess::sparse_support(ts,i,n,b,e,gid)) {
        if (support_count[gid] == 0U)
          return;
        for (const unsigned int* t=b; t<e; t++)
          deactivate_tuple(*t);
      }
    }

    void
    deactivate_for_all_domains(void) {
      unsigned int p = 0U;
      while (p < active_limit) {
        const unsigned int tid = active_ids[p];
        const unsigned int* row = tuple_gids(tid);
        bool keep = true;
        for (int i=0; i<arity; i++) {
          if (!x[i].in(gid_val[row[i]])) {
            keep = false;
            break;
          }
        }
        if (keep) {
          p++;
        } else {
          deactivate_tuple(tid);
        }
      }
    }

    void
    deactivate_for_domain(int i, const View& xv) {
      unsigned int p = 0U;
      while (p < active_limit) {
        const unsigned int tid = active_ids[p];
        const unsigned int gid = tuple_gid(tid,i);
        if (!xv.in(gid_val[gid])) {
          deactivate_tuple(tid);
        } else {
          p++;
        }
      }
    }

    void
    deactivate_removed_values(int i, const View& xv, const Delta& d) {
      if (xv.assigned()) {
        deactivate_for_domain(i,xv);
        return;
      }
      if (xv.any(d) || (xv.width(d) > xv.size())) {
        deactivate_for_domain(i,xv);
        return;
      }
      if (xv.min(d) == xv.max(d)) {
        const int n = xv.min(d);
        if (!xv.in(n))
          deactivate_value_support(i,n);
        return;
      }
      int n = xv.min(d);
      while (true) {
        if (!xv.in(n))
          deactivate_value_support(i,n);
        if (n == xv.max(d))
          break;
        n++;
      }
    }

    ExecStatus
    process_zero_queue(Space& home) {
      if (zero_queue_size == 0U)
        return ES_OK;

      Region r;
      const int arity = x.size();
      unsigned int* n_rm = r.alloc<unsigned int>(arity);
      unsigned int* p_rm = r.alloc<unsigned int>(arity);
      int** rm = r.alloc<int*>(arity);
      for (int i=0; i<arity; i++) {
        n_rm[i] = 0U;
        p_rm[i] = 0U;
        rm[i] = nullptr;
      }

      for (unsigned int i=0U; i<zero_queue_size; i++) {
        const unsigned int gid = zero_queue[i];
        assert(gid < n_vals);
        queued[gid] = 0U;
        const int a = gid_var[gid];
        if ((a < 0) || (a >= arity))
          continue;
        if (x[a].in(gid_val[gid]))
          n_rm[a]++;
      }

      for (int i=0; i<arity; i++)
        if (n_rm[i] > 0U)
          rm[i] = r.alloc<int>(n_rm[i]);

      for (unsigned int i=0U; i<zero_queue_size; i++) {
        const unsigned int gid = zero_queue[i];
        assert(gid < n_vals);
        const int a = gid_var[gid];
        if ((a < 0) || (a >= arity))
          continue;
        if (x[a].in(gid_val[gid]))
          rm[a][p_rm[a]++] = gid_val[gid];
      }

      zero_queue_size = 0U;

      for (int i=0; i<arity; i++) {
        if (p_rm[i] == 0U)
          continue;
        if (x[i].assigned())
          continue;
        if (p_rm[i] == 1U) {
          GECODE_ME_CHECK(x[i].nq(home,rm[i][0]));
          continue;
        }
        Support::quicksort(rm[i], static_cast<int>(p_rm[i]));
        unsigned int j = 1U;
        for (unsigned int k=1U; k<p_rm[i]; k++)
          if (rm[i][k] != rm[i][j-1U])
            rm[i][j++] = rm[i][k];
        if (j == 1U) {
          GECODE_ME_CHECK(x[i].nq(home,rm[i][0]));
        } else {
          Iter::Values::Array iv(rm[i],j);
          GECODE_ASSUME(j >= 2U);
          GECODE_ME_CHECK(x[i].minus_v(home,iv,false));
        }
      }
      return ES_OK;
    }

    bool
    all_assigned(void) const {
      for (int i=0; i<x.size(); i++)
        if (!x[i].assigned())
          return false;
      return true;
    }

  public:
    SparseInc(Home home, ViewArray<View>& x0, const TupleSet& ts0)
      : Propagator(home), x(home,x0), ts(ts0), c(home), arity(x0.size()),
        n_tuples(static_cast<unsigned int>(ts0.tuples())),
        n_vals(TupleSetAccess::sparse_values(ts0)),
        active_ids(static_cast<Space&>(home).alloc<unsigned int>(n_tuples)),
        pos_in_active(static_cast<Space&>(home).alloc<unsigned int>(n_tuples)),
        active_limit(n_tuples),
        support_count(static_cast<Space&>(home).alloc<unsigned int>(n_vals)),
        gid_var(static_cast<Space&>(home).alloc<int>(n_vals)),
        gid_val(static_cast<Space&>(home).alloc<int>(n_vals)),
        zero_queue(static_cast<Space&>(home).alloc<unsigned int>(n_vals)),
        zero_queue_size(0U),
        queued(static_cast<Space&>(home).alloc<unsigned char>(n_vals)),
        tv(TupleSetAccess::sparse_tuple_value_ids(ts0)),
        in_propagate(false) {
      home.notice(*this, AP_DISPOSE);
      assert(tv != nullptr);
      for (unsigned int i=0U; i<n_tuples; i++) {
        active_ids[i] = i;
        pos_in_active[i] = i;
      }
      for (unsigned int i=0U; i<n_vals; i++)
        queued[i] = 0U;

      init_gid_maps();
      init_support_counts();

      for (int i=0; i<arity; i++)
        if (!x[i].assigned())
          (void) new (home) SparseAdvisor(home,*this,c,x[i],i);

      deactivate_for_all_domains();
    }

    SparseInc(Space& home, SparseInc<View,pos>& p)
      : Propagator(home,p), x(), ts(p.ts), c(home), arity(p.arity),
        n_tuples(p.n_tuples), n_vals(p.n_vals),
        active_ids(home.alloc<unsigned int>(p.n_tuples)),
        pos_in_active(home.alloc<unsigned int>(p.n_tuples)),
        active_limit(p.active_limit),
        support_count(home.alloc<unsigned int>(p.n_vals)),
        gid_var(home.alloc<int>(p.n_vals)),
        gid_val(home.alloc<int>(p.n_vals)),
        zero_queue(home.alloc<unsigned int>(p.n_vals)),
        zero_queue_size(p.zero_queue_size),
        queued(home.alloc<unsigned char>(p.n_vals)),
        tv(TupleSetAccess::sparse_tuple_value_ids(ts)),
        in_propagate(false) {
      x.update(home,p.x);
      c.update(home,p.c);
      for (unsigned int i=0U; i<n_tuples; i++) {
        active_ids[i] = p.active_ids[i];
        pos_in_active[i] = p.pos_in_active[i];
      }
      for (unsigned int i=0U; i<n_vals; i++) {
        support_count[i] = p.support_count[i];
        gid_var[i] = p.gid_var[i];
        gid_val[i] = p.gid_val[i];
        queued[i] = p.queued[i];
        zero_queue[i] = p.zero_queue[i];
      }
    }

    static ExecStatus
    post(Home home, ViewArray<View>& x, const TupleSet& ts) {
      bool assigned = true;
      for (int i=0; i<x.size(); i++)
        if (!x[i].assigned()) {
          assigned = false;
          break;
        }
      if (assigned) {
        bool in_table = false;
        for (int t=0; t<ts.tuples() && !in_table; t++) {
          TupleSet::Tuple tuple = ts[t];
          bool same = true;
          for (int i=0; i<x.size(); i++)
            if (tuple[i] != x[i].val()) {
              same = false;
              break;
            }
          in_table = same;
        }
        if (pos)
          return in_table ? ES_OK : ES_FAILED;
        return in_table ? ES_FAILED : ES_OK;
      }

      if (pos) {
        if (x.size() == 0)
          return (ts.tuples() == 0) ? ES_FAILED : ES_OK;
        if (ts.tuples() == 0)
          return ES_FAILED;

        for (int i=0; i<x.size(); i++) {
          TupleSet::Ranges r(ts,i);
          GECODE_ME_CHECK(x[i].inter_r(home, r, false));
        }

        if ((x.size() <= 1) || (ts.tuples() <= 1))
          return ES_OK;

        SparseInc<View,pos>* p = new (home) SparseInc<View,pos>(home,x,ts);
        if (p->active_limit == 0U)
          return ES_FAILED;
        View::schedule(home,*p,ME_INT_DOM);
        return ES_OK;
      }

      if (x.size() == 0)
        return (ts.tuples() == 0) ? ES_OK : ES_FAILED;
      if (ts.tuples() == 0)
        return ES_OK;

      SparseInc<View,pos>* p = new (home) SparseInc<View,pos>(home,x,ts);
      View::schedule(home,*p,ME_INT_DOM);
      return ES_OK;
    }

    virtual Actor*
    copy(Space& home) {
      return new (home) SparseInc<View,pos>(home,*this);
    }

    virtual PropCost
    cost(const Space&, const ModEventDelta&) const {
      return PropCost::quadratic(PropCost::HI,x.size());
    }

    virtual void
    reschedule(Space& home) {
      View::schedule(home,*this,ME_INT_DOM);
    }

    virtual size_t
    dispose(Space& home) {
      home.ignore(*this, AP_DISPOSE);
      c.dispose(home);
      ts.~TupleSet();
      (void) Propagator::dispose(home);
      return sizeof(*this);
    }

    virtual ExecStatus
    propagate(Space& home, const ModEventDelta&) {
      if (pos) {
        if (active_limit == 0U)
          return ES_FAILED;

        in_propagate = true;
        ExecStatus es = process_zero_queue(home);
        in_propagate = false;
        if (es != ES_OK)
          return es;

        if (active_limit == 0U)
          return ES_FAILED;
        return all_assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
      }

      if (active_limit == 0U)
        return home.ES_SUBSUMED(*this);

      const unsigned long long cap_all =
        static_cast<unsigned long long>(active_limit);
      unsigned long long all = 1ULL;
      for (int i=0; i<x.size(); i++) {
        const unsigned long long sz = static_cast<unsigned long long>(x[i].size());
        if ((all > cap_all) || ((sz > 0ULL) && (all > cap_all / sz))) {
          all = cap_all + 1ULL;
          break;
        }
        all *= sz;
      }
      if (all == cap_all)
        return ES_FAILED;

      Region r;
      in_propagate = true;
      for (int i=0; i<x.size(); i++) {
        if (x[i].assigned())
          continue;
        const unsigned long long cap =
          static_cast<unsigned long long>(active_limit);
        unsigned long long other = 1ULL;
        for (int j=0; j<x.size(); j++) {
          if (j == i)
            continue;
          const unsigned long long sz =
            static_cast<unsigned long long>(x[j].size());
          if ((other > cap) || ((sz > 0ULL) && (other > cap / sz))) {
            other = cap + 1ULL;
            break;
          }
          other *= sz;
        }
        if (other > cap)
          continue;

        int* rm = r.alloc<int>(x[i].size());
        unsigned int n_rm = 0U;
        for (const TupleSet::Range* rg=ts.fst(i); rg<=ts.lst(i); rg++) {
          int v = rg->min;
          while (true) {
            const unsigned int gid =
              rg->sparse_base + static_cast<unsigned int>(v-rg->min);
            assert(gid < n_vals);
            if (x[i].in(v) &&
                (support_count[gid] ==
                 static_cast<unsigned int>(other)))
              rm[n_rm++] = v;
            if (v == rg->max)
              break;
            v++;
          }
        }
        if (n_rm == 0U)
          continue;
        if (n_rm == 1U) {
          GECODE_ME_CHECK(x[i].nq(home,rm[0]));
        } else {
          Iter::Values::Array iv(rm,n_rm);
          GECODE_ASSUME(n_rm >= 2U);
          GECODE_ME_CHECK(x[i].minus_v(home,iv,false));
        }
        deactivate_for_domain(i,x[i]);
        if (active_limit == 0U) {
          in_propagate = false;
          return home.ES_SUBSUMED(*this);
        }
        r.free();
      }
      in_propagate = false;
      return ES_FIX;
    }

    virtual ExecStatus
    advise(Space& home, Advisor& a0, const Delta& d) {
      if (active_limit == 0U)
        return pos ? ES_FAILED : ES_NOFIX;

      SparseAdvisor& sa = static_cast<SparseAdvisor&>(a0);
      View xv = sa.view();
      if (in_propagate)
        return xv.assigned() ? home.ES_FIX_DISPOSE(c,sa) : ES_FIX;

      const int i = sa.index();

      if (xv.assigned()) {
        deactivate_for_domain(i,xv);
        if (active_limit == 0U)
          return pos ? ES_FAILED : home.ES_NOFIX_DISPOSE(c,sa);
        return home.ES_NOFIX_DISPOSE(c,sa);
      }
      deactivate_removed_values(i,xv,d);

      if (active_limit == 0U)
        return pos ? ES_FAILED : ES_NOFIX;
      return ES_NOFIX;
    }
  };

  template<class View, class CtrlView, ReifyMode rm>
  class SparseReifInc : public Propagator {
  protected:
    class SparseAdvisor : public ViewAdvisor<View> {
    public:
      using ViewAdvisor<View>::view;
    protected:
      int i;
    public:
      SparseAdvisor(Space& home, Propagator& p, Council<SparseAdvisor>& c,
                    View x0, int i0)
        : ViewAdvisor<View>(home,p,c,x0), i(i0) {}
      SparseAdvisor(Space& home, SparseAdvisor& a)
        : ViewAdvisor<View>(home,a), i(a.i) {}
      int index(void) const {
        return i;
      }
      void dispose(Space& home, Council<SparseAdvisor>& c) {
        (void) ViewAdvisor<View>::dispose(home,c);
      }
    };

    ViewArray<View> x;
    CtrlView b;
    TupleSet ts;
    Council<SparseAdvisor> c;
    int arity;
    unsigned int n_tuples;
    unsigned int n_vals;
    unsigned int* active_ids;
    unsigned int* pos_in_active;
    unsigned int active_limit;
    int* gid_val;
    const unsigned int* tv;
    bool in_propagate;

    forceinline const unsigned int*
    tuple_gids(unsigned int tid) const {
      const unsigned long long idx =
        static_cast<unsigned long long>(tid) *
        static_cast<unsigned long long>(arity);
      return tv + idx;
    }

    unsigned int
    tuple_gid(unsigned int tid, int a) const {
      const unsigned int gid = tuple_gids(tid)[a];
      assert(gid < n_vals);
      return gid;
    }

    void
    init_gid_values(void) {
      for (unsigned int i=0U; i<n_vals; i++)
        gid_val[i] = 0;
      for (int a=0; a<x.size(); a++) {
        for (const TupleSet::Range* r=ts.fst(a); r<=ts.lst(a); r++) {
          int n = r->min;
          while (true) {
            const unsigned int gid =
              r->sparse_base + static_cast<unsigned int>(n-r->min);
            assert(gid < n_vals);
            gid_val[gid] = n;
            if (n == r->max)
              break;
            n++;
          }
        }
      }
    }

    void
    deactivate_tuple(unsigned int tid) {
      if (tid >= n_tuples)
        return;
      const unsigned int p = pos_in_active[tid];
      if ((p >= active_limit) || (active_ids[p] != tid))
        return;

      const unsigned int last = active_limit - 1U;
      const unsigned int last_tid = active_ids[last];
      active_ids[p] = last_tid;
      pos_in_active[last_tid] = p;
      active_limit = last;
    }

    void
    deactivate_value_support(int i, int n) {
      const unsigned int* begin = nullptr;
      const unsigned int* end = nullptr;
      unsigned int gid = 0U;
      if (TupleSetAccess::sparse_support(ts,i,n,begin,end,gid))
        for (const unsigned int* t=begin; t<end; t++)
          deactivate_tuple(*t);
    }

    void
    deactivate_for_domain(int i, const View& xv) {
      unsigned int p = 0U;
      while (p < active_limit) {
        const unsigned int tid = active_ids[p];
        const unsigned int gid = tuple_gid(tid,i);
        if (!xv.in(gid_val[gid]))
          deactivate_tuple(tid);
        else
          p++;
      }
    }

    void
    deactivate_removed_values(int i, const View& xv, const Delta& d) {
      if (xv.assigned()) {
        deactivate_for_domain(i,xv);
        return;
      }
      if (xv.any(d) || (xv.width(d) > xv.size())) {
        deactivate_for_domain(i,xv);
        return;
      }
      if (xv.min(d) == xv.max(d)) {
        const int n = xv.min(d);
        if (!xv.in(n))
          deactivate_value_support(i,n);
        return;
      }
      int n = xv.min(d);
      while (true) {
        if (!xv.in(n))
          deactivate_value_support(i,n);
        if (n == xv.max(d))
          break;
        n++;
      }
    }

    void
    deactivate_for_all_domains(void) {
      unsigned int p = 0U;
      while (p < active_limit) {
        const unsigned int tid = active_ids[p];
        const unsigned int* row = tuple_gids(tid);
        bool keep = true;
        for (int i=0; i<arity; i++) {
          if (!x[i].in(gid_val[row[i]])) {
            keep = false;
            break;
          }
        }
        if (keep) {
          p++;
        } else {
          deactivate_tuple(tid);
        }
      }
    }

  public:
    static ExecStatus
    post_pos(Home home, ViewArray<View>& x, const TupleSet& ts) {
      return SparseInc<View,true>::post(home,x,ts);
    }

    static ExecStatus
    post_neg(Home home, ViewArray<View>& x, const TupleSet& ts) {
      return SparseInc<View,false>::post(home,x,ts);
    }

    SparseReifInc(Home home, ViewArray<View>& x0, const TupleSet& ts0, CtrlView b0)
      : Propagator(home), x(home,x0), b(b0), ts(ts0), c(home),
        arity(x0.size()),
        n_tuples(static_cast<unsigned int>(ts0.tuples())),
        n_vals(TupleSetAccess::sparse_values(ts0)),
        active_ids(static_cast<Space&>(home).alloc<unsigned int>(n_tuples)),
        pos_in_active(static_cast<Space&>(home).alloc<unsigned int>(n_tuples)),
        active_limit(n_tuples),
        gid_val(static_cast<Space&>(home).alloc<int>(n_vals)),
        tv(TupleSetAccess::sparse_tuple_value_ids(ts0)),
        in_propagate(false) {
      home.notice(*this, AP_DISPOSE);
      assert(tv != nullptr);
      for (unsigned int i=0U; i<n_tuples; i++) {
        active_ids[i] = i;
        pos_in_active[i] = i;
      }
      init_gid_values();

      b.subscribe(home,*this,PC_BOOL_VAL);
      for (int i=0; i<arity; i++)
        if (!x[i].assigned())
          (void) new (home) SparseAdvisor(home,*this,c,x[i],i);

      deactivate_for_all_domains();
    }

    SparseReifInc(Space& home, SparseReifInc<View,CtrlView,rm>& p)
      : Propagator(home,p), x(), b(), ts(p.ts), c(home),
        arity(p.arity),
        n_tuples(p.n_tuples), n_vals(p.n_vals),
        active_ids(home.alloc<unsigned int>(p.n_tuples)),
        pos_in_active(home.alloc<unsigned int>(p.n_tuples)),
        active_limit(p.active_limit),
        gid_val(home.alloc<int>(p.n_vals)),
        tv(TupleSetAccess::sparse_tuple_value_ids(ts)),
        in_propagate(false) {
      x.update(home,p.x);
      b.update(home,p.b);
      c.update(home,p.c);
      for (unsigned int i=0U; i<n_tuples; i++) {
        active_ids[i] = p.active_ids[i];
        pos_in_active[i] = p.pos_in_active[i];
      }
      for (unsigned int i=0U; i<n_vals; i++)
        gid_val[i] = p.gid_val[i];
    }

    static ExecStatus
    post(Home home, ViewArray<View>& x, const TupleSet& ts, CtrlView b) {
      if (b.one()) {
        if (rm == RM_PMI)
          return ES_OK;
        return SparseInc<View,true>::post(home,x,ts);
      }
      if (b.zero()) {
        if (rm == RM_IMP)
          return ES_OK;
        return SparseInc<View,false>::post(home,x,ts);
      }
      SparseReifInc<View,CtrlView,rm>* p =
        new (home) SparseReifInc<View,CtrlView,rm>(home,x,ts,b);
      View::schedule(home,*p,ME_INT_DOM);
      return ES_OK;
    }

    virtual Actor*
    copy(Space& home) {
      return new (home) SparseReifInc<View,CtrlView,rm>(home,*this);
    }

    virtual PropCost
    cost(const Space&, const ModEventDelta&) const {
      return PropCost::quadratic(PropCost::HI,x.size());
    }

    virtual void
    reschedule(Space& home) {
      View::schedule(home,*this,ME_INT_DOM);
    }

    virtual size_t
    dispose(Space& home) {
      home.ignore(*this, AP_DISPOSE);
      c.dispose(home);
      b.cancel(home,*this,PC_BOOL_VAL);
      ts.~TupleSet();
      (void) Propagator::dispose(home);
      return sizeof(*this);
    }

    virtual ExecStatus
    propagate(Space& home, const ModEventDelta&) {
      if (b.one()) {
        if (rm == RM_PMI)
          return home.ES_SUBSUMED(*this);
        TupleSet keep(ts);
        GECODE_REWRITE(*this,post_pos(home(*this),x,keep));
      }
      if (b.zero()) {
        if (rm == RM_IMP)
          return home.ES_SUBSUMED(*this);
        TupleSet keep(ts);
        GECODE_REWRITE(*this,post_neg(home(*this),x,keep));
      }

      if (active_limit == 0U) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
        return home.ES_SUBSUMED(*this);
      }

      const unsigned long long cap_all =
        static_cast<unsigned long long>(active_limit);
      unsigned long long all = 1ULL;
      for (int i=0; i<x.size(); i++) {
        const unsigned long long sz = static_cast<unsigned long long>(x[i].size());
        if ((all > cap_all) || ((sz > 0ULL) && (all > cap_all / sz))) {
          all = cap_all + 1ULL;
          break;
        }
        all *= sz;
      }
      if (all == cap_all) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
        return home.ES_SUBSUMED(*this);
      }
      return ES_FIX;
    }

    virtual ExecStatus
    advise(Space& home, Advisor& a0, const Delta& d) {
      SparseAdvisor& sa = static_cast<SparseAdvisor&>(a0);
      if (b.assigned())
        return home.ES_NOFIX_DISPOSE(c,sa);

      if (active_limit == 0U)
        return ES_NOFIX;

      View xv = sa.view();
      if (in_propagate)
        return xv.assigned() ? home.ES_FIX_DISPOSE(c,sa) : ES_FIX;

      const int i = sa.index();
      if (xv.assigned()) {
        deactivate_for_domain(i,xv);
        return home.ES_NOFIX_DISPOSE(c,sa);
      }
      deactivate_removed_values(i,xv,d);
      return ES_NOFIX;
    }
  };

  enum DispatchKind {
    DD_DENSE,
    DD_SPARSE,
    DD_DENSE_COMPRESSED
  };

  forceinline DispatchKind
  dispatch_kind(const TupleSet& t, ExtensionalPropKind epk) {
    switch (epk) {
    case EPK_AUTO:
      if (TupleSetAccess::dense_compressed_support(t))
        return DD_DENSE_COMPRESSED;
      if (TupleSetAccess::sparse_support(t))
        return DD_SPARSE;
      if (TupleSetAccess::dense_support(t))
        return DD_DENSE;
      throw OutOfLimits("Int::extensional");
    case EPK_DENSE:
      if (TupleSetAccess::dense_support(t))
        return DD_DENSE;
      throw OutOfLimits("Int::extensional");
    case EPK_SPARSE:
      if (TupleSetAccess::sparse_support(t))
        return DD_SPARSE;
      if (TupleSetAccess::dense_support(t))
        return DD_DENSE;
      throw OutOfLimits("Int::extensional");
    case EPK_DENSE_COMPRESSED:
      if (TupleSetAccess::dense_compressed_support(t))
        return DD_DENSE_COMPRESSED;
      throw OutOfLimits("Int::extensional");
    default:
      GECODE_NEVER;
      return DD_DENSE;
    }
  }

  template<class View>
  ExecStatus
  postposcompact_levels(Home home, ViewArray<View>& x, const TupleSet& t,
                        const IntPropLevelArgs& ipl,
                        ExtensionalPropKind epk) {
    const DispatchKind dk = dispatch_kind(t,epk);
    switch (dk) {
    case DD_DENSE:
      return postposcompact(home,x,t,ipl);
    case DD_DENSE_COMPRESSED:
      return postposcompact_compressed(home,x,t,ipl);
    case DD_SPARSE:
      throw OutOfLimits("Int::extensional");
    default:
      GECODE_NEVER;
      return ES_FAILED;
    }
  }

}}}

namespace Gecode {

  void
  extensional(Home home, const IntVarArgs& x, const TupleSet& t, bool pos,
              IntPropLevel ipl) {
    extensional(home,x,t,pos,ipl,EPK_AUTO);
  }

  void
  extensional(Home home, const IntVarArgs& x, const TupleSet& t, bool pos,
              IntPropLevel ipl,
              ExtensionalPropKind epk) {
    using namespace Int;
    if (!t.finalized())
      throw NotYetFinalized("Int::extensional");
    if (t.arity() != x.size())
      throw ArgumentSizeMismatch("Int::extensional");
    if (same(x))
      throw ArgumentSame("Int::extensional");
    GECODE_POST;

    if (x.size() == 0) {
      if (pos ? (t.tuples() > 0) : (t.tuples() == 0))
        return;
      GECODE_ES_FAIL(ES_FAILED);
      return;
    }
    if (t.tuples() == 0) {
      if (!pos)
        return;
      GECODE_ES_FAIL(ES_FAILED);
      return;
    }

    if (pos && (vbd(ipl) == IPL_BND)) {
      ViewArray<IntView> xv(home,x);
      IntPropLevelArgs ipls(x.size());
      for (int i=0; i<x.size(); i++)
        ipls[i] = IPL_BND;
      GECODE_ES_FAIL((Extensional::postposcompact_levels(home,xv,t,ipls,epk)));
      return;
    }

    const Int::Extensional::DispatchKind dk =
      Int::Extensional::dispatch_kind(t,epk);
    if (dk == Int::Extensional::DD_SPARSE) {
      ViewArray<IntView> xv(home,x);
      if (pos)
        GECODE_ES_FAIL((Extensional::SparseInc<IntView,true>::post(home,xv,t)));
      else
        GECODE_ES_FAIL((Extensional::SparseInc<IntView,false>::post(home,xv,t)));
      return;
    }
    ViewArray<IntView> xv(home,x);
    if (dk == Int::Extensional::DD_DENSE_COMPRESSED) {
      if (pos)
        GECODE_ES_FAIL((Extensional::postposcompact_compressed<IntView>(home,xv,t)));
      else
        GECODE_ES_FAIL((Extensional::postnegcompact_compressed<IntView>(home,xv,t)));
    } else if (pos) {
      GECODE_ES_FAIL((Extensional::postposcompact<IntView>(home,xv,t)));
    } else {
      GECODE_ES_FAIL((Extensional::postnegcompact<IntView>(home,xv,t)));
    }
  }

  void
  extensional(Home home, const IntVarArgs& x, const TupleSet& t, bool pos,
              const IntPropLevelArgs& ipl) {
    extensional(home,x,t,pos,ipl,EPK_AUTO);
  }

  void
  extensional(Home home, const IntVarArgs& x, const TupleSet& t, bool pos,
              const IntPropLevelArgs& ipl, ExtensionalPropKind epk) {
    using namespace Int;
    if (!t.finalized())
      throw NotYetFinalized("Int::extensional");
    if ((t.arity() != x.size()) || (ipl.size() != x.size()))
      throw ArgumentSizeMismatch("Int::extensional");
    if (same(x))
      throw ArgumentSame("Int::extensional");
    GECODE_POST;

    if (!pos || Int::Extensional::tuple_set_all_domain(ipl)) {
      extensional(home,x,t,pos,IPL_DOM,epk);
      return;
    }

    if (x.size() == 0) {
      if (t.tuples() > 0)
        return;
      GECODE_ES_FAIL(ES_FAILED);
      return;
    }

    ViewArray<IntView> xv(home,x);
    GECODE_ES_FAIL((Extensional::postposcompact_levels(home,xv,t,ipl,epk)));
  }

  void
  extensional(Home home, const IntVarArgs& x, const TupleSet& t, bool pos,
              Reify r, IntPropLevel ipl) {
    extensional(home,x,t,pos,r,ipl,EPK_AUTO);
  }

  void
  extensional(Home home, const IntVarArgs& x, const TupleSet& t, bool pos,
              Reify r,
              IntPropLevel,
              ExtensionalPropKind epk) {
    using namespace Int;
    if (!t.finalized())
      throw NotYetFinalized("Int::extensional");
    if (t.arity() != x.size())
      throw ArgumentSizeMismatch("Int::extensional");
    if (same(x))
      throw ArgumentSame("Int::extensional");
    GECODE_POST;

    if (x.size() == 0) {
      const bool c = pos ? (t.tuples() > 0) : (t.tuples() == 0);
      BoolView b(r.var());
      switch (r.mode()) {
      case RM_EQV:
        if (c)
          GECODE_ME_FAIL(b.one(home));
        else
          GECODE_ME_FAIL(b.zero(home));
        break;
      case RM_IMP:
        if (!c)
          GECODE_ME_FAIL(b.zero(home));
        break;
      case RM_PMI:
        if (c)
          GECODE_ME_FAIL(b.one(home));
        break;
      default:
        GECODE_NEVER;
      }
      return;
    }
    if (t.tuples() == 0) {
      const bool c = !pos;
      BoolView b(r.var());
      switch (r.mode()) {
      case RM_EQV:
        if (c)
          GECODE_ME_FAIL(b.one(home));
        else
          GECODE_ME_FAIL(b.zero(home));
        break;
      case RM_IMP:
        if (!c)
          GECODE_ME_FAIL(b.zero(home));
        break;
      case RM_PMI:
        if (c)
          GECODE_ME_FAIL(b.one(home));
        break;
      default:
        GECODE_NEVER;
      }
      return;
    }

    const Int::Extensional::DispatchKind dk =
      Int::Extensional::dispatch_kind(t,epk);
    if (dk == Int::Extensional::DD_SPARSE) {
      ViewArray<IntView> xv(home,x);
      if (pos) {
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Extensional::SparseReifInc<IntView,BoolView,RM_EQV>
                          ::post(home,xv,t,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Extensional::SparseReifInc<IntView,BoolView,RM_IMP>
                          ::post(home,xv,t,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Extensional::SparseReifInc<IntView,BoolView,RM_PMI>
                          ::post(home,xv,t,r.var())));
          break;
        default: throw UnknownReifyMode("Int::extensional");
        }
      } else {
        NegBoolView n(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Extensional::SparseReifInc<IntView,NegBoolView,RM_EQV>
                          ::post(home,xv,t,n)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Extensional::SparseReifInc<IntView,NegBoolView,RM_PMI>
                          ::post(home,xv,t,n)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Extensional::SparseReifInc<IntView,NegBoolView,RM_IMP>
                          ::post(home,xv,t,n)));
          break;
        default: throw UnknownReifyMode("Int::extensional");
        }
      }
      return;
    }
    ViewArray<IntView> xv(home,x);
    if (dk == Int::Extensional::DD_DENSE_COMPRESSED) {
      if (pos) {
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<IntView,BoolView,RM_EQV>
                          (home,xv,t,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<IntView,BoolView,RM_IMP>
                          (home,xv,t,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<IntView,BoolView,RM_PMI>
                          (home,xv,t,r.var())));
          break;
        default: throw UnknownReifyMode("Int::extensional");
        }
      } else {
        NegBoolView n(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<IntView,NegBoolView,RM_EQV>
                          (home,xv,t,n)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<IntView,NegBoolView,RM_PMI>
                          (home,xv,t,n)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<IntView,NegBoolView,RM_IMP>
                          (home,xv,t,n)));
          break;
        default: throw UnknownReifyMode("Int::extensional");
        }
      }
      return;
    }

    if (pos) {
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Extensional::postrecompact<IntView,BoolView,RM_EQV>
                        (home,xv,t,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Extensional::postrecompact<IntView,BoolView,RM_IMP>
                        (home,xv,t,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Extensional::postrecompact<IntView,BoolView,RM_PMI>
                        (home,xv,t,r.var())));
        break;
      default: throw UnknownReifyMode("Int::extensional");
      }
    } else {
      NegBoolView n(r.var());
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Extensional::postrecompact<IntView,NegBoolView,RM_EQV>
                        (home,xv,t,n)));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Extensional::postrecompact<IntView,NegBoolView,RM_PMI>
                        (home,xv,t,n)));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Extensional::postrecompact<IntView,NegBoolView,RM_IMP>
                        (home,xv,t,n)));
        break;
      default: throw UnknownReifyMode("Int::extensional");
      }
    }
  }

  void
  extensional(Home home, const BoolVarArgs& x, const TupleSet& t, bool pos,
              IntPropLevel ipl) {
    extensional(home,x,t,pos,ipl,EPK_AUTO);
  }

  void
  extensional(Home home, const BoolVarArgs& x, const TupleSet& t, bool pos,
              IntPropLevel ipl,
              ExtensionalPropKind epk) {
    using namespace Int;
    if (!t.finalized())
      throw NotYetFinalized("Int::extensional");
    if (t.arity() != x.size())
      throw ArgumentSizeMismatch("Int::extensional");
    if ((t.min() < 0) || (t.max() > 1))
      throw NotZeroOne("Int::extensional");
    if (same(x))
      throw ArgumentSame("Int::extensional");
    GECODE_POST;

    if (x.size() == 0) {
      if (pos ? (t.tuples() > 0) : (t.tuples() == 0))
        return;
      GECODE_ES_FAIL(ES_FAILED);
      return;
    }
    if (t.tuples() == 0) {
      if (!pos)
        return;
      GECODE_ES_FAIL(ES_FAILED);
      return;
    }

    if (pos && (vbd(ipl) == IPL_BND)) {
      ViewArray<BoolView> xv(home,x);
      IntPropLevelArgs ipls(x.size());
      for (int i=0; i<x.size(); i++)
        ipls[i] = IPL_BND;
      GECODE_ES_FAIL((Extensional::postposcompact_levels(home,xv,t,ipls,epk)));
      return;
    }

    const Int::Extensional::DispatchKind dk =
      Int::Extensional::dispatch_kind(t,epk);
    if (dk == Int::Extensional::DD_SPARSE) {
      ViewArray<BoolView> xv(home,x);
      if (pos)
        GECODE_ES_FAIL((Extensional::SparseInc<BoolView,true>::post(home,xv,t)));
      else
        GECODE_ES_FAIL((Extensional::SparseInc<BoolView,false>::post(home,xv,t)));
      return;
    }
    ViewArray<BoolView> xv(home,x);
    if (dk == Int::Extensional::DD_DENSE_COMPRESSED) {
      if (pos)
        GECODE_ES_FAIL((Extensional::postposcompact_compressed<BoolView>(home,xv,t)));
      else
        GECODE_ES_FAIL((Extensional::postnegcompact_compressed<BoolView>(home,xv,t)));
    } else if (pos) {
      GECODE_ES_FAIL((Extensional::postposcompact<BoolView>(home,xv,t)));
    } else {
      GECODE_ES_FAIL((Extensional::postnegcompact<BoolView>(home,xv,t)));
    }
  }

  void
  extensional(Home home, const BoolVarArgs& x, const TupleSet& t, bool pos,
              const IntPropLevelArgs& ipl) {
    extensional(home,x,t,pos,ipl,EPK_AUTO);
  }

  void
  extensional(Home home, const BoolVarArgs& x, const TupleSet& t, bool pos,
              const IntPropLevelArgs& ipl, ExtensionalPropKind epk) {
    using namespace Int;
    if (!t.finalized())
      throw NotYetFinalized("Int::extensional");
    if (t.arity() != x.size())
      throw ArgumentSizeMismatch("Int::extensional");
    if ((t.min() < 0) || (t.max() > 1))
      throw NotZeroOne("Int::extensional");
    if (ipl.size() != x.size())
      throw ArgumentSizeMismatch("Int::extensional");
    if (same(x))
      throw ArgumentSame("Int::extensional");
    GECODE_POST;

    if (!pos || Int::Extensional::tuple_set_all_domain(ipl)) {
      extensional(home,x,t,pos,IPL_DOM,epk);
      return;
    }

    if (x.size() == 0) {
      if (t.tuples() > 0)
        return;
      GECODE_ES_FAIL(ES_FAILED);
      return;
    }

    ViewArray<BoolView> xv(home,x);
    GECODE_ES_FAIL((Extensional::postposcompact_levels(home,xv,t,ipl,epk)));
  }

  void
  extensional(Home home, const BoolVarArgs& x, const TupleSet& t, bool pos,
              Reify r, IntPropLevel ipl) {
    extensional(home,x,t,pos,r,ipl,EPK_AUTO);
  }

  void
  extensional(Home home, const BoolVarArgs& x, const TupleSet& t, bool pos,
              Reify r,
              IntPropLevel,
              ExtensionalPropKind epk) {
    using namespace Int;
    if (!t.finalized())
      throw NotYetFinalized("Int::extensional");
    if (t.arity() != x.size())
      throw ArgumentSizeMismatch("Int::extensional");
    if ((t.min() < 0) || (t.max() > 1))
      throw NotZeroOne("Int::extensional");
    if (same(x))
      throw ArgumentSame("Int::extensional");
    GECODE_POST;

    if (x.size() == 0) {
      const bool c = pos ? (t.tuples() > 0) : (t.tuples() == 0);
      BoolView b(r.var());
      switch (r.mode()) {
      case RM_EQV:
        if (c)
          GECODE_ME_FAIL(b.one(home));
        else
          GECODE_ME_FAIL(b.zero(home));
        break;
      case RM_IMP:
        if (!c)
          GECODE_ME_FAIL(b.zero(home));
        break;
      case RM_PMI:
        if (c)
          GECODE_ME_FAIL(b.one(home));
        break;
      default:
        GECODE_NEVER;
      }
      return;
    }
    if (t.tuples() == 0) {
      const bool c = !pos;
      BoolView b(r.var());
      switch (r.mode()) {
      case RM_EQV:
        if (c)
          GECODE_ME_FAIL(b.one(home));
        else
          GECODE_ME_FAIL(b.zero(home));
        break;
      case RM_IMP:
        if (!c)
          GECODE_ME_FAIL(b.zero(home));
        break;
      case RM_PMI:
        if (c)
          GECODE_ME_FAIL(b.one(home));
        break;
      default:
        GECODE_NEVER;
      }
      return;
    }

    const Int::Extensional::DispatchKind dk =
      Int::Extensional::dispatch_kind(t,epk);
    if (dk == Int::Extensional::DD_SPARSE) {
      ViewArray<BoolView> xv(home,x);
      if (pos) {
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Extensional::SparseReifInc<BoolView,BoolView,RM_EQV>
                          ::post(home,xv,t,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Extensional::SparseReifInc<BoolView,BoolView,RM_IMP>
                          ::post(home,xv,t,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Extensional::SparseReifInc<BoolView,BoolView,RM_PMI>
                          ::post(home,xv,t,r.var())));
          break;
        default: throw UnknownReifyMode("Int::extensional");
        }
      } else {
        NegBoolView n(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Extensional::SparseReifInc<BoolView,NegBoolView,RM_EQV>
                          ::post(home,xv,t,n)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Extensional::SparseReifInc<BoolView,NegBoolView,RM_PMI>
                          ::post(home,xv,t,n)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Extensional::SparseReifInc<BoolView,NegBoolView,RM_IMP>
                          ::post(home,xv,t,n)));
          break;
        default: throw UnknownReifyMode("Int::extensional");
        }
      }
      return;
    }
    ViewArray<BoolView> xv(home,x);
    if (dk == Int::Extensional::DD_DENSE_COMPRESSED) {
      if (pos) {
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<BoolView,BoolView,RM_EQV>
                          (home,xv,t,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<BoolView,BoolView,RM_IMP>
                          (home,xv,t,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<BoolView,BoolView,RM_PMI>
                          (home,xv,t,r.var())));
          break;
        default: throw UnknownReifyMode("Int::extensional");
        }
      } else {
        NegBoolView n(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<BoolView,NegBoolView,RM_EQV>
                          (home,xv,t,n)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<BoolView,NegBoolView,RM_PMI>
                          (home,xv,t,n)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Extensional::postrecompact_compressed<BoolView,NegBoolView,RM_IMP>
                          (home,xv,t,n)));
          break;
        default: throw UnknownReifyMode("Int::extensional");
        }
      }
      return;
    }

    if (pos) {
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Extensional::postrecompact<BoolView,BoolView,RM_EQV>
                        (home,xv,t,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Extensional::postrecompact<BoolView,BoolView,RM_IMP>
                        (home,xv,t,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Extensional::postrecompact<BoolView,BoolView,RM_PMI>
                        (home,xv,t,r.var())));
        break;
      default: throw UnknownReifyMode("Int::extensional");
      }
    } else {
      NegBoolView n(r.var());
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Extensional::postrecompact<BoolView,NegBoolView,RM_EQV>
                        (home,xv,t,n)));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Extensional::postrecompact<BoolView,NegBoolView,RM_PMI>
                        (home,xv,t,n)));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Extensional::postrecompact<BoolView,NegBoolView,RM_IMP>
                        (home,xv,t,n)));
        break;
      default: throw UnknownReifyMode("Int::extensional");
      }
    }
  }

}

// STATISTICS: int-post
