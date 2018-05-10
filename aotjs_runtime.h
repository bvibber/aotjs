#include <cinttypes>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace AotJS {
  using ::std::string;
  using ::std::vector;
  using ::std::hash;
  using ::std::unordered_set;
  using ::std::unordered_map;

  typedef const char *Type;

  extern Type typeof_undefined;
  extern Type typeof_number;
  extern Type typeof_boolean;
  extern Type typeof_string;
  extern Type typeof_object;

  class Engine;
  class Scope;
  class Val;
  typedef Val (*FunctionBody)(Scope *scope);

  class GCThing;
  class String;
  class Symbol;
  class Object;

  class Undefined {
    // just a tag
  };

  class Null {
    // just a tag
  };

  ///
  /// Polymorphic JS values are handled by using 64-bit values with
  /// NaN signalling, so they may contain either a double-precision float
  /// or a typed value with up to a 48-bit pointer or integer payload.
  ///
  /// This is similar to, but not the same as, the "Pun-boxing" used in
  /// Mozilla's SpiderMonkey engine:
  /// * https://github.com/mozilla/gecko-dev/blob/master/js/public/Value.h
  ///
  /// May not be optimal for wasm32 yet.
  class Val {
    union {
      uint64_t val_raw;
      int32_t val_int32;
      double val_double;
    };

  public:

    // 13 bits reserved at top for NaN
    //   one bit for the sign, haughty on his throne
    //   eleven 1s for exponent, expanding through the 'verse
    //   last 1 for the NaN marker, whispered in the night
    // 3 bits to mark the low-level tag type
    //   alchemy clouds its mind
    // up to 48 bits for payload
    //   x86_64 needs all 48 bits for pointers, or just 47 for user mode...?
    //   aarch64 may need 48 bits too, and ... isn't signed?
    //   ints, bools, 32-bit pointers use bottom 32 bits
    //   null, undefined don't use any of the payload
    static const uint64_t sign_bit       = 0b1000000000000000'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_mask       = 0b1111111111111111'0000000000000000'0000000000000000'0000000000000000;
    // double cutoff: canonical NAN rep with sign/signal bit on
    static const uint64_t tag_max_double = 0b1111111111111000'0000000000000000'0000000000000000'0000000000000000;
    // integer types:
    static const uint64_t tag_int32      = 0b1111111111111001'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_bool       = 0b1111111111111010'0000000000000000'0000000000000000'0000000000000000;
    // tag-only types
    static const uint64_t tag_null       = 0b1111111111111011'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_undefined  = 0b1111111111111100'0000000000000000'0000000000000000'0000000000000000;
    // pointer types:
    static const uint64_t tag_min_gc     = 0b1111111111111101'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_string     = 0b1111111111111101'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_symbol     = 0b1111111111111110'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_object     = 0b1111111111111111'0000000000000000'0000000000000000'0000000000000000;

    Val(const Val &val) : val_raw(val.raw()) {}
    Val(double val)     : val_double(val) {}
    Val(int32_t val)    : val_raw(((uint64_t)((int64_t)val) & ~tag_mask) | tag_int32) {}
    Val(bool val)       : val_raw(((uint64_t)val & ~tag_mask) | tag_bool) {}
    Val(String *val)    : val_raw((reinterpret_cast<uint64_t>(val) & ~tag_mask) | tag_string) {}
    Val(Symbol *val)    : val_raw((reinterpret_cast<uint64_t>(val) & ~tag_mask) | tag_symbol) {}
    Val(Object *val)    : val_raw((reinterpret_cast<uint64_t>(val) & ~tag_mask) | tag_object) {}
    Val(Undefined val)  : val_raw(tag_undefined) {}
    Val(Null val)       : val_raw(tag_null) {}

    uint64_t raw() const {
      return val_raw;
    }

    uint64_t tag() const {
      return val_raw & tag_mask;
    }

    bool isDouble() const {
      // Saw this trick in SpiderMonkey.
      // Our tagged values will be > tag_max_double in uint64_t interpretation
      // Any non-NaN negative double will be < that
      // Any positive double, inverted, will be < that
      return (val_raw | sign_bit) <= tag_max_double;
    }

    bool isInt32() const {
      return tag() == tag_int32;
    }

    bool isBool() const {
      return tag() == tag_bool;
    }

    bool isUndefined() const {
      return tag() == tag_undefined;
    }

    bool isNull() const {
      return tag() == tag_null;
    }

    bool isGCThing() const {
      // Another clever thing.
      // All pointer types will have at least this value!
      return val_raw >= tag_min_gc;
    }

    bool isString() const {
      return tag() == tag_string;
    }

    bool isSymbol() const {
      return tag() == tag_symbol;
    }

    bool isObject() const {
      return tag() == tag_object;
    }

    double asDouble() const {
      // Interpret all bits as double-precision float
      return val_double;
    }

    int32_t asInt32() const {
      // Bottom 32 bits are ours for ints.
      return val_int32;
    }

    bool asBool() const {
      // Bottom 1 bit is all we need!
      // But treat it like an int32.
      return (bool)val_int32;
    }

    Null asNull() const {
      Null nullx;
      return nullx;
    }

    Undefined asUndefined() const {
      Undefined undef;
      return undef;
    }

    void *asPointer() const {
      #if (PTRDIFF_MAX) > 2147483647
        // 64-bit host -- drop the top 16 bits of NaN and tag.
        // Assumes address space has only 48 significant bits
        // but may be signed, as on x86_64.
        return reinterpret_cast<void *>((val_raw << 16) >> 16);
      #else
        // 32 bit host -- bottom bits are ours, like an int.
        return reinterpret_cast<void *>(val_int32);
      #endif
    }

    GCThing *asGCThing() const {
      return static_cast<GCThing *>(asPointer());
    }

    String *asString() const {
      return static_cast<String *>(asPointer());
    }

    Symbol *asSymbol() const {
      return static_cast<Symbol *>(asPointer());
    }

    Object *asObject() const {
      return static_cast<Object *>(asPointer());
    }

    bool operator==(const Val &rhs) const;

    string dump() const;
  };

}

namespace std {
  template<> struct hash<::AotJS::Val> {
      size_t operator()(::AotJS::Val const& Val) const noexcept;
  };
}

namespace AotJS {

  class GCThing {
    bool marked;

  public:
    GCThing()
    :
      marked(false)
    {
      //
    }

    virtual ~GCThing();

    bool isMarkedForGC();
    void markForGC();
    void clearForGC();
    virtual void markRefsForGC();

    virtual string dump();
  };

  class Object : public GCThing {
    Object *prototype;
    unordered_map<Val,Val> props;

    friend class PropList;

  public:
    Object()
    :
      prototype(nullptr)
    {
      // Special for creating the root Object
    }

    Object(Object *aPrototype) :
      prototype(aPrototype)
    {
      // When we have a prototype chain...
    }

    ~Object() override;

    void markRefsForGC() override;
    string dump() override;

    Type getTypeof() const {
      return typeof_object;
    }

    Val getProp(Val name);
    void setProp(Val name, Val val);
  };

  class String : public GCThing {
    string data;

  public:
    String(string const &aStr)
    :
      GCThing(),
      data(aStr)
    {
      //
    }

    ~String() override;

    const Type typeof() const {
      return typeof_string;
    }

    string dump() override;

    operator string() const {
      return data;
    }

    bool operator==(const String &rhs) const {
      return data == rhs.data;
    }
  };

  class Symbol : public GCThing {
    string name;

  public:
    Symbol(string const &aName) :
      name(aName)
    {
      //
    }

    ~Symbol() override;

    const Type typeof() const {
      return typeof_string;
    }

    string dump() override;

    const string &getName() const {
      return name;
    }
  };

  class Engine {
    Object *root;
    std::vector<Scope *>stack;
    unordered_set<GCThing *> objects;
    void registerForGC(GCThing *obj);

  public:
    Engine() {
      root = newObject(nullptr);
    }

    Object *getRoot() const {
      return root;
    }

    Object *newObject(Object *prototype);
    String *newString(const string &aStr);
    Symbol *newSymbol(const string &aName);
    Scope *newScope(Scope *parent, std::vector<Val> args, std::vector<Val *>captures);
    Scope *newScope(std::vector<Val> args, std::vector<Val *>captures);

    Val call(FunctionBody func, std::vector<Val>args, std::vector<Val *>captures);

    void gc();
    string dump();
  };

  class Scope : public GCThing {
    Engine *engine;
    Scope *parent;
    std::vector<Val> args;
    std::vector<Val *> captures;
    std::vector<Val> locals;

  public:
    ~Scope() override;

    Scope(Engine *aEngine,
      std::vector<Val> aArgs,
      std::vector<Val *> aCaptures)
    :
      engine(aEngine),
      parent(nullptr),
      args(aArgs),
      captures(aCaptures)
    {
      // Special for the global context
    }

    Scope(Scope *aParent,
      std::vector<Val> aArgs,
      std::vector<Val *> aCaptures)
    :
      engine(aParent->engine),
      parent(aParent),
      args(aArgs),
      captures(aCaptures)
    {
      // Regular sub-scopes and call records
    }

    void allocLocals(size_t count) {
      for (size_t i = 0; i < count; i++) {
        locals.push_back(Undefined());
      }
    }

    Val *findLocal(size_t index) {
      return &(locals.data()[index]);
    }

    Val *findArg(size_t index) {
      return &(args.data()[index]);
    }

    Val *findCapture(size_t index) {
      return captures.data()[index];
    }

    Object *newObject(Object *prototype);
    String *newString(const string &aStr);
    Symbol *newSymbol(const string &aName);
    Scope *newScope(std::vector<Val> args, std::vector<Val *>captures);
    Val call(FunctionBody func, std::vector<Val> args, std::vector<Val *>captures);

    void markRefsForGC() override;
    string dump() override;
  };

}
