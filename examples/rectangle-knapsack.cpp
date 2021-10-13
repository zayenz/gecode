/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
 *     Mikael Lagerkvist, 2021
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

using namespace Gecode;

/**
 * \name Options for rectangle kanpsack problems
 *
 * \relates RectangleKnapsack
 */
//@

/**
 * \brief Propagation to use for capacity constraints
 *
 * \relates RectangleKnapsack
 */
enum CapacityPropagation {
  CAPACITY_NONE,          ///< Use no capacity reasoning
  CAPACITY_REIFIED,       ///< Use reified constraints
  CAPACITY_CUMULATIVES,   ///< Use cumulatives constraint
  CAPACITY_CUMULATIVE_TT, ///< Use cumulative constraint with time tabling
  CAPACITY_CUMULATIVE_EF, ///< Use cumulative constraint with edge finding
  CAPACITY_CUMULATIVE     ///< Use cumulative constraint with both time tabling and edge finding
};
/// The IntPropLevel to use for cumulative given a certain CapacityPropagation level
IntPropLevel cumulative_ipl(CapacityPropagation propagation) {
  switch (propagation) {
    case CAPACITY_CUMULATIVE_TT:
      return Gecode::IPL_BASIC;
    case CAPACITY_CUMULATIVE_EF:
      return Gecode::IPL_ADVANCED;
    default:
      return Gecode::IPL_BASIC_ADVANCED;
  }
}

/**
 * \brief Propagation to use for used area constraints
 *
 * \relates RectangleKnapsack
 */
enum UsedAreaPropagation {
  USED_AREA_NONE,       ///< No propagation on used area
  USED_AREA_LINEAR,     ///< Use linear constraint for used area
  USED_AREA_BINPACKING, ///< Use binpacking constraint to propagate the used area
};

/**
 * \brief Symmetry breaking to use
 *
 * \relates RectangleKnapsack
 */
enum SymmetryBreaking {
  SYMMETRY_NONE,  ///< Do not break symmetries
  SYMMETRY_TYPES, ///< Break symmetries for each rectangle type (if it is used and where it is placed)
};

/**
 * \brief Branching strategy to use
 *
 * \relates RectangleKnapsack
 */
enum BranchingStrategy {
  BRANCH_ORDER,           ///< Branch on rectangles in value/area order. Place each rectangle fully.
  BRANCH_ORDER_SEPARATED, ///< Branch on rectangles in value/area order. PLace rectangles in x-direction before y-direction.
  BRANCH_SEPARATED,       ///< Branch on usage, x-position, width
};

/** \brief Options for %RectangleKnapsack problems
 *
 * \relates RectangleKnapsack
 */
class RectangleOptions : public SizeOptions {
private:
  /// How to propagate the capacity constraints
  Driver::StringOption _capacity;
  /// How to propagate the used area constraints
  Driver::StringOption _used_area;
public:
  /// Initialize options with file name \a s
  RectangleOptions(const char* s)
  : SizeOptions(s),
    _capacity("capacity", "Propagation to use for capacity constraints", CAPACITY_CUMULATIVES),
    _used_area("used-area", "Propagation to use for used area constraints", USED_AREA_BINPACKING)
  {
    add(_capacity);
    add(_used_area);

    // Add capacity options
    _capacity.add(CapacityPropagation::CAPACITY_NONE,          "none",        "No capacity propagation");
    _capacity.add(CapacityPropagation::CAPACITY_REIFIED,       "reified",     "Use reified capacity constraints");
    _capacity.add(CapacityPropagation::CAPACITY_CUMULATIVES,   "cumulatives", "Use cumulatives for capacity constraints");
    _capacity.add(CapacityPropagation::CAPACITY_CUMULATIVE_TT, "cumulative-tt", "Use cumulative with time tabling for capacity constraints");
    _capacity.add(CapacityPropagation::CAPACITY_CUMULATIVE_EF, "cumulative-ef", "Use cumulative with edge finding for capacity constraints");
    _capacity.add(CapacityPropagation::CAPACITY_CUMULATIVE,    "cumulative",   "Use cumulative with time tabling and edge finding for capacity constraints");

    // Add used area options
    _used_area.add(UsedAreaPropagation::USED_AREA_NONE,       "none",       "No used area propagation");
    _used_area.add(UsedAreaPropagation::USED_AREA_LINEAR,     "linear",     "Use linear propagation for used area");
    _used_area.add(UsedAreaPropagation::USED_AREA_BINPACKING, "binpacking", "Use binpacking propagation for used area");

    // Set symmetry options
    symmetry(SymmetryBreaking::SYMMETRY_TYPES);
    symmetry(SymmetryBreaking::SYMMETRY_NONE,  "none", "No symmetry breaking");
    symmetry(SymmetryBreaking::SYMMETRY_TYPES, "types", "Break symmetry among rectangle types");

    // Set branching options
    branching(BranchingStrategy::BRANCH_ORDER);
    branching(BranchingStrategy::BRANCH_ORDER,           "order", "Use value per area ordering");
    branching(BranchingStrategy::BRANCH_ORDER_SEPARATED, "order-separated", "Use vvalue per area ordering, x before y");
    branching(BranchingStrategy::BRANCH_SEPARATED,       "separated", "Branch on used, then x, then width, then y, and lastly height.");

    // Set recomputation options
    a_d(5);
    c_d(20);
  }
  /// Parse options from arguments \a argv (number is \a argc)
  void parse(int& argc, char* argv[]) {
    // Parse regular options
    Options::parse(argc,argv);
  }
  /// The way to propagate capacity constraints
  CapacityPropagation capacity() const {
    return (CapacityPropagation) _capacity.value();
  }
  /// The way to propagate used area constraints
  UsedAreaPropagation used_area() const {
    return (UsedAreaPropagation) _used_area.value();
  }
};
//@}


/**
 * \name Specifications for rectangle kanpsack problems
 *
 * \relates RectangleKnapsack
 */
//@
/**
 * \brief Specification of a rectangle type
 *
 * \relates RectangleKnapsack
 */
struct RectangleTypes {
  int width;
  int height;
  int count;
  int value;
};

/**
 * \brief Specification of a single rectangle
 *
 * \relates RectangleKnapsack
 */
struct Rectangle {
  int width;
  int height;
  int value;
  unsigned int id;
  unsigned int type;

  int min_domain() const {
    return std::min(width, height);
  }

  int max_domain() const {
    return std::max(width, height);
  }

  bool is_square() const {
    return width == height;
  }

  int area() {
    return width * height;
  }
};

/**
 * \brief Specification of a rectangle knapsack packing instance
 *
 * \relates RectangleKnapsack
 */
struct Instance {
  int width;
  int height;
  std::vector<RectangleTypes> rectangle_types;

  /// The maximum value if all rectangles are used
  int max_total_value() const {
    int value = 0;
    for (unsigned int i = 0; i < rectangle_types.size(); ++i) {
      value += rectangle_types[i].count * rectangle_types[i].value;
    }
    return value;
  }

  /// Create vector of all rectangles based on the rectangle types and their count
  std::vector<Rectangle> make_rectangles() const {
    int count = 0;
    for (const auto& rectangle_type : rectangle_types) {
      count += rectangle_type.count;
    }
    std::vector<Rectangle> result;
    result.reserve(count);
    unsigned int id = 0;
    for (unsigned int t = 0; t < rectangle_types.size(); ++t) {
      for (int i = 0; i < rectangle_types[t].count; ++i) {
        result.emplace_back(Rectangle {
          .width = rectangle_types[t].width,
          .height = rectangle_types[t].height,
          .value = rectangle_types[t].value,
          .id = id,
          .type = t,
        });
        ++id;
      }
    }
    return result;
  }
};

/// Instance from http://yetanothermathprogrammingconsultant.blogspot.com/2021/10/2d-knapsack-problem.html
const Instance yampc = {
  .width = 30,
  .height = 20,
  .rectangle_types = {
     { 20,  4, 2, 338984},
     { 12, 17, 6, 849246},
     { 20, 12, 2, 524022},
     { 16,  7, 9, 263303},
     {  3,  6, 3, 113436},
     { 13,  5, 3, 551072},
     {  4,  7, 6,  86166},
     {  6, 18, 8, 755094},
     { 14,  2, 7, 223516},
     {  9, 11, 5, 369560},
  },
};

/// All instances
const Instance* instances[] = {
  &yampc
};
/// The total number of defined instances
const unsigned int n_instances = sizeof(instances) / sizeof(Instance*);
//@}

/**
 * \brief %Example: Packing squares into a rectangle
 *
 * See description at http://yetanothermathprogrammingconsultant.blogspot.com/2021/10/2d-knapsack-problem.html
 *
 * \ingroup Example
 */
class RectangleKnapsack : public IntMaximizeScript {
protected:
  std::vector<Rectangle> rectangles;
  int area_width;
  int area_height;
  /// Array of x-coordinates of rectangles
  IntVarArray x;
  /// Array of y-coordinates of rectangles
  IntVarArray y;
  /// Array of width of rectangles
  IntVarArray width;
  /// Array of height of rectangles
  IntVarArray height;
  /// Array indicating if a rectangle is used
  BoolVarArray used;
  /// Total value of placed rectangles
  IntVar value;
public:
  /// Actual model
  RectangleKnapsack(const RectangleOptions& opt)
    : IntMaximizeScript(opt),
      rectangles(instances[opt.size()]->make_rectangles()),
      area_width(instances[opt.size()]->width),
      area_height(instances[opt.size()]->height),
      x(*this,rectangles.size(),0,area_width),
      y(*this,rectangles.size(),0,area_height),
      width(*this,rectangles.size(),0,area_width),
      height(*this,rectangles.size(),0,area_height),
      used(*this, rectangles.size(), 0, 1),
      value(*this, 0, instances[opt.size()]->max_total_value()) {

    /*
     * Basic constraints
     */

    // Value of all used rectangles, the optimization variable
    IntArgs values;
    for (unsigned int r = 0; r < rectangles.size(); ++r) {
      values << rectangles[r].value;
    }
    linear(*this, values, used, Gecode::IRT_EQ, value);

    // Set up domains for heights and widths
    for (unsigned int r = 0; r < rectangles.size(); ++r) {
      const Rectangle& rect = rectangles[r];
      IntSet domain{rect.min_domain(), rect.max_domain()};
      dom(*this,  width[r], domain);
      dom(*this, height[r], domain);
      if (!rect.is_square()) {
        rel(*this, width[r] != height[r]);
      }
    }

    // Restrict position according to area size
    for (unsigned int r = 0; r < rectangles.size(); r++) {
      rel(*this, x[r], IRT_LQ, area_width  - rectangles[r].width);
      rel(*this, y[r], IRT_LQ, area_height - rectangles[r].height);
    }

    // Compute end-coordinates for rectangles
    IntVarArgs x_end(*this, rectangles.size(), 0, area_width);
    IntVarArgs y_end(*this, rectangles.size(), 0, area_height);
    for (unsigned int r = 0; r < rectangles.size(); ++r) {
      rel(*this, x[r] +  width[r] == x_end[r]);
      rel(*this, y[r] + height[r] == y_end[r]);
    }

    // Squares do not overlap. Main packing constraint
    nooverlap(*this, x, width, x_end, y, height, y_end, used);

    /*
     * Propagation for used area
     */
    if (opt.used_area() != USED_AREA_NONE) {
      IntArgs rectangles_areas;
      int total_placeable_area = 0;
      for (unsigned int r = 0; r < rectangles.size(); ++r) {
        int area = rectangles[r].area();
        rectangles_areas << area;
        total_placeable_area += area;
      }
      int placement_area = area_height * area_width;

      if (opt.used_area() == USED_AREA_LINEAR) {
        linear(*this, rectangles_areas, used, IRT_LQ, placement_area);
      }

      if (opt.used_area() == USED_AREA_BINPACKING) {
        IntVarArgs loads{
          IntVar(*this, 0, total_placeable_area),
          IntVar(*this, 0, placement_area)

        };
        IntVarArgs bin;
        for (unsigned int r = 0; r < rectangles.size(); ++r) {
          bin << channel(*this, used[r]);
        }
        binpacking(*this, loads, bin, rectangles_areas);
      }
    }

    /*
     * Symmetry breaking constraints.
     */
    if (opt.symmetry() == SYMMETRY_TYPES)
    {
      // Symmetry among copies of the same type
      for (unsigned int t = 0; t < instances[opt.size()]->rectangle_types.size(); ++t) {
        // Variables indicating if a rectangle of type t is used
        BoolVarArgs t_used;
        // Combination of x and y for the rectangles of type t
        IntVarArgs t_xy;
        for (unsigned int r = 0; r < rectangles.size(); ++r) {
          if (rectangles[r].type == t) {
            t_used << used[r];
            t_xy << expr(*this, x[r] * area_height + y[r]);
          }
        }
        // Earlier instances are used before later instances
        rel(*this, t_used, Gecode::IRT_GQ);
        // Earlier instances are placed before later instances
        rel(*this, t_xy, Gecode::IRT_LE);
      }
    }

    /*
     * Capacity constraints
     */

    // Use reified constraints for capacity limits
    // For each row and column, identify the rectangles overlapping it, ensuring that their extents in
    // that direction are at most the extent of the area.
    if (opt.capacity() == CAPACITY_REIFIED) {
      IntArgs rectangle_heights;
      IntArgs rectangle_widths;
      for (unsigned int r = 0; r < rectangles.size(); ++r) {
        rectangle_heights << rectangles[r].height;
        rectangle_widths << rectangles[r].width;
      }
      for (int cx = 0; cx < area_width; cx++) {
        BoolVarArgs rectangle_overlaps_cx;
        for (unsigned int r = 0; r < rectangles.size(); r++) {
          rectangle_overlaps_cx << expr(*this, used[r] && x[r] <= cx && cx <= x_end[r]);
        }
        linear(*this, rectangle_heights, rectangle_overlaps_cx, IRT_LQ, area_height);
      }
      for (int cy = 0; cy < area_height; cy++) {
        BoolVarArgs rectangle_overlaps_cy;
        for (unsigned int r = 0; r < rectangles.size(); r++) {
          rectangle_overlaps_cy << expr(*this, used[r] && y[r] <= cy && cy <= y_end[r]);
        }
        linear(*this, rectangle_widths, rectangle_overlaps_cy, IRT_LQ, area_width);
      }
    }

    // Use cumulatives constraints for capacity limits
    // Encode optional rectangles by giving them their own machine when not used
    if (opt.capacity() == CAPACITY_CUMULATIVES) {
      IntVarArgs machine(*this, rectangles.size(), 0, rectangles.size());
      for (unsigned int r = 0; r < rectangles.size(); ++r) {
        int not_used = r + 1;
        rel(*this, (machine[r] == 0) == used[r]);
        rel(*this, (machine[r] == not_used) == !used[r]);
        dom(*this, machine[r], IntSet{0, not_used});
      }
      // All machines have the same area limit
      IntArgs width_list = IntArgs::create(rectangles.size() + 1, area_width, 0);
      IntArgs height_list = IntArgs::create(rectangles.size() + 1, area_height, 0);

      cumulatives(*this, machine, x, width, x_end, height, height_list, true);
      cumulatives(*this, machine, y, height, y_end, width, width_list, true);
    }

    // Use cumulative constraint for capacities
    // Encode rotation with mutually exclusive variants of each rectangle when it is not square.
    if (opt.capacity() == CAPACITY_CUMULATIVE_TT || opt.capacity() == CAPACITY_CUMULATIVE_EF || opt.capacity() == CAPACITY_CUMULATIVE) {
      IntPropLevel ipl = cumulative_ipl(opt.capacity());

      IntVarArgs nr_x_start;
      IntVarArgs nr_y_start;
      IntArgs nr_height;
      IntArgs nr_width;
      BoolVarArgs nr_used;
      for (unsigned int r = 0; r < rectangles.size(); ++r) {
        if (rectangles[r].is_square()) {
          nr_x_start << x[r];
          nr_y_start << y[r];
          nr_height << rectangles[r].height;
          nr_width  << rectangles[r].width;
          nr_used << used[r];
        } else {
          nr_x_start << x[r] << x[r];
          nr_y_start << y[r] << y[r];
          nr_height << rectangles[r].height << rectangles[r].width;
          nr_width  << rectangles[r].width  << rectangles[r].height;
          BoolVar original(*this, 0, 1);
          BoolVar flipped(*this, 0, 1);
          nr_used << original << flipped;
          // At most one of original and flipped is true when the rectangle is used, otherwise neither is used
          rel(*this, original + flipped <= used[r]);
          // If the rectangle is used, it is used in at least one orientation
          rel(*this, original + flipped >= used[r]);
          // When original/flipped is true, set the corresponding width and height
          rel(*this, original >> (width[r] == rectangles[r].width));
          rel(*this, original >> (height[r] == rectangles[r].height));
          rel(*this, flipped >> (width[r] == rectangles[r].height));
          rel(*this, flipped >> (height[r] == rectangles[r].width));
          // When a rectangle is used and the width/height is known, set if it is original or flipped
          rel(*this, original << (used[r] && width[r] == rectangles[r].width));
          rel(*this, original << (used[r] && height[r] == rectangles[r].height));
          rel(*this, flipped << (used[r] && width[r] == rectangles[r].height));
          rel(*this, flipped << (used[r] && height[r] == rectangles[r].width));
        }
      }

      cumulative(*this, area_height, nr_x_start, nr_width, nr_height, nr_used, ipl);
      cumulative(*this, area_width, nr_y_start, nr_height, nr_width, nr_used, ipl);
    }

    /*
     * Branching
     */

    // Set up ordering of rectangles based on their contributed value per area unit
    std::vector<Rectangle> order(rectangles);
    std::sort(order.begin(), order.end(), [](const Rectangle& a, const Rectangle& b) -> bool
    {
      if (a.type == b.type) {
        return a.id < b.id;
      }
      double a_weight = ((double)a.value) / (a.height * a.width);
      double b_weight = ((double)b.value) / (b.height * b.width);
      return a_weight > b_weight;
    });

    switch (opt.branching()) {
      case BRANCH_ORDER: {
        for (unsigned int o = 0; o < order.size(); ++o) {
          unsigned int pos = order[o].id;

          branch(*this, used[pos], BOOL_VAL_MAX());
          IntVarArgs rectangle_variables{x[pos], width[pos], y[pos], height[pos]};
          branch(*this, rectangle_variables, INT_VAR_NONE(), INT_VAL_MIN());
        }
      }
      break;
      case BRANCH_ORDER_SEPARATED: {
        for (unsigned int o = 0; o < order.size(); ++o) {
          unsigned int pos = order[o].id;

          branch(*this, used[pos], BOOL_VAL_MAX());
          branch(*this, x[pos], INT_VAL_MIN());
          branch(*this, width[pos], INT_VAL_MAX());
        }
        for (unsigned int o = 0; o < order.size(); ++o) {
          unsigned int pos = order[o].id;

          branch(*this, y[pos], INT_VAL_MIN());
          branch(*this, height[pos], INT_VAL_MIN());
        }

      }
      break;
      case BRANCH_SEPARATED: {
        branch(*this, used, BOOL_VAR_AFC_MAX(), BOOL_VAL_MAX());
        branch(*this, x, INT_VAR_MIN_MIN(), INT_VAL_MIN());
        branch(*this, width, INT_VAR_MIN_MIN(), INT_VAL_MIN());
        branch(*this, y, INT_VAR_MIN_MIN(), INT_VAL_MIN());
        branch(*this, height, INT_VAR_MIN_MIN(), INT_VAL_MIN());
      }
    }

  }

  /// Return variable with current cost
  virtual IntVar cost(void) const {
    return value;
  }


  /// Constructor for cloning \a s
  RectangleKnapsack(RectangleKnapsack& s) : IntMaximizeScript(s) {
    x.update(*this, s.x);
    y.update(*this, s.y);
    width.update(*this, s.width);
    height.update(*this, s.height);
    used.update(*this, s.used);
    value.update(*this, s.value);
  }
  /// Copy during cloning
  virtual Space*
  copy(void) {
    return new RectangleKnapsack(*this);
  }
  /// Print solution
  virtual void
  print(std::ostream& os) const {
    os << "\t";
    for (int r=0; r<x.size(); r++) {
      if (used[r].max() == 1) {
        os << r << "@(" << x[r] << "," << y[r] << ") w=" << width[r] << ",h=" << height[r] << "  ";
      }
    }
    os << std::endl;
    os << "\tUnused: ";
    for (int r=0; r<x.size(); r++) {
      if (used[r].max() == 0) {
        os << r << ", ";
      }
    }
    os << std::endl;
    os << "\tValue = " << value << std::endl;
  }
};

/** \brief Main-function
 *  \relates RectangleKnapsack
 */
int
main(int argc, char* argv[]) {
  RectangleOptions opt("RectangleKnapsack");
  opt.parse(argc,argv);
  if (opt.size() >= n_instances) {
    std::cerr << "Error: size must be between 0 and " << n_instances - 1
              << std::endl;
    return 1;
  }
  Script::run<RectangleKnapsack,BAB,RectangleOptions>(opt);
  return 0;
}

// STATISTICS: example-any

