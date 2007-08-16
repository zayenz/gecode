/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2005
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

#include "test/int.hh"
#include "test/log.hh"

#include "gecode/minimodel.hh"

namespace {

  IntSet ds_22(-2,2);
  IntSet ds_14(-1,4);

  DFA da = ((REG(0) | REG(2)) +
            (REG(-1) | REG(1)) +
            (REG(7) | REG(0) | REG(1)) +
            (REG(0) | REG(1)));

  class RegularA : public IntTest {
  public:
    RegularA(const char* t)
      : IntTest(t,4,ds_22,false,ICL_DOM) {}
    virtual bool solution(const Assignment& x) const {
      return (((x[0] == 0) || (x[0] == 2)) &&
              ((x[1] == -1) || (x[1] == 1)) &&
              ((x[2] == 0) || (x[2] == 1)) &&
              ((x[3] == 0) || (x[3] == 1)));
    }
    virtual void post(Space* home, IntVarArray& x) {
      regular(home, x, da);
    }
  };
  
  DFA db = ((REG(-2) + REG(-1) + REG(0) + REG(1)) |
            (REG(-2) + REG(-1) + REG(0) + REG(2)) |
            (REG(-2) + REG(-1) + REG(1) + REG(2)) |
            (REG(-2) + REG(0) + REG(1) + REG(2)) |
            (REG(-1) + REG(0) + REG(1) + REG(2)));

  class RegularB : public IntTest {
  public:
    RegularB(const char* t)
      : IntTest(t,4,ds_22,false,ICL_DOM) {}
    virtual bool solution(const Assignment& x) const {
      return (x[0]<x[1]) && (x[1]<x[2]) && (x[2]<x[3]);
    }
    virtual void post(Space* home, IntVarArray& x) {
      regular(home, x, db);
    }
  };
  
  DFA dd = ((REG(0)+REG(1)+REG(2)+REG(3)) |
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

  class RegularDistinct : public IntTest {
  public:
    RegularDistinct(const char* t)
      : IntTest(t,4,ds_14,false,ICL_DOM) {}
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
    virtual void post(Space* home, IntVarArray& x) {
      regular(home, x, dd);
    }
  };
  
  class RegularShared : public IntTest {
  public:
    RegularShared(const char* t)
      : IntTest(t,2,ds_22,false,ICL_DOM) {}
    virtual bool solution(const Assignment& x) const {
      return (((x[0] == 0) || (x[0] == 2)) &&
              ((x[1] == -1) || (x[1] == 1)) &&
              ((x[0] == 0) || (x[0] == 1)) &&
              ((x[1] == 0) || (x[1] == 1)));
    }
    virtual void post(Space* home, IntVarArray& x) {
      IntVarArgs y(4);
      y[0]=x[0]; y[1]=x[1]; y[2]=x[0]; y[3]=x[1];
      regular(home, y, da);
    }
    virtual void description(std::ostream& h, std::ostream& c) {
      h << "post regular: x[0]x[1]x[0]x[1] in (0|2)(-1|1)(7|0|1)(0|1)" << std::endl;
      c << "\tREG r = \n"
        << "\t  (REG(0) | REG(2)) +\n"
        << "\t  (REG(-1) | REG(1)) +\n"
        << "\t  (REG(7) | REG(0) | REG(1)) +\n"
        << "\t  (REG(0) | REG(1));\n"
        << "\tDFA d(r);\n"
        << "\tIntVarArgs y(4);\n"
        << "\ty[0]=x[0]; y[1]=x[1]; y[2]=x[0]; y[3]=x[1];\n"
        << "\tregular(home, y, d);\n" << std::endl;
    }
  };


  RegularA _rega("Regular::A");
  RegularB _regb("Regular::B");

  RegularDistinct _regd("Regular::Distinct");

  RegularShared _regs("Regular::Shared");

}

// STATISTICS: test-int

