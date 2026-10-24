/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *  http://www.gecode.org
 */

#include "test/string.hh"

#include <memory>

namespace Test { namespace String { namespace Find {

  class FirstA : public Test {
  private:
    int index;

    static std::string name(int index0) {
      return (index0 == 0) ? "Absent" : "First";
    }

  public:
    FirstA(int index0)
      : Test("Find::" + name(index0), 1, "ab", 2), index(index0) {}

    virtual bool solution(const Assignment& assignment) const {
      const std::string::size_type position = assignment[0].find('a');
      return (position == std::string::npos) ? index == 0 :
        static_cast<int>(position) + 1 == index;
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x) {
      Gecode::find(home, Gecode::StringVar(home, "a"), x[0],
                   Gecode::IntVar(home, index, index));
    }
  };

  FirstA absent(0);
  FirstA first(1);

  class LastA : public Test {
  private:
    int index;

    static std::string name(int index0) {
      return (index0 == 0) ? "Absent" : "Last";
    }

  public:
    LastA(int index0)
      : Test("RFind::" + name(index0), 1, "ab", 2), index(index0) {}

    virtual bool solution(const Assignment& assignment) const {
      const std::string::size_type position = assignment[0].rfind('a');
      return (position == std::string::npos) ? index == 0 :
        static_cast<int>(position) + 1 == index;
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x) {
      Gecode::rfind(home, Gecode::StringVar(home, "a"), x[0],
                     Gecode::IntVar(home, index, index));
    }
  };

  LastA rabsent(0);
  LastA last(2);

  class LastAB : public Test {
  private:
    int index;

    static std::string name(int index0) {
      return (index0 == 0) ? "Absent" : "Last";
    }

  public:
    LastAB(int index0)
      : Test("RFind::AB::" + name(index0), 1, "ab", 3), index(index0) {}

    virtual bool solution(const Assignment& assignment) const {
      const std::string::size_type position = assignment[0].rfind("ab");
      return (position == std::string::npos) ? index == 0 :
        static_cast<int>(position) + 1 == index;
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x) {
      Gecode::rfind(home, Gecode::StringVar(home, "ab"), x[0],
                     Gecode::IntVar(home, index, index));
    }
  };

  LastAB rabsent_ab(0);
  LastAB last_ab(2);

  class ReverseSpace : public Gecode::Space {
  public:
    Gecode::StringVar pattern;
    Gecode::StringVar source;
    Gecode::IntVar index;

    ReverseSpace(const std::string& pattern_value,
                 const std::string& source_value, bool assigned)
      : pattern(assigned ? Gecode::StringVar(*this, pattern_value) :
          Gecode::StringVar(*this, Gecode::String::NSIntSet('a', 'b'), 0, 2)),
        source(assigned ? Gecode::StringVar(*this, source_value) :
          Gecode::StringVar(*this, Gecode::String::NSIntSet('a', 'b'), 0, 3)),
        index(*this, 0, 4) {
      Gecode::rfind(*this, pattern, source, index);
      if (!assigned) {
        Gecode::rel(*this, pattern, Gecode::STRT_EQ,
                    Gecode::StringVar(*this, pattern_value));
        Gecode::rel(*this, source, Gecode::STRT_EQ,
                    Gecode::StringVar(*this, source_value));
      }
      Gecode::branch(*this, index, Gecode::INT_VAL_MIN());
    }

    ReverseSpace(ReverseSpace& s) : Gecode::Space(s) {
      pattern.update(*this, s.pattern);
      source.update(*this, s.source);
      index.update(*this, s.index);
    }

    virtual Gecode::Space* copy(void) {
      return new ReverseSpace(*this);
    }
  };

  class ReverseBoundaries : public Base {
  private:
    static int last_position(const std::string& pattern,
                             const std::string& source) {
      int last = 0;
      for (int offset = 0; offset <= static_cast<int>(source.size()); ++offset)
        if (source.compare(offset, pattern.size(), pattern) == 0)
          last = offset + 1;
      return last;
    }

    static bool matches(const std::string& pattern,
                        const std::string& source, bool assigned) {
      ReverseSpace root(pattern, source, assigned);
      Gecode::Search::Options options;
      options.threads = 1;
      Gecode::DFS<ReverseSpace> search(&root, options);
      std::unique_ptr<ReverseSpace> solution(search.next());
      if (!solution || !solution->index.assigned() ||
          solution->index.val() != last_position(pattern, source))
        return false;
      std::unique_ptr<ReverseSpace> extra(search.next());
      return !extra;
    }

  public:
    ReverseBoundaries(void) : Base("String::RFind::Boundaries") {}

    virtual bool run(void) {
      for (Assignment pattern(1, "ab", 2); pattern(); ++pattern)
        for (Assignment source(1, "ab", 3); source(); ++source)
          for (bool assigned : {false, true})
            if (!matches(pattern[0], source[0], assigned))
              return false;
      return true;
    }
  };

  ReverseBoundaries reverse_boundaries;

}}}

// STATISTICS: test-string
