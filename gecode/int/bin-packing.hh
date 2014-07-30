/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Stefano Gualandi <stefano.gualandi@gmail.com>
 *
 *  Copyright:
 *     Stefano Gualandi, 2013
 *     Christian Schulte, 2010
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
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

#ifndef __GECODE_INT_BIN_PACKING_HH__
#define __GECODE_INT_BIN_PACKING_HH__

#include <gecode/int.hh>

/**
 * \namespace Gecode::Int::BinPacking
 * \brief %Bin-packing propagators
 */

namespace Gecode { namespace Int { namespace BinPacking {

  /**
   * \brief Item combining bin and size information
   */
  class Item : public DerivedView<IntView> {
  protected:
    using DerivedView<IntView>::x;
    /// Size of item
    int s;
  public:
    /// Default constructor
    Item(void);
    /// Constructor
    Item(IntView b, int s);

    /// Return bin of item
    IntView bin(void) const;
    /// Set bin of item to \a b
    void bin(IntView b);
    /// Return size of item
    int size(void) const;
    /// Set size of item to \a s
    void size(int s);

    /// Update item during cloning
    void update(Space& home, bool share, Item& i);
  };

  /// Whether two items are the same
  bool same(const Item& i, const Item& j);
  /// Test whether one item is before another
  bool before(const Item& i, const Item& j);

  /// For sorting according to size
  bool operator <(const Item& i, const Item& j);


  /// Size sets
  class SizeSet {
  protected:
    /// Number of size entries in the set
    int n;
    /// Total size of the set
    int t;
    /// Array of sizes (will have more elements)
    int* s;
  public:
    /// Default constructor
    SizeSet(void);
    /// Initialize for at most \a n_max items
    SizeSet(Region& region, int n_max);
    /// Add new size \a s
    void add(int s);
    /// Return cardinality of set (number of entries)
    int card(void) const;
    /// Return total size
    int total(void) const;
    /// Return size of item \a i
    int operator [](int i) const;
  };

  /// Size sets with one element discarded
  class SizeSetMinusOne : public SizeSet {
  protected:
    /// Position of discarded item
    int p;
  public:
    /// Default constructor
    SizeSetMinusOne(void);
    /// Initialize for at most \n n_max entries
    SizeSetMinusOne(Region& region, int n);
    /// Discard size \a s
    void minus(int s);
    /// Return cardinality of set (number of entries)
    int card(void) const;
    /// Return total size
    int total(void) const;
    /// Return size of item \a i
    int operator [](int i) const;
  };


  /**
   * \brief Bin-packing propagator
   *
   * The algorithm is taken from:
   *   Paul Shaw. A Constraint for Bin Packing. CP 2004.
   *
   * Requires \code #include <gecode/int/bin-packing.hh> \endcode
   *
   * \ingroup FuncIntProp
   */
  class Pack : public Propagator {
  protected:
    /// Views for load of bins
    ViewArray<OffsetView> l;
    /// Items with bin and size
    ViewArray<Item> bs;
    /// Total size of all items
    int t;
    /// Constructor for posting
    Pack(Home home, ViewArray<OffsetView>& l, ViewArray<Item>& bs);
    /// Constructor for cloning \a p
    Pack(Space& home, bool share, Pack& p);
  public:
    /// Post propagator for loads \a l and items \a bs
    GECODE_INT_EXPORT 
    static ExecStatus post(Home home, 
                           ViewArray<OffsetView>& l, ViewArray<Item>& bs);
    /// Detect non-existence of sums in \a a .. \a b
    template<class SizeSet>
    bool nosum(const SizeSet& s, int a, int b, int& ap, int& bp);
    /// Detect non-existence of sums in \a a .. \a b
    template<class SizeSet>
    bool nosum(const SizeSet& s, int a, int b);
    /// Perform propagation
    GECODE_INT_EXPORT 
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Cost function
    GECODE_INT_EXPORT 
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Copy propagator during cloning
    GECODE_INT_EXPORT 
    virtual Actor* copy(Space& home, bool share);
    /// Destructor
    virtual size_t dispose(Space& home);
  };


  /// Graph containing conflict information
  class ConflictGraph {
  protected:
    /// Home space
    Space& home;
    /// Bin variables
    const IntVarArgs& b;
    /// Number of bins
    unsigned int bins;
    /// Return number of nodes
    int nodes(void) const;
    /// Degree for each node
    unsigned int* d;
    /// Weight for each node (initlized with degree before graph is reduced)
    unsigned int* w;
    /// Bitset for adjacent nodes
    Support::RawBitSetBase a;
    /// Find position for edge between nodes \a i and \a j in adjacency bitset
    unsigned int pos(int i, int j=0) const;

    /// Iterator for neighbors of a node in the conflict graph
    class Neighbors {
    private:
      /// Current neighbour
      unsigned int c;
    public:
      /// Initialize for neighbors of node \a i in graph \a cg 
      Neighbors(const ConflictGraph& cg, int i);
      /// \name Iteration control
      //@{
      /// Test whether iterator is still at a neighbor or done
      bool operator ()(const ConflictGraph& cg, int i) const;
      /// Move iterator to next neighbor (if possible)
      void inc(const ConflictGraph& cg, int i);
      //@}
      
      /// \name %Node access
      //@{
      /// Return current neighbor node
      int val(const ConflictGraph& cg, int i) const;
      //@}
    };

    /// Sets of graph nodes
    class NodeSet : public Support::RawBitSetBase {
    public:
      /// Initialize node set
      NodeSet(Region& r, const ConflictGraph& cg);
      /// Initialize node set as copy of \a ns
      NodeSet(Region& r, const ConflictGraph& cg, const NodeSet& ns);
      /// Test whether node \a i is included
      bool in(int i) const;
      /// Include node \a i
      void incl(int i);
      /// Exclude node \a i
      void excl(int i);
      /// Copy elements from node set \a ns
      void copy(const ConflictGraph& cg, const NodeSet& ns);
      /// Clear the whole node set
      void empty(const ConflictGraph& cg);
    };
    /// Iterator over node sets
    class Nodes {
    private:
      /// Current node
      unsigned int c;
    public:
      /// Initialize for nodes in graph \a cg 
      Nodes(const ConflictGraph& cg, const NodeSet& ns);
      /// \name Iteration control
      //@{
      /// Test whether iterator is still at a neighbor or done
      bool operator ()(const ConflictGraph& cg, const NodeSet& ns) const;
      /// Move iterator to next node (if possible)
      void inc(const ConflictGraph& cg, const NodeSet& ns);
      //@}
      
      /// \name %Node access
      //@{
      /// Return current node
      int val(const ConflictGraph& cg, const NodeSet& ns) const;
      //@}
    };
    
    /// \name Routines for Bosch-Kerbron algorithm
    //@{
    /// Initialize \a iwn as intersection of \a n and the neighbors of node \a i, return whether result is empty
    bool iwn(NodeSet& iwn, const NodeSet& n, int i);
    /// Find a pivot node with maximal degree from \a a or \a b
    int pivot(const NodeSet& a, const NodeSet& b) const;
    /// Run Bosch-Kerbron algorithm for finding max cliques
    GECODE_INT_EXPORT
    ExecStatus bk(NodeSet& r, unsigned int cr, unsigned int wr,
                  NodeSet& p, NodeSet& x);
    //@}

    /// \name Managing maximal cliques
    //@{
    /// Largest clique so far
    NodeSet m;
    /// Size of largest clique
    unsigned int cm;
    /// Weight of largest clique
    unsigned int wm;
    /// Found a clique in \a r of size \a cr and weight \a wr
    ExecStatus clique(const NodeSet& r, unsigned int cr, unsigned int wr);
    /// Found a clique of node \a i
    ExecStatus clique(int i);
    /// Found a clique of nodes \a i and \a j with weight \a w
    ExecStatus clique(int i, int j, unsigned int w);
    /// Found a clique of nodes \a i, \a j, and \a k with weight \a w
    ExecStatus clique(int i, int j, int k, unsigned int w);
    //@}
  public:
    /// Initialize graph
    ConflictGraph(Space& home, Region& r, const IntVarArgs& b,
                  int m);
    /// Add or remove an edge between nodes \a i and \a j (\a i must be less than \a j)
    void edge(int i, int j, bool add=true);
    /// Test whether nodes \a i and \a j are adjacent
    bool adjacent(int i, int j) const;
    /// Post additional constraints
    ExecStatus post(void);
    /// Return maximal clique found
    IntSet maxclique(void) const;
    /// Destructor
    ~ConflictGraph(void);
  };

}}}

#include <gecode/int/bin-packing/propagate.hpp>
#include <gecode/int/bin-packing/conflict-graph.hpp>

#endif

// STATISTICS: int-prop

