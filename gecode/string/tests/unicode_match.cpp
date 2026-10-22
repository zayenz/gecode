#include <gecode/search.hh>
#include <gecode/string.hh>
#include <gecode/string/match.hh>

#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

using Gecode::IntVar;
using Gecode::Space;
using Gecode::StringVar;
using Gecode::String::MATCH_AUTO;
using Gecode::String::MATCH_DFA;
using Gecode::String::MATCH_NFA;
using Gecode::String::MatchImplementation;
using Gecode::String::NSIntSet;
using Gecode::String::RegEx;
using Gecode::String::RegExParser;
using Gecode::String::StringAlphabet;
using Gecode::String::StringVal;
using Gecode::String::TrimDFAHandle;
using Gecode::String::match_default_implementation;
using Gecode::String::post_match;

namespace {

  class MatchSpace : public Space {
  public:
    StringVar text;
    IntVar index;

    MatchSpace(const StringVal& value, const std::string& pattern,
               MatchImplementation implementation)
      : text(*this, value),
        index(*this, 0, static_cast<int>(value.size())) {
      if (post_match(*this, text, pattern, index, implementation) ==
          Gecode::ES_FAILED)
        fail();
    }

    MatchSpace(MatchSpace& other)
      : Space(other) {
      text.update(*this, other.text);
      index.update(*this, other.index);
    }

    virtual Space* copy(void) {
      return new MatchSpace(*this);
    }
  };

  int match_index(const StringVal& value, const std::string& pattern,
                  MatchImplementation implementation) {
    MatchSpace* space = new MatchSpace(value, pattern, implementation);
    assert(space->status() != Gecode::SS_FAILED);
    assert(space->index.assigned());
    const int result = space->index.val();
    delete space;
    return result;
  }

  void check_all(const StringVal& value, const std::string& pattern,
                 int expected) {
    assert(match_index(value, pattern, MATCH_AUTO) == expected);
    assert(match_index(value, pattern, MATCH_DFA) == expected);
    assert(match_index(value, pattern, MATCH_NFA) == expected);
  }

  std::size_t transition_count(const Gecode::String::trimDFA& dfa) {
    std::size_t count = 0;
    for (const Gecode::String::trimDFA::delta_t::value_type& transitions :
         dfa.delta)
      count += transitions.size();
    return count;
  }

}

int main(void) {
  const std::string japanese =
    StringVal::from_symbols({0x65E5, 0x672C}).to_utf8();
  const std::string day =
    StringVal::from_symbols({0x65E5}).to_utf8();
  const std::string book =
    StringVal::from_symbols({0x672C}).to_utf8();
  const std::string emoji =
    StringVal::from_symbols({0x1F600}).to_utf8();
  const std::string high =
    StringVal::from_symbols({0x10FFFF}).to_utf8();

  check_all(StringVal::from_symbols({0x61, 0x65E5, 0x672C, 0x62}),
            japanese, 2);
  check_all(StringVal::from_symbols({0x61, 0x1F600, 0x62}), emoji, 2);
  check_all(StringVal::from_symbols({0x65E5, 0x1F600}),
            day + ".", 1);
  check_all(StringVal::from_symbols({0x65E5, 0x672C}),
            "[^" + day + "]", 2);
  check_all(StringVal::from_symbols({0x61, 0x1F600}),
            "[" + emoji + "-" +
              StringVal::from_symbols({0x1F64F}).to_utf8() + "]",
            2);
  check_all(StringVal::from_symbols({0x61, 0x10FFFF}), high, 2);
  check_all(StringVal::from_symbols({0x65E5}), book, 0);
  check_all(StringVal::from_symbols({0x1F600}), "\\u{1F600}", 1);

  const NSIntSet unicode = StringAlphabet::unicode_scalars().symbols();
  std::unique_ptr<RegEx> literal = RegExParser(emoji).parse();
  TrimDFAHandle literal_dfa(literal->symbolic_dfa(unicode));
  assert(match_default_implementation(*literal_dfa) == MATCH_DFA);

  std::unique_ptr<RegEx> alternatives =
    RegExParser("[" + day + emoji + "]").parse();
  TrimDFAHandle alternatives_dfa(alternatives->symbolic_dfa(unicode));
  assert(match_default_implementation(*alternatives_dfa) == MATCH_NFA);

  std::unique_ptr<RegEx> wildcard = RegExParser(".").parse();
  TrimDFAHandle wildcard_dfa(wildcard->symbolic_dfa(unicode));
  assert(match_default_implementation(*wildcard_dfa) == MATCH_NFA);
  TrimDFAHandle full(
    wildcard->symbolic_dfa(unicode, true, true));
  assert(transition_count(*full) < 10);
  assert(full->alphabet() == unicode);

  std::unique_ptr<RegEx> oversized = RegExParser("[a,1,501]").parse();
  try {
    oversized->dfa();
    assert(false);
  } catch (const std::length_error&) {
  }

  try {
    delete oversized->symbolic_dfa(NSIntSet('a'));
    assert(false);
  } catch (const std::length_error&) {
  }

  return 0;
}
