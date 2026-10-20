namespace Gecode { namespace String { namespace Branch {

    None_LLLL::None_LLLL(Home home, ViewArray<String::StringView>& x0):
    StringBrancher(home, x0) {};

    None_LLLL::None_LLLL(Home home, None_LLLL& b):
    StringBrancher(home, b) {}

    Actor*
    None_LLLL::copy(Space& home) {
      return new (home) None_LLLL(home, *this);
    }

    void
    None_LLLL::post(Home home, ViewArray<String::StringView>& x) {
      (void) new (home) None_LLLL(home, x);
    }

    Choice*
    None_LLLL::choice(Space&) {
      const DashedString* p = &x[start].domain();
      // std::cerr << "Var. " << start << " chosen: " << *p;
      return val_llll(start, p);
    }

    ExecStatus
    None_LLLL::commit(Space& home, const Choice& c, unsigned a) {
      // this->print(home, c, a, std::cerr); std::cerr << '\n';
      return commit_choice(home, c, Block::FIRST, a);
    }

}}}

// STATISTICS: string-branch
