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

  template<class View>
  class SparsePosInc : public Propagator {
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
      for (unsigned int i=0U; i<n_vals; i++)
        support_count[i] = 0U;
      const unsigned long long n_tv =
        static_cast<unsigned long long>(n_tuples) *
        static_cast<unsigned long long>(x.size());
      for (unsigned long long i=0ULL; i<n_tv; i++) {
        const unsigned int gid = tv[i];
        assert(gid < n_vals);
        support_count[gid]++;
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

      for (int a=0; a<x.size(); a++) {
        const unsigned long long idx =
          static_cast<unsigned long long>(tid) *
          static_cast<unsigned long long>(x.size()) +
          static_cast<unsigned long long>(a);
        const unsigned int gid = tv[idx];
        assert(gid < n_vals);
        assert(support_count[gid] > 0U);
        support_count[gid]--;
        if (support_count[gid] == 0U)
          enqueue_zero(gid);
      }
    }

    unsigned int
    tuple_gid(unsigned int tid, int a) const {
      const unsigned long long idx =
        static_cast<unsigned long long>(tid) *
        static_cast<unsigned long long>(x.size()) +
        static_cast<unsigned long long>(a);
      const unsigned int gid = tv[idx];
      assert(gid < n_vals);
      return gid;
    }

    void
    deactivate_value_support(int i, int n) {
      const unsigned int* b = nullptr;
      const unsigned int* e = nullptr;
      unsigned int gid = 0U;
      if (ts.sparse_support(i,n,b,e,gid)) {
        for (const unsigned int* t=b; t<e; t++)
          deactivate_tuple(*t);
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
    SparsePosInc(Home home, ViewArray<View>& x0, const TupleSet& ts0)
      : Propagator(home), x(home,x0), ts(ts0), c(home),
        n_tuples(static_cast<unsigned int>(ts0.tuples())),
        n_vals(ts0.sparse_values()),
        active_ids(static_cast<Space&>(home).alloc<unsigned int>(n_tuples)),
        pos_in_active(static_cast<Space&>(home).alloc<unsigned int>(n_tuples)),
        active_limit(n_tuples),
        support_count(static_cast<Space&>(home).alloc<unsigned int>(n_vals)),
        gid_var(static_cast<Space&>(home).alloc<int>(n_vals)),
        gid_val(static_cast<Space&>(home).alloc<int>(n_vals)),
        zero_queue(static_cast<Space&>(home).alloc<unsigned int>(n_vals)),
        zero_queue_size(0U),
        queued(static_cast<Space&>(home).alloc<unsigned char>(n_vals)),
        tv(ts0.sparse_tuple_value_ids()),
        in_propagate(false) {
      assert(tv != nullptr);
      for (unsigned int i=0U; i<n_tuples; i++) {
        active_ids[i] = i;
        pos_in_active[i] = i;
      }
      for (unsigned int i=0U; i<n_vals; i++)
        queued[i] = 0U;

      init_gid_maps();
      init_support_counts();

      for (int i=0; i<x.size(); i++)
        if (!x[i].assigned())
          (void) new (home) SparseAdvisor(home,*this,c,x[i],i);

      for (int i=0; i<x.size(); i++)
        deactivate_for_domain(i,x[i]);
    }

    SparsePosInc(Space& home, SparsePosInc<View>& p)
      : Propagator(home,p), x(), ts(p.ts), c(home),
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
        tv(ts.sparse_tuple_value_ids()),
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

      SparsePosInc<View>* p = new (home) SparsePosInc<View>(home,x,ts);
      if (p->active_limit == 0U)
        return ES_FAILED;
      View::schedule(home,*p,ME_INT_DOM);
      return ES_OK;
    }

    virtual Actor*
    copy(Space& home) {
      return new (home) SparsePosInc<View>(home,*this);
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
      c.dispose(home);
      ts.~TupleSet();
      (void) Propagator::dispose(home);
      return sizeof(*this);
    }

    virtual ExecStatus
    propagate(Space& home, const ModEventDelta&) {
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

    virtual ExecStatus
    advise(Space& home, Advisor& a0, const Delta& d) {
      if (active_limit == 0U)
        return ES_FAILED;

      SparseAdvisor& sa = static_cast<SparseAdvisor&>(a0);
      View xv = sa.view();
      if (in_propagate)
        return xv.assigned() ? home.ES_FIX_DISPOSE(c,sa) : ES_FIX;

      const int i = sa.index();

      if (xv.assigned()) {
        deactivate_for_domain(i,xv);
        if (active_limit == 0U)
          return ES_FAILED;
        return home.ES_NOFIX_DISPOSE(c,sa);
      }
      deactivate_removed_values(i,xv,d);

      if (active_limit == 0U)
        return ES_FAILED;
      return ES_NOFIX;
    }
  };

  forceinline bool
  use_sparse_backend(const TupleSet& t, bool pos, bool reified,
                     ExtensionalPropKind epk) {
    if (!pos || reified) {
      if (epk == EPK_SPARSE) {
        // Sparse negative/reified posting is not implemented yet.
        // Fall back to dense only if available.
        if (t.dense_support())
          return false;
        throw OutOfLimits("Int::extensional");
      }
      return false;
    }

    switch (epk) {
    case EPK_AUTO:
      return t.sparse_support();
    case EPK_DENSE:
      return false;
    case EPK_SPARSE:
      if (t.sparse_support())
        return true;
      if (t.dense_support())
        return false;
      throw OutOfLimits("Int::extensional");
    default:
      GECODE_NEVER;
      return false;
    }
  }

}}}

namespace Gecode {

  void
  extensional(Home home, const IntVarArgs& x, const TupleSet& t, bool pos,
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
      if (pos ? (t.tuples() > 0) : (t.tuples() == 0))
        return;
      GECODE_ES_FAIL(ES_FAILED);
      return;
    }

    if (Int::Extensional::use_sparse_backend(t,pos,false,epk)) {
      ViewArray<IntView> xv(home,x);
      GECODE_ES_FAIL((Extensional::SparsePosInc<IntView>::post(home,xv,t)));
      return;
    }
    if (!t.dense_support())
      throw OutOfLimits("Int::extensional");

    ViewArray<IntView> xv(home,x);
    if (pos)
      GECODE_ES_FAIL((Extensional::postposcompact<IntView>(home,xv,t)));
    else
      GECODE_ES_FAIL((Extensional::postnegcompact<IntView>(home,xv,t)));
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

    (void) Int::Extensional::use_sparse_backend(t,pos,true,epk);
    if (!t.dense_support())
      throw OutOfLimits("Int::extensional");

    ViewArray<IntView> xv(home,x);
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
      if (pos ? (t.tuples() > 0) : (t.tuples() == 0))
        return;
      GECODE_ES_FAIL(ES_FAILED);
      return;
    }

    if (Int::Extensional::use_sparse_backend(t,pos,false,epk)) {
      ViewArray<BoolView> xv(home,x);
      GECODE_ES_FAIL((Extensional::SparsePosInc<BoolView>::post(home,xv,t)));
      return;
    }
    if (!t.dense_support())
      throw OutOfLimits("Int::extensional");

    ViewArray<BoolView> xv(home,x);
    if (pos)
      GECODE_ES_FAIL((Extensional::postposcompact<BoolView>(home,xv,t)));
    else
      GECODE_ES_FAIL((Extensional::postnegcompact<BoolView>(home,xv,t)));
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

    (void) Int::Extensional::use_sparse_backend(t,pos,true,epk);
    if (!t.dense_support())
      throw OutOfLimits("Int::extensional");

    ViewArray<BoolView> xv(home,x);
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
