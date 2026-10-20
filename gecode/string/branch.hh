#ifndef __GECODE_STRING_BRANCH_HH__
#define __GECODE_STRING_BRANCH_HH__

#include <gecode/string.hh>

/**
 * \namespace Gecode::String::Branch
 * \brief String branchers
 */

namespace Gecode { namespace String { namespace Branch {

  class StringBrancher : public Brancher {

  protected:

    ViewArray<StringView> x;
    mutable int start;

    class PosLevVal : public Choice {
    public:

      unsigned pos;
      Level lev;
      Value val;
      int split;
      int index;
      int length;
      NSIntSet alphabet;
      NSIntSet chars;
      StringVal fixed;

      PosLevVal(
        const StringBrancher& b, unsigned p, Level l, Value v, int s
      ):
        Choice(b, 2), pos(p), lev(l), val(v), split(s), index(-1), length(-1),
        alphabet(), chars(), fixed() {}

      PosLevVal(
        const StringBrancher& b, unsigned p, Value v, int s, int i, int n,
        NSIntSet&& a, NSIntSet&& c
      ):
        Choice(b, 2), pos(p), lev(Level::BASE), val(v), split(s), index(i),
        length(n), alphabet(std::move(a)), chars(std::move(c)), fixed() {}

      PosLevVal(
        const StringBrancher& b, unsigned p, StringVal&& value
      ):
        Choice(b, 2), pos(p), lev(Level::BASE), val(Value::MIN), split(0),
        index(-1), length(value.size()), alphabet(), chars(),
        fixed(std::move(value)) {}

      virtual size_t
      size(void) const {
        return sizeof(*this) +
          (alphabet.length() + chars.length()) * sizeof(NSRange) +
          fixed.size() * sizeof(StringSymbol);
      }

      virtual void
      archive(Archive& e) const {
        Choice::archive(e);
        e << pos << lev << val << split << index << length;
        e << alphabet.length();
        for (NSRange* range = alphabet.first(); range; range = range->next)
          e << range->l << range->u;
        e << chars.length();
        for (NSRange* range = chars.first(); range; range = range->next)
          e << range->l << range->u;
        e << static_cast<int>(fixed.size());
        for (StringVal::size_type i = 0; i < fixed.size(); ++i)
          e << fixed[i];
      }

    };

    PosLevVal*
    decision(
      int pos, const Gecode::String::DashedString* p, Level lev, Value val,
      const NSIntSet* preferred = nullptr
    ) {
      if (lev == Level::LENGTH) {
        int split = val == Value::MIN ? p->min_length() : p->max_length();
        return new PosLevVal(*this, pos, lev, val, split);
      }
      assert(lev == Level::BASE);
      assert(p->min_length() == p->max_length());
      int index = -1;
      NSIntSet chars;
      std::vector<StringSymbol> concrete;
      concrete.reserve(p->min_length());
      for (int i = 0; i < p->min_length(); ++i) {
        NSIntSet position_chars(p->chars_at(i));
        assert(!position_chars.empty());
        if (index < 0 && position_chars.size() > 1) {
          index = i;
          chars = position_chars;
        }
        concrete.push_back(position_chars.min());
      }
      if (index < 0)
        return new PosLevVal(
          *this, pos, StringVal::from_symbols(std::move(concrete)));
      int split;
      switch (val) {
        case Value::MIN:
          split = chars.min();
          break;
        case Value::MAX:
          split = chars.max();
          break;
        case Value::MUSTMIN:
        case Value::MUSTMAX: {
          NSIntSet must;
          if (preferred != nullptr) {
            must = *preferred;
            must.intersect(chars);
          }
          if (must.empty())
            split = val == Value::MUSTMIN ?
              chars.min() : chars.max();
          else
            split = val == Value::MUSTMIN ? must.min() : must.max();
          break;
        }
        default:
          GECODE_NEVER;
      }
      return new PosLevVal(
        *this, pos, val, split, index, p->min_length(),
        p->may_chars(), std::move(chars));
    }

    ExecStatus
    commit_choice(Space& home, const PosLevVal& p, unsigned a) {
      ModEvent me;
      if (p.lev == Level::LENGTH) {
        if (p.val == Value::MIN) {
          me = a == 0 ?
            x[p.pos].ub(home, p.split) :
            x[p.pos].lb(home, p.split + 1);
        } else {
          me = a == 0 ?
            x[p.pos].lb(home, p.split) :
            x[p.pos].ub(home, p.split - 1);
        }
      } else {
        assert(p.lev == Level::BASE);
        if (p.index < 0) {
          if (a != 0)
            return ES_FAILED;
          me = x[p.pos].eq(home, p.fixed);
          return me_failed(me) ? ES_FAILED : ES_OK;
        }
        NSIntSet selected(p.chars);
        if (a == 0)
          selected = NSIntSet(p.split);
        else
          selected.remove(p.split);
        if (selected.empty())
          return ES_FAILED;
        NSBlocks restriction;
        if (p.index > 0)
          restriction.emplace_back(p.alphabet, p.index, p.index);
        restriction.emplace_back(std::move(selected), 1, 1);
        if (p.index + 1 < p.length)
          restriction.emplace_back(
            p.alphabet, p.length - p.index - 1, p.length - p.index - 1);
        me = x[p.pos].dom(home, restriction);
        return me_failed(me) ? ES_FAILED : ES_OK;
      }
      return me_failed(me) ? ES_FAILED : ES_OK;
    }

    Choice*
    val_llll(int pos, const Gecode::String::DashedString* p) {
      if (p->min_length() < p->max_length())
        return decision(pos, p, Level::LENGTH, Value::MIN);
      return decision(pos, p, Level::BASE, Value::MIN);
    }

    Choice*
    val_llul(int pos, const Gecode::String::DashedString* p) {
      if (p->min_length() < p->max_length())
        return decision(pos, p, Level::LENGTH, Value::MIN);
      return decision(pos, p, Level::BASE, Value::MIN);
    }

    Choice*
    val_ulul(int pos, const Gecode::String::DashedString* p) {
      if (p->min_length() < p->max_length())
        return decision(pos, p, Level::LENGTH, Value::MAX);
      return decision(pos, p, Level::BASE, Value::MAX);
    }

    Choice*
    val_lllm(
      int pos, const Gecode::String::DashedString* p,
      const NSIntSet& preferred
    ) {
      if (p->min_length() < p->max_length())
        return decision(pos, p, Level::LENGTH, Value::MIN);
      return decision(pos, p, Level::BASE, Value::MUSTMIN, &preferred);
    }

  public:

    StringBrancher(Home home, ViewArray<String::StringView>& x0):
      Brancher(home), x(x0), start(0) {};

    StringBrancher(Space& home, StringBrancher& b):
      Brancher(home, b), start(b.start) {
      x.update(home, b.x);
    };

    bool
    status(const Space&) const {
      for (int i = this-> start; i < x.size(); ++i)
        if (!x[i].assigned()) {
          start = i;
          return true;
        }
      return false;
    }

    Choice*
    choice(const Space&, Archive& e) {
      int pos, lev, val, split, index, length, range_count;
      e >> pos >> lev >> val >> split >> index >> length >> range_count;
      NSIntSet alphabet;
      for (int i = 0; i < range_count; ++i) {
        int lower, upper;
        e >> lower >> upper;
        alphabet.include(NSIntSet(lower, upper));
      }
      e >> range_count;
      NSIntSet chars;
      for (int i = 0; i < range_count; ++i) {
        int lower, upper;
        e >> lower >> upper;
        chars.include(NSIntSet(lower, upper));
      }
      int value_size;
      e >> value_size;
      std::vector<StringSymbol> symbols;
      symbols.reserve(value_size);
      for (int i = 0; i < value_size; ++i) {
        int symbol;
        e >> symbol;
        symbols.push_back(symbol);
      }
      if (Level(lev) == Level::LENGTH)
        return new PosLevVal(
          *this, pos, Level(lev), Value(val), split);
      if (index < 0)
        return new PosLevVal(
          *this, pos, StringVal::from_symbols(std::move(symbols)));
      return new PosLevVal(
        *this, pos, Value(val), split, index, length,
        std::move(alphabet), std::move(chars));
    }

    virtual void
    print(const Space&, const Choice& c, unsigned a, std::ostream& o) const {
      const PosLevVal& p = static_cast<const PosLevVal&>(c);
      o << "Alt. " << a << " -- Var. " << p.pos << " -- Lev. " << p.lev
        << " -- Val. " << p.val << " of " << x[p.pos].domain();
    }

  };

}}}

namespace Gecode { namespace String { namespace Branch {

  struct None_LLLL: public StringBrancher {

    None_LLLL(Home home, ViewArray<String::StringView>& x0);

    None_LLLL(Home home, None_LLLL& b);

    Actor* copy(Space& home);

    static void post(Home home, ViewArray<String::StringView>& x);

    Choice* choice(Space&);

    ExecStatus commit(Space& home, const Choice& c, unsigned a);

  };

}}}
#include <gecode/string/branch/none_llll.hpp>

namespace Gecode { namespace String { namespace Branch {

  struct LenMin_LLLL: public StringBrancher {

    LenMin_LLLL(Home home, ViewArray<String::StringView>& x0);

    LenMin_LLLL(Home home, LenMin_LLLL& b);

    Actor* copy(Space& home);

    static void post(Home home, ViewArray<String::StringView>& x);

    Choice* choice(Space&);

    ExecStatus commit(Space& home, const Choice& c, unsigned a);

  };

}}}
#include <gecode/string/branch/lenmin_llll.hpp>

namespace Gecode { namespace String { namespace Branch {

  struct LenMax_LLLL: public StringBrancher {

    LenMax_LLLL(Home home, ViewArray<String::StringView>& x0);

    LenMax_LLLL(Home home, LenMax_LLLL& b);

    Actor* copy(Space& home);

    static void post(Home home, ViewArray<String::StringView>& x);

    Choice* choice(Space&);

    ExecStatus commit(Space& home, const Choice& c, unsigned a);

  };

}}}
#include <gecode/string/branch/lenmax_llll.hpp>

namespace Gecode { namespace String { namespace Branch {

  struct SizeMin_LLUL: public StringBrancher {

    SizeMin_LLUL(Home home, ViewArray<String::StringView>& x0);

    SizeMin_LLUL(Home home, SizeMin_LLUL& b);

    Actor* copy(Space& home);

    static void post(Home home, ViewArray<String::StringView>& x);

    Choice* choice(Space&);

    ExecStatus commit(Space& home, const Choice& c, unsigned a);

  };

}}}
#include <gecode/string/branch/sizemin_llul.hpp>

namespace Gecode { namespace String { namespace Branch {

  struct BlockMin_LLLL: public StringBrancher {

    BlockMin_LLLL(Home home, ViewArray<String::StringView>& x0);

    BlockMin_LLLL(Home home, BlockMin_LLLL& b);

    Actor* copy(Space& home);

    static void post(Home home, ViewArray<String::StringView>& x);

    Choice* choice(Space&);

    ExecStatus commit(Space& home, const Choice& c, unsigned a);

  };

}}}
#include <gecode/string/branch/blockmin_llll.hpp>

namespace Gecode { namespace String { namespace Branch {

  struct BlockMin_LLLM: public StringBrancher {

    BlockMin_LLLM(Home home, ViewArray<String::StringView>& x0);

    BlockMin_LLLM(Home home, BlockMin_LLLM& b);

    Actor* copy(Space& home);

    static void post(Home home, ViewArray<String::StringView>& x);

    Choice* choice(Space&);

    ExecStatus commit(Space& home, const Choice& c, unsigned a);

  };

}}}
#include <gecode/string/branch/blockmin_lllm.hpp>

namespace Gecode { namespace String { namespace Branch {

  struct LenBlockMin_LLLM: public StringBrancher {

    LenBlockMin_LLLM(Home home, ViewArray<String::StringView>& x0);

    LenBlockMin_LLLM(Home home, LenBlockMin_LLLM& b);

    Actor* copy(Space& home);

    static void post(Home home, ViewArray<String::StringView>& x);

    Choice* choice(Space&);

    ExecStatus commit(Space& home, const Choice& c, unsigned a);

  };

}}}
#include <gecode/string/branch/lenblockmin_lllm.hpp>

#endif

// STATISTICS: string-branch
