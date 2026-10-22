namespace Gecode { namespace String {

  /**
   * \defgroup FuncThrowString String exceptions
   * \ingroup FuncThrow
   */

  /// %Exception: Value out of limits
  class GECODE_STRING_EXPORT OutOfLimits : public Exception {
  public:
    /// Initialize with location \a l
    OutOfLimits(const char* l);
  };

  /// %Exception: %Variable created with empty domain
  class GECODE_STRING_EXPORT VariableEmptyDomain : public Exception {
  public:
    /// Initialize with location \a l
    VariableEmptyDomain(const char* l);
  };

  /// %Exception: %Empty dashed string
  class GECODE_STRING_EXPORT EmptyDashedString : public Exception {
  public:
    /// Initialize with location \a l
    EmptyDashedString(const char* l);
  };

  /// %Exception: Unknown string relation
  class GECODE_STRING_EXPORT UnknownRelation : public Exception {
  public:
    /// Initialize with location \a l
    UnknownRelation(const char* l);
  };

  /// %Exception: Unknown value or variable selection passed as argument
  class GECODE_STRING_EXPORT UnknownBranching : public Exception {
  public:
    /// Initialize with location \a l
    UnknownBranching(const char* l);
  };

}}

// STATISTICS: string-core
