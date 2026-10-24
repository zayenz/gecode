/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
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
 */

#include "test/string.hh"

namespace Test { namespace String { namespace Extensional {

  /// Test regular-language membership and its reified variants
  class Regular : public Test {
  public:
    enum Language {
      EVEN_B,
      A_STAR_B
    };

  private:
    Language language;

    Gecode::DFA automaton(void) const {
      using Gecode::DFA;
      switch (language) {
      case EVEN_B:
        return DFA(0, {
          DFA::Transition(0, 'a', 0),
          DFA::Transition(0, 'b', 1),
          DFA::Transition(1, 'a', 1),
          DFA::Transition(1, 'b', 0)
        }, {0});
      case A_STAR_B:
        return DFA(0, {
          DFA::Transition(0, 'a', 0),
          DFA::Transition(0, 'b', 1)
        }, {1});
      default:
        GECODE_NEVER;
      }
      return DFA();
    }

  public:
    Regular(const std::string& name, Language language0)
      : Test("Extensional::" + name, 1, "ab", 3, true),
        language(language0) {}

    virtual bool solution(const Assignment& a) const {
      const std::string& x = a[0];
      switch (language) {
      case EVEN_B: {
        unsigned int count = 0;
        for (std::string::const_iterator c = x.begin(); c != x.end(); ++c)
          if (*c == 'b')
            ++count;
        return (count % 2) == 0;
      }
      case A_STAR_B:
        if (x.empty() || (x[x.size() - 1] != 'b'))
          return false;
        for (std::string::size_type i = 0; i + 1 < x.size(); ++i)
          if (x[i] != 'a')
            return false;
        return true;
      default:
        return false;
      }
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x) {
      Gecode::extensional(home, x[0], automaton());
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x,
                      Gecode::Reify r) {
      Gecode::DFA d = automaton();
      Gecode::extensional(home, x[0], d, r.var(), r.mode());
    }
  };

  Regular even_b("EvenB", Regular::EVEN_B);
  Regular a_star_b("AStarB", Regular::A_STAR_B);

  /// Space for the DFA reification truth table
  class DfaReificationSpace : public Gecode::Space {
  public:
    Gecode::StringVar x;
    Gecode::BoolVar b;

    DfaReificationSpace(int symbol, int control, Gecode::ReifyMode mode,
                        bool assigned)
      : b(*this, control < 0 ? 0 : control, control < 0 ? 1 : control) {
      using namespace Gecode;
      const Gecode::String::StringVal value =
        Gecode::String::StringVal::from_symbols({symbol});
      if (assigned) {
        x = StringVar(*this, value);
      } else {
        Gecode::String::NSIntSet alphabet('a');
        alphabet.add('b');
        alphabet.add('c');
        alphabet.add(0x100);
        alphabet.add(0x101);
        alphabet.add(0x102);
        x = StringVar(*this, alphabet, 1, 1);
      }
      const DFA d(0, {
        DFA::Transition(0, 'a', 1),
        DFA::Transition(0, 'c', 0),
        DFA::Transition(0, 0x100, 1),
        DFA::Transition(0, 0x102, 0)
      }, {1});
      extensional(*this, x, d, b, mode);
      if (!assigned)
        rel(*this, x, STRT_EQ, StringVar(*this, value));
    }

    DfaReificationSpace(DfaReificationSpace& s) : Gecode::Space(s) {
      x.update(*this, s.x);
      b.update(*this, s.b);
    }

    virtual Gecode::Space* copy(void) {
      return new DfaReificationSpace(*this);
    }
  };

  /// Check both assignment orders, including the byte and scalar fast paths
  class DfaReification : public Base {
  private:
    bool assigned;
  public:
    DfaReification(bool assigned0)
      : Base(std::string("String::Extensional::DfaReification::") +
             (assigned0 ? "Assigned" : "Deferred")), assigned(assigned0) {}

    virtual bool run(void) {
      using namespace Gecode;
      const int symbols[] = {'a', 'b', 'c', 0x100, 0x101, 0x102};
      const ReifyMode modes[] = {RM_EQV, RM_IMP, RM_PMI};
      for (int symbol : symbols) {
        const bool accepted = (symbol == 'a') || (symbol == 0x100);
        for (ReifyMode mode : modes) {
          const bool allow_zero = (mode == RM_IMP) || !accepted;
          const bool allow_one = (mode == RM_PMI) || accepted;
          for (int control = -1; control <= 1; ++control) {
            DfaReificationSpace s(symbol, control, mode, assigned);
            const bool failed = s.status() == SS_FAILED;
            const bool contradiction =
              ((control == 0) && !allow_zero) ||
              ((control == 1) && !allow_one);
            const int min = control < 0 ? (allow_zero ? 0 : 1) : control;
            const int max = control < 0 ? (allow_one ? 1 : 0) : control;
            if ((failed != contradiction) ||
                (!failed && ((s.b.min() != min) || (s.b.max() != max)))) {
              if (opt.log)
                olog << "DFA reification: symbol=" << symbol
                     << " mode=" << mode << " control=" << control
                     << " assigned=" << assigned << std::endl;
              return false;
            }
          }
        }
      }
      return true;
    }
  };

  DfaReification dfa_reification_assigned(true);
  DfaReification dfa_reification_deferred(false);

  class Regex : public Test {
  public:
    Regex(void) : Test("Extensional::Regex", 1, "ab", 3, true) {}

    virtual bool solution(const Assignment& assignment) const {
      const std::string& value = assignment[0];
      if (value.empty() || (value[value.size() - 1] != 'b'))
        return false;
      for (std::string::size_type i = 0; i + 1 < value.size(); ++i)
        if (value[i] != 'a')
          return false;
      return true;
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x) {
      Gecode::extensional(home, x[0], "a*b");
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x,
                      Gecode::Reify r) {
      Gecode::extensional(home, x[0], "a*b", r.var(), r.mode());
    }
  };

  class FiniteLanguage : public Test {
  public:
    FiniteLanguage(void) : Test("Extensional::FiniteLanguage", 1, "ab", 2) {}

    virtual bool solution(const Assignment& assignment) const {
      const std::string& value = assignment[0];
      return (value == "") || (value == "a") || (value == "bb");
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x) {
      Gecode::extensional(home, x[0],
        std::vector<std::string>{"", "a", "bb"});
    }
  };

  Regex regex;
  FiniteLanguage finite_language;

}}}

// STATISTICS: test-string
