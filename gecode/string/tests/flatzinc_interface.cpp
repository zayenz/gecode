#include <gecode/flatzinc.hh>
#include <gecode/search.hh>

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace Gecode;

std::string
solve(const std::string& model) {
  FlatZinc::Printer printer;
  std::ostringstream errors;
  std::istringstream input(model);
  FlatZinc::FlatZincSpace* root = FlatZinc::parse(input, printer, errors);
  assert(root != NULL);
  assert(errors.str().empty());

  FlatZinc::FlatZincOptions options("FlatZinc string test");
  root->createBranchers(printer, root->solveAnnotations(), options, true,
                        errors);
  DFS<FlatZinc::FlatZincSpace> engine(root);
  FlatZinc::FlatZincSpace* solution = engine.next();
  assert(solution != NULL);

  std::ostringstream output;
  solution->print(output, printer);
  delete solution;
  return output.str();
}

bool
satisfiable(const std::string& model) {
  FlatZinc::Printer printer;
  std::ostringstream errors;
  std::istringstream input(model);
  FlatZinc::FlatZincSpace* root = FlatZinc::parse(input, printer, errors);
  assert(root != NULL);
  assert(errors.str().empty());
  if (root->status() == SS_FAILED) {
    delete root;
    return false;
  }

  FlatZinc::FlatZincOptions options("FlatZinc string test");
  root->createBranchers(printer, root->solveAnnotations(), options, true,
                        errors);
  DFS<FlatZinc::FlatZincSpace> engine(root);
  FlatZinc::FlatZincSpace* solution = engine.next();
  bool result = solution != NULL;
  delete solution;
  return result;
}

bool
parse_rejected(const std::string& model) {
  FlatZinc::Printer printer;
  std::ostringstream errors;
  std::istringstream input(model);
  try {
    FlatZinc::FlatZincSpace* root =
      FlatZinc::parse(input, printer, errors);
    const bool rejected = root == NULL || !errors.str().empty();
    delete root;
    return rejected;
  } catch (const std::runtime_error&) {
    return true;
  } catch (const std::invalid_argument&) {
    return true;
  } catch (const FlatZinc::AST::TypeError&) {
    return true;
  }
}

int
main(void) {
  assert(String::DashedString::_DECOMP_REGEX);
  assert(solve(
    "var string: text :: output_var;\n"
    "constraint str_eq(text, \"b\");\n"
    "solve satisfy;\n") == "text = b;\n");

  assert(solve(
    "string: expected = \"cat\";\n"
    "var string: source = expected;\n"
    "var string: text :: output_var = source;\n"
    "constraint str_eq(text, \"cat\");\n"
    "solve satisfy;\n") == "text = cat;\n");

  std::string disequality = solve(
    "var string: text :: output_var;\n"
    "constraint str_ne(text, \"b\");\n"
    "solve satisfy;\n");
  assert(disequality.find("text = ") == 0);
  assert(disequality != "text = b;\n");

  assert(solve(
    "var int: n :: output_var;\n"
    "var string: text = \"cat\";\n"
    "constraint str_len(text, n);\n"
    "solve satisfy;\n") == "n = 3;\n");

  assert(solve(
    "var int: n :: output_var;\n"
    "var string: text = \"\\\\\";\n"
    "constraint str_len(text, n);\n"
    "solve satisfy;\n") == "n = 1;\n");

  assert(solve(
    "var int: n :: output_var;\n"
    "var string: text = \"\\x5cA\";\n"
    "constraint str_len(text, n);\n"
    "solve satisfy;\n") == "n = 2;\n");

  assert(solve(
    "var int: n :: output_var;\n"
    "var string: text = \"a\\x00\";\n"
    "constraint str_len(text, n);\n"
    "solve satisfy;\n") == "n = 2;\n");

  assert(solve(
    "var string: text :: output_var;\n"
    "constraint str_eq(text, \"日本😀\");\n"
    "solve satisfy;\n") == "text = 日本😀;\n");

  assert(solve(
    "var int: n :: output_var;\n"
    "var string: text = \"日本😀\";\n"
    "constraint str_len(text, n);\n"
    "solve satisfy;\n") == "n = 3;\n");

  assert(solve(
    "var int: n :: output_var;\n"
    "var string: text = \"\\u65E5\\u672C\\u{1F600}\";\n"
    "constraint str_len(text, n);\n"
    "solve satisfy;\n") == "n = 3;\n");

  assert(solve(
    "var int: code :: output_var;\n"
    "constraint str_char2code(\"\\u{1F600}\", code);\n"
    "solve satisfy;\n") == "code = 128512;\n");

  assert(solve(
    "var int: raw_length :: output_var;\n"
    "var int: byte_length :: output_var;\n"
    "constraint str_len(\"é\", raw_length);\n"
    "constraint str_len(\"\\xC3\\xA9\", byte_length);\n"
    "solve satisfy;\n") ==
      "byte_length = 2;\nraw_length = 1;\n");

  assert(solve(
    "var string: text :: output_var = \"\\xFF\";\n"
    "solve satisfy;\n") == "text = ÿ;\n");

  assert(solve(
    "var int: code :: output_var;\n"
    "var string(1) of {\"\\x00\"}: text;\n"
    "constraint str_len(text, 1);\n"
    "constraint str_char2code(text, code);\n"
    "solve satisfy;\n") == "code = 0;\n");

  assert(satisfiable(
    "array [1..1] of var string: text;\n"
    "constraint str_eq(text[1], \"ok\");\n"
    "solve satisfy;\n"));

  assert(!satisfiable(
    "var string(1): text = \"ab\";\n"
    "solve satisfy;\n"));

  assert(!satisfiable(
    "var string of {\"a\"}: text = \"b\";\n"
    "solve satisfy;\n"));

  assert(!satisfiable(
    "var string: source = \"ab\";\n"
    "var string(1): text = source;\n"
    "solve satisfy;\n"));

  assert(satisfiable(
    "string: pattern = \"\\(\";\n"
    "var string: text = \"(\";\n"
    "constraint str_reg(text, pattern);\n"
    "solve satisfy;\n"));

  assert(satisfiable(
    "constraint str_reg(\".\", \"\\x2e\");\n"
    "constraint str_reg(\"\\xff\", \"\\xff\");\n"
    "solve satisfy;\n"));

  assert(!satisfiable(
    "constraint str_reg(\"x\", \"\\x2e\");\n"
    "solve satisfy;\n"));

  assert(satisfiable(
    "constraint str_reg(\"😀\", \"\\u{1F600}\");\n"
    "constraint str_reg(\"本\", \"[日-本]\");\n"
    "solve satisfy;\n"));

  assert(solve(
    "var string(1): text :: output_var;\n"
    "constraint str_reg(text, \"[α-ω]\");\n"
    "constraint str_char2code(text, 946);\n"
    "solve satisfy;\n") == "text = β;\n");

  assert(solve(
    "string: alphabet_hint = \"β\";\n"
    "var string(1): text :: output_var;\n"
    "constraint str_reg(text, \".\");\n"
    "constraint str_char2code(text, 946);\n"
    "solve satisfy;\n") == "text = β;\n");

  assert(solve(
    "var int: index :: output_var;\n"
    "constraint str_match(\"a😀b\", \"\\u{1F600}\", index);\n"
    "solve satisfy;\n") == "index = 2;\n");

  assert(satisfiable(
    "var string(1) of {\"日\", \"😀\"}: text = \"\\u{1F600}\";\n"
    "solve satisfy;\n"));

  assert(satisfiable(
    "var string(1) of \"日\"..\"本\": text = \"本\";\n"
    "solve satisfy;\n"));

  assert(solve(
    "var string(1) of \"α\"..\"ω\": text :: output_var;\n"
    "constraint str_char2code(text, 946);\n"
    "solve satisfy;\n") == "text = β;\n");

  assert(satisfiable(
    "constraint str_alphabet(\"日本\", {\"日\", \"本\"});\n"
    "solve satisfy;\n"));

  assert(solve(
    "array [1..2] of string: values = [\"日\", \"😀\"];\n"
    "var string: text :: output_var;\n"
    "constraint array_string_element(2, values, text);\n"
    "solve satisfy;\n") == "text = 😀;\n");

  assert(parse_rejected(
    "constraint str_eq(\"\\uD800\", \"\");\n"
    "solve satisfy;\n"));
  assert(parse_rejected(
    "constraint str_eq(\"\\u{110000}\", \"\");\n"
    "solve satisfy;\n"));
  assert(parse_rejected(
    "constraint str_eq(\"\\u{}\", \"\");\n"
    "solve satisfy;\n"));
  std::string malformed_utf8 = "constraint str_eq(\"";
  malformed_utf8.push_back(static_cast<char>(0xFF));
  malformed_utf8 += "\", \"\");\nsolve satisfy;\n";
  assert(parse_rejected(malformed_utf8));

  assert(satisfiable(
    "var string: text = \"ab\";\n"
    "constraint str_alphabet(text, {\"a\", \"b\"});\n"
    "solve satisfy;\n"));

  assert(!satisfiable(
    "var string: text = \"a\";\n"
    "constraint str_alphabet(text, {\"a\", \"b\"});\n"
    "solve satisfy;\n"));

  assert(satisfiable(
    "var string: text = \"\";\n"
    "constraint str_alphabet(text, {});\n"
    "solve satisfy;\n"));

  String::DashedString::_DECOMP_REGEX = false;
  assert(satisfiable(
    "constraint str_reg(\"b\", \"[a-c]&[b-d]\");\n"
    "constraint str_reg_reif(\"b\", \"[a-c]&[b-d]\", true);\n"
    "constraint str_reg_reif(\"a\", \"[a-c]&[b-d]\", false);\n"
    "solve satisfy;\n"));

  assert(!satisfiable(
    "constraint str_reg(\"a\", \"[a-c]&[b-d]\");\n"
    "solve satisfy;\n"));
  String::DashedString::_DECOMP_REGEX = true;

  assert(satisfiable(
    "array [1..2] of int: transitions = [2, 0];\n"
    "var bool: accepted;\n"
    "constraint str_dfa_reif(\"a\", 2, {\"a\"}, transitions, 1, 2..2, "
      "accepted);\n"
    "constraint bool_eq(accepted, true);\n"
    "solve satisfy;\n"));

  assert(satisfiable(
    "array [1..0] of int: transitions = [];\n"
    "constraint str_dfa(\"\", 1, {}, transitions, 1, 1..1);\n"
    "solve satisfy;\n"));

  assert(!satisfiable(
    "var string(1) of {\"a\", \"b\"}: x;\n"
    "var string(1) of {\"a\", \"b\"}: y;\n"
    "var string(1) of {\"a\", \"b\"}: z;\n"
    "constraint str_len(x, 1);\n"
    "constraint str_len(y, 1);\n"
    "constraint str_len(z, 1);\n"
    "constraint str_ne(x, y);\n"
    "constraint str_ne(x, z);\n"
    "constraint str_ne(y, z);\n"
    "solve satisfy;\n"));

  assert(solve(
    "var int: code :: output_var;\n"
    "var string(1) of {\"\\xff\"}: text;\n"
    "constraint str_len(text, 1);\n"
    "constraint str_char2code(text, code);\n"
    "solve satisfy;\n") == "code = 255;\n");

  assert(solve(
    "var string: text :: output_var;\n"
    "constraint str_concat(\"ca\", \"t\", text);\n"
    "solve satisfy;\n") == "text = cat;\n");

  assert(solve(
    "var string: text :: output_var;\n"
    "constraint str_sub(\"concatenate\", 1, 3, text);\n"
    "solve satisfy;\n") == "text = con;\n");

  assert(solve(
    "var int: n :: output_var;\n"
    "constraint str_find(\"cat\", \"concatenate\", n);\n"
    "solve satisfy;\n") == "n = 4;\n");

  assert(solve(
    "var string: text :: output_var;\n"
    "constraint str_replace(\"cat\", \"a\", \"o\", text);\n"
    "solve satisfy;\n") == "text = cot;\n");

  assert(solve(
    "var bool: b :: output_var;\n"
    "constraint str_contains_reif(\"concatenate\", \"cat\", b);\n"
    "solve satisfy;\n") == "b = true;\n");

  FlatZinc::Printer printer;
  std::ostringstream errors;
  std::istringstream inconsistent(
    "var string: text;\n"
    "constraint str_eq(text, \"b\");\n"
    "constraint str_ne(text, \"b\");\n"
    "solve satisfy;\n");
  FlatZinc::FlatZincSpace* root =
    FlatZinc::parse(inconsistent, printer, errors);
  assert(root != NULL);
  assert(root->status() == SS_FAILED);
  delete root;
  return 0;
}