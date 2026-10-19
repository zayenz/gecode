#include <gecode/string.hh>
#include <gecode/string/extensional.hh>
#include <gecode/string/extensional/parse-reg.hpp>

namespace Gecode { namespace String {

  trimDFA*
  RegEx::symbolic_dfa(const NSIntSet& active_alphabet) const {
    return symbolic_dfa(active_alphabet, false, false);
  }

  trimDFA*
  RegEx::symbolic_dfa(const NSIntSet& active_alphabet,
                      bool leading_wildcard, bool trailing_wildcard) const {
    std::vector<NSIntSet> sets;
    collect_symbol_sets(active_alphabet, sets);
    if (leading_wildcard || trailing_wildcard) {
      std::vector<NSIntSet> active_sets;
      active_sets.reserve(sets.size() + 1);
      for (const NSIntSet& set : sets) {
        NSIntSet overlap(set);
        overlap.intersect(active_alphabet);
        if (!overlap.empty())
          active_sets.push_back(std::move(overlap));
      }
      sets.swap(active_sets);
    }
    if ((leading_wildcard || trailing_wildcard) && !active_alphabet.empty())
      sets.push_back(active_alphabet);

    std::vector<NSIntSet> partitions;
    for (const NSIntSet& set : sets) {
      NSIntSet remaining(set);
      const unsigned int count = partitions.size();
      for (unsigned int i = 0; i < count; ++i) {
        NSIntSet overlap(partitions[i]);
        overlap.intersect(set);
        if (overlap.empty())
          continue;
        remaining.exclude(overlap);
        if (!(overlap == partitions[i])) {
          NSIntSet outside(partitions[i]);
          outside.exclude(set);
          partitions[i] = std::move(overlap);
          partitions.push_back(std::move(outside));
        }
      }
      if (!remaining.empty())
        partitions.push_back(std::move(remaining));
    }

    try {
      REG pattern = symbolic_reg(partitions, active_alphabet);
      if (leading_wildcard || trailing_wildcard) {
        REG sigma_star = *symbolic_class(active_alphabet, partitions);
        if (leading_wildcard && trailing_wildcard)
          return new trimDFA(
            DFA(sigma_star + pattern + sigma_star), partitions);
        if (leading_wildcard)
          return new trimDFA(DFA(sigma_star + pattern), partitions);
        return new trimDFA(DFA(pattern + sigma_star), partitions);
      }
      return new trimDFA(DFA(pattern), partitions);
    }
    catch (const MiniModel::TooFewArguments&) {
      DFA::Transition t[1] = { DFA::Transition(-1, -1, -1) };
      int f[1] = {-1};
      return new trimDFA(DFA(0, t, f), partitions);
    }
  }

}}

namespace Gecode {

  namespace {

    void
    post_native_regex(Home home, StringVar x, const String::RegEx& expression) {
      if (expression.empty()) {
        rel(home, x, STRT_EQ, StringVar(home, ""));
        return;
      }
      const String::InterEx* intersection =
        dynamic_cast<const String::InterEx*>(&expression);
      if (intersection != nullptr) {
        post_native_regex(home, x, intersection->left());
        post_native_regex(home, x, intersection->right());
        return;
      }
      String::TrimDFAHandle automaton(
        expression.symbolic_dfa(x.may_chars()));
      GECODE_ES_FAIL(String::Reg::post(
        home, String::StringView(x), automaton.get()));
    }

    void
    post_native_regex(Home home, StringVar x, const String::RegEx& expression,
                      BoolVar b, ReifyMode rm) {
      if (expression.empty()) {
        rel(home, x, STRT_EQ, StringVar(home, ""), Reify(b, rm));
        return;
      }
      const String::InterEx* intersection =
        dynamic_cast<const String::InterEx*>(&expression);
      if (intersection == nullptr) {
        String::TrimDFAHandle automaton(
          expression.symbolic_dfa(x.may_chars()));
        String::CompDFAHandle complete(
          new String::compDFA(*automaton, x.may_chars()));
        switch (rm) {
          case RM_IMP:
            GECODE_ES_FAIL((String::ReReg<Int::BoolView, RM_IMP>::post(
              home, String::StringView(x), complete.get(), b)));
            break;
          case RM_PMI:
            GECODE_ES_FAIL((String::ReReg<Int::BoolView, RM_PMI>::post(
              home, String::StringView(x), complete.get(), b)));
            break;
          default:
            GECODE_ES_FAIL((String::ReReg<Int::BoolView, RM_EQV>::post(
              home, String::StringView(x), complete.get(), b)));
        }
        return;
      }

      BoolVar left(home, 0, 1);
      BoolVar right(home, 0, 1);
      BoolVar conjunction(home, 0, 1);
      post_native_regex(home, x, intersection->left(), left, RM_EQV);
      post_native_regex(home, x, intersection->right(), right, RM_EQV);
      rel(home, left, BOT_AND, right, conjunction);
      switch (rm) {
        case RM_IMP:
          rel(home, b, IRT_LQ, conjunction);
          break;
        case RM_PMI:
          rel(home, conjunction, IRT_LQ, b);
          break;
        default:
          rel(home, conjunction, IRT_EQ, b);
      }
    }

  }

  void
  extensional(Home home, StringVar x, String::RegEx* r) {
    if (Gecode::String::DashedString::_DECOMP_REGEX && r->decomp()) {
      VarArgs v;
      r->post(home, x, v);
    }
    else
      post_native_regex(home, x, *r);
  }

  void
  extensional(Home home, StringVar x, std::string regex, VarArgs& v) {
    std::unique_ptr<String::RegEx> r = String::RegExParser(regex).parse();
    if (Gecode::String::DashedString::_DECOMP_REGEX && r->decomp())
      r->post(home, x, v);
    else
      post_native_regex(home, x, *r);
  }

  void
  extensional(Home home, StringVar x, std::string regex) {
    std::unique_ptr<String::RegEx> r = String::RegExParser(regex).parse();
    // std::cerr << "x = " << x << '\n';
    // std::cerr << "Expression: " << r->str() << '\n';
    // std::cerr << "DFA: " << r->dfa() << '\n';
    // std::cerr << "Reg.: " << r->reg() << '\n';
    // std::cerr << "Dec: " << r->decomp() << '\n';
    // std::cerr << "DECOMP:" << Gecode::String::DashedString::_DECOMP_REGEX << '\n';
    // std::cerr << "Intro: " << r->intro_vars() << '\n';
    if (Gecode::String::DashedString::_DECOMP_REGEX && !r->intro_vars()) {
      VarArgs v;
      r->post(home, x, v);
    }
    else
      post_native_regex(home, x, *r);
  }

  void
  extensional(
    Home home, StringVar x, const DFA& d, BoolVar b, ReifyMode rm = RM_EQV
  ) {
    switch (rm) {
      case RM_IMP:
        GECODE_ES_FAIL((String::ReReg<Int::BoolView, RM_IMP>
          ::post(home, x, d, b)));
        return;
      case RM_PMI:
        GECODE_ES_FAIL((String::ReReg<Int::BoolView, RM_PMI>
          ::post(home, x, d, b)));
        return;
      default:
        GECODE_ES_FAIL((String::ReReg<Int::BoolView, RM_EQV>
          ::post(home, x, d, b)));
    }
  }

  void
  extensional(
    Home home, StringVar x, std::string regex, BoolVar b, ReifyMode rm = RM_EQV
  ) {
    std::unique_ptr<String::RegEx> r = String::RegExParser(regex).parse();
    // std::cerr << "Expression: " << r->str() << '\n';
    // std::cerr << "DFA: " << r->dfa() << '\n';
    // std::cerr << "Reg.: " << r->reg() << '\n';
    // std::cerr << "Dec: " << r->decomp() << '\n';
    // std::cerr << "DECOMP:" << Gecode::String::DashedString::_DECOMP_REGEX << '\n';
    // std::cerr << "Intro: " << r->intro_vars() << '\n';
    post_native_regex(home, x, *r, b, rm);
  }

  void
  extensional(Home home, StringVar x, DFA d) {
    GECODE_ES_FAIL((String::Reg::post(home, x, d)));
  }

  void
  extensional(Home home, StringVar x, StringVarArgs y) {
    IntVar i(home, 1, y.size());
    element(home, y, i, x);
  }

  void
  extensional(
    Home home, StringVar x, const std::vector<String::StringVal>& y
  ) {
    IntVar i(home, 1, y.size());
    StringVarArgs a;
    for (const String::StringVal& value : y)
      a << StringVar(home, value);
    element(home, a, i, x);
  }

  void
  extensional(Home home, StringVar x, const std::vector<std::string>& y) {
    IntVar i(home, 1, y.size());
    StringVarArgs a;
    for (auto& s : y)
      a << StringVar(home, s);
    element(home, a, i, x);
  }

}

// STATISTICS: string-post
