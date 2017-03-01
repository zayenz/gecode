/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
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

#include <gecode/kernel.hh>

namespace Gecode {

  /*
   * Variable type disposer
   *
   */
  void
  VarImpDisposerBase::dispose(Space&,VarImpBase*) {}

  VarImpDisposerBase::~VarImpDisposerBase(void) {}



  /*
   * Actor
   *
   */
  Actor* Actor::sentinel;

  Actor::~Actor(void) {
    assert(false);
  }


  /*
   * Propagator
   *
   */
  ExecStatus
  Propagator::advise(Space&, Advisor&, const Delta&) {
    GECODE_NEVER;
    return ES_FAILED;
  }
  void
  Propagator::advise(Space&, Advisor&) {
    GECODE_NEVER;
  }


  /*
   * No-goods
   *
   */
  void
  NoGoods::post(Space&) const {
  }

  NoGoods NoGoods::eng;

  /*
   * Brancher
   *
   */
  NGL*
  Brancher::ngl(Space&, const Choice&, unsigned int) const {
    return NULL;
  }

  void
  Brancher::print(const Space&, const Choice&, unsigned int,
                  std::ostream&) const {
  }


  /*
   * Space: Misc
   *
   */
  StatusStatistics Space::unused_status;
  CloneStatistics Space::unused_clone;
  CommitStatistics Space::unused_commit;

#ifdef GECODE_HAS_VAR_DISPOSE
  VarImpDisposerBase* Space::vd[AllVarConf::idx_d];
#endif

  Space::Space(void) : sm(new SharedMemory), mm(sm) {
#ifdef GECODE_HAS_VAR_DISPOSE
    for (int i=0; i<AllVarConf::idx_d; i++)
      _vars_d[i] = NULL;
#endif
    // Initialize propagator and brancher links
    pl.init();
    bl.init();
    b_status = b_commit = Brancher::cast(&bl);
    // Initialize array for forced deletion to be empty
    d_fst = d_cur = d_lst = NULL;
    // Initialize space as stable but not failed
    pc.p.active = &pc.p.queue[0]-1;
    // Initialize propagator queues
    for (int i=0; i<=PropCost::AC_MAX; i++)
      pc.p.queue[i].init();
    pc.p.bid = reserved_bid+1;
    pc.p.n_sub = 0;
  }

  void
  Space::ap_notice_dispose(Actor& a, bool duplicate) {
    if (duplicate && (d_fst != NULL)) {
      for (Actor** f = d_fst; f < d_cur; f++)
        if (&a == *f)
          return;
    }
    if (d_cur == d_lst) {
      // Resize
      if (d_fst == NULL) {
        // Create new array
        d_fst = alloc<Actor*>(4);
        d_cur = d_fst;
        d_lst = d_fst+4;
      } else {
        // Resize existing array
        unsigned int n = static_cast<unsigned int>(d_lst - d_fst);
        assert(n != 0);
        d_fst = realloc<Actor*>(d_fst,n,2*n);
        d_cur = d_fst+n;
        d_lst = d_fst+2*n;
      }
    }
    *(d_cur++) = &a;
  }

  void
  Space::ap_ignore_dispose(Actor& a, bool duplicate) {
    assert(d_fst != NULL);
    Actor** f = d_fst;
    if (duplicate) {
      while (f < d_cur)
        if (&a == *f)
          break;
        else
          f++;
      if (f == d_cur)
        return;
    } else {
      while (&a != *f)
        f++;
    }
    *f = *(--d_cur);
  }

  void
  Space::flush(void) {
    // Flush malloc cache
    sm->flush();
  }

  Space::~Space(void) {
    // Mark space as failed
    fail();
    // Delete actors that must be deleted
    {
      Actor** a = d_fst;
      Actor** e = d_cur;
      // So that d_unforce knows that deletion is in progress
      d_fst = NULL;
      while (a < e) {
        (void) (*a)->dispose(*this);
        a++;
      }
    }
#ifdef GECODE_HAS_VAR_DISPOSE
    // Delete variables that were registered for disposal
    for (int i=AllVarConf::idx_d; i--;)
      if (_vars_d[i] != NULL)
        vd[i]->dispose(*this, _vars_d[i]);
#endif
    // Release memory from memory manager
    mm.release(sm);
    // Release shared memory
    if (sm->release())
      delete sm;
  }



  /*
   * Space: propagation
   *
   */

  SpaceStatus
  Space::status(StatusStatistics& stat) {
    // Check whether space is failed
    if (failed())
      return SS_FAILED;
    assert(pc.p.active <= &pc.p.queue[PropCost::AC_MAX+1]);
    // Check whether space is stable but not failed
    if (pc.p.active >= &pc.p.queue[0]) {
      Propagator* p;
      ModEventDelta med_o;
      goto unstable;
    execute:
      stat.propagate++;
      if (p->disabled())
        goto put_into_idle;
      pc.p.ei.propagator(*p);
      // Keep old modification event delta
      med_o = p->u.med;
      // Clear med but leave propagator in queue
      p->u.med = 0;
      switch (p->propagate(*this,med_o)) {
      case ES_FAILED:
        // Count failure
        gpi.fail(p->gpi());
        // Mark as failed
        fail();
        {
          // Propagate top priority propagators
          ActorLink* e = &pc.p.queue[PropCost::AC_RECORD];
          for (ActorLink* a = e->next(); a != e; a = a->next()) {
            Propagator* top = Propagator::cast(a);
            pc.p.ei.propagator(*top);
            // Keep old modification event delta
            ModEventDelta top_med_o = top->u.med;
            // Clear med but leave propagator in queue
            top->u.med = 0;
            switch (top->propagate(*this,top_med_o)) {
            case ES_FIX:
            case __ES_SUBSUMED:
              break;
            default:
              GECODE_NEVER;
            }
          }
        }
        return SS_FAILED;
      case ES_NOFIX:
        // Find next, if possible
        if (p->u.med != 0) {
        unstable:
          // There is at least one propagator in a queue
          do {
            assert(pc.p.active >= &pc.p.queue[0]);
            // First propagator or link back to queue
            ActorLink* fst = pc.p.active->next();
            if (pc.p.active != fst) {
              p = Propagator::cast(fst);
              goto execute;
            }
            pc.p.active--;
          } while (true);
          GECODE_NEVER;
        }
        // Fall through
      case ES_FIX:
      put_into_idle:
        // Clear med
        p->u.med = 0;
        // Put into idle queue
        p->unlink(); pl.head(p);
      stable_or_unstable:
        // There might be a propagator in the queue
        do {
          assert(pc.p.active >= &pc.p.queue[0]);
          // First propagator or link back to queue
          ActorLink* fst = pc.p.active->next();
          if (pc.p.active != fst) {
            p = Propagator::cast(fst);
            goto execute;
          }
        } while (--pc.p.active >= &pc.p.queue[0]);
        assert(pc.p.active < &pc.p.queue[0]);
        goto stable;
      case __ES_SUBSUMED:
        p->unlink(); rfree(p,p->u.size);
        goto stable_or_unstable;
      case __ES_PARTIAL:
        // Schedule propagator with specified propagator events
        assert(p->u.med != 0);
        enqueue(p);
        goto unstable;
      default:
        GECODE_NEVER;
      }
    }
  stable:
    pc.p.ei.other();
    /*
     * Find the next brancher that has still alternatives left
     *
     * It is important to note that branchers reporting to have no more
     * alternatives left cannot be deleted. They cannot be deleted
     * as there might be choices to be used in commit
     * that refer to one of these branchers. This e.g. happens when
     * we combine branch-and-bound search with adaptive recomputation: during
     * recomputation, a copy is constrained to be better than the currently
     * best solution, then the first half of the choices are posted,
     * and a fixpoint computed (for storing in the middle of the path). Then
     * the remaining choices are posted, and because of the additional
     * constraints that the space must be better than the previous solution,
     * the corresponding Branchers may already have no alternatives left.
     *
     * The same situation may arise due to weakly monotonic propagators.
     *
     * A brancher reporting that no more alternatives exist is exhausted.
     * All exhausted branchers will be left of the current pointer b_status.
     * Only when it is known that no more choices
     * can be used for commit an exhausted brancher can actually be deleted.
     * This becomes known when choice is called.
     */
    while (b_status != Brancher::cast(&bl))
      if (b_status->status(*this)) {
        // Brancher still has choices to generate
        return SS_BRANCH;
      } else {
        // Brancher is exhausted
        b_status = Brancher::cast(b_status->next());
      }
    // No brancher with alternatives left, space is solved
    return SS_SOLVED;
  }


  const Choice*
  Space::choice(void) {
    if (!stable())
      throw SpaceNotStable("Space::choice");
    if (failed() || (b_status == Brancher::cast(&bl))) {
      // There are no more choices to be generated
      // Delete all branchers
      Brancher* b = Brancher::cast(bl.next());
      while (b != Brancher::cast(&bl)) {
        Brancher* d = b;
        b = Brancher::cast(b->next());
        rfree(d,d->dispose(*this));
      }
      bl.init();
      b_status = b_commit = Brancher::cast(&bl);
      return NULL;
    }
    /*
     * The call to choice() says that no older choices
     * can be used. Hence, all branchers that are exhausted can be deleted.
     */
    Brancher* b = Brancher::cast(bl.next());
    while (b != b_status) {
      Brancher* d = b;
      b = Brancher::cast(b->next());
      d->unlink();
      rfree(d,d->dispose(*this));
    }
    // Make sure that b_commit does not point to a deleted brancher!
    b_commit = b_status;
    return b_status->choice(*this);
  }

  const Choice*
  Space::choice(Archive& e) const {
    unsigned int id; e >> id;
    Brancher* b_cur = Brancher::cast(bl.next());
    while (b_cur != Brancher::cast(&bl)) {
      if (id == b_cur->id())
        return b_cur->choice(*this,e);
      b_cur = Brancher::cast(b_cur->next());
    }
    throw SpaceNoBrancher("Space::choice");
  }

  void
  Space::_commit(const Choice& c, unsigned int a) {
    if (a >= c.alternatives())
      throw SpaceIllegalAlternative("Space::commit");
    if (failed())
      return;
    if (Brancher* b = brancher(c.bid)) {
      // There is a matching brancher
      pc.p.ei.brancher(*b);
      ExecStatus es = b->commit(*this,c,a);
      pc.p.ei.other();
      if (es == ES_FAILED)
        fail();
    } else {
      // There is no matching brancher!
      throw SpaceNoBrancher("Space::commit");
    }
  }

  void
  Space::_trycommit(const Choice& c, unsigned int a) {
    if (a >= c.alternatives())
      throw SpaceIllegalAlternative("Space::commit");
    if (failed())
      return;
    if (Brancher* b = brancher(c.bid)) {
      // There is a matching brancher
      pc.p.ei.brancher(*b);
      ExecStatus es = b->commit(*this,c,a);
      pc.p.ei.other();
      if (es == ES_FAILED)
        fail();
    }
  }

  NGL*
  Space::ngl(const Choice& c, unsigned int a) {
    if (a >= c.alternatives())
      throw SpaceIllegalAlternative("Space::ngl");
    if (failed())
      return NULL;
    if (Brancher* b = brancher(c.bid)) {
      // There is a matching brancher
      return b->ngl(*this,c,a);
    } else {
      return NULL;
    }
  }

  void
  Space::print(const Choice& c, unsigned int a, std::ostream& o) const {
    if (a >= c.alternatives())
      throw SpaceIllegalAlternative("Space::print");
    if (failed())
      return;
    if (Brancher* b = const_cast<Space&>(*this).brancher(c.bid)) {
      // There is a matching brancher
      b->print(*this,c,a,o);
    } else {
      // There is no matching brancher!
      throw SpaceNoBrancher("Space::print");
    }
  }

  void
  Space::kill_brancher(unsigned int id) {
    if (failed())
      return;
    for (Brancher* b = Brancher::cast(bl.next());
         b != Brancher::cast(&bl); b = Brancher::cast(b->next()))
      if (b->id() == id) {
        kill(*b);
        return;
      }
  }


  /*
   * Space cloning
   *
   * Cloning is performed in two steps:
   *  - The space itself is copied by the copy constructor. This
   *    also copies all propagators, branchers, and variables.
   *    The copied variables are recorded.
   *  - In the second step the dependency information of the recorded
   *    variables is updated and their forwarding information is reset.
   *
   */
  Space::Space(bool share, Space& s)
    : sm(s.sm->copy(share)),
      mm(sm,s.mm,s.pc.p.n_sub*sizeof(Propagator**)),
      gpi(s.gpi),
      d_fst(&Actor::sentinel) {
#ifdef GECODE_HAS_VAR_DISPOSE
    for (int i=0; i<AllVarConf::idx_d; i++)
      _vars_d[i] = NULL;
#endif
    for (int i=0; i<AllVarConf::idx_c; i++)
      pc.c.vars_u[i] = NULL;
    pc.c.vars_noidx = NULL;
    pc.c.shared = NULL;
    pc.c.local = NULL;
    // Copy all propagators
    {
      ActorLink* p = &pl;
      ActorLink* e = &s.pl;
      for (ActorLink* a = e->next(); a != e; a = a->next()) {
        Actor* c = Actor::cast(a)->copy(*this,share);
        // Link copied actor
        p->next(ActorLink::cast(c)); ActorLink::cast(c)->prev(p);
        // Note that forwarding is done in the constructors
        p = c;
      }
      // Link last actor
      p->next(&pl); pl.prev(p);
    }
    // Copy all branchers
    {
      ActorLink* p = &bl;
      ActorLink* e = &s.bl;
      for (ActorLink* a = e->next(); a != e; a = a->next()) {
        Actor* c = Actor::cast(a)->copy(*this,share);
        // Link copied actor
        p->next(ActorLink::cast(c)); ActorLink::cast(c)->prev(p);
        // Note that forwarding is done in the constructors
        p = c;
      }
      // Link last actor
      p->next(&bl); bl.prev(p);
    }
    // Setup brancher pointers
    if (s.b_status == &s.bl) {
      b_status = Brancher::cast(&bl);
    } else {
      b_status = Brancher::cast(s.b_status->prev());
    }
    if (s.b_commit == &s.bl) {
      b_commit = Brancher::cast(&bl);
    } else {
      b_commit = Brancher::cast(s.b_commit->prev());
    }
  }

  Space*
  Space::_clone(bool share_data, bool share_info) {
    if (failed())
      throw SpaceFailed("Space::clone");
    if (!stable())
      throw SpaceNotStable("Space::clone");

    // Copy all data structures (which in turn will invoke the constructor)
    Space* c = copy(share_data);

    if (c->d_fst != &Actor::sentinel)
      throw SpaceNotCloned("Space::clone");

    // Setup array for actor disposal in c
    {
      unsigned int n = static_cast<unsigned int>(d_cur - d_fst);
      if (n == 0) {
        // No actors
        c->d_fst = c->d_cur = c->d_lst = NULL;
      } else {
        // Leave one entry free
        c->d_fst = c->alloc<Actor*>(n+1);
        c->d_cur = c->d_fst;
        c->d_lst = c->d_fst+n+1;
        for (Actor** d_fst_iter = d_fst; d_fst_iter != d_cur; d_fst_iter++) {
          if ((*d_fst_iter)->prev())
            *(c->d_cur++) = Actor::cast((*d_fst_iter)->prev());
        }
      }
    }

    // Update variables without indexing structure
    VarImp<NoIdxVarImpConf>* x =
      static_cast<VarImp<NoIdxVarImpConf>*>(c->pc.c.vars_noidx);
    while (x != NULL) {
      VarImp<NoIdxVarImpConf>* n = x->next();
      x->b.base = NULL; x->u.idx[0] = 0;
      if (sizeof(ActorLink**) > sizeof(unsigned int))
        *(1+&x->u.idx[0]) = 0;
      x = n;
    }
    // Update variables with indexing structure
    c->update(static_cast<ActorLink**>(c->mm.subscriptions()));

    // Re-establish prev links (reset forwarding information)
    {
      ActorLink* p_a = &pl;
      ActorLink* c_a = p_a->next();
      // First update propagators and advisors
      while (c_a != &pl) {
        Propagator* p = Propagator::cast(c_a);
        if (p->u.advisors != NULL) {
          ActorLink* a = p->u.advisors;
          p->u.advisors = NULL;
          do {
            a->prev(p); a = a->next();
          } while (a != NULL);
        }
        c_a->prev(p_a); p_a = c_a; c_a = c_a->next();
      }
    }
    {
      ActorLink* p_a = &bl;
      ActorLink* c_a = p_a->next();
      // Update branchers
      while (c_a != &bl) {
        c_a->prev(p_a); p_a = c_a; c_a = c_a->next();
      }
    }

    // Reset links for shared objects
    for (SharedHandle::Object* s = c->pc.c.shared; s != NULL; s = s->next)
      s->fwd = NULL;

    // Reset links for local objects
    for (ActorLink* l = c->pc.c.local; l != NULL; l = l->next())
      l->prev(NULL);

    // Initialize propagator queue
    c->pc.p.active = &c->pc.p.queue[0]-1;
    for (int i=0; i<=PropCost::AC_MAX; i++)
      c->pc.p.queue[i].init();
    // Copy propagation only data
    c->pc.p.n_sub = pc.p.n_sub;
    c->pc.p.bid = pc.p.bid;

    if (!share_info) {
      // Re-allocate afc information
      for (ActorLink* c_a = c->pl.next(); c_a != &c->pl; c_a = c_a->next()) {
        Propagator* p = Propagator::cast(c_a);
        GPI::Info* gpi = c->gpi.allocate(p->gpi().gid);
        if (p->disabled())
          p->gpi_disabled = Support::mark(gpi);
        else
          p->gpi_disabled = gpi;
      }
    }
    // Reset execution information
    c->pc.p.ei.other(); pc.p.ei.other();

    return c;
  }

  void
  Space::constrain(const Space&) {
  }

  bool
  Space::master(const MetaInfo& mi) {
    switch (mi.type()) {
    case MetaInfo::RESTART:
      if (mi.last() != NULL)
        constrain(*mi.last());
      mi.nogoods().post(*this);
      // Perform a restart even if a solution has been found
      return true;
    case MetaInfo::PORTFOLIO:
      // Kill all branchers
      BrancherGroup::all.kill(*this);
      return true;
    default: GECODE_NEVER;
      return true;
    }
  }

  bool
  Space::slave(const MetaInfo&) {
    return true;
  }


  void
  LocalObject::fwdcopy(Space& home, bool share) {
    ActorLink::cast(this)->prev(copy(home,share));
    next(home.pc.c.local);
    home.pc.c.local = this;
  }

  void
  Choice::archive(Archive& e) const {
    e << id();
  }

  bool
  NGL::notice(void) const {
    return false;
  }

  NGL::~NGL(void) {
  }


  /*
   * Groups
   */

  Group Group::all(GROUPID_ALL);
  Group Group::def(GROUPID_DEF);

  PropagatorGroup PropagatorGroup::all(GROUPID_ALL);
  PropagatorGroup PropagatorGroup::def(GROUPID_DEF);

  BrancherGroup BrancherGroup::all(GROUPID_ALL);
  BrancherGroup BrancherGroup::def(GROUPID_DEF);

  unsigned int Group::next = GROUPID_DEF+1;
  Support::Mutex Group::m;


  Group::Group(void) {
    m.acquire();
    gid = next++;
    m.release();
    if (gid == GROUPID_MAX)
      throw TooManyGroups("Group::Group");
  }


  PropagatorGroup&
  PropagatorGroup::move(Space& home, PropagatorGroup g) {
    if ((id() != GROUPID_ALL) && (id() != g.id()))
      for (Space::Propagators ps(home); ps(); ++ps)
        if (g.in(ps.propagator().group()))
          ps.propagator().group(*this);
    return *this;
  }

  PropagatorGroup&
  PropagatorGroup::move(Space& home, unsigned int pid) {
    if (id() == GROUPID_ALL)
      return *this;
    for (Space::Propagators ps(home); ps(); ++ps)
      if (ps.propagator().id() == pid) {
        ps.propagator().group(*this);
        return *this;
      }
    throw UnknownPropagator("PropagatorGroup::move");
    GECODE_NEVER;
    return *this;
  }

  unsigned int
  PropagatorGroup::size(Space& home) const {
    if (home.failed())
      return 0;
    unsigned int n=0;
    for (Space::Propagators ps(home); ps(); ++ps)
      if (in(ps.propagator().group()))
        n++;
    return n;
  }

  void
  PropagatorGroup::kill(Space& home) {
    if (home.failed())
      return;
    Space::Propagators ps(home);
    while (ps()) {
      Propagator& p = ps.propagator();
      ++ps;
      if (in(p.group()))
        home.kill(p);
    }
  }

  void
  PropagatorGroup::disable(Space& home) {
    if (home.failed())
      return;
    for (Space::Propagators ps(home); ps(); ++ps)
      if (in(ps.propagator().group()))
        ps.propagator().disable();
  }

  void
  PropagatorGroup::enable(Space& home, bool s) {
    if (home.failed())
      return;
    if (s) {
      Space::Propagators ps(home);
      while (ps()) {
        Propagator& p = ps.propagator();
        ++ps;
        if (in(p.group())) {
          p.enable();
          p.reschedule(home);
        }
      }
    } else {
      for (Space::Propagators ps(home); ps(); ++ps)
        if (in(ps.propagator().group()))
          ps.propagator().enable();
    }
  }


  BrancherGroup&
  BrancherGroup::move(Space& home, BrancherGroup g) {
    if ((id() != GROUPID_ALL) && (id() != g.id()))
      for (Space::Branchers bs(home); bs(); ++bs)
        if (g.in(bs.brancher().group()))
          bs.brancher().group(*this);
    return *this;
  }

  BrancherGroup&
  BrancherGroup::move(Space& home, unsigned int bid) {
    if (id() == GROUPID_ALL)
      return *this;
    for (Space::Branchers bs(home); bs(); ++bs)
      if (bs.brancher().id() == bid) {
        bs.brancher().group(*this);
        return *this;
      }
    throw UnknownBrancher("BrancherGroup::move");
    GECODE_NEVER;
    return *this;
  }

  unsigned int
  BrancherGroup::size(Space& home) const {
    if (home.failed())
      return 0;
    unsigned int n=0;
    for (Space::Branchers bs(home); bs(); ++bs)
      if (in(bs.brancher().group()))
        n++;
    return n;
  }

  void
  BrancherGroup::kill(Space& home) {
    if (home.failed())
      return;
    Space::Branchers bs(home);
    while (bs()) {
      Brancher& b = bs.brancher();
      ++bs;
      if (in(b.group()))
        home.kill(b);
    }
  }


}

// STATISTICS: kernel-core
