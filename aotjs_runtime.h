#ifndef AOTJS_RUNTIME
#define AOTJS_RUNTIME

#ifdef DEBUG
#include <iostream>
#endif

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

  typedef const char *Typeof;

  extern Typeof typeof_jsthing;
  extern Typeof typeof_undefined;
  extern Typeof typeof_number;
  extern Typeof typeof_boolean;
  extern Typeof typeof_string;
  extern Typeof typeof_symbol;
  extern Typeof typeof_function;
  extern Typeof typeof_object;

  class Val;
}

namespace std {
  template<> struct hash<::AotJS::Val> {
    size_t operator()(::AotJS::Val const& ref) const noexcept;
  };
}

namespace AotJS {
  typedef Val* Binding;
  class Local;

  template <class T>
  class Retained;

  class GCThing;
  class Cell;
  class Frame;

  class PropIndex;
  class String;
  class Symbol;

  class Object;
  class Engine;
  class Function;

  class Undefined {
    // just a tag
  };

  class Null {
    // just a tag
  };

  class Deleted {
    // just a tag
  };

  typedef Local (*FunctionBody)(Function& func, Frame& frame);

  ///
  /// Represents an entire JS world.
  ///
  /// Garbage collection is run when gc() is called manually,
  /// or just let everything deallocate when the engine is
  /// destroyed.
  ///
  class Engine {
    // Global root object.
    Object *mRoot;

    // a stack of pointers to values on the 'real' stack.
    // Or, in theory we could scan the real stack but may have false values
    // and could run into problems with values optimized into locals which
    // aren't on the emscripten actual stack in wasm!
    std::vector<Val> mLocalStack;

    // Set of all live objects.
    // Todo: replace this with an allocator we know how to walk!
    unordered_set<GCThing *> mObjects;
    void registerForGC(GCThing& aObj);
    friend class GCThing;

    friend class Local;
    Val* pushLocal(Val ref);
    void popLocal();

  public:
    static const size_t defaultStackSize = 256 * 1024;

    // Todo allow specializing the root object at instantiation time?
    Engine(size_t aStackSize);

    Engine()
    : Engine(defaultStackSize)
    {
      //
    }

    // ick!
    void setRoot(Object& aRoot) {
      mRoot = &aRoot;
    }

    Object* root() const {
      return mRoot;
    }

    void gc();
    string dump();
  };

  // Singleton
  extern Engine* engine_singleton;

  // Singleton access
  static inline Engine& engine() {
    return *engine_singleton;
  }

  ///
  /// Base class for an item that can be garbage-collected and may
  /// reference other GC-able items.
  ///
  /// Not necessarily exposed to JS.
  ///
  class GCThing {
    ///
    /// GC mark state -- normally false, except during GC marking
    /// when it records true if the object is reachable.
    ///
    bool mMarked;

  public:
    GCThing()
    :
      mMarked(false)
    {
      #ifdef FORCE_GC
      // Force garbage collection to happen on every allocation.
      // Should shake out some bugs.
      engine().gc();
      #endif

      // We need a set of *all* allocated objects to do sweep.
      // todo: put this in the allocator?
      engine().registerForGC(*this);
    }

    virtual ~GCThing();

    // To be called only by Engine...

    bool isMarkedForGC() const {
      return mMarked;
    }

    void markForGC() {
      if (!isMarkedForGC()) {
        mMarked = true;
        #ifdef DEBUG
        std::cerr << "marking object live " << dump() << "\n";
        #endif
        markRefsForGC();
      }
    }

    void clearForGC() {
      mMarked = false;
    }

    virtual void markRefsForGC();

    // Really public!
    virtual string dump();
  };

  // Internal classes that should not be exposed to JS
  class Internal : public GCThing {
  public:
    Internal()
    : GCThing()
    {
      //
    }

    ~Internal() override;
  };

  ///
  /// Child classes are JS-exposed objects, but not necessarily Objects.
  ///
  class JSThing : public GCThing {
  public:
    JSThing()
    : GCThing()
    {
      //
    }

    ~JSThing() override;

    virtual Typeof typeof() const;

    Retained<String> toString() const virtual;
    Local operator+(const Local& rhs) const virtual;
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
  class alignas(8) Val {
    union {
      uint64_t mRaw;
      int32_t mInt32;
      double mDouble;
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
    static const uint64_t tag_deleted    = 0b1111111111111101'0000000000000000'0000000000000000'0000000000000000;
    // GC'd pointer types:
    static const uint64_t tag_min_gc     = 0b1111111111111110'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_internal   = 0b1111111111111110'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_jsthing    = 0b1111111111111111'0000000000000000'0000000000000000'0000000000000000;

    Val(const Val &aVal) : mRaw(aVal.raw()) {}
    Val(double aVal)     : mDouble(aVal) {}
    Val(int32_t aVal)    : mRaw(((uint64_t)((int64_t)aVal) & ~tag_mask) | tag_int32) {}
    Val(bool aVal)       : mRaw(((uint64_t)aVal & ~tag_mask) | tag_bool) {}
    Val(Undefined aVal)  : mRaw(tag_undefined) {}
    Val(Null aVal)       : mRaw(tag_null) {}
    Val(Deleted aVal)    : mRaw(tag_deleted) {}
    Val(Internal* aVal)  : mRaw((reinterpret_cast<uint64_t>(aVal) & ~tag_mask) | tag_internal) {}
    Val(JSThing* aVal)   : mRaw((reinterpret_cast<uint64_t>(aVal) & ~tag_mask) | tag_jsthing) {}
    /*
    // fixme how do we genericize these subclasses?
    // it keeps sending me to the bool variant if I don't declare every variation.
    // and can't use static_cast because it thinks they're not inheritence-related yet.
    */
    Val(Cell* aVal)     : Val(reinterpret_cast<Internal*>(aVal)) {}
    Val(Frame* aVal)    : Val(reinterpret_cast<Internal*>(aVal)) {}
    Val(String* aVal)   : Val(reinterpret_cast<JSThing*>(aVal)) {}
    Val(Symbol* aVal)   : Val(reinterpret_cast<JSThing*>(aVal)) {}
    Val(Function* aVal) : Val(reinterpret_cast<JSThing*>(aVal)) {}
    Val(Object* aVal)   : Val(reinterpret_cast<JSThing*>(aVal)) {}


    Val &operator=(const Val &aVal) {
      mRaw = aVal.mRaw;
      return *this;
    }

    uint64_t raw() const {
      return mRaw;
    }

    uint64_t tag() const {
      return mRaw & tag_mask;
    }

    bool isDouble() const {
      // Saw this trick in SpiderMonkey.
      // Our tagged values will be > tag_max_double in uint64_t interpretation
      // Any non-NaN negative double will be < that
      // Any positive double, inverted, will be < that
      return (mRaw | sign_bit) <= tag_max_double;
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
      return mRaw >= tag_min_gc;
    }

    bool isInternal() const {
      return tag() == tag_internal;
    }

    bool isJSThing() const {
      return tag() == tag_jsthing;
    }

    bool isObject() const {
      return isJSThing() && (asJSThing().typeof() == typeof_object);
    }

    bool isString() const {
      return isJSThing() && (asJSThing().typeof() == typeof_string);
    }

    bool isSymbol() const {
      return isJSThing() && (asJSThing().typeof() == typeof_symbol);
    }

    bool isFunction() const {
      return isJSThing() && (asJSThing().typeof() == typeof_function);
    }

    double asDouble() const {
      // Interpret all bits as double-precision float
      return mDouble;
    }

    int32_t asInt32() const {
      // Bottom 32 bits are ours for ints.
      return mInt32;
    }

    bool asBool() const {
      // Bottom 1 bit is all we need!
      // But treat it like an int32.
      return (bool)mInt32;
    }

    Null asNull() const {
      return Null();
    }

    Undefined asUndefined() const {
      return Undefined();
    }

    Deleted asDeleted() const {
      return Deleted();
    }

    void *asPointer() const {
      #if (PTRDIFF_MAX) > 2147483647
        // 64-bit host -- drop the top 16 bits of NaN and tag.
        // Assumes address space has only 48 significant bits
        // but may be signed, as on x86_64.
        // todo fix, we lost that sign extension. but it works?
        return reinterpret_cast<void *>((mRaw << 16) >> 16);
      #else
        // 32 bit host -- bottom bits are ours, like an int.
        return reinterpret_cast<void *>(mInt32);
      #endif
    }

    // Un-checked conversions returning a raw thingy

    GCThing& asGCThing() const {
      return *static_cast<GCThing *>(asPointer());
    }

    Internal& asInternal() const {
      return *static_cast<Internal *>(asPointer());
    }

    JSThing& asJSThing() const {
      return *static_cast<JSThing*>(asPointer());
    }

    Object& asObject() const {
      return *static_cast<Object *>(asPointer());
    }

    String& asString() const {
      return *static_cast<String *>(asPointer());
    }

    Symbol& asSymbol() const {
      return *static_cast<Symbol *>(asPointer());
    }

    Function& asFunction() const {
      return *static_cast<Function *>(asPointer());
    }

    // Unchecked conversions returning a Retained<T>
    template <class T>
    Retained<T> as() const {
      return new Retained<T>(static_cast<T*>(asPointer()));
    }

    // Checked conversions
    bool toBool() const;
    int32_t toInt32() const;
    double toDouble() const;
    Retained<String> toString() const;

    bool operator==(const Val& rhs) const;

    Local operator+(const Local& rhs) const;

    string dump() const;

    void markForGC() const {
      if (isGCThing()) {
        asGCThing().markForGC();
      }
    }

    Val call(Val aThis, std::vector<Val> aArgs) const;

  };

  ///
  /// GC-safe wrapper cell for a Val allocated on the stack.
  /// Do NOT store a Local in the heap, it will explode!
  ///
  /// The engine keeps a second stack with the actual Val cells,
  /// so while we're in the scope they stay alive during GC. When we go out
  /// of scope we pop back off the stack in correct order.
  ///
  class Local {
    // The Local structure is a single pointer word to the value we want
    // to work with.
    Val* mRecord;

  public:
    Local(Val aVal)
    : mRecord(engine().pushLocal(aVal))
    {
      //
    }

    Local()
    : Local(Undefined())
    {
      //
    }

    ~Local() {
      engine().popLocal();
    }

    // Deref ops
    Val& operator*() const {
      return *mRecord;
    }

    const Val* operator->() const {
      return mRecord;
    }

    // Conversion ops

    operator Val*() const {
      return mRecord;
    }
  };

  template <class T>
  class Retained {
    Local mLocal;

  public:

    Retained(T* aVal)
    : mLocal(aVal)
    {
      //
    }

    template <typename... Args>
    Retained(Args&&... aArgs)
    : mLocal(new T(std::forward<Args>(aArgs)...))
    {
      //
    }

    T* asPointer() const {
      if (mLocal->isGCThing()) {
        // fixme this should be static_cast but it won't let me
        // since it doesn't think they're related by inheritence
        return reinterpret_cast<T*>(&(mLocal->asGCThing()));
      } else {
        // todo: handle
        std::abort();
      }
    }

    // Deref ops

    T& operator*() const {
      return *asPointer();
    }

    T* operator->() const {
      return asPointer();
    }

    // Conversion ops

    operator T*() const {
      return asPointer();
    }

    operator Local() const {
      return mLocal;
    }
  };

  template <class T, typename... Args>
  inline Retained<T> retain(Args&&... aArgs)
  {
    return new T(std::forward<Args>(aArgs)...);
  }

  class PropIndex : public JSThing {
  public:
    PropIndex()
    : JSThing() {}

    ~PropIndex() override;
  };

  class String : public PropIndex {
    string data;

  public:
    String(string const &aStr)
    : PropIndex(),
      data(aStr)
    {
      //
    }

    ~String() override;

    Typeof typeof() const override;

    string dump() override;

    size_t length() const {
      return data.size();
    }

    operator string() const {
      return data;
    }

    bool operator==(const String &rhs) const {
      return data == rhs.data;
    }

    Local operator+(const Local &rhs) const override {
      return retain<String>(data + rhs->toString()->data);
    }
  };

  class Symbol : public PropIndex {
    string name;

  public:
    Symbol(string const &aName)
    : PropIndex(),
      name(aName)
    {
      //
    }

    ~Symbol() override;

    Typeof typeof() const override;

    string dump() override;

    const string &getName() const {
      return name;
    }
  };

  ///
  /// Represents a regular JavaScript object, with properties and
  /// a prototype chain. String and Symbol are subclasses.
  ///
  /// Todo: throw exceptions on bad prop lookups
  /// Todo: implement getters, setters, enumeration, etc
  ///
  class Object : public JSThing {
    Object *mPrototype;
    unordered_map<Val,Val> mProps;

  public:
    Object()
    : JSThing(),
      mPrototype(nullptr)
    {
      //
    }

    Object(Object& aPrototype)
    : JSThing(),
      mPrototype(&aPrototype)
    {
      // Note this doesn't call the constructor,
      // which would be done by outside code.
    }

    ~Object() override;

    void markRefsForGC() override;
    string dump() override;
    Typeof typeof() const override;

    Val getProp(Val name);
    void setProp(Val name, Val val);
  };

  ///
  /// Represents a closure-captured JS variable, allocated on the heap in
  /// this wrapper object.
  ///
  class Cell : public Internal {
    Val mVal;

  public:
    Cell()
    : mVal(Undefined())
    {
      //
    }

    Cell(Val aVal)
    : mVal(aVal)
    {
      //
    }

    ~Cell() override;

    Binding binding() {
      return &mVal;
    }

    Val& val() {
      return mVal;
    }

    void markRefsForGC() override;
    string dump() override;
  };

  ///
  /// Represents a runtime function object.
  /// References the
  ///
  class Function : public Object {
    FunctionBody mBody;
    std::string mName;
    size_t mArity;
    std::vector<Cell*> mCaptures;

  public:
    // For function with no captures
    Function(
      std::string aName,
      size_t aArity,
      FunctionBody aBody)
    : Object(), // todo: have a function prototype object!
      mName(aName),
      mArity(aArity),
      mCaptures(),
      mBody(aBody)
    {
      //
    }

    // For function with captures
    Function(
      std::string aName,
      size_t aArity,
      std::vector<Cell*> aCaptures,
      FunctionBody aBody)
    : Object(), // todo: have a function prototype object!
      mName(aName),
      mArity(aArity),
      mCaptures(aCaptures),
      mBody(aBody)
    {
      //
    }

    ~Function() override;

    Typeof typeof() const override;

    std::string name() const {
      return mName;
    }

    ///
    /// Number of defined arguments
    ///
    size_t arity() const {
      return mArity;
    }

    Val call(Val aThis, std::vector<Val> aArgs);

    ///
    /// Return one of the captured variable pointers.
    ///
    Cell& capture(size_t aIndex) {
      return *mCaptures[aIndex];
    }

    void markRefsForGC() override;
    string dump() override;
  };

  ///
  /// Represents a JS stack frame.
  /// Contains function reference, `this` pointer, and arguments.
  ///
  class Frame : public Internal {
    Function *mFunc;
    Val mThis;
    size_t mArity;
    std::vector<Val> mArgs;

  public:
    Frame(
      Function& aFunc,
      Val aThis,
      std::vector<Val> aArgs)
    : Internal(),
      mFunc(&aFunc),
      mThis(aThis),
      mArity(aArgs.size()),
      mArgs(aArgs)
    {
      // Guarantee the expected arg count is always there,
      // reserving space and initializing them.
      // todo: implement es6 default parameters
      while (mArgs.size() < mFunc->arity()) {
        mArgs.push_back(Undefined());
      }
    }

    ~Frame() override;

    Function& func() const {
      return *mFunc;
    }

    ///
    /// Get a reference to the first argument.
    ///
    /// Guaranteed to be valid up to the number of expected arguments
    /// from the function's arity, even if the actual argument arity
    /// is lower.
    ///
    Val* arg(size_t index) {
      return &mArgs[index];
    }

    ///
    /// Return the actual number of arguments passed.
    ///
    size_t arity() {
      return mArity;
    }

    void markRefsForGC() override;
    string dump() override;
  };

}

#endif
