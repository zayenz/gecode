#ifndef __GECODE_STRING_EXT_HH__
#define __GECODE_STRING_EXT_HH__

namespace Gecode { namespace String {

  /**
   * \brief Shared immutable interface for string automata
   *
   * States use zero as the initial state and a contiguous interval of final
   * states. Transition lookup consumes solver symbols, not encoded bytes.
   */
  class stringDFA : public SharedHandle::Object {
  protected:
    int final_fst;
    int final_lst;
  public:
    int n_states;
    stringDFA(const DFA& d): final_fst(d.final_fst()),
                             final_lst(d.final_lst()-1),
                             n_states(d.n_states()) {};
    stringDFA(const stringDFA& d)
      : final_fst(d.final_fst), final_lst(d.final_lst),
        n_states(d.n_states) {}
    bool accepting(int q) const {
      return final_fst <= q && q <= final_lst;
    }
    NSIntSet accepting_states(void) const {
      return NSIntSet(final_fst, final_lst);
    }
    /// Test a legacy one-byte-per-symbol suffix beginning at \a begin
    bool accepted(const string& s, string::size_type begin = 0) const {
      assert(begin <= s.size());
      int q = 0;
      for (string::size_type i = begin; i < s.size(); ++i) {
        q = search(q, static_cast<unsigned char>(s[i]));
        if (q == -1)
          return false;
      }
      return accepting(q);
    };
    /// Test an encoding-neutral suffix beginning at scalar offset \a begin
    bool accepted(const StringVal& s, StringVal::size_type begin = 0) const {
      assert(begin <= s.size());
      int q = 0;
      for (StringVal::size_type i = begin; i < s.size(); ++i) {
        q = search(q, s[i]);
        if (q == -1)
          return false;
      }
      return accepting(q);
    };
    virtual NSIntSet alphabet() const = 0;
    virtual int search(int, int) const = 0;
    virtual NSIntSet neighbours(int) const = 0;
  };

  /**
   * \brief Trimmed symbolic DFA for non-reified regular propagation
   *
   * Transitions with the same source and destination coalesce their symbols
   * into one NSIntSet label. The automaton is immutable after construction and
   * shared by propagator clones.
   */
  class trimDFA : public stringDFA {
  public:
    typedef std::vector<std::vector<std::pair<NSIntSet, int>>> delta_t;
    delta_t delta;
    trimDFA(const DFA&);
    trimDFA(const DFA&, const std::vector<NSIntSet>&);
    NSIntSet alphabet() const;
    int min_word_length() const;
    int search(int, int) const;
    template<class CharacterSet>
    delta_t reverse_transitions(const CharacterSet&) const;
    NSIntSet neighbours(int) const;
    template<class CharacterSet, class Visitor>
    void visit_neighbours(int, const CharacterSet&, Visitor) const;
    template<class CharacterSet>
    void include_neighbours(NSIntSet&, int, const CharacterSet&) const;
    template<class CharacterSet>
    bool include_all_neighbours(NSIntSet&, int, const CharacterSet&) const;
    /// Return destinations only if every symbol has an outgoing transition
    NSIntSet neighbot(int, const DSIntSet&) const;
    /// Return reachable states, or no states if any represented word is rejected
    NSIntSet reach_all(const DSBlock&, const NSIntSet&) const;
  };

  /**
   * \brief Complete symbolic DFA for reified regular propagation
   *
   * Missing transitions are redirected to \a q_bot, which allows language
   * complementation by changing accepting states without rebuilding labels.
   */
  class compDFA : public stringDFA {
    int nstate(int) const;
  public:
    int q_bot;
    typedef std::vector<std::vector<std::pair<NSIntSet, int>>> Delta_t;
    Delta_t delta;
    compDFA(const DFA&, const NSIntSet&);
    compDFA(const trimDFA&, const NSIntSet&);
    void negate();
    NSIntSet alphabet() const;
    int search(int, int) const;
    template<class CharacterSet>
    Delta_t reverse_transitions(const CharacterSet&) const;
    NSIntSet neighbours(int) const;
    template<class CharacterSet, class Visitor>
    void visit_neighbours(int, const CharacterSet&, Visitor) const;
    template<class CharacterSet>
    void include_neighbours(NSIntSet&, int, const CharacterSet&) const;
  };

  /**
   * \brief Compact NFA used by substring-match propagation
   *
   * A negative destination \c -k encodes both \c k and the sink state, keeping
   * the common deterministic path compact. Reverse entries identify their
   * source state and edge index.
   */
  struct matchNFA : public compDFA {
    typedef std::pair<int, int> ReverseTransition;
    typedef std::vector<ReverseTransition> ReverseTransitions;
    typedef std::vector<ReverseTransitions> ReverseDelta;

    /// Iterator over predecessors whose transition label intersects a block
    class Predecessors {
    private:
      const matchNFA& nfa;
      const DSIntSet& characters;
      ReverseTransitions::const_iterator current;
      ReverseTransitions::const_iterator finish;
      void skip_disjoint(void);
    public:
      Predecessors(const matchNFA&, int, const DSIntSet&);
      bool operator ()(void) const;
      void operator ++(void);
      int operator *(void) const;
    };

    // 0 = initial state q_0, 1 = accepting state q_F, bot = q_⊥ state.
    ReverseDelta reverse;
    matchNFA(const trimDFA&, const NSIntSet&);
    template<class Visitor>
    void visit_neighbours(int, const DSIntSet&, Visitor) const;
    void include_neighbours(NSIntSet&, int, const DSIntSet&) const;
  };

  /**
   * \brief Typed shared handle for immutable automata
   *
   * Copying a handle shares the underlying SharedHandle::Object across
   * propagator clones.
   */
  template<class Automaton>
  class AutomatonHandle : public SharedHandle {
  public:
    AutomatonHandle(void) {}
    AutomatonHandle(Automaton* automaton) : SharedHandle(automaton) {}
    AutomatonHandle(const AutomatonHandle& handle) : SharedHandle(handle) {}
    AutomatonHandle& operator =(const AutomatonHandle& handle) {
      SharedHandle::operator =(handle);
      return *this;
    }
    AutomatonHandle& operator =(Automaton* automaton) {
      object(automaton);
      return *this;
    }
    Automaton* operator ->(void) const {
      return static_cast<Automaton*>(object());
    }
    Automaton* get(void) const {
      return static_cast<Automaton*>(object());
    }
    Automaton& operator *(void) const {
      return *static_cast<Automaton*>(object());
    }
  };

  typedef AutomatonHandle<trimDFA> TrimDFAHandle;
  typedef AutomatonHandle<compDFA> CompDFAHandle;
  typedef AutomatonHandle<matchNFA> MatchNFAHandle;


  /**
   * \brief %Propagator for DFA.
   *
   */
  class Reg : public UnaryPropagator<StringView, PC_STRING_DOM> {
  private:
    TrimDFAHandle dfa;
  protected:
    using UnaryPropagator<StringView, PC_STRING_DOM>::x0;
    /// Constructor for cloning \a p
    Reg(Space& home, Reg& p);
    /// Constructor for posting
    Reg(Home home, StringView, trimDFA* p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator
    static ExecStatus post(Home home, StringView x, const DFA& dfa);
    /// Post using an already constructed shared automaton
    static ExecStatus post(Home home, StringView x, trimDFA* pdfa);
    /// Return the broad dashed domain induced by the automaton language
    static NSBlocks dom(trimDFA*);
    /// Compute states reachable after a block in forward or reverse order
    template <typename Block>
    static std::vector<NSIntSet> reach_fwd(trimDFA*, const NSIntSet&,
      const Block&, bool rev = false
    );
    /// Refine one block from backward reachability information
    template <typename Block>
    static NSBlocks reach_bwd(trimDFA*, const std::vector<NSIntSet>&,
      NSIntSet&, const Block&, bool&, bool rev = false
    );
    /// Compute forward reachability for a complete DFA
    template <typename Block>
    static std::vector<NSIntSet> reach_fwd(compDFA*, const NSIntSet&,
                                                     const Block&);
    /// Refine one block using a complete DFA
    template <typename Block>
    static NSBlocks reach_bwd(compDFA*, const std::vector<NSIntSet>&, NSIntSet&,
                                                      const Block&, bool&);
    /// Run and atomically commit a complete forward/backward block pass
    template <typename DFA_t>
    static ExecStatus propagate_blocks(Space&, NSBlocks&, DFA_t*);
  };

  /**
   * \brief %Propagator for reified DFA.
   *
   */
  template<class CtrlView, ReifyMode rm>
  class ReReg : public Propagator {
  protected:
    StringView x0;
    CtrlView b;
    /// Constructor for cloning \a p
    ReReg(Space& home, ReReg&);
    /// Constructor for posting
    ReReg(Home home, StringView x, compDFA* d, CtrlView b);
  private:
    CompDFAHandle dfa;
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home);
    /// Cost function (defined as PC_TERNARY_LO)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Schedule function
    virtual void reschedule(Space& home);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ (x=y) \Leftrightarrow b\f$
    static ExecStatus post(Home home, StringView x, const DFA& d, CtrlView b);
    /// Post using an already completed shared automaton
    static ExecStatus post(Home home, StringView x, compDFA* d, CtrlView b);
  };

}}

#include <gecode/string/extensional/reg.hpp>
#include <gecode/string/extensional/re-reg.hpp>

#endif

// STATISTICS: string-prop
