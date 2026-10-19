#include <gecode/string.hh>
#include <gecode/string/extensional.hh>
#include <gecode/string/extensional/parse-reg.hpp>

#include <cassert>
#include <stdexcept>
#include <string>

using Gecode::BoolVar;
using Gecode::ReifyMode;
using Gecode::RM_EQV;
using Gecode::Space;
using Gecode::StringVar;
using Gecode::String::DashedString;
using Gecode::String::NSIntSet;
using Gecode::String::RegEx;
using Gecode::String::RegExParser;
using Gecode::String::StringAlphabet;
using Gecode::String::StringVal;
using Gecode::String::TrimDFAHandle;

namespace {

  class RegexSpace : public Space {
  public:
    StringVar value;

    RegexSpace(const StringVal& fixed, const std::string& expression,
               bool decompose)
      : value(*this, fixed) {
      if (decompose) {
        Gecode::VarArgs introduced;
        Gecode::extensional(*this, value, expression, introduced);
      } else {
        Gecode::extensional(*this, value, expression);
      }
    }

    RegexSpace(RegexSpace& other)
      : Space(other) {
      value.update(*this, other.value);
    }

    virtual Space* copy(void) {
      return new RegexSpace(*this);
    }
  };

  class ReifiedRegexSpace : public Space {
  public:
    StringVar value;
    BoolVar result;

    ReifiedRegexSpace(const StringVal& fixed, const std::string& expression)
      : value(*this, fixed), result(*this, 0, 1) {
      Gecode::extensional(*this, value, expression, result, RM_EQV);
    }

    ReifiedRegexSpace(ReifiedRegexSpace& other)
      : Space(other) {
      value.update(*this, other.value);
      result.update(*this, other.result);
    }

    virtual Space* copy(void) {
      return new ReifiedRegexSpace(*this);
    }
  };

  bool accepts(const StringVal& value, const std::string& expression,
               bool decompose) {
    DashedString::_DECOMP_REGEX = decompose;
    RegexSpace* space = new RegexSpace(value, expression, decompose);
    const bool accepted = space->status() != Gecode::SS_FAILED;
    delete space;
    return accepted;
  }

  bool reified_result(const StringVal& value, const std::string& expression) {
    ReifiedRegexSpace* space = new ReifiedRegexSpace(value, expression);
    assert(space->status() != Gecode::SS_FAILED);
    assert(space->result.assigned());
    const bool accepted = space->result.val() == 1;
    delete space;
    return accepted;
  }

  void expect_parse_error(const std::string& expression) {
    bool rejected = false;
    try {
      (void) RegExParser(expression).parse();
    } catch (const std::exception&) {
      rejected = true;
    }
    assert(rejected);
  }

  void check_both(const StringVal& value, const std::string& expression,
                  bool expected) {
    assert(accepts(value, expression, true) == expected);
    assert(accepts(value, expression, false) == expected);
  }

}

int main(void) {
  const std::string japanese =
    StringVal::from_symbols({0x65E5, 0x672C}).to_utf8();
  const std::string japanese_class =
    "[" + japanese +
    StringVal::from_symbols({0x1F600}).to_utf8() + "]";

  check_both(StringVal::from_symbols({0x65E5, 0x672C}), japanese, true);
  check_both(StringVal::from_symbols({0x65E5}), japanese, false);

  check_both(StringVal::from_symbols({0x65E5}), "[日-本]", true);
  check_both(StringVal::from_symbols({0x1F600}), japanese_class, true);
  check_both(StringVal::from_symbols({0x1F600}), "[😀-🙏]", true);
  check_both(StringVal::from_symbols({0x65E5, 0x672C}),
             "[日-本,2,2]", true);
  check_both(StringVal::from_symbols({0x672C}), "[^日]", true);
  check_both(StringVal::from_symbols({0x65E5}), "[^日]", false);
  check_both(StringVal::from_symbols({0x672C}), "[本-日]", false);
  check_both(StringVal::from_symbols({0x672C}), "[^本-日]", true);

  check_both(StringVal::from_symbols({0x10FFFF}), ".", true);
  check_both(StringVal::from_symbols({0x1F600}), "\\u{1F600}", true);
  check_both(StringVal::from_symbols({0x65E5}), "\\u65E5", true);
  check_both(StringVal::from_symbols({0xE9}), "\\xE9", true);
  check_both(StringVal::from_symbols({0}), "\\u0000", true);

  assert(reified_result(StringVal::from_symbols({0x1F600}), "."));
  assert(!reified_result(StringVal::from_symbols({0x65E5}), "[^日]"));

  std::unique_ptr<RegEx> wildcard = RegExParser(".").parse();
  const NSIntSet unicode = StringAlphabet::unicode_scalars().symbols();
  TrimDFAHandle wildcard_dfa(wildcard->symbolic_dfa(unicode));
  assert(wildcard_dfa->alphabet() == unicode);
  assert(wildcard_dfa->delta[0].size() == 1);
  assert(wildcard_dfa->delta[0][0].first == unicode);

  std::unique_ptr<RegEx> negated = RegExParser("[^日]").parse();
  TrimDFAHandle negated_dfa(negated->symbolic_dfa(unicode));
  NSIntSet expected(unicode);
  expected.exclude(NSIntSet(0x65E5));
  assert(negated_dfa->alphabet() == expected);
  assert(!negated_dfa->alphabet().in(0x65E5));
  assert(negated_dfa->alphabet().in(0x1F600));

  expect_parse_error(std::string("\x80", 1));
  expect_parse_error("\\uD800");
  expect_parse_error("\\u{110000}");
  expect_parse_error("[^]");

  DashedString::_DECOMP_REGEX = true;
  return 0;
}
