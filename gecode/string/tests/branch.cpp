#include <gecode/search.hh>
#include <gecode/string.hh>

#include <cassert>
#include <set>
#include <string>

using namespace Gecode;

class DegreeZeroModel : public Space {
public:
  StringVarArray strings;

  DegreeZeroModel(int brancher, bool multi_block) {
    StringVar empty(*this, "");
    assert(empty.domain().min_str().empty());
    StringVarArgs variables;
    if (multi_block) {
      String::NSBlocks domain;
      domain.emplace_back(String::NSIntSet('a'), 0, 2);
      domain.emplace_back(String::NSIntSet('b'), 1, 3);
      variables << StringVar(*this, domain, 4, 5)
                << StringVar(*this, domain, 4, 5);
    } else {
      variables << StringVar(*this, String::NSIntSet('a', 'b'), 1, 1)
                << StringVar(*this, String::NSIntSet('a', 'b'), 1, 1);
    }
    strings = StringVarArray(*this, variables);
    assert(strings[1].domain().min_str() == (multi_block ? "aabb" : "a"));
    switch (brancher) {
    case 0:
      branch(*this, variables, STRING_VAR_NONE(), STRING_VAL_LLLL());
      break;
    case 1:
      branch(*this, variables, STRING_VAR_LENMIN(), STRING_VAL_LLLL());
      break;
    case 2:
      branch(*this, variables, STRING_VAR_LENMAX(), STRING_VAL_LLLL());
      break;
    case 3:
      branch(*this, variables, STRING_VAR_SIZEMIN(), STRING_VAL_LLUL());
      break;
    case 4:
      branch(*this, variables, STRING_VAR_BLOCKMIN(), STRING_VAL_LLLL());
      break;
    case 5:
      branch(*this, variables, STRING_VAR_BLOCKMIN(), STRING_VAL_LLLM());
      break;
    default:
      branch(*this, variables, STRING_VAR_LENBLOCKMIN(), STRING_VAL_LLLM());
      break;
    }
  }

  DegreeZeroModel(DegreeZeroModel& other) : Space(other) {
    strings.update(*this, other.strings);
  }

  virtual Space* copy(void) {
    return new DegreeZeroModel(*this);
  }
};

class UnicodeBranchModel : public Space {
public:
  StringVar value;

  explicit UnicodeBranchModel(int brancher) : value() {
    String::NSIntSet symbols(0x65E5);
    symbols.include(0x1F600);
    value = StringVar(*this, symbols, 1, 1);
    StringVarArgs variables;
    variables << value;
    switch (brancher) {
    case 0:
      branch(*this, variables, STRING_VAR_NONE(), STRING_VAL_LLLL());
      break;
    case 1:
      branch(*this, variables, STRING_VAR_LENMIN(), STRING_VAL_LLLL());
      break;
    case 2:
      branch(*this, variables, STRING_VAR_LENMAX(), STRING_VAL_LLLL());
      break;
    case 3:
      branch(*this, variables, STRING_VAR_SIZEMIN(), STRING_VAL_LLUL());
      break;
    case 4:
      branch(*this, variables, STRING_VAR_BLOCKMIN(), STRING_VAL_LLLL());
      break;
    case 5:
      branch(*this, variables, STRING_VAR_BLOCKMIN(), STRING_VAL_LLLM());
      break;
    default:
      branch(*this, variables, STRING_VAR_LENBLOCKMIN(), STRING_VAL_LLLM());
      break;
    }
  }

  UnicodeBranchModel(UnicodeBranchModel& other) : Space(other) {
    value.update(*this, other.value);
  }

  virtual Space* copy(void) {
    return new UnicodeBranchModel(*this);
  }
};

class InvalidBranchModel : public Space {
public:
  StringVar value;

  InvalidBranchModel() : value(*this, String::NSIntSet('a', 'b'), 1, 1) {
    StringVarArgs variables;
    variables << value;
    branch(*this, variables, STRING_VAR_NONE(), STRING_VAL_LLLM());
  }

  InvalidBranchModel(InvalidBranchModel& other) : Space(other) {
    value.update(*this, other.value);
  }

  virtual Space* copy(void) {
    return new InvalidBranchModel(*this);
  }
};

class SemanticBranchModel : public Space {
public:
  StringVar value;

  explicit SemanticBranchModel(int brancher) : value() {
    String::NSIntSet left_chars('a', 'b');
    String::NSIntSet right_chars('b', 'c');
    String::NSBlocks domain;
    domain.emplace_back(left_chars, 0, 2);
    domain.emplace_back(right_chars, 1, 2);
    value = StringVar(*this, domain, 2, 2);
    StringVarArgs variables;
    variables << value;
    switch (brancher) {
    case 0:
      branch(*this, variables, STRING_VAR_NONE(), STRING_VAL_LLLL());
      break;
    case 1:
      branch(*this, variables, STRING_VAR_LENMIN(), STRING_VAL_LLLL());
      break;
    case 2:
      branch(*this, variables, STRING_VAR_LENMAX(), STRING_VAL_LLLL());
      break;
    case 3:
      branch(*this, variables, STRING_VAR_SIZEMIN(), STRING_VAL_LLUL());
      break;
    case 4:
      branch(*this, variables, STRING_VAR_BLOCKMIN(), STRING_VAL_LLLL());
      break;
    case 5:
      branch(*this, variables, STRING_VAR_BLOCKMIN(), STRING_VAL_LLLM());
      break;
    default:
      branch(*this, variables, STRING_VAR_LENBLOCKMIN(), STRING_VAL_LLLM());
      break;
    }
  }

  SemanticBranchModel(SemanticBranchModel& other) : Space(other) {
    value.update(*this, other.value);
  }

  virtual Space* copy(void) {
    return new SemanticBranchModel(*this);
  }
};

int
main(void) {
  bool rejected_invalid_branching = false;
  try {
    InvalidBranchModel invalid;
  } catch (const String::UnknownBranching&) {
    rejected_invalid_branching = true;
  }
  assert(rejected_invalid_branching);

  for (int brancher = 0; brancher < 7; ++brancher) {
    for (int multi_block = 0; multi_block < 2; ++multi_block) {
      DegreeZeroModel* model = new DegreeZeroModel(brancher, multi_block != 0);
      DFS<DegreeZeroModel> search(model);
      delete model;
      int solutions = 0;
      while (DegreeZeroModel* solution = search.next()) {
        for (const StringVar& variable : solution->strings) {
          assert(variable.assigned());
          const std::string value = variable.val();
          if (multi_block) {
            const std::string::size_type split = value.find('b');
            assert(value.size() >= 4 && value.size() <= 5);
            assert(split <= 2);
            assert(value.find_first_not_of('a') == split);
            assert(value.find_first_not_of('b', split) == std::string::npos);
            assert(value.size() - split >= 1 && value.size() - split <= 3);
          } else {
            assert(value == "a" || value == "b");
          }
        }
        ++solutions;
        delete solution;
      }
      const int expected = multi_block ? 9 : 4;
      assert(solutions == expected);
    }
  }
  const int clone_distances[] = {1, 2, 8};
  const std::set<std::string> expected_semantic = {
    "ab", "ac", "bb", "bc", "cb", "cc"
  };
  for (int brancher = 0; brancher < 7; ++brancher) {
    for (int clone_distance : clone_distances) {
      SemanticBranchModel* model = new SemanticBranchModel(brancher);
      Search::Options options;
      options.c_d = clone_distance;
      DFS<SemanticBranchModel> search(model, options);
      delete model;
      std::set<std::string> observed;
      int solutions = 0;
      while (SemanticBranchModel* solution = search.next()) {
        assert(solution->value.assigned());
        observed.insert(solution->value.val());
        ++solutions;
        delete solution;
      }
      assert(observed == expected_semantic);
      assert(solutions == static_cast<int>(expected_semantic.size()));
    }
  }
  for (int brancher = 0; brancher < 7; ++brancher) {
    for (int clone_distance : clone_distances) {
      UnicodeBranchModel* model = new UnicodeBranchModel(brancher);
      Search::Options options;
      options.c_d = clone_distance;
      DFS<UnicodeBranchModel> search(model, options);
      delete model;
      int solutions = 0;
      bool seen_bmp = false;
      bool seen_supplementary = false;
      while (UnicodeBranchModel* solution = search.next()) {
        assert(solution->value.assigned());
        const String::StringVal concrete = solution->value.val_symbols();
        assert(concrete.size() == 1);
        if (concrete[0] == 0x65E5)
          seen_bmp = true;
        else {
          assert(concrete[0] == 0x1F600);
          seen_supplementary = true;
        }
        ++solutions;
        delete solution;
      }
      assert(solutions == 2);
      assert(seen_bmp && seen_supplementary);
    }
  }
  return 0;
}
