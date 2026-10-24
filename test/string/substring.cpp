/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *  http://www.gecode.org
 */

#include "test/string.hh"

#include <memory>
#include <sstream>

namespace Test { namespace String { namespace Substring {

  class Slice : public Test {
  private:
    int from;
    int to;

    static std::string name(int from0, int to0) {
      std::ostringstream os;
      os << "Slice::" << from0 << "To" << to0;
      return os.str();
    }

    static std::string slice(const std::string& source, int from, int to) {
      const int start = std::max(1, from);
      const int end = std::min(static_cast<int>(source.size()), to);
      return (end < start) ? "" : source.substr(start - 1, end - start + 1);
    }

  public:
    Slice(int from0, int to0)
      : Test("Substring::" + name(from0, to0), 2, "ab", 2),
        from(from0), to(to0) {}

    virtual bool solution(const Assignment& assignment) const {
      return assignment[1] == slice(assignment[0], from, to);
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x) {
      Gecode::IntVar first(home, from, from);
      Gecode::IntVar last(home, to, to);
      Gecode::substr(home, x[0], first, last, x[1]);
    }
  };

  /// Exercise both the concrete-source propagator and the fallback.
  class SliceSpace : public Gecode::Space {
  public:
    Gecode::StringVar source;
    Gecode::StringVar result;
    Gecode::IntVar from;
    Gecode::IntVar to;

    SliceSpace(const std::string& value, bool assigned)
      : source(assigned ? Gecode::StringVar(*this, value) :
          Gecode::StringVar(*this, Gecode::String::NSIntSet('a', 'b'),
                            static_cast<int>(value.size()),
                            static_cast<int>(value.size()))),
        result(*this, Gecode::String::NSIntSet('a', 'b'), 0, 3),
        from(*this, -1, 4), to(*this, -1, 4) {
      Gecode::substr(*this, source, from, to, result);
      Gecode::IntVarArgs indices;
      indices << from << to;
      Gecode::branch(*this, indices, Gecode::INT_VAR_NONE(),
                     Gecode::INT_VAL_MIN());
      Gecode::branch(*this, Gecode::StringVarArgs() << result,
                     Gecode::STRING_VAR_NONE(), Gecode::STRING_VAL_LLLL());
    }

    SliceSpace(SliceSpace& s) : Gecode::Space(s) {
      source.update(*this, s.source);
      result.update(*this, s.result);
      from.update(*this, s.from);
      to.update(*this, s.to);
    }

    virtual Gecode::Space* copy(void) {
      return new SliceSpace(*this);
    }
  };

  class ClippedBoundaries : public Base {
  private:
    static std::string slice(const std::string& source, int from, int to) {
      std::string result;
      for (int position = 1; position <= static_cast<int>(source.size());
           ++position)
        if (position >= from && position <= to)
          result += source[position - 1];
      return result;
    }

    static bool retains_empty_indices(void) {
      SliceSpace negative_end("aa", false);
      Gecode::rel(negative_end, negative_end.from, Gecode::IRT_EQ, 1);
      Gecode::dom(negative_end, negative_end.to, -1, 1);
      Gecode::rel(negative_end, negative_end.result, Gecode::STRT_EQ,
                  Gecode::StringVar(negative_end, ""));
      if (negative_end.status() == Gecode::SS_FAILED ||
          !negative_end.to.in(-1) || negative_end.to.in(1))
        return false;

      SliceSpace inverted("aaa", false);
      Gecode::dom(inverted, inverted.from, 1, 3);
      Gecode::rel(inverted, inverted.to, Gecode::IRT_EQ, 1);
      Gecode::rel(inverted, inverted.result, Gecode::STRT_EQ,
                  Gecode::StringVar(inverted, ""));
      return inverted.status() != Gecode::SS_FAILED &&
        inverted.from.in(3) && !inverted.from.in(1);
    }

    static bool matches(const std::string& source, bool assigned) {
      SliceSpace root(source, assigned);
      if (!assigned)
        Gecode::rel(root, root.source, Gecode::STRT_EQ,
                    Gecode::StringVar(root, source));
      Gecode::Search::Options options;
      options.threads = 1;
      Gecode::DFS<SliceSpace> search(&root, options);
      bool seen[6][6] = {};
      int count = 0;
      while (SliceSpace* solution = search.next()) {
        std::unique_ptr<SliceSpace> s(solution);
        if (!s->from.assigned() || !s->to.assigned() || !s->result.assigned())
          return false;
        const int from = s->from.val(), to = s->to.val();
        if (seen[from + 1][to + 1] || s->result.val() != slice(source, from, to))
          return false;
        seen[from + 1][to + 1] = true;
        ++count;
      }
      return count == 36;
    }

  public:
    ClippedBoundaries(void) : Base("String::Substring::ClippedBoundaries") {}

    virtual bool run(void) {
      if (!retains_empty_indices())
        return false;
      for (Assignment source(1, "ab", 3); source(); ++source)
        for (bool assigned : {false, true})
          if (!matches(source[0], assigned))
            return false;
      return true;
    }
  };

  ClippedBoundaries clipped_boundaries;

  Slice first_two(1, 2);
  Slice after_end(3, 4);

}}}

// STATISTICS: test-string
