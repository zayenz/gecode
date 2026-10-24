/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *  http://www.gecode.org
 */

#include "test/string.hh"

#include <set>
#include <utility>
#include <vector>

namespace Test { namespace String { namespace Replace {

  class Occurrence : public Test {
  public:
    enum Kind {
      ALL
    };

  private:
    Kind kind;

    static std::string name(Kind kind0) {
      switch (kind0) {
      case ALL: return "All";
      default:    GECODE_NEVER;
      }
      return "Unknown";
    }

    static std::string replace(const std::string& source, Kind kind) {
      std::string result(source);
      for (std::string::size_type position = result.find('a');
           position != std::string::npos;
           position = result.find('a', position + 1))
        result[position] = 'b';
      return result;
    }

  public:
    Occurrence(Kind kind0)
      : Test("Replace::" + name(kind0), 2, "ab", 3), kind(kind0) {}

    virtual bool solution(const Assignment& assignment) const {
      return assignment[1] == replace(assignment[0], kind);
    }

    virtual void post(Gecode::Space& home, Gecode::StringVarArray& x) {
      Gecode::StringVar query(home, "a");
      Gecode::StringVar replacement(home, "b");
      switch (kind) {
      case ALL:
        Gecode::replace_all(home, x[0], query, replacement, x[1]);
        break;
      default:
        GECODE_NEVER;
      }
    }
  };

  class AllProgressSpace : public Gecode::Space {
  public:
    Gecode::StringVar source;
    Gecode::StringVar query;
    Gecode::StringVar replacement;
    Gecode::StringVar result;

    AllProgressSpace(Gecode::String::StringSymbol symbol,
                     bool assigned_source, bool assigned_replacement)
      : source(*this, Gecode::String::NSIntSet(symbol),
               assigned_source ? 2 : 1, 2),
        query(*this, ""),
        replacement(*this, Gecode::String::NSIntSet('b'),
                    1, assigned_replacement ? 1 : 2) {
      Gecode::String::NSIntSet alphabet(symbol);
      alphabet.add('b');
      result = Gecode::StringVar(*this, alphabet, 3, 8);
      Gecode::replace_all(*this, source, query, replacement, result);
      Gecode::StringVarArgs operands;
      operands << source << query << replacement << result;
      Gecode::branch(*this, operands,
                     Gecode::STRING_VAR_NONE(), Gecode::STRING_VAL_LLLL());
    }

    AllProgressSpace(void)
      : source(*this, "ab"),
        query(*this, Gecode::String::NSIntSet('a', 'b'), 1, 1),
        replacement(*this, "x"), result(*this, "xb") {
      Gecode::replace_all(*this, source, query, replacement, result);
      Gecode::StringVarArgs operands;
      operands << source << query << replacement << result;
      Gecode::branch(*this, operands,
                     Gecode::STRING_VAR_NONE(), Gecode::STRING_VAL_LLLL());
    }

    AllProgressSpace(AllProgressSpace& s) : Gecode::Space(s) {
      source.update(*this, s.source);
      query.update(*this, s.query);
      replacement.update(*this, s.replacement);
      result.update(*this, s.result);
    }

    virtual Gecode::Space* copy(void) {
      return new AllProgressSpace(*this);
    }
  };

  class EmptyQuery : public Base {
  private:
    Gecode::String::StringSymbol symbol;
  public:
    EmptyQuery(bool scalar)
      : Base(scalar ? "String::Replace::All::EmptyQuery::Scalars"
                    : "String::Replace::All::EmptyQuery::Bytes"),
        symbol(scalar ? 0x100 : 'a') {}

    virtual bool run(void) {
      using Gecode::String::StringVal;
      const StringVal expected[2][2] = {
        {StringVal::from_symbols({'b', symbol, 'b'}),
         StringVal::from_symbols({'b', 'b', symbol, 'b', 'b'})},
        {StringVal::from_symbols({'b', symbol, 'b', symbol, 'b'}),
         StringVal::from_symbols({'b', 'b', symbol, 'b', 'b',
                                  symbol, 'b', 'b'})}
      };
      for (int assigned_source = 0; assigned_source <= 1; ++assigned_source)
        for (int assigned_replacement = 0;
             assigned_replacement <= 1; ++assigned_replacement) {
          AllProgressSpace root(symbol, assigned_source, assigned_replacement);
          Gecode::Search::Options options;
          options.threads = 1;
          Gecode::DFS<AllProgressSpace> search(&root, options);
          std::set<StringVal> actual;
          while (AllProgressSpace* solution = search.next()) {
            const int source_length = solution->source.min_length();
            const int replacement_length = solution->replacement.min_length();
            const bool assigned = solution->source.assigned() &&
              solution->replacement.assigned() && solution->result.assigned();
            bool valid = assigned &&
              (source_length >= 1) && (source_length <= 2) &&
              (replacement_length >= 1) && (replacement_length <= 2);
            if (valid) {
              const StringVal result = solution->result.val_symbols();
              valid = (result ==
                expected[source_length - 1][replacement_length - 1]) &&
                actual.insert(result).second;
            }
            delete solution;
            if (!valid)
              return false;
          }
          std::set<StringVal> expected_results;
          for (int source_length = assigned_source ? 2 : 1;
               source_length <= 2; ++source_length)
            for (int replacement_length = 1;
                 replacement_length <= (assigned_replacement ? 1 : 2);
                 ++replacement_length)
              expected_results.insert(
                expected[source_length - 1][replacement_length - 1]);
          if (actual != expected_results)
            return false;
        }
      return true;
    }
  };

  class VariableQuery : public Base {
  public:
    VariableQuery(void) : Base("String::Replace::All::VariableQuery") {}

    virtual bool run(void) {
      using Gecode::String::StringVal;
      AllProgressSpace root;
      if ((root.status() == Gecode::SS_FAILED) ||
          !root.query.may_chars().in('a'))
        return false;
      Gecode::Search::Options options;
      options.threads = 1;
      Gecode::DFS<AllProgressSpace> search(&root, options);
      std::set<StringVal> queries;
      while (AllProgressSpace* solution = search.next()) {
        const bool valid = solution->query.assigned() &&
          queries.insert(solution->query.val_symbols()).second;
        delete solution;
        if (!valid)
          return false;
      }
      return queries == std::set<StringVal>({StringVal::from_symbols({'a'})});
    }
  };

  class OverlapSpace : public Gecode::Space {
  public:
    Gecode::StringVar source;
    Gecode::StringVar result;

    OverlapSpace(Gecode::String::StringSymbol symbol, bool last,
                 bool assigned_source)
      : source(*this, Gecode::String::NSIntSet(symbol),
               assigned_source ? 3 : 2, 3) {
      Gecode::String::NSIntSet alphabet(symbol);
      alphabet.add('b');
      result = Gecode::StringVar(*this, alphabet, 1, 2);
      Gecode::StringVar query(*this,
        Gecode::String::StringVal::from_symbols({symbol, symbol}));
      Gecode::StringVar replacement(*this, "b");
      if (last)
        Gecode::replace_last(*this, source, query, replacement, result);
      else
        Gecode::replace(*this, source, query, replacement, result);
      Gecode::StringVarArgs operands;
      operands << source << result;
      Gecode::branch(*this, operands,
                     Gecode::STRING_VAR_NONE(), Gecode::STRING_VAL_LLLL());
    }

    OverlapSpace(OverlapSpace& s) : Gecode::Space(s) {
      source.update(*this, s.source);
      result.update(*this, s.result);
    }

    virtual Gecode::Space* copy(void) {
      return new OverlapSpace(*this);
    }
  };

  class Overlap : public Base {
  private:
    bool last;

    static Gecode::String::StringVal
    replace(const Gecode::String::StringVal& source, bool last) {
      using Gecode::String::StringSymbol;
      using Gecode::String::StringVal;
      // The source repeats one symbol and the query contains two copies.
      std::vector<StringSymbol> result(source.begin(), source.end());
      const StringVal::size_type position = last ? source.size() - 2 : 0;
      result.erase(result.begin() + position, result.begin() + position + 2);
      result.insert(result.begin() + position, 'b');
      return StringVal::from_symbols(std::move(result));
    }

    bool check_solutions(Gecode::String::StringSymbol symbol,
                         bool assigned_source) const {
      using Gecode::String::StringSymbol;
      using Gecode::String::StringVal;
      typedef std::pair<StringVal, StringVal> Assignment;
      OverlapSpace root(symbol, last, assigned_source);
      if ((root.status() == Gecode::SS_FAILED) ||
          (root.source.assigned() != assigned_source))
        return false;
      Gecode::Search::Options options;
      options.threads = 1;
      Gecode::DFS<OverlapSpace> engine(&root, options);
      std::set<Assignment> actual;
      while (OverlapSpace* solution = engine.next()) {
        const bool valid = solution->source.assigned() &&
          solution->result.assigned() && actual.insert(Assignment(
            solution->source.val_symbols(),
            solution->result.val_symbols())).second;
        delete solution;
        if (!valid)
          return false;
      }
      std::set<Assignment> expected;
      for (int length = assigned_source ? 3 : 2; length <= 3; ++length) {
        const StringVal source = StringVal::from_symbols(
          std::vector<StringSymbol>(length, symbol));
        expected.insert(Assignment(source, replace(source, last)));
      }
      return actual == expected;
    }

    bool check_assignment(Gecode::String::StringSymbol symbol,
                          bool assigned_source, bool correct_result) const {
      using Gecode::String::StringVal;
      OverlapSpace root(symbol, last, assigned_source);
      if ((root.status() == Gecode::SS_FAILED) ||
          (root.source.assigned() != assigned_source))
        return false;
      const StringVal source = StringVal::from_symbols({symbol, symbol, symbol});
      const StringVal result = replace(source, correct_result ? last : !last);
      Gecode::rel(root, root.source, Gecode::STRT_EQ,
                  Gecode::StringVar(root, source));
      Gecode::rel(root, root.result, Gecode::STRT_EQ,
                  Gecode::StringVar(root, result));
      const bool failed = root.status() == Gecode::SS_FAILED;
      return correct_result
        ? !failed && root.result.assigned() &&
          (root.result.val_symbols() == result)
        : failed;
    }

  public:
    Overlap(bool last0)
      : Base(last0 ? "String::Replace::Last::Overlap"
                   : "String::Replace::First::Overlap"), last(last0) {}

    virtual bool run(void) {
      const Gecode::String::StringSymbol symbols[] = {'a', 0x100};
      for (unsigned int i = 0; i < sizeof(symbols) / sizeof(symbols[0]); ++i)
        for (int assigned_source = 0; assigned_source <= 1; ++assigned_source)
          if (!check_solutions(symbols[i], assigned_source) ||
              !check_assignment(symbols[i], assigned_source, true) ||
              !check_assignment(symbols[i], assigned_source, false))
            return false;
      return true;
    }
  };

  Occurrence all(Occurrence::ALL);
  EmptyQuery empty_query_bytes(false);
  EmptyQuery empty_query_scalars(true);
  VariableQuery variable_query;
  Overlap first_overlap(false);
  Overlap last_overlap(true);

}}}

// STATISTICS: test-string
