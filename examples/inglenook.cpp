/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     [Your Name] <your.email@domain.com>
 *
 *  Copyright:
 *     [Your Name], 2024
 *
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
 *
 */

#include <gecode/driver.hh>
#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace Gecode;

/// \brief %Options for %Inglenook example
class InglenookOptions : public Options {
protected:
  Driver::IntOption _start;     ///< Start state
  Driver::IntOption _target;    ///< Target state
  Driver::IntOption _size;      ///< Number of moves
  Driver::IntOption _numStates; ///< Number of states
  Driver::StringValueOption _csvFile; ///< CSV file path
public:
  /// Constructor
  InglenookOptions(void)
    : Options("Inglenook"),
      _start("start","start state",0),
      _target("target","target state",10),
      _size("size","number of moves",100),
      _numStates("states","number of states",50),
      _csvFile("csv","CSV file path","/Users/zayenz/gecode/gecode/data8.csv") {
    add(_start);
    add(_target);
    add(_size);
    add(_numStates);
    add(_csvFile);
  }
  /// Return start state
  int start(void) const { return _start.value(); }
  /// Return target state
  int target(void) const { return _target.value(); }
  /// Return number of moves
  int size(void) const { return _size.value(); }
  /// Return number of states
  int numStates(void) const { return _numStates.value(); }
  /// Return CSV file path
  const char* csvFile(void) const { return _csvFile.value(); }
};

/**
 * \brief %Example: Inglenook shunting puzzle
 *
 * Find a sequence of moves to transform a start state to a target state
 * using valid transitions defined in a CSV file.
 *
 * \ingroup Example
 */
class Inglenook : public Script {
protected:
  /// The sequence of states
  IntVarArray x;
  /// Number of moves
  const int size;
  /// Number of states
  const int numStates;

public:
  /// Constructor
  Inglenook(const InglenookOptions& opt)
    : Script(opt),
      x(*this, opt.size(), 0, opt.numStates() - 1),
      size(opt.size()), numStates(opt.numStates()) {
    
    // Constraint: x[0] = start
    rel(*this, x[0] == opt.start());

    // Constraint: x[size-1] = target
    rel(*this, x[size - 1] == opt.target());

    // Create a tuple set for valid transitions from the CSV file
    TupleSet ts(2);
    bool read_from_file = true;
    int line_count = 0;
    if (read_from_file) {
        std::ifstream file(opt.csvFile());
        if (!file.good()) {
            std::cerr << "Error: Could not open " << opt.csvFile() << std::endl;
            return;
        }

        std::cerr << "DEBUG: Opened CSV file: " << opt.csvFile() << std::endl;

        std::string line;
        // Skip the first line (column titles)
        if (std::getline(file, line)) {
            // First line is read but ignored
            //std::cerr << "DEBUG: Skipped header line: " << line << std::endl;
        }

        // Read the rest of the lines and populate the tuple set
        while (std::getline(file, line)) {
            line_count++;
            //std::cerr << "DEBUG: Processing line " << line_count << ": " << line << std::endl;

            std::istringstream stream(line);
            int first, second;
            char comma;

            // Parse "first,second" format
            stream >> first >> comma >> second;

            //std::cerr << "DEBUG: Parsed values: first=" << first << ", second=" << second << std::endl;

            ts.add(IntArgs({first, second}));
            //std::cerr << "DEBUG: Added tuple to TupleSet" << std::endl;
        }
    } else {
        int a = 0;
        int b = 0;
        for (int i = 0; i < 2284811; ++i) {
            line_count++;
            ts.add(IntArgs({a, b}));
            a = (a + 12) % 356161;
            b = (b + 35) % 356161;
        }
    }
    std::cerr << "DEBUG: About to call ts.finalize() with " << line_count << " tuples" << std::endl;
    ts.finalize();  // Finalize the tuple set after adding all tuples
    std::cerr << "DEBUG: ts.finalize() completed successfully" << std::endl;

    // Constraint: Valid transitions between consecutive states
    for (int i = 0; i < size - 1; ++i) {
      extensional(*this, IntVarArgs({x[i], x[i + 1]}), ts);
    }

    // Branching strategy: Select variables in order and values as minimum
    branch(*this, x, INT_VAR_NONE(), INT_VAL_MIN());
  }

  /// Constructor for cloning \a s
  Inglenook(Inglenook& s) : Script(s), size(s.size), numStates(s.numStates) {
    x.update(*this, s.x);
  }

  /// Perform copying during cloning
  virtual Space*
  copy(void) {
    return new Inglenook(*this);
  }

  /// Print solution
  virtual void
  print(std::ostream& os) const {
    os << "[";
    for (int i = 0; i < size; ++i) {
      if (x[i].assigned()) {
        os << x[i].val();
      } else {
        os << "?";
      }
      if (i < size - 1) {
        os << ",";
      }
    }
    os << "]" << std::endl;
  }
};

/** \brief Main-function
 *  \relates Inglenook
 */
int
main(int argc, char* argv[]) {
  InglenookOptions opt;
  opt.solutions(1);
  opt.parse(argc,argv);
  Script::run<Inglenook,DFS,InglenookOptions>(opt);
  return 0;
}

// STATISTICS: example-any