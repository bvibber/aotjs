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

  extern Typeof typeof_undefined;
  extern Typeof typeof_number;
  extern Typeof typeof_boolean;
  extern Typeof typeof_string;
  extern Typeof typeof_symbol;
  extern Typeof typeof_function;
  extern Typeof typeof_object;

  class Val;
  class Local;

  class GCThing;
  class Scope;
  class Frame;

  class JSThing;

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

  typedef Local (*FunctionBody)(Engine& engine, Function& func, Frame& frame);

  ///
  /// Represents an entire JS world.
  ///
  /// Garbage collection is run when gc() is called manually,
  /// or just let everything deallocate when the engine is
  /// destroyed.
  ///
  class Engine {
    Object *mRoot;
    Scope *mScope;
    Frame *mFrame;

    // Set of all live objects.
    // Todo: replace this with an allocator we know how to walk!
    unordered_set<GCThing *> mObjects;
    void registerForGC(GCThing& aObj);
    friend class GCThing;

    Frame& pushFrame(Function& aFunc, Val aThis, std::vector<Val> aArgs);
    void popFrame();

  public:
    // Todo allow specializing the root object at instantiation time?
    Engine();

    // ick!
    void setRoot(Object& aRoot) {
      mRoot = &aRoot;
    }

    Object& root() const {
      return *mRoot;
    }

    Local call(Local aFunc, Local aThis, std::vector<Val> aArgs);

    void gc();
    string dump();
  };

  ///
  /// Base class for an item that can be garbage-collected and may
  /// reference other GC-able items.
  ///
  /// Not necessarily exposed to JS.
  ///
  class GCThing {
    ///
    /// Reference count for stack-frame-based refs via Local.
    /// If count is non-zero, GC treats object as referenced
    /// and keeps it alive.
    ///
    /// This is needed because there's no way to introspect
    /// native WebAssembly stack frames for their local values.
    ///
    size_t mRefCount;

    ///
    /// GC mark state -- normally false, except during GC marking
    /// when it records true if the object is reachable.
    ///
    bool mMarked;

  public:
    GCThing(Engine& aEngine)
    :
      mRefCount(0),
      mMarked(false)
    {
      // We need a set of *all* allocated objects to do sweep.
      // todo: put this in the allocator?
      aEngine.registerForGC(*this);
    }

    virtual ~GCThing();

    // To be called only by Local...
    void retain() {
      #ifdef DEBUG
      std::cout << "retain(" << mRefCount << "++) " << this << " " << dump() << "\n";
      #endif
      mRefCount++;
    }

    void release() {
      #ifdef DEBUG
      std::cout << "release(" << mRefCount << "--) " << this << " " << dump() << "\n";
      if (mRefCount == 0) {
        // should not happen
        std::abort();
      }
      #endif
      mRefCount--;
    }

    // To be called only by Engine...

    size_t refCount() const {
      return mRefCount;
    }

    bool isMarkedForGC() const {
      return mMarked;
    }

    void markForGC() {
      if (!isMarkedForGC()) {
        mMarked = true;
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

  ///
  /// Represents a GC-able item that can be exposed to JavaScript.
  ///
  class JSThing : public GCThing {
  public:
    JSThing(Engine& aEngine)
    : GCThing(aEngine)
    {
      //
    }

    ~JSThing() override;

    string dump() override;

    virtual Typeof typeof() const;
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
    // GC'd pointer types:
    static const uint64_t tag_min_gc     = 0b1111111111111101'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_string     = 0b1111111111111101'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_symbol     = 0b1111111111111110'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_object     = 0b1111111111111111'0000000000000000'0000000000000000'0000000000000000;

    Val(const Val &aVal) : mRaw(aVal.raw()) {}
    Val(double aVal)     : mDouble(aVal) {}
    Val(int32_t aVal)    : mRaw(((uint64_t)((int64_t)aVal) & ~tag_mask) | tag_int32) {}
    Val(bool aVal)       : mRaw(((uint64_t)aVal & ~tag_mask) | tag_bool) {}
    Val(Undefined aVal)  : mRaw(tag_undefined) {}
    Val(Null aVal)       : mRaw(tag_null) {}
    Val(String* aVal)    : mRaw((reinterpret_cast<uint64_t>(aVal) & ~tag_mask) | tag_string) {}
    Val(Symbol* aVal)    : mRaw((reinterpret_cast<uint64_t>(aVal) & ~tag_mask) | tag_symbol) {}
    Val(Object* aVal)    : mRaw((reinterpret_cast<uint64_t>(aVal) & ~tag_mask) | tag_object) {}
    Val(String& aVal)    : Val(&aVal) {}
    Val(Symbol& aVal)    : Val(&aVal) {}
    Val(Object& aVal)    : Val(&aVal) {}
    // fixme how do we genericize these subclasses?
    // it keeps sending me to the bool variant if I don't declare every variation.
    // and can't use static_cast because it thinks they're not inheritence-related yet.
    Val(Function* aVal)  : Val(reinterpret_cast<Object *>(aVal)) {}
    Val(Function& aVal)  : Val(&aVal) {}


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

    bool isJSThing() const {
      // Another clever thing.
      // All pointer types will have at least this value!
      return mRaw >= tag_min_gc;
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

    bool isFunction() const {
      // currently no room for a separate function type. not sure about this.
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

    JSThing& asJSThing() const {
      return *static_cast<JSThing *>(asPointer());
    }

    String& asString() const {
      return *static_cast<String *>(asPointer());
    }

    Symbol& asSymbol() const {
      return *static_cast<Symbol *>(asPointer());
    }

    Object& asObject() const {
      return *static_cast<Object *>(asPointer());
    }

    Function& asFunction() const {
      return *static_cast<Function *>(asPointer());
    }

    bool operator==(const Val &rhs) const;

    string dump() const;
  };

  ///
  /// Represents a JavaScript variable (Val) wrapped in a reference count.
  /// Using this for local variables keeps them alive in case of GC, since
  /// there's no way to walk the WebAssembly stack to find them.
  ///
  class Local {
    Val mVal;

  public:
    Local()
    : mVal(Undefined())
    {
      // primitive
    }

    Local(double aVal)
    : mVal(aVal)
    {
      // primitive
    }

    Local(int32_t aVal)
    : mVal(aVal)
    {
      // primitive
    }

    Local(bool aVal)
    : mVal(aVal)
    {
      // primitive
    }

    Local(Null aVal)
    : mVal(aVal)
    {
      // primitive
    }

    Local(Undefined aVal)
    : mVal(aVal)
    {
      // primitive
    }

    Local(GCThing *aVal)
    : mVal(aVal)
    {
      aVal->retain();
    }

    Local(String *aVal)
    : mVal(aVal) {
      reinterpret_cast<GCThing*>(aVal)->retain();
    }

    Local(Symbol *aVal)
    : mVal(aVal)
    {
      reinterpret_cast<GCThing*>(aVal)->retain();
    }

    Local(Object *aVal)
    : mVal(aVal)
    {
      reinterpret_cast<GCThing*>(aVal)->retain();
    }

    Local(Function *aVal)
    : mVal(aVal)
    {
      reinterpret_cast<GCThing*>(aVal)->retain();
    }

    Local(String& aVal) : Local(&aVal) {}
    Local(Symbol& aVal) : Local(&aVal) {}
    Local(Object& aVal) : Local(&aVal) {}
    Local(Function& aVal) : Local(&aVal) {}

    Local(Val aVal)
    : mVal(aVal)
    {
      if (mVal.isJSThing()) {
        mVal.asJSThing().retain();
      }
    }

    // copy constructor
    Local(const Local &aLocal)
    : mVal(aLocal.mVal)
    {
      if (mVal.isJSThing()) {
        mVal.asJSThing().retain();
      }
    }

    // assignment operator also needed
    // or else everything goes to hell
    //
    // todo make these for primitive types too
    Local &operator=(const Local &aLocal) {
      if (isJSThing()) {
        mVal.asJSThing().release();
      }
      mVal = aLocal.mVal;
      if (isJSThing()) {
        mVal.asJSThing().retain();
      }
      return *this;
    }

    // todo implement move semantics?

    ~Local() {
      if (mVal.isJSThing()) {
        mVal.asJSThing().release();
      }
    }

    bool isDouble() const {
      return mVal.isDouble();
    }

    bool isInt32() const {
      return mVal.isInt32();
    }

    bool isBool() const {
      return mVal.isBool();
    }

    bool isJSThing() const {
      return mVal.isJSThing();
    }

    bool isString() const {
      return mVal.isString();
    }

    bool isSymbol() const {
      return mVal.isSymbol();
    }

    bool isObject() const {
      return mVal.isObject();
    }

    bool isFunction() const {
      return mVal.isFunction();
    }

    JSThing& asJSThing() const {
      return mVal.asJSThing();
    }

    String& asString() const {
      return mVal.asString();
    }

    Symbol& asSymbol() const {
      return mVal.asSymbol();
    }

    Object& asObject() const {
      return mVal.asObject();
    }

    Function& asFunction() const {
      return mVal.asFunction();
    }

    bool operator==(const Local &rhs) const {
      return mVal == rhs.mVal;
    }

    operator Val() const {
      return mVal;
    }

    string dump() const {
      return mVal.dump();
    }
  };

}

namespace std {
  template<> struct hash<::AotJS::Val> {
    size_t operator()(::AotJS::Val const& ref) const noexcept;
  };
}

namespace AotJS {

  class PropIndex : public JSThing {
  public:
    PropIndex(Engine& aEngine)
    : JSThing(aEngine) {}

    ~PropIndex() override;
  };

  class String : public PropIndex {
    string data;

  public:
    String(Engine& aEngine, string const &aStr)
    : PropIndex(aEngine),
      data(aStr)
    {
      //
    }

    ~String() override;

    Typeof typeof() const override;

    string dump() override;

    operator string() const {
      return data;
    }

    bool operator==(const String &rhs) const {
      return data == rhs.data;
    }
  };

  class Symbol : public PropIndex {
    string name;

  public:
    Symbol(Engine& aEngine, string const &aName)
    : PropIndex(aEngine),
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
  /// a prototype chain.
  ///
  /// Todo: throw exceptions on bad prop lookups
  /// Todo: implement getters, setters, enumeration, etc
  ///
  class Object : public JSThing {
    Object *mPrototype;
    unordered_map<Val,Val> mProps;

  public:
    Object(Engine& aEngine)
    : JSThing(aEngine),
      mPrototype(nullptr)
    {
      //
    }

    Object(Engine& aEngine, Object& aPrototype)
    : JSThing(aEngine),
      mPrototype(&aPrototype)
    {
      // Note this doesn't call the constructor,
      // which would be done by outside code.
    }

    ~Object() override;

    void markRefsForGC() override;
    string dump() override;
    Typeof typeof() const override;

    Local getProp(Local name);
    void setProp(Local name, Local val);
  };

  ///
  /// Represents a JS lexical capture scope.
  /// Contains local variables, and references the outer scopes.
  ///
  class Scope : public GCThing {
    Scope *mParent;
    std::vector<Val> mLocals;

  public:
    Scope(Engine& aEngine, Scope& aParent, size_t aCount)
    : GCThing(aEngine),
      mParent(&aParent),
      mLocals(aCount, Undefined())
    {
      //
    }

    ~Scope() override;

    Val& local(size_t aIndex) {
      return mLocals[aIndex];
    }

    Scope &parent() const {
      return *mParent;
    }

    void markRefsForGC() override;
    string dump() override;
  };

  ///
  /// Represents a runtime function object.
  /// References the
  ///
  class Function : public Object {
    Scope *mScope;
    FunctionBody mBody;
    std::string mName;
    size_t mArity;
    std::vector<Val *> mCaptures;

  public:
    // For function with no captures
    Function(Engine& aEngine,
      FunctionBody aBody,
      std::string aName,
      size_t aArity)
    : Object(aEngine), // todo: have a function prototype object!
      mBody(aBody),
      mName(aName),
      mArity(aArity),
      mScope(nullptr),
      mCaptures()
    {
      //
    }

    // For function with captures
    Function(Engine& aEngine,
      FunctionBody aBody,
      std::string aName,
      size_t aArity,
      Scope& aScope,
      std::vector<Val *> aCaptures)
    : Object(aEngine), // todo: have a function prototype object!
      mBody(aBody),
      mName(aName),
      mArity(aArity),
      mScope(&aScope),
      mCaptures(aCaptures)
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

    ///
    /// The actual function pointer.
    ///
    FunctionBody body() const {
      return mBody;
    }

    ///
    /// The lexical capture scope.
    ///
    Scope& scope() const {
      return *mScope;
    }

    ///
    /// Return one of the captured variable pointers.
    ///
    Val& capture(size_t aIndex) {
      return *mCaptures[aIndex];
    }

    void markRefsForGC() override;
    string dump() override;
  };

  ///
  /// Represents a JS stack frame.
  /// Contains function reference, `this` pointer, and arguments.
  ///
  class Frame : public GCThing {
    Frame *mParent;
    Function *mFunc;
    Val mThis;
    size_t mArity;
    std::vector<Val> mArgs;

  public:
    Frame(Engine& aEngine,
      Frame& aParent,
      Function& aFunc,
      Val aThis,
      std::vector<Val> aArgs)
    : GCThing(aEngine),
      mParent(&aParent),
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

    Frame& parent() const {
      return *mParent;
    }

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
    Val& arg(size_t index) {
      // Args are always at the start of the locals array.
      return mArgs[index];
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
