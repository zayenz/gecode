/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *     Linnea Ingmar <linnea.ingmar@hotmail.com>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Linnea Ingmar, 2017
 *     Mikael Lagerkvist, 2007
 *     Christian Schulte, 2005
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

#include "test/int.hh"

#include <gecode/minimodel.hh>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>

namespace Test { namespace Int {

   /// %Tests for extensional (relation) constraints
   namespace Extensional {

     /**
      * \defgroup TaskTestIntExtensional Extensional (relation) constraints
      * \ingroup TaskTestInt
      */
     //@{
     std::string
     extensional_kind_name(Gecode::ExtensionalPropKind epk) {
       switch (epk) {
       case Gecode::EPK_DENSE:
         return "Dense";
       case Gecode::EPK_SPARSE:
         return "Sparse";
       case Gecode::EPK_DENSE_COMPRESSED:
         return "DenseCompressed";
       case Gecode::EPK_AUTO:
         return "Auto";
       default:
         GECODE_NEVER;
         return "Unknown";
       }
     }

     /// %Test with simple regular expression
     class RegSimpleA : public Test {
     public:
       /// Create and register test
       RegSimpleA(void) : Test("Extensional::Reg::Simple::A",4,2,2) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         return (((x[0] == 0) || (x[0] == 2)) &&
                 ((x[1] == -1) || (x[1] == 1)) &&
                 ((x[2] == 0) || (x[2] == 1)) &&
                 ((x[3] == 0) || (x[3] == 1)));
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         extensional(home, x,
                     (REG(0) | REG(2)) +
                     (REG(-1) | REG(1)) +
                     (REG(7) | REG(0) | REG(1)) +
                     (REG(0) | REG(1)));
       }
     };

     /// %Test with simple regular expression
     class RegSimpleB : public Test {
     public:
       /// Create and register test
       RegSimpleB(void) : Test("Extensional::Reg::Simple::B",4,2,2) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         return (x[0]<x[1]) && (x[1]<x[2]) && (x[2]<x[3]);
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         extensional(home, x,
                     (REG(-2) + REG(-1) + REG(0) + REG(1)) |
                     (REG(-2) + REG(-1) + REG(0) + REG(2)) |
                     (REG(-2) + REG(-1) + REG(1) + REG(2)) |
                     (REG(-2) + REG(0) + REG(1) + REG(2)) |
                     (REG(-1) + REG(0) + REG(1) + REG(2)));
         }
     };

     /// %Test with simple regular expression
     class RegSimpleC : public Test {
     public:
       /// Create and register test
       RegSimpleC(void) : Test("Extensional::Reg::Simple::C",6,0,1) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         int pos = 0;
         int s = x.size();

         while (pos < s && x[pos] == 0) ++pos;
         if (pos + 4 > s) return false;

         for (int i = 0; i < 2; ++i, ++pos)
           if (x[pos] != 1) return false;
         if (pos + 2 > s) return false;

         for (int i = 0; i < 1; ++i, ++pos)
           if (x[pos] != 0) return false;
         while (pos < s && x[pos] == 0) ++pos;
         if (pos + 1 > s) return false;

         for (int i = 0; i < 1; ++i, ++pos)
           if (x[pos] != 1) return false;
         while (pos < s) if (x[pos++] != 0) return false;
         return true;

       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         extensional(home, x,
                     *REG(0) + REG(1)(2,2) + +REG(0) + REG(1)(1,1) + *REG(0));
       }
     };

     /// %Test with regular expression for distinct constraint
     class RegDistinct : public Test {
     public:
       /// Create and register test
       RegDistinct(void) : Test("Extensional::Reg::Distinct",4,-1,4) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         for (int i=0; i<x.size(); i++) {
           if ((x[i] < 0) || (x[i] > 3))
             return false;
           for (int j=i+1; j<x.size(); j++)
             if (x[i]==x[j])
               return false;
         }
         return true;
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         extensional(home, x,
                     (REG(0)+REG(1)+REG(2)+REG(3)) |
                     (REG(0)+REG(1)+REG(3)+REG(2)) |
                     (REG(0)+REG(2)+REG(1)+REG(3)) |
                     (REG(0)+REG(2)+REG(3)+REG(1)) |
                     (REG(0)+REG(3)+REG(1)+REG(2)) |
                     (REG(0)+REG(3)+REG(2)+REG(1)) |
                     (REG(1)+REG(0)+REG(2)+REG(3)) |
                     (REG(1)+REG(0)+REG(3)+REG(2)) |
                     (REG(1)+REG(2)+REG(0)+REG(3)) |
                     (REG(1)+REG(2)+REG(3)+REG(0)) |
                     (REG(1)+REG(3)+REG(0)+REG(2)) |
                     (REG(1)+REG(3)+REG(2)+REG(0)) |
                     (REG(2)+REG(0)+REG(1)+REG(3)) |
                     (REG(2)+REG(0)+REG(3)+REG(1)) |
                     (REG(2)+REG(1)+REG(0)+REG(3)) |
                     (REG(2)+REG(1)+REG(3)+REG(0)) |
                     (REG(2)+REG(3)+REG(0)+REG(1)) |
                     (REG(2)+REG(3)+REG(1)+REG(0)) |
                     (REG(3)+REG(0)+REG(1)+REG(2)) |
                     (REG(3)+REG(0)+REG(2)+REG(1)) |
                     (REG(3)+REG(1)+REG(0)+REG(2)) |
                     (REG(3)+REG(1)+REG(2)+REG(0)) |
                     (REG(3)+REG(2)+REG(0)+REG(1)) |
                     (REG(3)+REG(2)+REG(1)+REG(0)));
       }
     };

     /// %Test with simple regular expression from Roland Yap
     class RegRoland : public Test {
     public:
       /// Create and register test
       RegRoland(int n)
         : Test("Extensional::Reg::Roland::"+str(n),n,0,1) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         int n = x.size();
         return
           ((n > 1) && (x[n-2] == 0)) ||
           ((n > 0) && (x[n-1] == 0));
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         REG r0(0), r1(1);
         REG r01 = r0 | r1;
         extensional(home, x, *r01 + r0 + r01(0,1));
       }
     };

     /// %Test with simple regular expression and shared variables (uses unsharing)
     class RegSharedA : public Test {
     public:
       /// Create and register test
       RegSharedA(void) : Test("Extensional::Reg::Shared::A",4,2,2) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         return (((x[0] == 0) || (x[0] == 2)) &&
                 ((x[1] == -1) || (x[1] == 1)) &&
                 ((x[2] == 0) || (x[2] == 1)) &&
                 ((x[3] == 0) || (x[3] == 1)));
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         IntVarArgs y(8);
         for (int i=0; i<4; i++)
           y[i]=y[i+4]=x[i];
         unshare(home,y);
         extensional(home, y,
                     ((REG(0) | REG(2)) +
                      (REG(-1) | REG(1)) +
                      (REG(7) | REG(0) | REG(1)) +
                      (REG(0) | REG(1)))(2,2));
       }
     };

     /// %Test with simple regular expression and shared variables (uses unsharing)
     class RegSharedB : public Test {
     public:
       /// Create and register test
       RegSharedB(void) : Test("Extensional::Reg::Shared::B",4,2,2) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         return (((x[0] == 0) || (x[0] == 2)) &&
                 ((x[1] == -1) || (x[1] == 1)) &&
                 ((x[2] == 0) || (x[2] == 1)) &&
                 ((x[3] == 0) || (x[3] == 1)));
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         IntVarArgs y(12);
         for (int i=0; i<4; i++)
           y[i]=y[i+4]=y[i+8]=x[i];
         unshare(home,y);
         extensional(home, y,
                     ((REG(0) | REG(2)) +
                      (REG(-1) | REG(1)) +
                      (REG(7) | REG(0) | REG(1)) +
                      (REG(0) | REG(1)))(3,3));
       }
     };

     /// %Test with simple regular expression and shared variables (uses unsharing)
     class RegSharedC : public Test {
     public:
       /// Create and register test
       RegSharedC(void) : Test("Extensional::Reg::Shared::C",4,0,1) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         return (x[1]==1) && (x[2]==0) && (x[3]==1);
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         Gecode::BoolVarArgs y(8);
         for (int i=0; i<4; i++)
           y[i]=y[i+4]=channel(home,x[i]);
         unshare(home,y);
         extensional(home,y,
                     ((REG(0) | REG(1)) + REG(1) + REG(0) + REG(1))(2,2));
       }
     };

     /// %Test with simple regular expression and shared variables (uses unsharing)
     class RegSharedD : public Test {
     public:
       /// Create and register test
       RegSharedD(void) : Test("Extensional::Reg::Shared::D",4,0,1) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         return (x[1]==1) && (x[2]==0) && (x[3]==1);
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         Gecode::BoolVarArgs y(12);
         for (int i=0; i<4; i++)
           y[i]=y[i+4]=y[i+8]=channel(home,x[i]);
         unshare(home, y);
         extensional(home, y,
                     ((REG(0) | REG(1)) + REG(1) + REG(0) + REG(1))(3,3));
       }
     };

     /// %Test for empty DFA
     class RegEmptyDFA : public Test {
     public:
       /// Create and register test
       RegEmptyDFA(void) : Test("Extensional::Reg::Empty::DFA",1,0,0) {
         testsearch = false;
       }
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         (void)x;
         return false;
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         Gecode::DFA d;
         Gecode::extensional(home, x, d);
       }
     };

     /// %Test for empty regular expression
     class RegEmptyREG : public Test {
     public:
       /// Create and register test
       RegEmptyREG(void) : Test("Extensional::Reg::Empty::REG",1,0,0) {
         testsearch = false;
       }
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         (void)x;
         return false;
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         Gecode::REG r;
         Gecode::extensional(home, x, r);
       }
     };

     /// %Test for optimizations
     class RegOpt : public Test {
     protected:
       /// DFA size characteristic
       int n;
     public:
       /// Create and register test
       RegOpt(int n0)
         : Test("Extensional::Reg::Opt::"+str(n0),1,0,15), n(n0) {}
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         return (x[0] < n) && ((x[0] & 1) == 0);
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         DFA::Transition* t = new DFA::Transition[n+1];
         DFA::Transition* ti = t;
         int* f = new int[n+1];
         int* fi = f;
         for (int i=0; i<n; i++) {
           ti->i_state = 0;
           ti->symbol  = i;
           ti->o_state = i+1;
           ti++;
           if ((i & 1) == 0) {
             *fi = i+1; fi++;
           }
         }
         ti->i_state = -1;
         *fi = -1;
         DFA d(0, t, f, false);
         delete [] t;
         delete [] f;
         extensional(home, x, d);
       }

     };

     ///% Transform a TupleSet into a DFA
     Gecode::DFA tupleset2dfa(Gecode::TupleSet ts) {
       return ts.dfa();
     }

     /// %Test with tuple set
     class TupleSetBase : public Test {
     protected:
       /// Simple test tupleset
       Gecode::TupleSet t;
       /// Whether the table is positive or negative
       bool pos;
       /// Dense/sparse posting mode
       Gecode::ExtensionalPropKind epk;
     public:
       /// Create and register test
       TupleSetBase(bool p, Gecode::ExtensionalPropKind epk0)
         : Test("Extensional::TupleSet::" + extensional_kind_name(epk0) +
                "::" + str(p) + "::Base",
                4,1,5,true,Gecode::IPL_DOM),
           t(4), pos(p), epk(epk0) {
         using namespace Gecode;
         IntArgs t1({2, 1, 2, 4});
         IntArgs t2({2, 2, 1, 4});
         IntArgs t3({4, 3, 4, 1});
         IntArgs t4({1, 3, 2, 3});
         IntArgs t5({3, 3, 3, 2});
         t.add(t1).add(t1).add(t2).add(t2)
          .add(t3).add(t3).add(t4).add(t4)
          .add(t5).add(t5).add(t5).add(t5)
          .add(t5).add(t5).add(t5).add(t5)
          .add(t1).add(t1).add(t2).add(t2)
          .add(t3).add(t3).add(t4).add(t4)
          .add(t5).add(t5).add(t5).add(t5)
          .add(t5).add(t5).add(t5).add(t5)
          .finalize(epk);
       }
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         return pos == ((x[0] == 1 && x[1] == 3 && x[2] == 2 && x[3] == 3) ||
                        (x[0] == 2 && x[1] == 1 && x[2] == 2 && x[3] == 4) ||
                        (x[0] == 2 && x[1] == 2 && x[2] == 1 && x[3] == 4) ||
                        (x[0] == 3 && x[1] == 3 && x[2] == 3 && x[3] == 2) ||
                        (x[0] == 4 && x[1] == 3 && x[2] == 4 && x[3] == 1));
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         TupleSet ts = TupleSet(t.arity(),tupleset2dfa(t));
         assert(t == ts);
         extensional(home, x, t, pos, ipl, epk);
       }
       /// Post reified constraint on \a x for \a r
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x,
                         Gecode::Reify r) {
         extensional(home, x, t, pos, r, ipl, epk);
       }
     };

     /// %Test with tuple set
     class TupleSetTest : public Test {
     protected:
       /// Whether the table is positive or negative
       bool pos;
       /// Dense/sparse posting mode
       Gecode::ExtensionalPropKind epk;
       /// The tuple set to use
       Gecode::TupleSet ts;
       /// Whether to validate dfa2tupleset
       bool toDFA;
     public:
       /// Create and register test
       TupleSetTest(const std::string& s, bool p,
                    Gecode::IntSet d0, Gecode::TupleSet ts0, bool td,
                    Gecode::ExtensionalPropKind epk0)
         : Test("Extensional::TupleSet::" + extensional_kind_name(epk0) +
                "::" + str(p) + "::" + s,
                ts0.arity(),d0,true,Gecode::IPL_DOM),
           pos(p), epk(epk0), ts(ts0), toDFA(td) {
       }
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         using namespace Gecode;
         for (int i=ts.tuples(); i--; ) {
           TupleSet::Tuple t = ts[i];
           bool same = true;
           for (int j=0; (j < ts.arity()) && same; j++)
             if (t[j] != x[j])
               same = false;
           if (same)
             return pos;
         }
         return !pos;
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         if (toDFA) {
           TupleSet t = TupleSet(ts.arity(),tupleset2dfa(ts));
           assert(ts == t);
         }
         extensional(home, x, ts, pos, ipl, epk);
       }
       /// Post reified constraint on \a x for \a r
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x,
                         Gecode::Reify r) {
         using namespace Gecode;
         extensional(home, x, ts, pos, r, ipl, epk);
       }
     };

     class RandomTupleSetTest : public TupleSetTest {
     public:
       /// Create and register test
       RandomTupleSetTest(const std::string& s, bool p,
                          Gecode::IntSet d0, Gecode::TupleSet ts0,
                          Gecode::ExtensionalPropKind epk0)
         : TupleSetTest(s,p,d0,ts0,false,epk0) {
         testsearch = false;
       }
       /// Create and register initial assignment
       virtual Assignment* assignment(void) const {
         using namespace Gecode;
         return new RandomAssignment(arity, dom, 1000, _rand);
       }
     };

     /// Sparse fallback smoke test for very low-density unary tuplesets
     class SparseTupleSetFallback : public ::Test::Base {
     public:
       SparseTupleSetFallback(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::UnaryFallback") {}

       virtual bool run(void) {
         using namespace Gecode;

         const int n = 50000;
         TupleSet ts(1);
         for (int i=0; i<n; i++)
           ts.add(IntArgs({i}));
         ts.finalize(EPK_SPARSE);

         if (!ts.sparse_support()) {
           std::cerr << "ERROR: TupleSet did not select sparse support"
                     << std::endl;
           return false;
         }

         TupleSet rt(1, ts.dfa());
         if (!(ts == rt)) {
           std::cerr << "ERROR: TupleSet::dfa() round-trip failed"
                     << std::endl;
           return false;
         }

         class SparseUnarySpace : public Space {
         public:
           IntVarArray x;
           SparseUnarySpace(const TupleSet& t, int n0)
             : x(*this,1,0,n0-1) {
             extensional(*this, x, t, true, IPL_DOM, EPK_SPARSE);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           SparseUnarySpace(SparseUnarySpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space*
           copy(void) {
             return new SparseUnarySpace(*this);
           }
         };

         SparseUnarySpace* root = new SparseUnarySpace(ts,n);
         DFS<SparseUnarySpace> e(root);
         delete root;

         SparseUnarySpace* sol = e.next();
         if (sol == nullptr)
           return false;
         const bool ok = sol->x[0].assigned() && (sol->x[0].val() == 0);
         delete sol;
         return ok;
       }
     };

     /// Sparse fallback smoke test for low-density ternary tuplesets
     class SparseTupleSetFallbackTernary : public ::Test::Base {
     public:
       SparseTupleSetFallbackTernary(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::TernaryFallback") {}

       virtual bool run(void) {
         using namespace Gecode;

         const int n = 30000;
         TupleSet ts(3);
         for (int i=0; i<n; i++)
           ts.add(IntArgs({i, (i*7) % n, (i*11) % n}));
         ts.finalize(EPK_SPARSE);

         if (!ts.sparse_support()) {
           std::cerr << "ERROR: Ternary TupleSet did not select sparse support"
                     << std::endl;
           return false;
         }

         class SparseTernarySpace : public Space {
         public:
           IntVarArray x;
           SparseTernarySpace(const TupleSet& t, int n0)
             : x(*this,3,0,n0-1) {
             extensional(*this, x, t, true, IPL_DOM, EPK_SPARSE);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           SparseTernarySpace(SparseTernarySpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space*
           copy(void) {
             return new SparseTernarySpace(*this);
           }
         };

         SparseTernarySpace* root = new SparseTernarySpace(ts,n);
         DFS<SparseTernarySpace> e(root);
         delete root;

         SparseTernarySpace* sol = e.next();
         if (sol == nullptr)
           return false;
         const int a = sol->x[0].val();
         const int b = sol->x[1].val();
         const int c = sol->x[2].val();
         delete sol;
         return (b == ((a*7) % n)) && (c == ((a*11) % n));
       }
     };

     /// Sparse fallback smoke test for low-density higher-arity tuplesets
     class SparseTupleSetFallbackHighArity : public ::Test::Base {
     public:
       SparseTupleSetFallbackHighArity(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::HighArityFallback") {}

       virtual bool run(void) {
         using namespace Gecode;

         const int n = 20000;
         TupleSet ts(6);
         for (int i=0; i<n; i++)
           ts.add(IntArgs({i, (i*3) % n, (i*5) % n,
                           (i*7) % n, (i*11) % n, (i*13) % n}));
         ts.finalize(EPK_SPARSE);

         if (!ts.sparse_support()) {
           std::cerr << "ERROR: High-arity TupleSet did not select sparse support"
                     << std::endl;
           return false;
         }

         class SparseHighAritySpace : public Space {
         public:
           IntVarArray x;
           SparseHighAritySpace(const TupleSet& t, int n0)
             : x(*this,6,0,n0-1) {
             extensional(*this, x, t, true, IPL_DOM, EPK_SPARSE);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           SparseHighAritySpace(SparseHighAritySpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space*
           copy(void) {
             return new SparseHighAritySpace(*this);
           }
         };

         SparseHighAritySpace* root = new SparseHighAritySpace(ts,n);
         DFS<SparseHighAritySpace> e(root);
         delete root;

         SparseHighAritySpace* sol = e.next();
         if (sol == nullptr)
           return false;
         const int a = sol->x[0].val();
         const int b = sol->x[1].val();
         const int c = sol->x[2].val();
         const int d = sol->x[3].val();
         const int e0 = sol->x[4].val();
         const int f = sol->x[5].val();
         delete sol;
         return (b == ((a*3) % n)) &&
                (c == ((a*5) % n)) &&
                (d == ((a*7) % n)) &&
                (e0 == ((a*11) % n)) &&
                (f == ((a*13) % n));
       }
     };

     /// Sparse fallback smoke test for nullary tuplesets
     class SparseTupleSetFallbackNullary : public ::Test::Base {
     public:
       SparseTupleSetFallbackNullary(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::NullaryFallback") {}

       virtual bool run(void) {
         using namespace Gecode;

         class SparseNullarySpace : public Space {
         public:
           IntVarArray x;
           SparseNullarySpace(const TupleSet& t)
             : x(*this,0,0,0) {
             extensional(*this, x, t, true, IPL_DOM, EPK_SPARSE);
           }
           SparseNullarySpace(SparseNullarySpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space*
           copy(void) {
             return new SparseNullarySpace(*this);
           }
         };

         TupleSet sat(0);
         sat.add(IntArgs(0));
         sat.finalize(EPK_SPARSE);
         if (!sat.sparse_support()) {
           std::cerr << "ERROR: Nullary sat table not sparse" << std::endl;
           return false;
         }
         SparseNullarySpace* sat_root = new SparseNullarySpace(sat);
         DFS<SparseNullarySpace> sat_engine(sat_root);
         delete sat_root;
         SparseNullarySpace* sat_sol = sat_engine.next();
         if (sat_sol == nullptr) {
           std::cerr << "ERROR: Nullary sat table produced no solution"
                     << std::endl;
           return false;
         }
         delete sat_sol;

         TupleSet unsat(0);
         unsat.finalize(EPK_SPARSE);
         SparseNullarySpace* unsat_root = new SparseNullarySpace(unsat);
         DFS<SparseNullarySpace> unsat_engine(unsat_root);
         delete unsat_root;
         SparseNullarySpace* unsat_sol = unsat_engine.next();
         const bool ok = (unsat_sol == nullptr);
         if (!ok)
           std::cerr << "ERROR: Nullary empty table unexpectedly satisfiable"
                     << std::endl;
         delete unsat_sol;
         return ok;
       }
     };

     /// Sparse incremental smoke test for repeated and mixed delta updates
     class SparseTupleSetIncrementalDelta : public ::Test::Base {
     public:
       SparseTupleSetIncrementalDelta(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::IncrementalDelta") {}

       virtual bool run(void) {
         using namespace Gecode;

         class SparseDeltaSpace : public Space {
         public:
           IntVarArray x;
           SparseDeltaSpace(const TupleSet& t)
             : x(*this,2,0,3) {
             extensional(*this, x, t, true, IPL_DOM, EPK_SPARSE);
             rel(*this, x[0], IRT_NQ, 0);
             rel(*this, x[0], IRT_NQ, 1);
             rel(*this, x[1], IRT_NQ, 3);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           SparseDeltaSpace(SparseDeltaSpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space*
           copy(void) {
             return new SparseDeltaSpace(*this);
           }
         };

         TupleSet ts(2);
         ts.add(IntArgs({0,0})).add(IntArgs({1,1}))
           .add(IntArgs({2,2})).add(IntArgs({3,3}));
         ts.finalize(EPK_SPARSE);
         if (!ts.sparse_support())
           return false;

         SparseDeltaSpace* root = new SparseDeltaSpace(ts);
         DFS<SparseDeltaSpace> e(root);
         delete root;

         SparseDeltaSpace* sol = e.next();
         if (sol == nullptr)
           return false;
         SparseDeltaSpace* extra = e.next();
         const bool ok = sol->x[0].assigned() && sol->x[1].assigned() &&
                         (sol->x[0].val() == 2) && (sol->x[1].val() == 2) &&
                         (extra == nullptr);
         delete sol;
         delete extra;
         return ok;
       }
     };

     /// Sparse incremental test for assigned-variable advisor updates
     class SparseTupleSetIncrementalAssign : public ::Test::Base {
     public:
       SparseTupleSetIncrementalAssign(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::IncrementalAssign") {}

       virtual bool run(void) {
         using namespace Gecode;

         class SparseAssignSpace : public Space {
         public:
           IntVarArray x;
           SparseAssignSpace(const TupleSet& t)
             : x(*this,2,0,3) {
             extensional(*this, x, t, true, IPL_DOM, EPK_SPARSE);
             rel(*this, x[0], IRT_EQ, 2);
             rel(*this, x[1], IRT_NQ, 1);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           SparseAssignSpace(SparseAssignSpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space*
           copy(void) {
             return new SparseAssignSpace(*this);
           }
         };

         TupleSet ts(2);
         ts.add(IntArgs({0,0})).add(IntArgs({1,1}))
           .add(IntArgs({2,1})).add(IntArgs({3,3}));
         ts.finalize(EPK_SPARSE);
         if (!ts.sparse_support())
           return false;

         SparseAssignSpace* root = new SparseAssignSpace(ts);
         DFS<SparseAssignSpace> e(root);
         delete root;
         SparseAssignSpace* sol = e.next();
         const bool ok = (sol == nullptr);
         delete sol;
         return ok;
       }
     };

     /// Sparse incremental test for BoolView specialization
     class SparseTupleSetIncrementalBool : public ::Test::Base {
     public:
       SparseTupleSetIncrementalBool(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::IncrementalBool") {}

       virtual bool run(void) {
         using namespace Gecode;

         class SparseBoolSpace : public Space {
         public:
           BoolVarArray x;
           SparseBoolSpace(const TupleSet& t)
             : x(*this,2,0,1) {
             extensional(*this, x, t, true, IPL_DOM, EPK_SPARSE);
             rel(*this, x[0], IRT_NQ, 0);
             branch(*this, x, BOOL_VAR_NONE(), BOOL_VAL_MIN());
           }
           SparseBoolSpace(SparseBoolSpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space*
           copy(void) {
             return new SparseBoolSpace(*this);
           }
         };

         TupleSet ts(2);
         ts.add(IntArgs({0,1})).add(IntArgs({1,0}));
         ts.finalize(EPK_SPARSE);
         if (!ts.sparse_support())
           return false;

         SparseBoolSpace* root = new SparseBoolSpace(ts);
         DFS<SparseBoolSpace> e(root);
         delete root;

         SparseBoolSpace* sol = e.next();
         if (sol == nullptr)
           return false;
         SparseBoolSpace* extra = e.next();
         const bool ok = sol->x[0].assigned() && sol->x[1].assigned() &&
                         (sol->x[0].val() == 1) && (sol->x[1].val() == 0) &&
                         (extra == nullptr);
         delete sol;
         delete extra;
         return ok;
       }
     };

     /// Sparse posting fallback smoke test for negative tuple-set posting
     class SparseTupleSetNegativeFallback : public ::Test::Base {
     public:
       SparseTupleSetNegativeFallback(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::NegativeFallback") {}

       virtual bool run(void) {
         using namespace Gecode;

         class SparseNegativeSpace : public Space {
         public:
           IntVarArray x;
           SparseNegativeSpace(const TupleSet& t)
             : x(*this,2,0,1) {
             extensional(*this, x, t, false, IPL_DOM, EPK_SPARSE);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           SparseNegativeSpace(SparseNegativeSpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space*
           copy(void) {
             return new SparseNegativeSpace(*this);
           }
         };

         TupleSet ts(2);
         ts.add(IntArgs({0,0})).add(IntArgs({1,1}));
         ts.finalize(EPK_SPARSE);

         SparseNegativeSpace* root = new SparseNegativeSpace(ts);
         DFS<SparseNegativeSpace> e(root);
         delete root;

         int n = 0;
         while (SparseNegativeSpace* sol = e.next()) {
           if (sol->x[0].val() == sol->x[1].val()) {
             delete sol;
             return false;
           }
           n++;
           delete sol;
         }
         return n == 2;
       }
     };

     /// Sparse posting fallback smoke test for reified tuple-set posting
     class SparseTupleSetReifiedFallback : public ::Test::Base {
     public:
       SparseTupleSetReifiedFallback(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::ReifiedFallback") {}

       virtual bool run(void) {
         using namespace Gecode;

         class SparseReifiedSpace : public Space {
         public:
           IntVarArray x;
           BoolVar b;
           SparseReifiedSpace(const TupleSet& t)
             : x(*this,2,0,1), b(*this,0,1) {
             extensional(*this, x, t, true, Reify(b,RM_EQV), IPL_DOM,
                         EPK_SPARSE);
             rel(*this, b, IRT_EQ, 1);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           SparseReifiedSpace(SparseReifiedSpace& s)
             : Space(s) {
             x.update(*this,s.x);
             b.update(*this,s.b);
           }
           virtual Space*
           copy(void) {
             return new SparseReifiedSpace(*this);
           }
         };

         TupleSet ts(2);
         ts.add(IntArgs({0,0})).add(IntArgs({1,1}));
         ts.finalize(EPK_SPARSE);

         SparseReifiedSpace* root = new SparseReifiedSpace(ts);
         DFS<SparseReifiedSpace> e(root);
         delete root;

         int n = 0;
         while (SparseReifiedSpace* sol = e.next()) {
           if (sol->x[0].val() != sol->x[1].val()) {
             delete sol;
             return false;
           }
           n++;
           delete sol;
         }
         return n == 2;
       }
     };

     /// Sparse/compressed tuplesets should materialize only requested support
     class SparseTupleSetSingleRepresentation : public ::Test::Base {
     public:
       SparseTupleSetSingleRepresentation(void)
         : ::Test::Base("Extensional::TupleSet::Support::SingleRepresentation") {}

       virtual bool run(void) {
         using namespace Gecode;
         TupleSet ts(2);
         for (int i=0; i<100; i++)
           ts.add(IntArgs({i, (i*3) % 100}));
         ts.finalize(EPK_SPARSE);
         if (!ts.sparse_support()) {
           std::cerr << "ERROR: Sparse support not available" << std::endl;
           return false;
         }
         if (ts.dense_support()) {
           std::cerr << "ERROR: Dense support unexpectedly materialized"
                     << std::endl;
           return false;
         }
         if (ts.dense_compressed_support()) {
           std::cerr << "ERROR: Compressed support unexpectedly materialized"
                     << std::endl;
           return false;
         }

         TupleSet tc(2);
         for (int i=0; i<100; i++)
           tc.add(IntArgs({i, (i*7) % 100}));
         tc.finalize(EPK_DENSE_COMPRESSED);
         if (!tc.dense_compressed_support()) {
           std::cerr << "ERROR: Compressed support not available" << std::endl;
           return false;
         }
         if (tc.dense_support()) {
           std::cerr << "ERROR: Dense support unexpectedly materialized"
                     << std::endl;
           return false;
         }
         if (tc.sparse_support()) {
           std::cerr << "ERROR: Sparse support unexpectedly materialized"
                     << std::endl;
           return false;
         }

         TupleSet td(2);
         for (int i=0; i<100; i++)
           td.add(IntArgs({i, (i*11) % 100}));
         td.finalize();
         if (!td.dense_support()) {
           std::cerr << "ERROR: Default finalize did not keep dense support"
                     << std::endl;
           return false;
         }
         return true;
       }
     };

     /// Sparse negative should fail if all combinations are forbidden
     class SparseTupleSetNegativeFail : public ::Test::Base {
     public:
       SparseTupleSetNegativeFail(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::NegativeFail") {}

       virtual bool run(void) {
         using namespace Gecode;
         class NegativeFailSpace : public Space {
         public:
           IntVarArray x;
           NegativeFailSpace(const TupleSet& t)
             : x(*this,2,0,1) {
             extensional(*this, x, t, false, IPL_DOM, EPK_SPARSE);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           NegativeFailSpace(NegativeFailSpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space* copy(void) {
             return new NegativeFailSpace(*this);
           }
         };
         TupleSet ts(2);
         ts.add(IntArgs({0,0})).add(IntArgs({0,1}))
           .add(IntArgs({1,0})).add(IntArgs({1,1}));
         ts.finalize(EPK_SPARSE);
         if (ts.dense_support())
           return false;
         NegativeFailSpace* root = new NegativeFailSpace(ts);
         DFS<NegativeFailSpace> e(root);
         delete root;
         NegativeFailSpace* sol = e.next();
         const bool ok = (sol == nullptr);
         delete sol;
         return ok;
       }
     };

     /// Sparse negative should prune values whose completions are all forbidden
     class SparseTupleSetNegativePrune : public ::Test::Base {
     public:
       SparseTupleSetNegativePrune(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::NegativePrune") {}

       virtual bool run(void) {
         using namespace Gecode;
         class NegativePruneSpace : public Space {
         public:
           IntVarArray x;
           NegativePruneSpace(const TupleSet& t)
             : x(*this,2,0,1) {
             extensional(*this, x, t, false, IPL_DOM, EPK_SPARSE);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           NegativePruneSpace(NegativePruneSpace& s)
             : Space(s) {
             x.update(*this,s.x);
           }
           virtual Space* copy(void) {
             return new NegativePruneSpace(*this);
           }
         };
         TupleSet ts(2);
         ts.add(IntArgs({0,0})).add(IntArgs({0,1}));
         ts.finalize(EPK_SPARSE);
         if (ts.dense_support())
           return false;
         NegativePruneSpace* root = new NegativePruneSpace(ts);
         DFS<NegativePruneSpace> e(root);
         delete root;

         int n = 0;
         while (NegativePruneSpace* sol = e.next()) {
           if (sol->x[0].val() != 1) {
             delete sol;
             return false;
           }
           n++;
           delete sol;
         }
         return n == 2;
       }
     };

     /// Sparse reified posting should support all reify modes for positive/negative
     class SparseTupleSetReifiedModes : public ::Test::Base {
     public:
       SparseTupleSetReifiedModes(void)
         : ::Test::Base("Extensional::TupleSet::Sparse::ReifiedModes") {}

       virtual bool run(void) {
         using namespace Gecode;
         class ReifModeSpace : public Space {
         public:
           IntVarArray x;
           BoolVar b;
           ReifModeSpace(const TupleSet& t, bool pos, ReifyMode rm, int bv)
             : x(*this,2,0,1), b(*this,0,1) {
             extensional(*this, x, t, pos, Reify(b,rm), IPL_DOM, EPK_SPARSE);
             rel(*this, b, IRT_EQ, bv);
             branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
           }
           ReifModeSpace(ReifModeSpace& s)
             : Space(s) {
             x.update(*this,s.x);
             b.update(*this,s.b);
           }
           virtual Space* copy(void) {
             return new ReifModeSpace(*this);
           }
         };

         TupleSet ts(2);
         ts.add(IntArgs({0,0}));
         ts.finalize(EPK_SPARSE);
         if (ts.dense_support())
           return false;

         auto count = [&ts](bool pos, ReifyMode rm, int bv,
                            bool must_equal) {
           ReifModeSpace* root = new ReifModeSpace(ts,pos,rm,bv);
           DFS<ReifModeSpace> e(root);
           delete root;
           int n = 0;
           while (ReifModeSpace* sol = e.next()) {
             const bool eq = (sol->x[0].val() == 0) && (sol->x[1].val() == 0);
             if (must_equal != eq) {
               delete sol;
               return -1;
             }
             n++;
             delete sol;
           }
           return n;
         };

         if (count(true,RM_EQV,1,true) != 1)
           return false;
         if (count(true,RM_EQV,0,false) != 3)
           return false;
         if (count(true,RM_IMP,1,true) != 1)
           return false;
         if (count(true,RM_PMI,0,false) != 3)
           return false;
         if (count(false,RM_EQV,0,true) != 1)
           return false;
         if (count(false,RM_EQV,1,false) != 3)
           return false;

         return true;
       }
     };

     /// %Test with large tuple set
     class TupleSetLarge : public Test {
     protected:
       /// Whether the table is positive or negative
       bool pos;
       /// Dense/sparse posting mode
       Gecode::ExtensionalPropKind epk;
       /// Tupleset used for testing
       mutable Gecode::TupleSet t;
     public:
       /// Create and register test
       TupleSetLarge(double prob, bool p, Gecode::ExtensionalPropKind epk0)
         : Test("Extensional::TupleSet::" + extensional_kind_name(epk0) +
                "::" + str(p) + "::Large",
                5,1,5,true,Gecode::IPL_DOM),
           pos(p), epk(epk0), t(5) {
         using namespace Gecode;

         CpltAssignment ass(5, IntSet(1, 5));
         while (ass.has_more()) {
           if (_rand(100) <= prob*100) {
             IntArgs tuple(5);
             for (int i = 5; i--; ) tuple[i] = ass[i];
             t.add(tuple);
           }
           ass.next(_rand);
         }
         t.finalize(epk);
       }
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         using namespace Gecode;
         for (int i = 0; i < t.tuples(); ++i) {
           TupleSet::Tuple l = t[i];
           bool same = true;
           for (int j = 0; j < t.arity() && same; ++j)
             if (l[j] != x[j]) same = false;
           if (same)
             return pos;
         }
         return !pos;
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         extensional(home, x, t, pos, ipl, epk);
       }
       /// Post reified constraint on \a x for \a r
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x,
                         Gecode::Reify r) {
         using namespace Gecode;
         extensional(home, x, t, pos, r, ipl, epk);
       }
     };

     /// %Test with bool tuple set
     class TupleSetBool : public Test {
     protected:
       /// Whether the table is positive or negative
       bool pos;
       /// Dense/sparse posting mode
       Gecode::ExtensionalPropKind epk;
       /// Tupleset used for testing
       mutable Gecode::TupleSet t;
     public:
       /// Create and register test
       TupleSetBool(double prob, bool p, Gecode::ExtensionalPropKind epk0)
         : Test("Extensional::TupleSet::" + extensional_kind_name(epk0) +
                "::" + str(p) + "::Bool",
                5,0,1,true), pos(p), epk(epk0), t(5) {
         using namespace Gecode;

         CpltAssignment ass(5, IntSet(0, 1));
         while (ass.has_more()) {
           if (_rand(100) <= prob*100) {
             IntArgs tuple(5);
             for (int i = 5; i--; ) tuple[i] = ass[i];
             t.add(tuple);
           }
           ass.next(_rand);
         }
         t.finalize(epk);
       }
       /// %Test whether \a x is solution
       virtual bool solution(const Assignment& x) const {
         using namespace Gecode;
         for (int i = 0; i < t.tuples(); ++i) {
           TupleSet::Tuple l = t[i];
           bool same = true;
           for (int j = 0; j < t.arity() && same; ++j)
             if (l[j] != x[j])
               same = false;
           if (same)
             return pos;
         }
         return !pos;
       }
       /// Post constraint on \a x
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x) {
         using namespace Gecode;
         BoolVarArgs y(x.size());
         for (int i = x.size(); i--; )
           y[i] = channel(home, x[i]);
         extensional(home, y, t, pos, ipl, epk);
       }
       /// Post reified constraint on \a x for \a r
       virtual void post(Gecode::Space& home, Gecode::IntVarArray& x,
                         Gecode::Reify r) {
         using namespace Gecode;
         BoolVarArgs y(x.size());
         for (int i = x.size(); i--; )
           y[i] = channel(home, x[i]);
         extensional(home, y, t, pos, r, ipl, epk);
       }
     };

     /// Help class to create and register tests with a fixed table size
     class TupleSetTestSize {
     public:
       /// Perform creation and registration
       TupleSetTestSize(int size, bool pos, Gecode::ExtensionalPropKind epk,
                        Gecode::Support::RandomGenerator& rand) {
         using namespace Gecode;
         /// Find the arity needed for creating sufficient number of tuples
         int arity = 2;
         int n_tuples = 5*5;
         while (n_tuples < size) {
           arity++;
           n_tuples*=5;
         }
         /// Build TupleSet
         TupleSet ts(arity);
         CpltAssignment ass(arity, IntSet(0, 4));
         for (int i = size; i--; ) {
           assert(ass.has_more());
           IntArgs tuple(arity);
           for (int j = arity; j--; ) tuple[j] = ass[j];
           ts.add(tuple);
           ass.next(rand);
         }
         ts.finalize(epk);
         assert(ts.tuples() == size);
         // Create and register test
         (void) new TupleSetTest(std::to_string(size),pos,IntSet(0,4),ts,
                                 size <= 128, epk);
       }
     };

     Gecode::TupleSet randomTupleSet(int n, int min, int max, double prob,
                                     Gecode::ExtensionalPropKind epk,
                                     Gecode::Support::RandomGenerator& rand) {
       using namespace Gecode;
       TupleSet t(n);
       CpltAssignment ass(n, IntSet(min, max));
       while (ass.has_more()) {
         if (rand(100) <= prob*100) {
           IntArgs tuple(n);
           for (int i = n; i--; ) tuple[i] = ass[i];
           t.add(tuple);
         }
         ass.next(rand);
       }
       t.finalize(epk);
       return t;
     }

     /// Help class to create and register tests
     class Create {
     public:
       /// Perform creation and registration
       Create(void) {
         // This code is executed on load, and thus a random number generator source from the supplied
         // seed is not available.
         // In order to get interesting data her, but still have deterministic and repeatable execution, a fixed seed
         // is used for a local random number generator.
         // TODO: Make this code later on test run, and use the supplied seed/random number generator.
         Gecode::Support::RandomGenerator rand(42);

         using namespace Gecode;
         for (ExtensionalPropKind epk :
                { EPK_DENSE, EPK_SPARSE, EPK_DENSE_COMPRESSED }) {
           for (bool pos : { false, true }) {
           {
             TupleSet ts(4);
             ts.add({2, 1, 2, 4}).add({2, 2, 1, 4})
               .add({4, 3, 4, 1}).add({1, 3, 2, 3})
               .add({3, 3, 3, 2}).add({5, 1, 4, 4})
               .add({2, 5, 1, 5}).add({4, 3, 5, 1})
               .add({1, 5, 2, 5}).add({5, 3, 3, 2})
               .finalize(epk);
             (void) new TupleSetTest("A",pos,IntSet(0,6),ts,true,epk);
           }
           {
             TupleSet ts(4);
             ts.finalize(epk);
             (void) new TupleSetTest("Empty",pos,IntSet(1,2),ts,true,epk);
           }
           {
             TupleSet ts(4);
             for (int n=1024*16; n--; )
               ts.add({1,2,3,4});
             ts.finalize(epk);
             (void) new TupleSetTest("Assigned",pos,IntSet(1,4),ts,true,epk);
           }
           {
             TupleSet ts(1);
             ts.add({1}).add({2}).add({3}).finalize(epk);
             (void) new TupleSetTest("Single",pos,IntSet(-4,4),ts,true,epk);
           }
           {
             int m = Gecode::Int::Limits::min;
             TupleSet ts(3);
             ts.add({m+0,m+1,m+2}).add({m+4,m+1,m+3})
               .add({m+2,m+3,m+0}).add({m+2,m+3,m+0})
               .add({m+1,m+2,m+5}).add({m+2,m+3,m+0})
               .add({m+3,m+6,m+5}).finalize(epk);
             (void) new TupleSetTest("Min",pos,IntSet(m,m+7),ts,true,epk);
           }
           {
             int M = Gecode::Int::Limits::max;
             TupleSet ts(3);
             ts.add({M-0,M-1,M-2}).add({M-4,M-1,M-3})
               .add({M-2,M-3,M-0}).add({M-2,M-3,M-0})
               .add({M-1,M-2,M-5}).add({M-2,M-3,M-0})
               .add({M-3,M-6,M-5}).finalize(epk);
             (void) new TupleSetTest("Max",pos,IntSet(M-7,M),ts,true,epk);
           }
           {
             int m = Gecode::Int::Limits::min;
             int M = Gecode::Int::Limits::max;
             TupleSet ts(3);
             ts.add({M-0,m+1,M-2}).add({m+4,M-1,M-3})
               .add({m+2,M-3,m+0}).add({M-2,M-3,M-0})
               .finalize(epk);
             (void) new TupleSetTest("MinMax",pos,
                                     IntSet(IntArgs({m,m+1,m+4,M-3,M-2,M})),
                                     ts,true,epk);
           }
           {
             TupleSet ts(7);
             const int triangle_tuples = (epk == EPK_DENSE) ? 10000 : 2000;
             for (int i = 0; i < triangle_tuples; i++) {
               IntArgs tuple(7);
               for (int j = 0; j < 7; j++) {
                 tuple[j] = rand(j+1);
               }
               ts.add(tuple);
             }
             ts.finalize(epk);
             (void) new RandomTupleSetTest("Triangle",pos,IntSet(0,6),ts,epk);
           }
           {
             for (int i = 0; i <= 64*6; i+=32)
               (void) new TupleSetTestSize(i, pos, epk, rand);
           }
           {
             const double prob_small = (epk == EPK_DENSE) ? 0.05 : 0.01;
             const double prob_large = (epk == EPK_DENSE) ? 0.05 : 0.005;
             (void) new RandomTupleSetTest("Rand(10,-1,2)", pos,
                                           IntSet(-1,2),
                                           randomTupleSet(10, -1, 2, prob_small,
                                                          epk, rand),
                                           epk);
             (void) new RandomTupleSetTest("Rand(5,-10,10)", pos,
                                           IntSet(-10,10),
                                           randomTupleSet(5, -10, 10, prob_large,
                                                          epk, rand),
                                           epk);
           }
           {
             TupleSet t(5);
             CpltAssignment ass(4, IntSet(1, 4));
             while (ass.has_more()) {
               IntArgs tuple(5);
               tuple[4] = 1;
               for (int i = 4; i--; ) tuple[i] = ass[i];
               t.add(tuple);
               ass.next(rand);
             }
             t.add({2,2,4,3,4});
             t.finalize(epk);
             (void) new TupleSetTest("FewLast",pos,IntSet(1,4),t,false,epk);
           }
           {
             TupleSet t(4);
             CpltAssignment ass(4, IntSet(1, 6));
             while (ass.has_more()) {
               t.add({ass[0],0,ass[1],ass[2]});
               ass.next(rand);
             }
             t.add({2,-1,3,4});
             t.finalize(epk);
             (void) new TupleSetTest("FewMiddle",pos,IntSet(-1,6),t,false,epk);
           }
           {
             TupleSet t(10);
             CpltAssignment ass(9, IntSet(1, 4));
             while (ass.has_more()) {
               if (rand(100) <= 0.25*100) {
                 IntArgs tuple(10);
                 tuple[0] = 2;
                 for (int i = 9; i--; ) tuple[i+1] = ass[i];
                 t.add(tuple);
               }
               ass.next(rand);
             }
             t.add({1,1,1,1,1,1,1,1,1,1});
             t.add({1,2,3,4,4,2,1,2,3,3});
             t.finalize(epk);
             (void) new RandomTupleSetTest("FewHuge",pos,IntSet(1,4),t,epk);
           }
           (void) new TupleSetBase(pos,epk);
           (void) new TupleSetLarge(0.05,pos,epk);
           (void) new TupleSetBool(0.3,pos,epk);
           }
         }
       }
     };

     Create c;

     RegSimpleA ra;
     RegSimpleB rb;
     RegSimpleC rc;

     RegDistinct rd;

     RegRoland rr1(1);
     RegRoland rr2(2);
     RegRoland rr3(3);
     RegRoland rr4(4);

     RegSharedA rsa;
     RegSharedB rsb;
     RegSharedC rsc;
     RegSharedD rsd;

     RegEmptyDFA redfa;
     RegEmptyREG rereg;

     RegOpt ro0(CHAR_MAX-1);
     RegOpt ro1(CHAR_MAX);
     RegOpt ro2(static_cast<int>(UCHAR_MAX-1));
     RegOpt ro3(static_cast<int>(UCHAR_MAX));
     RegOpt ro4(SHRT_MAX-1);
     RegOpt ro5(SHRT_MAX);
     RegOpt ro6(static_cast<int>(USHRT_MAX-1));
     RegOpt ro7(static_cast<int>(USHRT_MAX));

     SparseTupleSetFallback sparse_tuple_set_fallback;
     SparseTupleSetFallbackTernary sparse_tuple_set_fallback_ternary;
     SparseTupleSetFallbackHighArity sparse_tuple_set_fallback_high_arity;
     SparseTupleSetFallbackNullary sparse_tuple_set_fallback_nullary;
     SparseTupleSetIncrementalDelta sparse_tuple_set_incremental_delta;
     SparseTupleSetIncrementalAssign sparse_tuple_set_incremental_assign;
     SparseTupleSetIncrementalBool sparse_tuple_set_incremental_bool;
     SparseTupleSetNegativeFallback sparse_tuple_set_negative_fallback;
     SparseTupleSetReifiedFallback sparse_tuple_set_reified_fallback;
     SparseTupleSetSingleRepresentation sparse_tuple_set_single_representation;
     SparseTupleSetNegativeFail sparse_tuple_set_negative_fail;
     SparseTupleSetNegativePrune sparse_tuple_set_negative_prune;
     SparseTupleSetReifiedModes sparse_tuple_set_reified_modes;
     //@}

   }
}}


// STATISTICS: test-int
